// player_fit.cpp - see player_fit.h.
//
// The server tests shots on boxes hung on the bones of its player model (the same twenty for every CS model), and the
// models are slimmer than their boxes: a share of what can be hit shows nothing (measured over the animations, from
// the front and the side: a seventh of the boxes' outline for the bulkiest models, over a quarter for the slimmest),
// while a few per cent of each model lies outside every box. Drawn bigger, a model shows more of what can be hit - as
// long as it does not grow out of the boxes:
//   - Every part grows about the middle of its own hit box, so it stays where its box is (the whole model scaled from
//     one point soon has head, hands and feet outside theirs). A bone without a box goes with the nearest boxed bone
//     above it.
//   - A part other boxed parts hang from (the limbs, the trunk) grows across its bone only, not along it: the parts keep
//     meeting where they did (grown along the bone too, the ends of two parts cross at the joint).
//   - An end part (head, hand, foot) grows alike in every direction, and only as far as all its vertices stay inside
//     its own box: the game's heads fill their boxes already, so they stay as they are.
//   - How much: one factor a model, the most (kTop at most) that leaves the share of the model seen with no hit box
//     behind it at kLimit - judged in the model's rest pose from the front and the side, triangle by triangle. So a
//     slim model grows more than a bulky one and all end up filling their boxes about alike (the bulkiest hardly grow).
// Only the bones' matrices change, for the drawing of the body: the vertices are the model's own.
// Used for the beta set's player models only, in full (vismods.cpp).
// (user: "Can you make models a bit bigger maybe?", then "Remove Player Size and make Biggest default for Beta only")
#include "player_fit.h"

#include <math.h>
#include <string.h>
#include <memory>
#include <vector>

