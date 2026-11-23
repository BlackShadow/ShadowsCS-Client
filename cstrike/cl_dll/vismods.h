// vismods.h - client visual options: cl_nosky (solid black sky) and cl_teamcolors (solid team-coloured players)
#pragma once

struct movevars_s;
struct cl_entity_s;

void VisMods_Init();
void VisMods_VidInit();
void VisMods_Frame( float time );
void VisMods_BeforeWorld();	// V_CalcRefdef, before the world is drawn: cl_nosky
void VisMods_SetMoveVars( struct movevars_s* movevars );

// players: colour for player entity index (1..MAX_PLAYERS); false when team colours are off or the player has no team
bool VisMods_PlayerColor( int entindex, float color[3] );
// the VIP of an assassination round (servers also flag paying "VIP" members on the scoreboard)
bool VisMods_IsRoundVIP( int entindex );
// client-side corpses: tag stored in the corpse's curstate.iuser4 when it is made, colour looked up when drawn
int VisMods_CorpseTag( int team, int player );
bool VisMods_EntityColor( const struct cl_entity_s* ent, float color[3] );

// cl_weaponmodels: the weapon models' set (the Advanced window's Weapons). A model about to be drawn, swapped for the
// set's copy of it (models/shadows/<set>/<name>) when it is a weapon model (models/v_, p_ or w_*.mdl) and the set has
// it (the beta set: else CS 1.5's, for the weapons the beta does not have); otherwise the model itself
struct model_s* VisMods_WeaponModel( struct model_s* model );
// cl_playermodels: the player models' set (the Advanced window's Players). A player model about to be drawn
// (models/player/<name>/<name>.mdl), swapped for the set's copy of it (models/shadows/<set>/player/<name>/<name>.mdl)
// when the set has it (the beta set: else CS 1.0's from the retail set); otherwise the model itself
struct model_s* VisMods_PlayerModel( struct model_s* model );
// cl_overridemodels: a player whose model is a server's own (not one of the game's) drawn in one of the game's for
// their team - its number in cdll_int.cpp's sPlayerModelFiles; -1 for the game's own models, with no team or the option
// off. (A server's own weapon models are drawn as the game's by VisMods_WeaponModel.)
int VisMods_GamePlayerModel( const char* model, int entindex, int team, bool vip );
// is a view model being drawn (after VisMods_WeaponModel) made for the right hand? CS 1.5's are; the game's are made for
// the left hand (the client mirrors them), all but the knife: gameRightHanded says whether the game's model would be.
// The beta's are all made for the left hand, the knife too
bool VisMods_WeaponModelRightHanded( const struct model_s* drawn, bool gameRightHanded );
// the folder under sprites/ holding the weapon set's own lists for a weapon's pictures (weapon menu, pickup history),
// "shadows/retail/" for the knife with the retail or beta set (CS 1.5's knife picture), else "" (the game's lists)
const char* VisMods_WeaponIconFolder( const char* weapon );

// A gun (p_ model) about to be hung on a player's hand, both as they are drawn: the move, in the hand's frame, putting
// a gun made for the other skeleton's hold (beta 6.1's on the game's players, or the other way round) where this
// player holds it - three rows of four numbers (models/shadows/beta/hold.txt) - or NULL when the gun was made for this
// player's hold. By the models, or (the Advanced window's preview) by their files
const float* VisMods_HeldMove( const struct model_s* weapon, const struct model_s* player );
const float* VisMods_HeldMoveByPath( const char* weaponPath, const char* playerPath );

// The beta set's player models are drawn a little bigger, towards their hit boxes (player_fit.cpp). A player's
// body about to be drawn: its posed bones grown so when its model is one of the beta's (drawn: the model drawn;
// entityModel: the player's own model, whose hit boxes the server tests). True when they were changed, with the bones
// as they were in 'saved' (to be put back after the drawing)
bool VisMods_GrowPlayer( const struct model_s* drawn, const struct model_s* entityModel, float ( *bones )[3][4],
	float ( *saved )[3][4] );

// cl_headcolor: players' heads in one flat colour ("r g b"), with solid team colours on; false when off
bool VisMods_HeadColor( float color[3] );
// a player just drawn: its head drawn again over it in the head colour. header: its studiohdr_t, body: its body value,
// bones: the bone transforms it was drawn with (before a weapon model merges its own), eye: the view origin, alpha:
// how solid (1, or less for a see-through player)
void VisMods_DrawPlayerHead( const void* header, int body, const float ( *bones )[3][4], const float eye[3], float alpha );

void VisMods_BeginSolid( const float color[3] ); // following model draws use a solid colour
void VisMods_EndSolid();
void VisMods_PreDrawPoints( int rendermode ); // called right before IEngineStudio.StudioDrawPoints()
void VisMods_PostDrawPoints();                // called right after it

// viewmodel_fov: draw the weapon with a closer near clip plane (restores the engine's projection on End)
bool VisMods_BeginViewModelNear( float zNear );
void VisMods_EndViewModelNear();
