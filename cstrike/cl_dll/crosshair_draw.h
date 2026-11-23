// crosshair_draw.h - the crosshair: how wide it opens (the HUD's spread), how big it is drawn (cl_crosshair_size) and
// its parts (the four lines, plus the Advanced window's options: thickness, gap, black outline, centre dot, T shape).
// Shared by the HUD (ammo.cpp, which fills the parts with the engine's FillRGBA / FillRGBABlend) and the Advanced
// window's crosshair preview (which draws them with OpenGL the same way). No engine headers: the same code runs in an
// offline test.
#pragma once

// what the crosshair options are set to (the cvars, or the Advanced window's controls before OK / Apply); all zero
// except the colour, thickness 1, translucent, dynamic and recoil is the game's own crosshair
struct CrosshairSettings
{
	int r, g, b;		// cl_crosshair_rgb
	int size;			// cl_crosshair_size: 0 auto, 1 small, 2 medium, 3 large
	bool translucent;	// cl_crosshair_additive: the lines add their light (FillRGBA), else blended (FillRGBABlend)
	int alpha;			// cl_crosshair_alpha: 0 = the game's (fades while shooting), else fixed 1..255
	int thickness;		// cl_crosshair_thickness: pixels, 1 = the game's
	int length;			// cl_crosshair_length: added to the lines' length, before the size scaling (-5: no lines)
	int gap;			// cl_crosshair_gap: added to the lines' distance from the middle, before the size scaling
	int outline;		// cl_crosshair_outline: a black edge this many pixels wide round every part (0: none)
	int dot;			// cl_crosshair_dot: a dot this many pixels square in the middle (0: none)
	bool tshape;		// cl_crosshair_t: no top line
	bool dynamic;		// cl_dynamiccrosshair: wider while moving, jumping and so on
	bool recoil;		// cl_crosshair_recoil: wider while shooting
	bool sniper;		// cl_crosshair_sniper: sniper rifles show it too while not zoomed
};

// one filled rectangle of the crosshair
struct CrosshairRect
{
	int x, y, w, h;
	int r, g, b, a;
	bool additive;		// FillRGBA (adds its light), else FillRGBABlend
};

enum
{
	CROSSHAIR_MAX_RECTS = 10
};

// cl_crosshair_size's text: 0 auto, 1 small, 2 medium, 3 large, -1 not one of those (ammo.cpp's own reading)
int Crosshair_ParseSize( const char* text );

// the screen width the crosshair's numbers are made for at this size (bigger screens scale them up)
int Crosshair_ScaleBase( int size, int screenWide );

// one frame of the HUD's spread: a shot this frame opens it up (when 'recoil'), otherwise it closes back towards
// 'baseDistance' (the weapon's resting distance this frame); alpha fades with each shot and comes back
void Crosshair_Step( float& distance, int& alpha, int baseDistance, int delta, bool shot, bool recoil );

// the resting distance with the widening for movement (cl_dynamiccrosshair, weapons with accuracy flags): in the air
// x2, ducking x0.5, faster than the weapon's limit x1.5, flags 8 and 16 x1.4 each (ammo.cpp DrawCrosshair's sums)
int Crosshair_RestingDistance( int distance, int accuracy, bool onGround, bool ducking, bool fast );

// every gun's crosshair (the HUD's and the Advanced window's preview): the AK-47's numbers, so the crosshair set up in
// the Advanced window is the same on every gun (the game gives each gun its own). Resting distance, opening per shot,
// accuracy flags (the widening: x2 in the air, x1.5 faster than fastSpeed) and that speed.
struct CrosshairProfile
{
	int distance;
	int delta;
	int accuracy;
	int fastSpeed;
};
const CrosshairProfile kCrosshairProfile = { 4, 4, 3, 140 };	// the game's WEAPON_AK47

// the gap (pixels from the middle lines to the lines' inner ends) and the line length on the screen
void Crosshair_Measure( float distance, int baseDistance, int gapSetting, int lengthSetting, int screenWide, int scaleBase,
	int* gap, int* length );

// the rectangles to fill, in order (outline first), for a crosshair centred on cx, cy; 'alpha' is the game's this frame
// (used unless the settings fix one); returns how many
int Crosshair_Build( const CrosshairSettings& settings, bool additive, int r, int g, int b, int alpha, int cx, int cy,
	int gap, int length, CrosshairRect* out );

// the Advanced window's preview: a little routine with every gun's crosshair (standing, running, a burst of seven
// shots, settling), stepped a hundred times a second with the HUD's own sums, so "wider while moving / shooting" show
class CCrosshairRoutine
{
public:
	CCrosshairRoutine();
	void Advance( double now, const CrosshairSettings& settings );	// steps up to 'now' (seconds)
	float Distance() const { return m_flDistance; }
	int Alpha() const { return m_iAlpha; }
	int BaseDistance() const { return m_iBase; }

private:
	double m_flStart;	// < 0: not started
	double m_flDone;	// routine time stepped so far
	float m_flDistance;
	int m_iAlpha;
	int m_iBase;
	int m_iShots;		// shots of the current burst fired
};

// how far the preview's crosshair reaches from its middle at the most (pixels, at the widest of its routine: 15,
// Crosshair_Step's most), its outline and thickness with it
int Crosshair_PreviewReach( const CrosshairSettings& settings, int screenWide );

// the preview: the crosshair as set, at its real size, in the middle of the box x, y, wide x tall (screen pixels of a
// screenWide x screenTall screen), drawn with OpenGL the way the engine fills rectangles; GL state put back afterwards
void Crosshair_DrawPreview( const CrosshairSettings& settings, const CCrosshairRoutine& routine, int x, int y, int wide,
	int tall, int screenWide, int screenTall );