namespace
{

const float kTop = 1.30f;	// the most a model's limbs and trunk are thickened
const float kStep = 0.01f;
const float kLimit = 0.13f;	// the share of the model seen with no hit box behind it that growing may reach
const int kShield = 8;		// the hit group of the shield's box (tested only on a player carrying one)

typedef float Matrix[3][4];

// the studio model's own structures (engine/studio.h), as far as they are read here
struct Header	// studiohdr_t
{
	int id;
	int version;
	char name[64];
	int length;
	float eyeposition[3], min[3], max[3], bbmin[3], bbmax[3];
	int flags;
	int numbones, boneindex;
	int numbonecontrollers, bonecontrollerindex;
	int numhitboxes, hitboxindex;
	int numseq, seqindex;
	int numseqgroups, seqgroupindex;
	int numtextures, textureindex, texturedataindex;
	int numskinref, numskinfamilies, skinindex;
	int numbodyparts, bodypartindex;
	int numattachments, attachmentindex;
	int soundtable, soundindex, soundgroups, soundgroupindex;
	int numtransitions, transitionindex;
};
struct Bone	// mstudiobone_t
{
	char name[32];
	int parent;
	int flags;
	int bonecontroller[6];
	float value[6];	// the bone's own place: position, then angles
	float scale[6];
};
struct Box	// mstudiobbox_t
{
	int bone;
	int group;
	float lo[3];
	float hi[3];
};
struct BodyPart	// mstudiobodyparts_t
{
	char name[64];
	int nummodels;
	int base;
	int modelindex;
};
struct Model	// mstudiomodel_t
{
	char name[64];
	int type;
	float boundingradius;
	int nummesh, meshindex;
	int numverts, vertinfoindex, vertindex;
	int numnorms, norminfoindex, normindex;
	int numgroups, groupindex;
};
struct Mesh	// mstudiomesh_t
{
	int numtris;
	int triindex;
	int skinref;
	int numnorms;
	int normindex;
};
static_assert( sizeof( Header ) == 244 && sizeof( Bone ) == 112 && sizeof( Box ) == 32 && sizeof( BodyPart ) == 76
	&& sizeof( Model ) == 112 && sizeof( Mesh ) == 20, "the studio model's structures" );

// a studio model this can read, with its bones inside it
const Header* AsModel( const void* data )
{
	const Header* header = (const Header*)data;
	if ( !header || header->id != 0x54534449 /* IDST */ || header->version != 10 || header->length < (int)sizeof( Header ) )
		return NULL;
	if ( header->numbones <= 0 || header->numbones > PlayerFit::MAX_BONES )
		return NULL;
	return header;
}

// how far into the model is read: nothing looked at here lies in or after the textures' pixels (the last thing in a
// model, and one a model held in memory need not keep)
int Readable( const Header* header )
{
	if ( header->textureindex > 0 && header->texturedataindex >= (int)sizeof( Header ) && header->texturedataindex < header->length )
		return header->texturedataindex;
	return header->length;
}

// 'count' things of 'size' bytes at 'offset', inside the model?
bool Inside( const Header* header, int offset, int count, int size )
{
	return offset > 0 && count >= 0 && (long long)offset + (long long)count * size <= Readable( header );
}

template <typename T>
const T* At( const Header* header, int offset )
{
	return (const T*)( (const unsigned char*)header + offset );
}

bool SameName( const char* a, const char* b, int size )
{
	for ( int i = 0; i < size; i++ )
	{
		char x = a[i], y = b[i];
		x = x >= 'A' && x <= 'Z' ? (char)( x + 32 ) : x;
		y = y >= 'A' && y <= 'Z' ? (char)( y + 32 ) : y;
		if ( x != y )
			return false;
		if ( !x )
			return true;
	}
	return true;
}

int FindBone( const Header* header, const char* name )
{
	const Bone* bones = At<Bone>( header, header->boneindex );
	for ( int i = 0; i < header->numbones; i++ )
	{
		if ( SameName( bones[i].name, name, sizeof( bones[i].name ) ) )
			return i;
	}
	return -1;
}

void Concat( const Matrix a, const Matrix b, Matrix out )
{
	for ( int row = 0; row < 3; row++ )
	{
		for ( int col = 0; col < 4; col++ )
			out[row][col] = a[row][0] * b[0][col] + a[row][1] * b[1][col] + a[row][2] * b[2][col];
		out[row][3] += a[row][3];
	}
}

void Transform( const Matrix m, const float in[3], float out[3] )
{
	for ( int row = 0; row < 3; row++ )
		out[row] = m[row][0] * in[0] + m[row][1] * in[1] + m[row][2] * in[2] + m[row][3];
}

void Rotate( const Matrix m, const float in[3], float out[3] )
{
	for ( int row = 0; row < 3; row++ )
		out[row] = m[row][0] * in[0] + m[row][1] * in[1] + m[row][2] * in[2];
}

// the way back through a matrix (any that has one: the software renderer's bones are not plain turns and moves)
bool Invert( const Matrix m, Matrix out )
{
	const double a = m[0][0], b = m[0][1], c = m[0][2], d = m[1][0], e = m[1][1], f = m[1][2], g = m[2][0], h = m[2][1],
		i = m[2][2];
	const double det = a * ( e * i - f * h ) - b * ( d * i - f * g ) + c * ( d * h - e * g );
	const double size = fabs( a ) + fabs( b ) + fabs( c ) + fabs( d ) + fabs( e ) + fabs( f ) + fabs( g ) + fabs( h ) + fabs( i );
	if ( !( fabs( det ) > 1e-12 * size * size * size ) )
		return false;
	const double inverse[3][3] = {
		{ ( e * i - f * h ) / det, ( c * h - b * i ) / det, ( b * f - c * e ) / det },
		{ ( f * g - d * i ) / det, ( a * i - c * g ) / det, ( c * d - a * f ) / det },
		{ ( d * h - e * g ) / det, ( b * g - a * h ) / det, ( a * e - b * d ) / det },
	};
	for ( int row = 0; row < 3; row++ )
	{
		for ( int col = 0; col < 3; col++ )
			out[row][col] = (float)inverse[row][col];
		out[row][3] = (float)-( inverse[row][0] * m[0][3] + inverse[row][1] * m[1][3] + inverse[row][2] * m[2][3] );
	}
	return true;
}

// a bone's own place against its parent (the engine's AngleQuaternion and QuaternionMatrix on the bone's values)
void BoneMatrix( const float value[6], Matrix m )
{
	const float sy = sinf( value[5] * 0.5f ), cy = cosf( value[5] * 0.5f );
	const float sp = sinf( value[4] * 0.5f ), cp = cosf( value[4] * 0.5f );
	const float sr = sinf( value[3] * 0.5f ), cr = cosf( value[3] * 0.5f );
	const float q[4] = { sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy,
		cr * cp * cy + sr * sp * sy };
	m[0][0] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
	m[1][0] = 2.0f * q[0] * q[1] + 2.0f * q[3] * q[2];
	m[2][0] = 2.0f * q[0] * q[2] - 2.0f * q[3] * q[1];
	m[0][1] = 2.0f * q[0] * q[1] - 2.0f * q[3] * q[2];
	m[1][1] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[2] * q[2];
	m[2][1] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];
	m[0][2] = 2.0f * q[0] * q[2] + 2.0f * q[3] * q[1];
	m[1][2] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
	m[2][2] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[1] * q[1];
	m[0][3] = value[0];
	m[1][3] = value[1];
	m[2][3] = value[2];
}

