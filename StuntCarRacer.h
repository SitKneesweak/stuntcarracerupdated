
#ifndef	_STUNT_CAR_RACER
#define	_STUNT_CAR_RACER

/*	========= */
/*	Constants */
/*	========= */
#define SCR_BASE_COLOUR	26

// Screen resolution constants
#define BASE_WIDTH_STANDARD		640		// Standard 4:3 base width
#define BASE_WIDTH_MAX			960		// As wide as the base space is ever allowed to get
#define BASE_HEIGHT				480		// Base height for both modes

/*	The base space is 480 tall and gBaseWidth across.  The cockpit is always the original
	640-wide panel, centred; anything beyond it is extra world, seen past the roll cage to
	the left and right.  Chosen once at startup from the window's shape (ApplyViewport) so
	that dragging the window to another display never re-lays-out the 2D art, clamped to
	BASE_WIDTH_MAX and kept a multiple of 4 so the half-offset below stays whole.	*/
extern int gBaseWidth;
extern int wideScreen;					// gBaseWidth > 640; kept for the 2D layout tests

/*	How far right the 640-wide cockpit panel shifts, in the 320x200 space the art is
	authored in - so twice this in base units.  40 when gBaseWidth is 800, the value the
	old COCKPIT_WIDESCREEN_OFFSET constant hard-coded.	*/
static inline float CockpitWideOffset( void )
	{
	return (float)(gBaseWidth - BASE_WIDTH_STANDARD) * 0.25f;
	}

typedef enum
	{
	TRACK_MENU = 0,
	TRACK_PREVIEW,
	GAME_IN_PROGRESS,
	GAME_OVER
	} GameModeType;

/*
// Untransformed coloured vertex
#define D3DFVF_UTVERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE)
*/
// Untransformed coloured textured vertex
#define D3DFVF_UTVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

/*	===================== */
/*	Structure definitions */
/*	===================== */
/*
// Untransformed coloured vertex
struct UTVERTEX
{
    D3DXVECTOR3 pos;	// The untransformed position for the vertex
	D3DXVECTOR3 normal;	// The surface normal for the vertex
    DWORD color;		// The vertex diffuse color value
};
*/
#ifndef SCR_PORTABLE
// Untransformed coloured textured vertex
struct UTVERTEX
{
    D3DXVECTOR3 pos;	// The untransformed position for the vertex
    DWORD color;		// The vertex diffuse color value
	FLOAT tu,tv;		// The texture co-ordinates
};
#endif

/*	============================== */
/*	External function declarations */
/*	============================== */
extern void GetScreenDimensions( long *screen_width,
								 long *screen_height );

extern DWORD SCRGB (long colour_index);
extern DWORD SCRGBShaded (long colour_index, float shade);
extern DWORD SCColour (long colour_index);

extern void SetSolidColour (long colour_index);
extern void SetLineColour (long colour_index);
extern void SetTextureColour (long colour_index);

// TRUE when the legacy world clock (50Hz / frameGap, the Amiga's race.loop rate) has ticked
// since the last time the render side looked.  DrawSceneParticles() clears it.
extern bool bWorldStepDue;

// Duration of one such step, in seconds.
extern double gWorldStepSeconds;

// The race camera's eye point, in world units.  Only valid while a race is running - see
// the note on the definition.
extern void GetEyeWorldPosition( D3DXVECTOR3 *out );

// Place a point given in the opponent car's model space into world space, using the same
// interpolated render state its world matrix was built from this frame.
extern void OpponentPointToWorld( const D3DXVECTOR3 *local, D3DXVECTOR3 *out );

// Debug
extern long VALUE1, VALUE2, VALUE3;

#endif	/* _STUNT_CAR_RACER */
