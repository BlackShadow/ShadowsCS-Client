// player_fit.h - how much bigger a player model can be drawn before it sticks out of the hit boxes the server tests
// shots on, and the growing itself (the beta set's players are drawn so; see player_fit.cpp). Pure standard C++ on the
// studio model's own data, no engine headers: the game's renderer (vismods.cpp), the Advanced window's preview
// (studio_preview_gl.cpp) and an offline test all run it.
#pragma once

struct PlayerFit
{
	enum
	{
		MAX_BONES = 128	// MAXSTUDIOBONES
	};
	int numBones;					// the drawn model's; 0: nothing to grow
	short carrier[MAX_BONES];		// per bone: the boxed bone its vertices grow with (itself, for a boxed bone); -1: none
	signed char axis[MAX_BONES];	// per boxed bone: the axis (0..2) it runs along, towards the boxed bones under it;
									// -1: an end part (head, hand, foot)
	float centre[MAX_BONES][3];		// per boxed bone: the middle of its hit box, in the bone's space
	float room[MAX_BONES];			// per end part: how many times its size still keeps it inside its own box (1 at least)
	float factor;					// the model's: how many times as thick its limbs and trunk can be drawn (1 at least)
};

// How much of its plan a beta player is drawn grown by: about a third - 4 to 10 per cent thicker. (All of it, 11 to 30
// per cent, came first; user: "theyre way chunky now")
const float kBetaPlayerGrowth = 0.35f;

// The plan for the model 'drawn' against the hit boxes of 'server' (the model the server tests shots on; the drawn
// model's own boxes when it has not the server's bones). Both are studio models' data (studiohdr_t), whole.
void PlayerFit_Plan( const void* drawn, const void* server, PlayerFit* fit );

// The posed bones of the drawn model, grown by 'amount' (0 none .. 1 all) of the plan.
void PlayerFit_Grow( const PlayerFit* fit, float amount, float ( *bones )[3][4] );