// how the part of boxed bone 'a' grows along each axis of its bone, at the model factor 'factor'
void Scales( const PlayerFit& fit, int a, float factor, float scale[3] )
{
	if ( fit.axis[a] >= 0 )
	{
		for ( int k = 0; k < 3; k++ )
			scale[k] = k == fit.axis[a] ? 1.0f : factor;
		return;
	}
	const float all = fit.room[a] < factor ? fit.room[a] : factor;
	scale[0] = scale[1] = scale[2] = all;
}

struct Vertex
{
	int carrier;
	float p[3];	// in its carrier's space, from the middle of the carrier's box (no carrier: in the model's space)
};

// the model in its rest pose, for judging how far it can grow
struct Shape
{
	Matrix rest[PlayerFit::MAX_BONES];		// the bones' places
	Matrix back[PlayerFit::MAX_BONES];		// boxed bones: the way back from the model's space to the bone's
	float lo[PlayerFit::MAX_BONES][3];		// boxed bones: the hit box
	float hi[PlayerFit::MAX_BONES][3];
	std::vector<int> boxed;					// the boxed bones
	std::vector<Vertex> vertices;			// the body's
	std::vector<int> triangles;				// three vertex numbers each
	std::vector<float> posed;				// three per vertex, as last grown
};

// does the line through 'point' along 'direction' (both in the model's space) pass through the box of bone 'a'?
bool LineMeetsBox( const Shape& shape, int a, const float point[3], const float direction[3] )
{
	float p[3], d[3];
	Transform( shape.back[a], point, p );
	Rotate( shape.back[a], direction, d );
	float enter = -1e30f, leave = 1e30f;
	for ( int k = 0; k < 3; k++ )
	{
		if ( fabsf( d[k] ) < 1e-9f )
		{
			if ( p[k] < shape.lo[a][k] || p[k] > shape.hi[a][k] )
				return false;
			continue;
		}
		float t0 = ( shape.lo[a][k] - p[k] ) / d[k], t1 = ( shape.hi[a][k] - p[k] ) / d[k];
		if ( t0 > t1 )
		{
			const float swap = t0;
			t0 = t1;
			t1 = swap;
		}
		enter = t0 > enter ? t0 : enter;
		leave = t1 < leave ? t1 : leave;
		if ( enter > leave )
			return false;
	}
	return true;
}

