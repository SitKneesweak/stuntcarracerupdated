
#ifndef	_3D_ENGINE
#define	_3D_ENGINE

/*	========= */
/*	Constants */
/*	========= */
#define	PI			3.1415926535898

#define	MAX_ANGLE	65536	// value that is equivalent to 360 degrees

#define	_360_DEGREES	(MAX_ANGLE)
#define	_270_DEGREES	(3*MAX_ANGLE/4)
#define	_180_DEGREES	(MAX_ANGLE/2)
#define	_90_DEGREES		(MAX_ANGLE/4)
#define	_0_DEGREES		(0)


// could use enumerated type for the following
#define	X_X_COMP	0		// rotated x components
#define	X_Y_COMP	1
#define	X_Z_COMP	2

#define	Y_X_COMP	3		// rotated y components
#define	Y_Y_COMP	4
#define	Y_Z_COMP	5

#define	Z_X_COMP	6		// rotated z components
#define	Z_Y_COMP	7
#define	Z_Z_COMP	8

#define	NUM_TRIG_COEFFS		Z_Z_COMP + 1


#define	PRECISION	16384
#define	LOG_PRECISION	14	// to base 2

#define	MAX_POLY_SIDES	8

#define	MAX_COORDS	200

//#define	FOCUS	256			// for screen width of 320
//#define	LOG_FOCUS	8		// to base 2

#define	FOCUS	512			// for screen width of 640
#define	LOG_FOCUS	9		// to base 2

/*	=========================================================================================
	Amiga field of view (see SetSceneProjection in StuntCarRacer.cpp)

	The Amiga renders the world into a 256x128 playfield at a fixed 2048 pixels per 360
	degrees ("Reference only/StuntCarRacer.s": calculate.screen.y2 :16734 shifts the
	arctangent by 3, z.rotate :16763 shifts it by 2 more and adds the (128,64) centre).
	That is exactly 45 degrees horizontally and 22.5 degrees vertically.

	The number that actually matters is the angular scale, 360/2048 = 0.17578 degrees per
	Amiga pixel, the same on both axes. 45 and 22.5 are that scale times the 256x128
	playfield; they are not independent facts about the camera.

	Our cockpit art (Car.cpp DrawCockpit) leaves a window 328.8 base units tall, i.e. 137
	Amiga pixels, NOT the Amiga's 128 - so fitting that window to 22.5 degrees magnifies the
	world by 137/128 = 5.6%. Drive the focal length off the per-pixel scale instead and the
	window simply subtends the 24.08 degrees that 137 Amiga pixels are worth.
	=========================================================================================	*/

// Cockpit window, in base-resolution units. Must track the panel quads in DrawCockpit():
//   x: COCKPIT_TOP_X_OFFSET*2 .. COCKPIT_RIGHT_X_OFFSET*2      = 82 .. 558   (476 wide)
//   y: COCKPIT_TOP_HEIGHT*2.4 .. COCKPIT_SIDE_HEIGHT*2.4       = 38.4 .. 367.2 (328.8 tall)
#define	SCR_WINDOW_WIDTH	476.0f
#define	SCR_WINDOW_HEIGHT	328.8f
#define	SCR_WINDOW_LEFT		82.0f		// COCKPIT_TOP_X_OFFSET * 2 (add CockpitWideOffset()*2
										// in widescreen, where the whole panel shifts right)

// ...and where it sits. The opening runs y 38.4 .. 367.2, so its centre is 202.8, NOT the
// screen's 240. The Amiga always put the horizon at the centre of its playfield (z.rotate
// adds 64 of 128, StuntCarRacer.s:16777); centring on the screen instead drops our horizon
// ~11% of the window's height too low, which reads as the camera being perched up high.
#define	SCR_WINDOW_TOP		38.4f		// COCKPIT_TOP_HEIGHT  * 2.4
#define	SCR_WINDOW_CENTRE_Y	(SCR_WINDOW_TOP + SCR_WINDOW_HEIGHT * 0.5f)

// The Amiga playfield the world is projected into, in pixels.  2D effects that were plotted
// straight into it (see DrawSparks in Car_Behaviour.cpp) map onto the cockpit window above.
#define	AMIGA_PLAYFIELD_WIDTH	256
#define	AMIGA_PLAYFIELD_HEIGHT	128

#define	AMIGA_HALF_FOV_X	22.5f	// degrees, across the Amiga's 256x128 playfield - kept for
#define	AMIGA_HALF_FOV_Y	11.25f	// reference; the projection uses the per-pixel scale below

// 2048 pixels per 360 degrees, both axes (StuntCarRacer.s: the arctangent is shifted by 3 in
// calculate.screen.x/y and by 2 more in z.rotate, so 32 angle units of 65536 per pixel).
#define	AMIGA_DEG_PER_PIXEL	(360.0f / 2048.0f)		// 0.17578; 45/256 == 22.5/128

#ifndef SCR_DEG_TO_RAD
#define SCR_DEG_TO_RAD(d)	((d) * 3.14159265358979323846f / 180.0f)
#endif

// One Amiga pixel is 2.4 base units tall (DrawCockpit's 320x200 -> 640x480 scale), so this is
// the vertical focal length in base units. 782.3, against the 826.5 that fitting our 137-pixel
// window to 22.5 degrees used to give.
#define	AMIGA_FOCAL_Y_BASE	(2.4f / tanf(SCR_DEG_TO_RAD(AMIGA_DEG_PER_PIXEL)))

