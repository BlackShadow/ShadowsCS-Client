// studio_preview.h - a GoldSrc studio model (.mdl, version 10) read from memory and posed like the game poses it, for
// drawing a player outside a map (the Advanced window's player preview). Pure standard C++: no engine headers.
#pragma once

#include "player_fit.h"

#include <string>
#include <vector>

class CStudioPreviewModel
{
public:
	enum
	{
		MAX_BONES = 128	// MAXSTUDIOBONES
	};
	typedef float BoneMatrix[3][4];

	struct Texture
	{
		int width;
		int height;
		int flags;							// STUDIO_NF_* (2 = chrome)
		std::vector<unsigned char> rgba;	// width * height * 4
	};
	struct Corner
	{
		int vert;	// index into the posed positions
		int norm;	// index into the posed normals
		float s, t;	// texture position in texels
	};
	struct Mesh
	{
		int texture;					// index into Textures(), -1 = none
		bool chrome;
		std::vector<Corner> corners;	// triangles: three corners each
	};

	CStudioPreviewModel();

	// Reads the model (body 0, skin 0) and the animation named 'sequence' (the first one when there is no such name;
	// the middle blend of a 9-way aim blend, the level aim). With 'legs' (an animation such as "idle1") the root,
	// pelvis and legs come from that one, as the CS renderer does for a player's walk / idle animation. False, and
	// nothing kept, when the data is not a studio model this can read.
	bool Load( const unsigned char* data, int length, const char* sequence, const char* legs = 0 );
	bool IsLoaded() const { return m_bLoaded; }

	// the bones 'time' seconds into the looping animations
	void ComputeBones( double time, BoneMatrix* bones ) const;
	// the bones of a model carried by 'parent' (a p_ weapon on a player): each bone named like one of the parent's
	// takes that bone's place (the engine's StudioMergeBones), the others follow from this model's own first frame
	void MergeBones( const CStudioPreviewModel& parent, const BoneMatrix* parentBones, BoneMatrix* bones ) const;
	// model-space positions and normals of every vertex for the given bones (3 floats each); normalBones (if given):
	// the bones the normals are turned by, where 'bones' are grown ones (Fit)
	void Skin( const BoneMatrix* bones, std::vector<float>& positions, std::vector<float>& normals,
		const BoneMatrix* normalBones = 0 ) const;

	// (after Load) A p_ weapon made for another skeleton's hold: MergeBones moves it by 'rows' (three rows of four) in
	// the hand's frame, as the game does (vismods.cpp, models/shadows/beta/hold.txt)
	void SetHandMove( const float* rows );
	// (after Load) A player's growth towards the hit boxes (player_fit.cpp: the beta's players are drawn so); Fit: the
	// plan, or null when none was set
	void SetFit( const PlayerFit& fit );
	const PlayerFit* Fit() const { return m_bFit ? &m_fit : 0; }

	// is the vertex on the head: hung on "Bip01 Head" or a bone under it (vismods.cpp colours the same part in game)
	bool OnHead( int vert ) const;

	int NumBones() const { return m_iNumBones; }
	const std::vector<Mesh>& Meshes() const { return m_meshes; }
	const std::vector<Texture>& Textures() const { return m_textures; }

private:
	struct Frame
	{
		float pos[3];
		float quat[4];
	};
	struct Animation
	{
		int numFrames;
		float fps;
		std::vector<Frame> frames;	// [frame * bones + bone]
	};

	void LocalPose( const Animation& animation, double time, int bone, float pos[3], float quat[4] ) const;

	bool m_bLoaded;
	int m_iNumBones;
	std::vector<int> m_boneParent;
	std::vector<std::string> m_boneName;
	std::vector<bool> m_headBone;	// per bone: the head or under it
	Animation m_main;
	Animation m_legs;				// numFrames 0: none
	std::vector<bool> m_fromLegs;	// per bone: taken from m_legs
	std::vector<float> m_verts;		// bind-pose positions, 3 per vertex
	std::vector<int> m_vertBone;
	std::vector<float> m_norms;
	std::vector<int> m_normBone;
	std::vector<Mesh> m_meshes;
	std::vector<Texture> m_textures;
	bool m_bHandMove;
	float m_handMove[3][4];
	bool m_bFit;
	PlayerFit m_fit;
};