// the share of the model, as seen from the front and from the side, with no hit box behind it - at 'factor'
float SeenUnhittable( const PlayerFit& fit, Shape& shape, float factor )
{
	const size_t count = shape.vertices.size();
	shape.posed.resize( count * 3 );
	for ( size_t i = 0; i < count; i++ )
	{
		const Vertex& vertex = shape.vertices[i];
		float* out = &shape.posed[i * 3];
		if ( vertex.carrier < 0 )
		{
			out[0] = vertex.p[0];
			out[1] = vertex.p[1];
			out[2] = vertex.p[2];
			continue;
		}
		float scale[3], grown[3];
		Scales( fit, vertex.carrier, factor, scale );
		for ( int k = 0; k < 3; k++ )
			grown[k] = fit.centre[vertex.carrier][k] + scale[k] * vertex.p[k];
		Transform( shape.rest[vertex.carrier], grown, out );
	}
	static const float kViews[2][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
	double total = 0.0, outside = 0.0;
	for ( size_t t = 0; t + 2 < shape.triangles.size(); t += 3 )
	{
		const float* a = &shape.posed[(size_t)shape.triangles[t] * 3];
		const float* b = &shape.posed[(size_t)shape.triangles[t + 1] * 3];
		const float* c = &shape.posed[(size_t)shape.triangles[t + 2] * 3];
		const float u[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, v[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
		const float normal[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
		const float middle[3] = { ( a[0] + b[0] + c[0] ) / 3.0f, ( a[1] + b[1] + c[1] ) / 3.0f, ( a[2] + b[2] + c[2] ) / 3.0f };
		for ( int view = 0; view < 2; view++ )
		{
			// the triangle's size as seen from there
			const float seen = 0.5f * fabsf( normal[0] * kViews[view][0] + normal[1] * kViews[view][1] + normal[2] * kViews[view][2] );
			if ( seen <= 0.0f )
				continue;
			total += seen;
			bool met = false;
			for ( size_t i = 0; i < shape.boxed.size() && !met; i++ )
				met = LineMeetsBox( shape, shape.boxed[i], middle, kViews[view] );
			if ( !met )
				outside += seen;
		}
	}
	return total > 0.0 ? (float)( outside / total ) : 0.0f;
}

void Clear( PlayerFit* fit )
{
	memset( fit, 0, sizeof( *fit ) );
	for ( int i = 0; i < PlayerFit::MAX_BONES; i++ )
	{
		fit->carrier[i] = -1;
		fit->axis[i] = -1;
		fit->room[i] = 1.0f;
	}
	fit->factor = 1.0f;
}

// the first sub-model of the model's first body part (the body as it is worn), with everything of it inside the model
const Model* BodyOf( const Header* header )
{
	if ( header->numbodyparts <= 0 || !Inside( header, header->bodypartindex, header->numbodyparts, sizeof( BodyPart ) ) )
		return NULL;
	const BodyPart* part = At<BodyPart>( header, header->bodypartindex );
	if ( part->nummodels <= 0 || !Inside( header, part->modelindex, part->nummodels, sizeof( Model ) ) )
		return NULL;
	const Model* model = At<Model>( header, part->modelindex );
	if ( model->numverts <= 0 || !Inside( header, model->vertindex, model->numverts, 12 )
		|| !Inside( header, model->vertinfoindex, model->numverts, 1 )
		|| model->nummesh <= 0 || !Inside( header, model->meshindex, model->nummesh, sizeof( Mesh ) ) )
		return NULL;
	return model;
}

// the body's triangles, from the meshes' strips and fans
void ReadTriangles( const Header* header, const Model* model, std::vector<int>& triangles )
{
	const Mesh* mesh = At<Mesh>( header, model->meshindex );
	const short* end = (const short*)( (const unsigned char*)header + ( Readable( header ) & ~1 ) );
	for ( int m = 0; m < model->nummesh; m++ )
	{
		if ( mesh[m].triindex <= 0 || mesh[m].triindex >= Readable( header ) )
			continue;
		const short* command = At<short>( header, mesh[m].triindex );
		while ( command < end )
		{
			int count = *command++;
			if ( !count )
				break;
			const bool fan = count < 0;
			count = fan ? -count : count;
			if ( command + (size_t)count * 4 > end )
				return;
			for ( int i = 0; i + 2 < count; i++ )
			{
				// the vertex number leads each corner's four values
				const int corner[3] = { fan ? command[0] : command[i * 4], command[( i + 1 ) * 4], command[( i + 2 ) * 4] };
				if ( corner[0] < 0 || corner[1] < 0 || corner[2] < 0 || corner[0] >= model->numverts
					|| corner[1] >= model->numverts || corner[2] >= model->numverts )
					continue;
				triangles.push_back( corner[0] );
				triangles.push_back( corner[1] );
				triangles.push_back( corner[2] );
			}
			command += (size_t)count * 4;
		}
	}
}

} // namespace

void PlayerFit_Plan( const void* drawnData, const void* serverData, PlayerFit* fit )
{
	if ( !fit )
		return;
	Clear( fit );
	const Header* drawn = AsModel( drawnData );
	const Header* server = AsModel( serverData );
	if ( !drawn || !Inside( drawn, drawn->boneindex, drawn->numbones, sizeof( Bone ) ) )
		return;
	const Bone* bones = At<Bone>( drawn, drawn->boneindex );
	for ( int i = 0; i < drawn->numbones; i++ )
	{
		if ( bones[i].parent >= i )
			return;	// a bone's parent always comes before it
	}

	// the hit boxes: the server model's, by the bones' names - unless the drawn model has not all those bones (a model
	// of another skeleton: its own boxes then)
	const Header* boxes = drawn;
	if ( server && server != drawn && Inside( server, server->boneindex, server->numbones, sizeof( Bone ) )
		&& server->numhitboxes > 0 && Inside( server, server->hitboxindex, server->numhitboxes, sizeof( Box ) ) )
	{
		const Box* box = At<Box>( server, server->hitboxindex );
		const Bone* serverBones = At<Bone>( server, server->boneindex );
		bool all = true;
		for ( int i = 0; i < server->numhitboxes && all; i++ )
		{
			all = box[i].bone >= 0 && box[i].bone < server->numbones
				&& ( box[i].group == kShield || FindBone( drawn, serverBones[box[i].bone].name ) >= 0 );
		}
		if ( all )
			boxes = server;
	}
	if ( boxes->numhitboxes <= 0 || !Inside( boxes, boxes->hitboxindex, boxes->numhitboxes, sizeof( Box ) )
		|| !Inside( boxes, boxes->boneindex, boxes->numbones, sizeof( Bone ) ) )
		return;
	std::unique_ptr<Shape> shape( new Shape );
	bool boxed[PlayerFit::MAX_BONES] = {};
	float half[PlayerFit::MAX_BONES][3];
	{
		const Box* box = At<Box>( boxes, boxes->hitboxindex );
		const Bone* boxBones = At<Bone>( boxes, boxes->boneindex );
		for ( int i = 0; i < boxes->numhitboxes; i++ )
		{
			if ( box[i].group == kShield || box[i].bone < 0 || box[i].bone >= boxes->numbones )
				continue;
			const int bone = boxes == drawn ? box[i].bone : FindBone( drawn, boxBones[box[i].bone].name );
			if ( bone < 0 || boxed[bone] )
				continue;	// a bone's first box
			boxed[bone] = true;
			shape->boxed.push_back( bone );
			for ( int k = 0; k < 3; k++ )
			{
				shape->lo[bone][k] = box[i].lo[k];
				shape->hi[bone][k] = box[i].hi[k];
				fit->centre[bone][k] = ( box[i].lo[k] + box[i].hi[k] ) * 0.5f;
				half[bone][k] = ( box[i].hi[k] - box[i].lo[k] ) * 0.5f;
			}
		}
	}
	if ( shape->boxed.empty() )
		return;

	// each bone's carrier, and the model's rest pose
	for ( int i = 0; i < drawn->numbones; i++ )
	{
		int bone = i;
		while ( bone >= 0 && !boxed[bone] )
			bone = bones[bone].parent;
		fit->carrier[i] = (short)bone;
		Matrix own;
		BoneMatrix( bones[i].value, own );
		if ( bones[i].parent < 0 )
			memcpy( shape->rest[i], own, sizeof( Matrix ) );
		else
			Concat( shape->rest[bones[i].parent], own, shape->rest[i] );
	}
	for ( size_t i = 0; i < shape->boxed.size(); i++ )
	{
		if ( !Invert( shape->rest[shape->boxed[i]], shape->back[shape->boxed[i]] ) )
			return;
	}

	// the axis each boxed bone runs along: towards the farthest of the boxed bones hanging from it (none: an end part)
	for ( size_t i = 0; i < shape->boxed.size(); i++ )
	{
		const int a = shape->boxed[i];
		float farthest = -1.0f;
		for ( size_t j = 0; j < shape->boxed.size(); j++ )
		{
			const int b = shape->boxed[j];
			int above = bones[b].parent;
			while ( above >= 0 && !boxed[above] )
				above = bones[above].parent;
			if ( b == a || above != a )
				continue;
			const float origin[3] = { shape->rest[b][0][3], shape->rest[b][1][3], shape->rest[b][2][3] };
			float p[3];
			Transform( shape->back[a], origin, p );
			int along = 0;
			for ( int k = 1; k < 3; k++ )
				along = fabsf( p[k] ) > fabsf( p[along] ) ? k : along;
			if ( fabsf( p[along] ) > farthest )
			{
				farthest = fabsf( p[along] );
				fit->axis[a] = (signed char)along;
			}
		}
	}

	// the body's vertices, each in its carrier's space from the middle of the carrier's box
	const Model* body = BodyOf( drawn );
	if ( !body )
		return;
	const float* verts = At<float>( drawn, body->vertindex );
	const unsigned char* vertBone = At<unsigned char>( drawn, body->vertinfoindex );
	shape->vertices.resize( body->numverts );
	for ( int i = 0; i < body->numverts; i++ )
	{
		Vertex& vertex = shape->vertices[i];
		const int bone = vertBone[i] < drawn->numbones ? vertBone[i] : 0;
		float inModel[3];
		Transform( shape->rest[bone], verts + i * 3, inModel );
		vertex.carrier = fit->carrier[bone];
		if ( vertex.carrier < 0 )
		{
			memcpy( vertex.p, inModel, sizeof( vertex.p ) );
			continue;
		}
		Transform( shape->back[vertex.carrier], inModel, vertex.p );
		for ( int k = 0; k < 3; k++ )
			vertex.p[k] -= fit->centre[vertex.carrier][k];
	}

	// the end parts: how far each can grow inside its own box
	for ( size_t i = 0; i < shape->boxed.size(); i++ )
	{
		const int a = shape->boxed[i];
		if ( fit->axis[a] >= 0 )
			continue;
		float reach[3] = { 0.0f, 0.0f, 0.0f };
		int count = 0;
		for ( size_t v = 0; v < shape->vertices.size(); v++ )
		{
			if ( shape->vertices[v].carrier != a )
				continue;
			count++;
			for ( int k = 0; k < 3; k++ )
				reach[k] = fabsf( shape->vertices[v].p[k] ) > reach[k] ? fabsf( shape->vertices[v].p[k] ) : reach[k];
		}
		if ( count < 4 || reach[0] <= 1e-6f || reach[1] <= 1e-6f || reach[2] <= 1e-6f )
			continue;
		float room = half[a][0] / reach[0];
		for ( int k = 1; k < 3; k++ )
			room = half[a][k] / reach[k] < room ? half[a][k] / reach[k] : room;
		fit->room[a] = room > 1.0f ? room : 1.0f;
	}

	// the model's factor
	ReadTriangles( drawn, body, shape->triangles );
	fit->numBones = drawn->numbones;
	if ( shape->triangles.empty() )
		return;	// (nothing to judge by: as it is)
	const float asMade = SeenUnhittable( *fit, *shape, 1.0f );
	const float limit = asMade > kLimit ? asMade : kLimit;
	const int steps = (int)( ( kTop - 1.0f ) / kStep + 0.5f );
	for ( int i = 1; i <= steps; i++ )
	{
		const float factor = 1.0f + i * kStep;
		if ( SeenUnhittable( *fit, *shape, factor ) > limit )
			break;
		fit->factor = factor;
	}
}

void PlayerFit_Grow( const PlayerFit* fit, float amount, float ( *bones )[3][4] )
{
	if ( !fit || !bones || fit->numBones <= 0 || fit->numBones > PlayerFit::MAX_BONES || !( amount > 0.0f ) )
		return;
	const float factor = 1.0f + ( amount < 1.0f ? amount : 1.0f ) * ( fit->factor - 1.0f );
	// per boxed bone: its matrix grown, and the change as a matrix on the model's space (for the bones it carries) -
	// all from the bones as they are, before any is changed
	static Matrix grown[PlayerFit::MAX_BONES], change[PlayerFit::MAX_BONES];
	bool grows[PlayerFit::MAX_BONES];
	for ( int a = 0; a < fit->numBones; a++ )
	{
		grows[a] = false;
		if ( fit->carrier[a] != a )
			continue;
		float scale[3];
		Scales( *fit, a, factor, scale );
		if ( scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f )
			continue;
		Matrix back;
		if ( !Invert( bones[a], back ) )
			continue;
		// the bone's matrix times: from the box's middle, the growth, back to the middle
		for ( int row = 0; row < 3; row++ )
		{
			grown[a][row][3] = bones[a][row][3];
			for ( int k = 0; k < 3; k++ )
			{
				grown[a][row][k] = bones[a][row][k] * scale[k];
				grown[a][row][3] += bones[a][row][k] * fit->centre[a][k] * ( 1.0f - scale[k] );
			}
		}
		Concat( grown[a], back, change[a] );
		grows[a] = true;
	}
	for ( int i = 0; i < fit->numBones; i++ )
	{
		const int a = fit->carrier[i];
		if ( a < 0 || a >= fit->numBones || !grows[a] )
			continue;
		if ( i == a )
		{
			memcpy( bones[i], grown[a], sizeof( Matrix ) );
			continue;
		}
		Matrix moved;
		Concat( change[a], bones[i], moved );
		memcpy( bones[i], moved, sizeof( Matrix ) );
	}
}