// Pixel aspect (width:height) of one Amiga lores pixel on a PAL screen: 320 across the 4:3
// active width, 256 down the 4:3 active height, so (4/3)/(320/256). Slightly WIDER than
// square. The NTSC figure is (4/3)/(320/200) = 0.8333, i.e. 1.2x taller - that one is the
// origin of the "Amiga pixels are 1.2x tall" folklore, and it does not apply here. See the
// diwstrt/diwstop note in 3D_Engine.cpp for why this game is PAL.
#define	AMIGA_PAL_PIXEL_ASPECT	1.06667f

// The base 640x480 space holds the Amiga's 320x200 scaled by (2.0, 2.4) - see DrawCockpit()
// in Car.cpp - so one Amiga pixel is 1.2x taller than wide IN BASE SPACE. That is the number
// the 3D projection has to match (gAmigaFovStretch) for the world to sit in the same space
// as the 2D art.
#define	AMIGA_BASE_STRETCH	1.2f

// ...and this undoes it at present time, once, for the whole raster - exactly as the display
// did on real hardware. 1/(1.2 * 1.06667) = 0.78125, so the 480-unit base presents 375 units
// tall with 52.5 units of black above and below. Without it the base-space 1.2 leaks out as
// final geometry and the entire picture is 28% too tall.
#define	SCR_PRESENT_SQUASH	(1.0f / (AMIGA_BASE_STRETCH * AMIGA_PAL_PIXEL_ASPECT))

// The NTSC figure, (4/3)/(320/200). Presenting with this is the same as not squashing at
// all - 1/(1.2 * 0.8333) = 1.0 - so the 640x480 base goes out as a clean 4:3 picture, with
// black bars at the sides on a wide display instead of at the top and bottom. Geometrically
// it is the "one Amiga pixel is 1.2x tall" reading; historically it is NTSC, not this game.
#define	AMIGA_NTSC_PIXEL_ASPECT	0.83333f

// Which of the two the raster is actually presented with. A toggles at runtime; see
// ScrPresentSquash() and the SDLK_a case in StuntCarRacer.cpp.
extern float gPresentPixelAspect;

// The live squash factor, the runtime counterpart of SCR_PRESENT_SQUASH above.
static inline float ScrPresentSquash( void )
{
	return 1.0f / (AMIGA_BASE_STRETCH * gPresentPixelAspect);
}

extern bool  gAmigaFov;			// F toggles; see StuntCarRacer.cpp
extern float gAmigaFovStretch;	// , and . adjust; 1.2 = base space, see AMIGA_BASE_STRETCH

// Half-angle tangents of the *full screen* frustum, for the current mode.
extern void GetProjectionTangents( float *tan_half_x, float *tan_half_y );

// The same thing as pixel focal lengths, for the software-projected backdrop and scenery.
extern void GetProjectionFocals( long *focal_x, long *focal_y );

// Where the view axis lands on screen, in screen pixels.
extern void GetProjectionCentre( long *centre_x, long *centre_y );

// Software perspective divide + screen centring, matching the projection matrix.
extern void ProjectToScreen( long trans_x, long trans_y, long trans_z,
							 long *screen_x, long *screen_y );

/*	===================== */
/*	Structure definitions */
/*	===================== */
typedef struct
	{
	long	x;
	long	y;
	long	z;
	} COORD_3D;

typedef struct
	{
	long	x;
	long	y;
	} COORD_2D;

/*	Sub-pixel screen coordinate.  The backdrop works in a fixed 640x480 space that is then
	scaled to the drawable, so a whole unit there is several physical pixels - see
	ProjectToScreenF.															*/
typedef struct
	{
	double	x;
	double	y;
	} COORD_2DF;

/*	============================== */
/*	External function declarations */
/*	============================== */
extern void CreateSinCosTable( void );

extern void GetSinCos( long angle,
					   short *sin,
					   short *cos );

extern void SetWorldOffset( long x_offset,
							long y_offset,
							long z_offset );

extern void SetCoords( COORD_3D *tptr,
					   COORD_2D *sptr );

extern void DefaultCoords( void );

extern void CalcYXZTrigCoefficients( long x_angle,
									 long y_angle,
									 long z_angle );

extern short *TrigCoefficients( void );

extern void RotateCoordinate( long *xptr,
							  long *yptr,
							  long *zptr );

extern void WorldOffset( long *xptr,
					     long *yptr,
					     long *zptr );

extern long TransformCoordinates( COORD_3D *cptr,
								  long size );

extern long TransformedZ( long offset );

extern long TexturedPolygon( long *cptr,
							 long sides,
							 long *vptr );

extern long PolygonVisible( long *cptr );

extern long Polygon( long *cptr,
					 long sides );

extern long PolygonEx( long *cptr,
					   long sides,
					   long *optr );

extern void Line( long c1,
				  long c2 );

extern long PolygonZClipped( long *cptr,
							 long sides,
							 long check_orientation,
							 long *on_screen );

extern void LineZClipped( long c1,
						  long c2 );

extern void ZClip( COORD_3D *below,
				   COORD_3D *above,
				   long screen_width,
				   long screen_height,
				   long *x,
				   long *y );

extern void LockViewpointToTarget( long viewpoint_x,
								   long viewpoint_y,
								   long viewpoint_z,
								   long target_x,
								   long target_y,
								   long target_z,
								   long *viewpoint_x_angle,
								   long *viewpoint_y_angle );

extern HRESULT CreatePolygonVertexBuffer (IDirect3DDevice9 *pd3dDevice);
extern void FreePolygonVertexBuffer (void);

extern void ProjectToScreenF( long trans_x, long trans_y, long trans_z,
							  double *screen_x, double *screen_y );

extern void DrawPolygonF( const COORD_2DF *pptr,
						  long sides );

extern void DrawPolygon( POINT *pptr,
						 long sides );

extern void DrawFilledRectangle( long x1, long y1, long x2, long y2, DWORD colour );

#endif	/* _3D_ENGINE */
