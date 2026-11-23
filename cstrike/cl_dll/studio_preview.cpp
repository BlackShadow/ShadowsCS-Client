// studio_preview.cpp - see studio_preview.h.
//
// Every read is bounds-checked: a damaged or unusual model file is refused, never read past its end.
// Layouts (engine/studio.h): studiohdr_t 244 bytes, mstudiobone_t 112, mstudioseqdesc_t 176, mstudiobodyparts_t 76,
// mstudiomodel_t 112, mstudiomesh_t 20, mstudiotexture_t 80, mstudioanim_t 12 (six offsets), mstudioanimvalue_t 2.
// The bone maths are the engine's (StudioCalcBoneQuaternion / -Position, AngleQuaternion, QuaternionSlerp,
// QuaternionMatrix, R_ConcatTransforms, StudioMergeBones); the upper / lower body split is GameStudioModelRenderer's.
// Checked against an offline prototype on the CS player and p_ weapon models.
#include "studio_preview.h"

#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

namespace
{

const int kMaxBones = 128;			// MAXSTUDIOBONES
const int kMaxVerts = 2048;			// MAXSTUDIOVERTS
const int kMaxFrames = 4096;
const int kMaxTextureSide = 4096;
const int kMaxTextures = 256;
const int kMaxMeshes = 256;
const int kNfChrome = 0x0002;		// STUDIO_NF_CHROME
const int kNfMasked = 0x0040;		// STUDIO_NF_MASKED: palette index 255 is see-through

class Reader
{
public:
	Reader( const unsigned char* data, int length ) : m_data( data ), m_length( length ), m_bad( false ) {}
	bool Bad() const { return m_bad; }
	bool Has( long long offset, long long bytes )
	{
		if ( offset < 0 || bytes < 0 || offset + bytes > m_length )
		{
			m_bad = true;
			return false;
		}
		return true;
	}
	int Int( long long offset )
	{
		int value = 0;
		if ( Has( offset, 4 ) )
			memcpy( &value, m_data + offset, 4 );
		return value;
	}
	float Float( long long offset )
	{
		float value = 0.0f;
		if ( Has( offset, 4 ) )
			memcpy( &value, m_data + offset, 4 );
		return value;
	}
	short Short( long long offset )
	{
		short value = 0;
		if ( Has( offset, 2 ) )
			memcpy( &value, m_data + offset, 2 );
		return value;
	}
	unsigned short UShort( long long offset )
	{
		unsigned short value = 0;
		if ( Has( offset, 2 ) )
			memcpy( &value, m_data + offset, 2 );
		return value;
	}
	unsigned char Byte( long long offset ) { return Has( offset, 1 ) ? m_data[offset] : 0; }
	const unsigned char* Ptr( long long offset, long long bytes ) { return Has( offset, bytes ) ? m_data + offset : NULL; }

private:
	const unsigned char* m_data;
	int m_length;
	bool m_bad;
};

// one channel (0..5: x y z, x y z rotation) of one bone at a whole frame (StudioCalcBonePosition / -Quaternion)
float AnimValue( Reader& r, long long anim, int channel, int frame, float value, float scale )
{
	const int offset = r.UShort( anim + channel * 2 );
	if ( offset == 0 )
		return value;
	long long p = anim + offset;
	int k = frame;
	for ( int guard = 0; guard < 65536; guard++ )
	{
		const int valid = r.Byte( p ), total = r.Byte( p + 1 );
		if ( r.Bad() )
			return value;
		if ( total > k )
		{
			const short stored = valid > k ? r.Short( p + ( k + 1 ) * 2 ) : r.Short( p + valid * 2 );
			return value + stored * scale;
		}
		if ( total == 0 )
			break;	// would never advance
		k -= total;
		p += ( valid + 1 ) * 2;
	}
	r.Has( -1, 0 );	// malformed: mark the model bad
	return value;
}

void AngleQuaternion( const float angles[3], float quat[4] )
{
	const float sy = sinf( angles[2] * 0.5f ), cy = cosf( angles[2] * 0.5f );
	const float sp = sinf( angles[1] * 0.5f ), cp = cosf( angles[1] * 0.5f );
	const float sr = sinf( angles[0] * 0.5f ), cr = cosf( angles[0] * 0.5f );
	quat[0] = sr * cp * cy - cr * sp * sy;
	quat[1] = cr * sp * cy + sr * cp * sy;
	quat[2] = cr * cp * sy - sr * sp * cy;
	quat[3] = cr * cp * cy + sr * sp * sy;
}

void QuaternionSlerp( const float p[4], const float qIn[4], float t, float out[4] )
{
	float q[4] = { qIn[0], qIn[1], qIn[2], qIn[3] };
	// the shorter way round
	float a = 0.0f, b = 0.0f;
	for ( int i = 0; i < 4; i++ )
	{
		a += ( p[i] - q[i] ) * ( p[i] - q[i] );
		b += ( p[i] + q[i] ) * ( p[i] + q[i] );
	}
	if ( a > b )
	{
		for ( int i = 0; i < 4; i++ )
			q[i] = -q[i];
	}
	const float cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];
	float sclp, sclq;
	if ( 1.0f + cosom > 0.000001f )
	{
		if ( 1.0f - cosom > 0.000001f )
		{
			const float omega = acosf( cosom ), sinom = sinf( omega );
			sclp = sinf( ( 1.0f - t ) * omega ) / sinom;
			sclq = sinf( t * omega ) / sinom;
		}
		else
		{
			sclp = 1.0f - t;
			sclq = t;
		}
		for ( int i = 0; i < 4; i++ )
			out[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		const float half = 1.5707963f;
		const float perpendicular[4] = { -q[1], q[0], -q[3], q[2] };
		sclp = sinf( ( 1.0f - t ) * half );
		sclq = sinf( t * half );
		for ( int i = 0; i < 3; i++ )
			out[i] = sclp * p[i] + sclq * perpendicular[i];
		out[3] = perpendicular[3];
	}
}

void QuaternionMatrix( const float q[4], const float pos[3], float m[3][4] )
{
	m[0][0] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
	m[1][0] = 2.0f * q[0] * q[1] + 2.0f * q[3] * q[2];
	m[2][0] = 2.0f * q[0] * q[2] - 2.0f * q[3] * q[1];
	m[0][1] = 2.0f * q[0] * q[1] - 2.0f * q[3] * q[2];
	m[1][1] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[2] * q[2];
	m[2][1] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];
	m[0][2] = 2.0f * q[0] * q[2] + 2.0f * q[3] * q[1];
	m[1][2] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
	m[2][2] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[1] * q[1];
	m[0][3] = pos[0];
	m[1][3] = pos[1];
	m[2][3] = pos[2];
}

