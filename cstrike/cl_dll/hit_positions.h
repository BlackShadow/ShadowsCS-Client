// hit_positions.h - exact hit positions: the other players drawn where the server tests the player's shots at them
#pragma once

struct cl_entity_s;

void HitPositions_VidInit();	// a new map (HUD_VidInit)
// a player the engine has put in this frame (HUD_AddEntity): the place in its newest update is kept
void HitPositions_AddPlayer( struct cl_entity_s* player );
// where to draw another player (_StudioDrawPlayer): true with 'origin' set to the place the server tests shots at;
// false when the engine's own, smoothed place is the one to draw
bool HitPositions_Origin( const struct cl_entity_s* player, float* origin );
