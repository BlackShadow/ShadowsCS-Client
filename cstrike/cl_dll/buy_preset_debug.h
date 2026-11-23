//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef BUY_PRESET_DEBUG_H
#define BUY_PRESET_DEBUG_H

bool IsPresetDebuggingEnabled( void );			///< cl_preset_debug >= 1.0f
bool IsPresetFullCostDebuggingEnabled( void );	///< cl_preset_debug == 2.0f
bool IsPresetCurrentCostDebuggingEnabled( void );	///< cl_preset_debug == 3.0f
bool IsPresetCareerDebuggingEnabled( void );

#endif // BUY_PRESET_DEBUG_H