void ConcatTransforms( const float a[3][4], const float b[3][4], float out[3][4] )
{
	for ( int row = 0; row < 3; row++ )
	{
		for ( int col = 0; col < 4; col++ )
		{
			out[row][col] = a[row][0] * b[0][col] + a[row][1] * b[1][col] + a[row][2] * b[2][col];
			if ( col == 3 )
				out[row][col] += a[row][3];
		}
	}
}

// a 32-byte name field (not always terminated) against a name, ignoring case
bool NameIs( const unsigned char* field, int size, const char* name )
{
	int i = 0;
	for ( ; i < size && field[i]; i++ )
	{
		if ( !name[i] || tolower( field[i] ) != tolower( (unsigned char)name[i] ) )
			return false;
	}
	return name[i] == 0;
}

// the sequence named 'name' (none: -1)
int FindSequence( Reader& r, int seqIndex, int numSeq, const char* name )
{
	for ( int i = 0; name && i < numSeq; i++ )
	{
		const unsigned char* label = r.Ptr( seqIndex + i * 176LL, 32 );
		if ( label && NameIs( label, 32, name ) )
			return i;
	}
	return -1;
}

} // namespace

CStudioPreviewModel::CStudioPreviewModel() : m_bLoaded( false ), m_iNumBones( 0 ), m_bHandMove( false ), m_bFit( false )
{
	m_main.numFrames = m_legs.numFrames = 0;
	m_main.fps = m_legs.fps = 0.0f;
	memset( m_handMove, 0, sizeof( m_handMove ) );
	memset( &m_fit, 0, sizeof( m_fit ) );
}

