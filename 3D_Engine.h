
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

	Our cockpit art (Car.cpp DrawCockpit) leaves a window 476 x 328.8 wide in the 640x480 /
	800x480 base space, so those are the angles that window has to subtend.
	=========================================================================================	*/

// Cockpit window, in base-resolution units. Must track the panel quads in DrawCockpit():
//   x: COCKPIT_TOP_X_OFFSET*2 .. COCKPIT_RIGHT_X_OFFSET*2      = 82 .. 558   (476 wide)
//   y: COCKPIT_TOP_HEIGHT*2.4 .. COCKPIT_SIDE_HEIGHT*2.4       = 38.4 .. 367.2 (328.8 tall)
#define	SCR_WINDOW_WIDTH	476.0f
#define	SCR_WINDOW_HEIGHT	328.8f

// ...and where it sits. The opening runs y 38.4 .. 367.2, so its centre is 202.8, NOT the
// screen's 240. The Amiga always put the horizon at the centre of its playfield (z.rotate
// adds 64 of 128, StuntCarRacer.s:16777); centring on the screen instead drops our horizon
// ~11% of the window's height too low, which reads as the camera being perched up high.
#define	SCR_WINDOW_TOP		38.4f		// COCKPIT_TOP_HEIGHT  * 2.4
#define	SCR_WINDOW_CENTRE_Y	(SCR_WINDOW_TOP + SCR_WINDOW_HEIGHT * 0.5f)

#define	AMIGA_HALF_FOV_X	22.5f	// degrees, across the cockpit window
#define	AMIGA_HALF_FOV_Y	11.25f

extern bool  gAmigaFov;			// F toggles; see StuntCarRacer.cpp
extern float gAmigaFovStretch;	// , and . adjust; 1.0667 = the PAL Amiga's pixel aspect

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

extern void DrawPolygon( POINT *pptr,
						 long sides );

extern void DrawFilledRectangle( long x1, long y1, long x2, long y2, DWORD colour );

#endif	/* _3D_ENGINE */