void CStudioPreviewModel::SetHandMove( const float* rows )
{
	m_bHandMove = rows != 0;
	if ( rows )
		memcpy( m_handMove, rows, sizeof( m_handMove ) );
}

void CStudioPreviewModel::SetFit( const PlayerFit& fit )
{
	m_fit = fit;
	m_bFit = m_bLoaded && fit.numBones == m_iNumBones;
}

bool CStudioPreviewModel::Load( const unsigned char* data, int length, const char* sequence, const char* legs )
{
	*this = CStudioPreviewModel();
	Reader r( data, length );
	if ( !data || length < 244 || memcmp( data, "IDST", 4 ) || r.Int( 4 ) != 10 )
		return false;
	const int numBones = r.Int( 140 ), boneIndex = r.Int( 144 );
	const int numSeq = r.Int( 164 ), seqIndex = r.Int( 168 );
	const int numTextures = r.Int( 180 ), textureIndex = r.Int( 184 );
	const int numSkinRef = r.Int( 192 ), skinIndex = r.Int( 200 );
	const int numBodyParts = r.Int( 204 ), bodyPartIndex = r.Int( 208 );
	if ( numBones < 1 || numBones > kMaxBones || numSeq < 1 || numBodyParts < 1 || numBodyParts > 32 )
		return false;
	if ( !r.Has( boneIndex, numBones * 112LL ) || !r.Has( seqIndex, numSeq * 176LL ) )
		return false;

	// bones
	m_iNumBones = numBones;
	m_boneParent.resize( numBones );
	m_boneName.resize( numBones );
	std::vector<float> value( (size_t)numBones * 6 ), scale( (size_t)numBones * 6 );
	for ( int b = 0; b < numBones; b++ )
	{
		const long long bone = boneIndex + b * 112LL;
		const unsigned char* name = r.Ptr( bone, 32 );
		const int parent = r.Int( bone + 32 );
		if ( !name || parent < -1 || parent >= b )
			return false;	// parents always come first
		m_boneParent[b] = parent;
		m_boneName[b].assign( (const char*)name, strnlen( (const char*)name, 32 ) );
		if ( m_headBone.size() != (size_t)numBones )
			m_headBone.assign( numBones, false );
		m_headBone[b] = !_stricmp( m_boneName[b].c_str(), "Bip01 Head" ) || ( parent >= 0 && m_headBone[parent] );
		for ( int j = 0; j < 6; j++ )
		{
			value[(size_t)b * 6 + j] = r.Float( bone + 64 + j * 4 );
			scale[(size_t)b * 6 + j] = r.Float( bone + 88 + j * 4 );
		}
	}

	// the animations (sequence group 0: their data is in this file); a 9-way aim blend: the middle one, aiming level
	auto readAnimation = [&]( int seq, Animation& out ) -> bool {
		const long long seqDesc = seqIndex + seq * 176LL;
		const float fps = r.Float( seqDesc + 32 );
		const int numFrames = r.Int( seqDesc + 56 );
		const int numBlends = r.Int( seqDesc + 120 );
		const int animIndex = r.Int( seqDesc + 124 );
		const int seqGroup = r.Int( seqDesc + 156 );
		if ( seqGroup != 0 || numFrames < 1 || numFrames > kMaxFrames || numBlends < 1 || numBlends > 16 )
			return false;
		const long long anims = animIndex + ( numBlends > 1 ? numBlends / 2 : 0 ) * ( numBones * 12LL );
		if ( !r.Has( anims, numBones * 12LL ) )
			return false;
		out.numFrames = numFrames;
		out.fps = fps > 0.0f && fps < 1000.0f ? fps : 10.0f;
		out.frames.resize( (size_t)numFrames * numBones );
		for ( int b = 0; b < numBones; b++ )
		{
			const float* v = &value[(size_t)b * 6];
			const float* s = &scale[(size_t)b * 6];
			const long long anim = anims + b * 12LL;
			for ( int f = 0; f < numFrames; f++ )
			{
				Frame& frame = out.frames[(size_t)f * numBones + b];
				float angles[3];
				for ( int j = 0; j < 3; j++ )
				{
					frame.pos[j] = AnimValue( r, anim, j, f, v[j], s[j] );
					angles[j] = AnimValue( r, anim, j + 3, f, v[j + 3], s[j + 3] );
				}
				AngleQuaternion( angles, frame.quat );
			}
		}
		return !r.Bad();
	};
	const int mainSeq = FindSequence( r, seqIndex, numSeq, sequence );
	if ( !readAnimation( mainSeq >= 0 ? mainSeq : 0, m_main ) )
		return false;
	const int legsSeq = FindSequence( r, seqIndex, numSeq, legs );
	m_fromLegs.assign( numBones, false );
	if ( legsSeq >= 0 && readAnimation( legsSeq, m_legs ) )
	{
		// GameStudioModelRenderer.cpp: the bones before "Bip01 Spine", and every bone from one whose parent is
		// "Bip01 Pelvis" (the legs) on, come from the walk / idle animation
		bool copy = true;
		for ( int b = 0; b < numBones; b++ )
		{
			if ( m_boneName[b] == "Bip01 Spine" )
				copy = false;
			else if ( copy || ( m_boneParent[b] >= 0 && m_boneName[m_boneParent[b]] == "Bip01 Pelvis" ) )
			{
				m_fromLegs[b] = true;
				copy = true;
			}
		}
	}
	else
		m_legs = Animation();
	if ( r.Bad() )
		return false;

	// the skins
	if ( numTextures < 0 || numTextures > kMaxTextures || ( numTextures > 0 && !r.Has( textureIndex, numTextures * 80LL ) ) )
		return false;
	m_textures.resize( numTextures );
	for ( int t = 0; t < numTextures; t++ )
	{
		const long long texture = textureIndex + t * 80LL;
		Texture& out = m_textures[t];
		out.flags = r.Int( texture + 64 );
		out.width = r.Int( texture + 68 );
		out.height = r.Int( texture + 72 );
		const int index = r.Int( texture + 76 );
		if ( out.width < 1 || out.height < 1 || out.width > kMaxTextureSide || out.height > kMaxTextureSide )
			return false;
		const long long pixels = (long long)out.width * out.height;
		const unsigned char* indices = r.Ptr( index, pixels + 768 );
		if ( !indices )
			return false;
		const unsigned char* palette = indices + pixels;
		out.rgba.resize( (size_t)pixels * 4 );
		for ( long long i = 0; i < pixels; i++ )
		{
			const int c = indices[i];
			out.rgba[(size_t)i * 4 + 0] = palette[c * 3 + 0];
			out.rgba[(size_t)i * 4 + 1] = palette[c * 3 + 1];
			out.rgba[(size_t)i * 4 + 2] = palette[c * 3 + 2];
			out.rgba[(size_t)i * 4 + 3] = ( out.flags & kNfMasked ) && c == 255 ? 0 : 255;
		}
	}
	if ( numSkinRef < 0 || numSkinRef > kMaxTextures || ( numSkinRef > 0 && !r.Has( skinIndex, numSkinRef * 2LL ) ) )
		return false;

	// the geometry: body 0 of every body part
	for ( int part = 0; part < numBodyParts; part++ )
	{
		const long long bodyPart = bodyPartIndex + part * 76LL;
		const int numModels = r.Int( bodyPart + 64 ), modelIndex = r.Int( bodyPart + 72 );
		if ( r.Bad() )
			return false;
		if ( numModels < 1 )
			continue;
		const long long model = modelIndex;	// model 0
		const int numMesh = r.Int( model + 72 ), meshIndex = r.Int( model + 76 );
		const int numVerts = r.Int( model + 80 ), vertInfo = r.Int( model + 84 ), vertIndex = r.Int( model + 88 );
		const int numNorms = r.Int( model + 92 ), normInfo = r.Int( model + 96 ), normIndex = r.Int( model + 100 );
		if ( r.Bad() )
			return false;
		if ( numVerts == 0 || numMesh == 0 )
			continue;	// an empty choice (no backpack)
		if ( numVerts < 0 || numVerts > kMaxVerts || numNorms < 1 || numNorms > kMaxVerts || numMesh < 0 || numMesh > kMaxMeshes )
			return false;
		if ( !r.Has( vertInfo, numVerts ) || !r.Has( vertIndex, numVerts * 12LL ) || !r.Has( normInfo, numNorms )
			 || !r.Has( normIndex, numNorms * 12LL ) || !r.Has( meshIndex, numMesh * 20LL ) )
			return false;
		const int vertBase = (int)m_vertBone.size(), normBase = (int)m_normBone.size();
		for ( int v = 0; v < numVerts; v++ )
		{
			const int bone = r.Byte( vertInfo + v );
			if ( bone >= numBones )
				return false;
			m_vertBone.push_back( bone );
			for ( int j = 0; j < 3; j++ )
				m_verts.push_back( r.Float( vertIndex + v * 12LL + j * 4 ) );
		}
		for ( int n = 0; n < numNorms; n++ )
		{
			const int bone = r.Byte( normInfo + n );
			if ( bone >= numBones )
				return false;
			m_normBone.push_back( bone );
			for ( int j = 0; j < 3; j++ )
				m_norms.push_back( r.Float( normIndex + n * 12LL + j * 4 ) );
		}
		for ( int m = 0; m < numMesh; m++ )
		{
			const long long meshDesc = meshIndex + m * 20LL;
			const int triIndex = r.Int( meshDesc + 4 ), skinRef = r.Int( meshDesc + 8 );
			Mesh mesh;
			mesh.texture = -1;
			if ( skinRef >= 0 && skinRef < numSkinRef )
			{
				const int texture = r.Short( skinIndex + skinRef * 2LL );	// skin family 0
				if ( texture >= 0 && texture < numTextures )
					mesh.texture = texture;
			}
			mesh.chrome = mesh.texture >= 0 && ( m_textures[mesh.texture].flags & kNfChrome ) != 0;
			// triangle strips (count > 0) and fans (count < 0) of vertex/normal/s/t, ended by 0
			long long p = triIndex;
			std::vector<Corner> list;
			for ( int guard = 0; guard < 65536; guard++ )
			{
				int count = r.Short( p );
				p += 2;
				if ( r.Bad() )
					return false;
				if ( count == 0 )
					break;
				const bool fan = count < 0;
				count = abs( count );
				if ( count > 4096 || !r.Has( p, count * 8LL ) )
					return false;
				list.resize( count );
				for ( int i = 0; i < count; i++ )
				{
					const int vert = r.Short( p ), norm = r.Short( p + 2 );
					if ( vert < 0 || vert >= numVerts || norm < 0 || norm >= numNorms )
						return false;
					list[i].vert = vertBase + vert;
					list[i].norm = normBase + norm;
					list[i].s = (float)r.Short( p + 4 );
					list[i].t = (float)r.Short( p + 6 );
					p += 8;
				}
				for ( int i = 2; i < count; i++ )
				{
					if ( fan )
					{
						mesh.corners.push_back( list[0] );
						mesh.corners.push_back( list[i - 1] );
					}
					else if ( i & 1 )
					{
						mesh.corners.push_back( list[i - 1] );
						mesh.corners.push_back( list[i - 2] );
					}
					else
					{
						mesh.corners.push_back( list[i - 2] );
						mesh.corners.push_back( list[i - 1] );
					}
					mesh.corners.push_back( list[i] );
				}
			}
			if ( !mesh.corners.empty() )
				m_meshes.push_back( mesh );
		}
	}
	if ( r.Bad() || m_meshes.empty() )
	{
		*this = CStudioPreviewModel();
		return false;
	}
	m_bLoaded = true;
	return true;
}

// one bone's local position / rotation 'time' seconds into a looping animation (the engine loops over numframes - 1
// frames, the last one repeating the first; frames blended)
void CStudioPreviewModel::LocalPose( const Animation& animation, double time, int bone, float pos[3], float quat[4] ) const
{
	const int period = animation.numFrames > 1 ? animation.numFrames - 1 : 1;
	double f = fmod( time * animation.fps, (double)period );
	if ( f < 0.0 )
		f += period;
	int f0 = (int)f;
	if ( f0 >= period )
		f0 = period - 1;
	const int f1 = animation.numFrames > 1 ? f0 + 1 : f0;
	const float blend = (float)( f - f0 );
	const Frame& a = animation.frames[(size_t)f0 * m_iNumBones + bone];
	const Frame& c = animation.frames[(size_t)f1 * m_iNumBones + bone];
	QuaternionSlerp( a.quat, c.quat, blend, quat );
	for ( int j = 0; j < 3; j++ )
		pos[j] = a.pos[j] + ( c.pos[j] - a.pos[j] ) * blend;
}

void CStudioPreviewModel::ComputeBones( double time, BoneMatrix* bones ) const
{
	if ( !m_bLoaded )
		return;
	for ( int b = 0; b < m_iNumBones; b++ )
	{
		float pos[3], quat[4], local[3][4];
		LocalPose( m_fromLegs[b] ? m_legs : m_main, time, b, pos, quat );
		QuaternionMatrix( quat, pos, local );
		if ( m_boneParent[b] < 0 )
			memcpy( bones[b], local, sizeof( local ) );
		else
			ConcatTransforms( bones[m_boneParent[b]], local, bones[b] );
	}
}

void CStudioPreviewModel::MergeBones( const CStudioPreviewModel& parent, const BoneMatrix* parentBones, BoneMatrix* bones ) const
{
	if ( !m_bLoaded )
		return;
	for ( int b = 0; b < m_iNumBones; b++ )
	{
		int match = -1;
		for ( int p = 0; p < parent.m_iNumBones && match < 0; p++ )
		{
			if ( !_stricmp( m_boneName[b].c_str(), parent.m_boneName[p].c_str() ) )
				match = p;
		}
		if ( match >= 0 )
		{
			// (a gun made for another skeleton's hold: moved in the hand's frame, its own bones follow from the hand)
			if ( m_bHandMove && !_stricmp( m_boneName[b].c_str(), "Bip01 R Hand" ) )
				ConcatTransforms( parentBones[match], m_handMove, bones[b] );
			else
				memcpy( bones[b], parentBones[match], sizeof( BoneMatrix ) );
			continue;
		}
		float pos[3], quat[4], local[3][4];
		LocalPose( m_main, 0.0, b, pos, quat );
		QuaternionMatrix( quat, pos, local );
		if ( m_boneParent[b] < 0 )
			memcpy( bones[b], local, sizeof( local ) );
		else
			ConcatTransforms( bones[m_boneParent[b]], local, bones[b] );
	}
}

bool CStudioPreviewModel::OnHead( int vert ) const
{
	if ( vert < 0 || vert >= (int)m_vertBone.size() )
		return false;
	const int bone = m_vertBone[vert];
	return bone >= 0 && bone < (int)m_headBone.size() && m_headBone[bone];
}

void CStudioPreviewModel::Skin( const BoneMatrix* bones, std::vector<float>& positions, std::vector<float>& normals,
	const BoneMatrix* normalBones ) const
{
	if ( !normalBones )
		normalBones = bones;
	const int numVerts = (int)m_vertBone.size(), numNorms = (int)m_normBone.size();
	positions.resize( (size_t)numVerts * 3 );
	normals.resize( (size_t)numNorms * 3 );
	if ( !m_bLoaded )
		return;
	for ( int v = 0; v < numVerts; v++ )
	{
		const float( *m )[4] = bones[m_vertBone[v]];
		const float* in = &m_verts[(size_t)v * 3];
		for ( int j = 0; j < 3; j++ )
			positions[(size_t)v * 3 + j] = m[j][0] * in[0] + m[j][1] * in[1] + m[j][2] * in[2] + m[j][3];
	}
	for ( int n = 0; n < numNorms; n++ )
	{
		const float( *m )[4] = normalBones[m_normBone[n]];
		const float* in = &m_norms[(size_t)n * 3];
		for ( int j = 0; j < 3; j++ )
			normals[(size_t)n * 3 + j] = m[j][0] * in[0] + m[j][1] * in[1] + m[j][2] * in[2];
	}
}
