//--------------------------------------------------------------------------------------
// File: StuntCarRacer.cpp
//
// NOTE: This project builds with Microsoft Visual C++ 2008 Express Edition and requires
// Microsoft DirectX SDK (April 2007).  It is based on examples from that DirectX SDK.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "dxstdafx.h"

#include "resource.h"

#include "StuntCarRacer.h"
#include "3D_Engine.h"
#include "Backdrop.h"
#include "Track.h"
#include "Car.h"
#include "Car_Behaviour.h"
#include "Physics_FloatV2.h"
#include "Sim_Trace.h"
#include "Opponent_Behaviour.h"
#include "wavefunctions.h"
#include "Atlas.h"
#include "RoadTexture.h"
#include "AmigaMenu.h"
#include "MenuScreens.h"
#include "League.h"
/*	Net_Game.h / Net_Lockstep.h include only <cstdint>.  No socket header may ever be
	pulled in here - dx_linux.h typedefs DWORD/BOOL/HWND itself, with different
	underlying types than Winsock's.  Net_Socket.cpp is the only TU that sees one.	*/
#include "Net_Game.h"
#include "Net_Lockstep.h"
#include "version.h"
#include "Det_Rand.h"

#ifdef SCR_PORTABLE
#include <unistd.h>
#define STRING "%S"
#else
#define STRING L"%s"
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------

#ifdef SCR_PORTABLE
#define DEFAULT_FRAME_GAP	(6)		// 4 Used to limit frame rate.  Amiga StuntCarRacer uses value of 6 (called MIN.FRAMES)
#else
#define DEFAULT_FRAME_GAP	(4)
#endif

#define	HEIGHT_ABOVE_ROAD	(60)	// TODO: lower once FloatV2 port lands accurate per-wheel road-height clamping in LimitViewpointY

#define	FURTHEST_Z (131072.0f)

// Most the fog is allowed to wash out the opponent's car - see DrawOpponentsCar()
#define	OPPONENT_MAX_FOG (0.45f)

GameModeType GameMode = TRACK_MENU;

// Both the following are used for keyboard input
UINT keyPress = '\0';
DWORD lastInput = 0;

static IDirectSound8 *ds;
IDirectSoundBuffer8 *WreckSoundBuffer = NULL;
IDirectSoundBuffer8 *HitCarSoundBuffer = NULL;
IDirectSoundBuffer8 *GroundedSoundBuffer = NULL;
IDirectSoundBuffer8 *CreakSoundBuffer = NULL;
IDirectSoundBuffer8 *SmashSoundBuffer = NULL;
IDirectSoundBuffer8 *OffRoadSoundBuffer = NULL;
IDirectSoundBuffer8 *EngineSoundBuffers[8] = {NULL};

IDirect3DTexture9 *g_pAtlas = NULL;

	// The crane's chains.  Kept out of atlas.png: it is a 10x116 strip of 16-line links
	// that DrawCockpit tiles up the screen, so it wants its own texture rather than a
	// cell somebody could nudge.
IDirect3DTexture9 *g_pChain = NULL;

int wideScreen = 0;
float gCustomScale = 0.0f;	// -s option, in points; 0 = auto-fit the window

static long frameGap = DEFAULT_FRAME_GAP;
static bool bFrameMoved = FALSE;

// Latched by FrameMove whenever the legacy 50Hz/frameGap world clock ticked, and consumed
// (cleared) by the render side.  Effects that the Amiga advanced once per race.loop pass -
// the sparks and dust clouds - hang off this so they keep their original speed instead of
// running at whatever the display refresh happens to be.
bool bWorldStepDue = FALSE;

// How long one of those world steps lasts.  The Amiga advanced the sparks once per step,
// so this is the unit their velocities are expressed in; the effect integrates against it
// to draw the same trajectory at the display's refresh rate instead of in 8.3Hz jumps.
double gWorldStepSeconds = static_cast<double>(DEFAULT_FRAME_GAP) / 50.0;

// TRUE once the swap interval has been accepted, in which case the buffer swap paces the
// main loop and its wall-clock SDL_Delay stands down.  See the setup in the SDL init and
// the cap at the bottom of the main loop.
static bool bVsyncPacing = FALSE;

bool bShowStats = FALSE;
bool bNewGame = FALSE;
bool bPaused = FALSE;
bool bPlayerPaused = FALSE;
bool bOpponentPaused = FALSE;
// Escape during a race asks before leaving it rather than dropping out instantly.
// bQuitConfirmWasPaused remembers whether the game was already paused (via 'P')
// so cancelling puts things back the way they were.
bool bQuitConfirm = FALSE;
static bool bQuitConfirmWasPaused = FALSE;
long bTrackDrawMode = 0;
// Fixed-sun shading of the track faces, baked into the vertex colours (see FaceShade in Track.cpp).
// F3 toggles it; the track vertex buffer is rebuilt on the toggle.
long gTrackLighting = 1;
bool bOutsideView = FALSE;
/*	Widescreen shows the world past the cockpit's A-pillars and above the side panels, which
	the Amiga never did - its playfield stopped at the windscreen.  With this set, the scene
	is clipped to the windscreen aperture and the surround left black.  Press W.				*/
bool bAmigaWindscreen = FALSE;
long engineSoundPlaying = FALSE;
double gameStartTime, gameEndTime;
bool bSuperLeague = FALSE;

#if defined(DEBUG) || defined(_DEBUG)
FILE *out;
bool bTestKey = FALSE;
char OutputFile[] = "SCRlog.txt";
long VALUE1 = 1, VALUE2 = 2, VALUE3 = 3;
#endif

extern long TrackID;
extern long boostReserve, boostUnit, StandardBoost, SuperBoost;
// Per-wheel road penetration, for the Amiga y.pers.shift camera rule.
extern long front_left_amount_below_road, front_right_amount_below_road,
			rear_amount_below_road;
extern long INITIALISE_PLAYER;
extern bool raceFinished, raceWon;
extern long lapNumber[];
// Counts the frames between the car wrecking and the race ending; drives the prompt.
extern long wreck_countdown;

// League / Super League variable
extern long damaged_limit;
extern long road_cushion_value;
extern long engine_power;
extern long boost_unit_value;
extern long opp_engine_power;

//-----------------------------------------------------------------------------
// Static variables
//-----------------------------------------------------------------------------
// Player 1 orientation
static long player1_x = 0,
			player1_y = 0,
			player1_z = 0;

static long player1_x_angle = (0<<6),
			player1_y_angle = (0<<6),
			player1_z_angle = (0<<6);

/*	Render interpolation.  The physics runs on a fixed clock and must keep doing so - the
	handling is an artifact of the discrete step, and lockstep netplay needs every peer to
	compute the same numbers - but the *display* has no such obligation.  So keep the last
	two physics states and draw the car and camera at a blend between them, chosen by how
	much of a step the accumulator is still holding.  The simulation never sees this: the
	blended values are written into the globals just long enough to build the transforms
	and are put straight back afterwards (see PLAYER_STATE_RESTORE at the end of the
	GAME_IN_PROGRESS branch).

	This is what makes motion smooth without making the physics frame-rate-dependent, and
	it is the reason we can render above the physics rate at all.						*/
struct PLAYER_STATE
{
	long x, y, z;
	long x_angle, y_angle, z_angle;
};

static PLAYER_STATE gPlayerPrev = { 0,0,0, 0,0,0 };		// state before the most recent step
static PLAYER_STATE gPlayerCurr = { 0,0,0, 0,0,0 };		// state after it
static bool         gPlayerStatesValid = false;
static double       gInterpAlpha = 0.0;					// 0 = on gPlayerPrev, 1 = on gPlayerCurr
bool                gRenderInterpolation = true;		// F3 toggles, for A/B comparison

/*	A position jump this big in one step is a teleport, not motion: the crane placing the
	car, a respawn, the 'R' turnaround.  Interpolating across one would smear the car
	across the track, so snap instead.  One step at racing speed moves the car a small
	fraction of this.															*/
#define INTERP_TELEPORT_LIMIT	(2048 << LOG_PRECISION)		// world units; a car is ~240 long

// Opponent orientation
static long opponent_x = 0,
			opponent_y = 0,
			opponent_z = 0;

static float opponent_x_angle = 0.0f, opponent_y_angle = 0.0f, opponent_z_angle = 0.0f;

/*	Render interpolation for the opponent - the same idea as PLAYER_STATE above, and for
	an even more visible reason.  The other car's state is written on a clock that is not
	the display's: the legacy AI moves on the 8.3Hz frameGap clock, the FloatV2 AI and the
	netplay remote car move once per physics step.  Drawn straight from those globals at
	60fps or more the car does not move for several frames and then jumps, which reads as
	the opponent jerking around however smooth your own car is.

	So keep the last two states it was written into, along with when each arrived, and
	draw at a blend between them.  Timing the samples rather than reusing the player's
	alpha is what lets one piece of code cover all three clocks: the gap between the last
	two updates IS the step length, whatever produced it.  The cost is that the opponent
	is drawn one such gap in the past, which is the ordinary price of interpolation and is
	invisible next to the stepping it removes.										*/
struct OPPONENT_STATE
{
	long  x, y, z;
	float x_angle, y_angle, z_angle;
};

static OPPONENT_STATE gOppPrev = { 0,0,0, 0.0f,0.0f,0.0f };		// state before the most recent update
static OPPONENT_STATE gOppCurr = { 0,0,0, 0.0f,0.0f,0.0f };		// state after it
static double         gOppPrevTime = 0.0;						// when each was taken, so the
static double         gOppCurrTime = 0.0;						// blend can be paced by the clock
static bool           gOppStatesValid = false;					// two samples in hand?

// Viewpoint 1 orientation
static long viewpoint1_x, viewpoint1_y, viewpoint1_z;
static long viewpoint1_x_angle, viewpoint1_y_angle, viewpoint1_z_angle;

// Target (lookat) point
static long target_x, target_y, target_z;

/**************************************************************************
  DSInit

  Description:
    Initialize all the DirectSound specific stuff
 **************************************************************************/

bool DSInit()
	{
    HRESULT err;

	//
	//	First create a DirectSound object

	err = DirectSoundCreate8(NULL, &ds, NULL);

    if (err != DS_OK)
        return FALSE;

	//
	//	Now set the cooperation level

    err = ds->SetCooperativeLevel(DXUTGetHWND(), DSSCL_NORMAL );

    if (err != DS_OK)
        return FALSE;
	
	return TRUE;
	}

/**************************************************************************
  DSSetMode

	Initialises all DirectSound samples etc

 **************************************************************************/

bool DSSetMode()
	{
	int i;

	// Amiga channels 1 and 2 are right side, channels 0 and 3 are left side

	if ((WreckSoundBuffer = MakeSoundBuffer(ds, L"WRECK")) == NULL)
		return FALSE;
	WreckSoundBuffer->SetPan(DSBPAN_RIGHT);
	WreckSoundBuffer->SetVolume(AmigaVolumeToDirectX(64));

	if ((HitCarSoundBuffer = MakeSoundBuffer(ds, L"HITCAR")) == NULL)
		return FALSE;
	HitCarSoundBuffer->SetFrequency(AMIGA_PAL_HZ / 238);
	HitCarSoundBuffer->SetPan(DSBPAN_RIGHT);
	HitCarSoundBuffer->SetVolume(AmigaVolumeToDirectX(56));

	if ((GroundedSoundBuffer = MakeSoundBuffer(ds, L"GROUNDED")) == NULL)
		return FALSE;
	GroundedSoundBuffer->SetFrequency(AMIGA_PAL_HZ / 400);
	GroundedSoundBuffer->SetPan(DSBPAN_RIGHT);

	if ((CreakSoundBuffer = MakeSoundBuffer(ds, L"CREAK")) == NULL)
		return FALSE;
	CreakSoundBuffer->SetFrequency(AMIGA_PAL_HZ / 238);
	CreakSoundBuffer->SetPan(DSBPAN_RIGHT);
	CreakSoundBuffer->SetVolume(AmigaVolumeToDirectX(64));

	if ((SmashSoundBuffer = MakeSoundBuffer(ds, L"SMASH")) == NULL)
		return FALSE;
	SmashSoundBuffer->SetFrequency(AMIGA_PAL_HZ / 280);
	SmashSoundBuffer->SetPan(DSBPAN_LEFT);
	SmashSoundBuffer->SetVolume(AmigaVolumeToDirectX(64));

	if ((OffRoadSoundBuffer = MakeSoundBuffer(ds, L"OFFROAD")) == NULL)
		return FALSE;
	OffRoadSoundBuffer->SetPan(DSBPAN_RIGHT);
	OffRoadSoundBuffer->SetVolume(AmigaVolumeToDirectX(64));

	if ((EngineSoundBuffers[0] = MakeSoundBuffer(ds, L"TICKOVER")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[1] = MakeSoundBuffer(ds, L"ENGINEPITCH2")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[2] = MakeSoundBuffer(ds, L"ENGINEPITCH3")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[3] = MakeSoundBuffer(ds, L"ENGINEPITCH4")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[4] = MakeSoundBuffer(ds, L"ENGINEPITCH5")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[5] = MakeSoundBuffer(ds, L"ENGINEPITCH6")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[6] = MakeSoundBuffer(ds, L"ENGINEPITCH7")) == NULL)
		return FALSE;
	if ((EngineSoundBuffers[7] = MakeSoundBuffer(ds, L"ENGINEPITCH8")) == NULL)
		return FALSE;

	for (i = 0; i < 8; i++)
	{
		EngineSoundBuffers[i]->SetPan(DSBPAN_LEFT);
		// Original Amiga volume was 48, but have reduced this for testing
		EngineSoundBuffers[i]->SetVolume(AmigaVolumeToDirectX(48/2));
	}

	return TRUE;
	}

/**************************************************************************
  DSTerm
 **************************************************************************/

void DSTerm()
	{
    if (WreckSoundBuffer)		WreckSoundBuffer->Release(),	WreckSoundBuffer = NULL;
    if (HitCarSoundBuffer)		HitCarSoundBuffer->Release(),	HitCarSoundBuffer = NULL;
    if (GroundedSoundBuffer)	GroundedSoundBuffer->Release(),	GroundedSoundBuffer = NULL;
    if (CreakSoundBuffer)		CreakSoundBuffer->Release(),	CreakSoundBuffer = NULL;
    if (SmashSoundBuffer)		SmashSoundBuffer->Release(),	SmashSoundBuffer = NULL;
    if (OffRoadSoundBuffer)		OffRoadSoundBuffer->Release(),	OffRoadSoundBuffer = NULL;

	for (int i = 0; i < 8; i++)
	{
		if (EngineSoundBuffers[i]) EngineSoundBuffers[i]->Release(), EngineSoundBuffers[i] = NULL;
	}

    if (ds) ds->Release(), ds = NULL;
	}

/*	======================================================================================= */
/*	Function:		InitialiseData															*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static long InitialiseData( void )
	{
	long success = FALSE;
#if defined(DEBUG) || defined(_DEBUG)
	errno_t err;

	if ((err = fopen_s( &out, OutputFile, "w" )) != 0)
		return FALSE;
#endif

	CreateSinCosTable();

	ConvertAmigaTrack(LITTLE_RAMP);

	// Seed the random-number generator with current time so that
	// the numbers will be different every time we run
	srand( (unsigned)time( NULL ) );

	// The simulation's own stream needs the same treatment: it starts from a
	// fixed constant, so without this the first draw of a session is always the
	// same number and 'Random' track in a single race always lands on the same
	// track. A netplay session re-seeds both peers from the shared seed
	// (Net_Game.cpp) and --simtrace re-seeds from the trace, so this only
	// decides the single-player stream.
	scr::det::SeedRand( (uint32_t)time( NULL ) );

	success = TRUE;

	return(success);
	}

/*	======================================================================================= */
/*	Function:		FreeData																*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static void FreeData( void )
	{
	FreeTrackData();
	DSTerm();
#if defined(DEBUG) || defined(_DEBUG)
	fclose( out );
#endif
//	CloseAmigaRecording();
	return;
	}

/*	======================================================================================= */
/*	Function:		GetScreenDimensions														*/
/*																							*/
/*	Description:	Provide screen width and height											*/
/*	======================================================================================= */

/*	======================================================================================= */
/*	Function:		GetScreenDimensions															*/
/*																									*/
/*	Description:	Retrieve current screen/backbuffer width and height					*/
/*																									*/
/*	Parameters:		screen_width  - Output: current screen width in pixels				*/
/*					screen_height - Output: current screen height in pixels				*/
/*	======================================================================================= */

void GetScreenDimensions( long *screen_width,
						  long *screen_height )
	{
#ifdef SCR_PORTABLE
	/*const SDL_VideoInfo* info = SDL_GetVideoInfo();
	*screen_width = info->current_w;
	*screen_height = info->current_h; */
	*screen_width = (wideScreen)?800:640;
	*screen_height = 480;
#else
	const D3DSURFACE_DESC *desc;
	desc = DXUTGetBackBufferSurfaceDesc();

	*screen_width = desc->Width;
	*screen_height = desc->Height;
#endif
	}

/*	======================================================================================= */
/*	Function:		SetSceneProjection														*/
/*																							*/
/*	Description:	Build and install the 3D projection.  The half-angles come from			*/
/*					GetProjectionTangents (3D_Engine.cpp), which is also what the software-	*/
/*					projected backdrop and scenery use, so the two stay in step.			*/
/*																							*/
/*					The frustum is ANAMORPHIC in Amiga-FOV mode (x and y half-angles not		*/
/*					related by the screen's aspect ratio - see GetProjectionTangents) and	*/
/*					OFF-CENTRE: its principal point sits at the cockpit window's centre,	*/
/*					not the screen's.  The Amiga always put the horizon at the centre of		*/
/*					its playfield; our window's centre is 37.2 base units above the			*/
/*					screen's, so centring on the screen dropped the horizon that far into	*/
/*					the lower part of the window and made the camera feel perched.			*/
/*	======================================================================================= */

/*	Print the field of view the cockpit window actually ends up with, for comparison against
	the Amiga's 45.0 x 22.5 degrees at the 1.200 the base space is built on.  This is a
	base-space figure: the PAL display aspect is applied later, at present time.										*/
static void ReportFieldOfView( void )
{
	float tan_half_x, tan_half_y;
	GetProjectionTangents(&tan_half_x, &tan_half_y);

	const float base_width = wideScreen ? static_cast<float>(BASE_WIDTH_WIDESCREEN)
										: static_cast<float>(BASE_WIDTH_STANDARD);

	// Back out the full-screen frustum to the window's own subtended angles.
	const float focal_x = (base_width  * 0.5f) / tan_half_x;
	const float focal_y = (BASE_HEIGHT * 0.5f) / tan_half_y;

	const float rad_to_deg = 180.0f / 3.14159265358979323846f;

	printf("Amiga FOV %s - cockpit window %.1f x %.1f degrees, stretch %.3f"
		   "  (Amiga: 45.0 x 22.5, stretch 1.200)\n",
		   gAmigaFov ? "ON" : "OFF",
		   2.0f * atanf((SCR_WINDOW_WIDTH  * 0.5f) / focal_x) * rad_to_deg,
		   2.0f * atanf((SCR_WINDOW_HEIGHT * 0.5f) / focal_y) * rad_to_deg,
		   focal_y / focal_x);
	fflush(stdout);
}

void SetSceneProjection( IDirect3DDevice9 *pd3dDevice )
{
	float tan_half_x, tan_half_y;
	GetProjectionTangents(&tan_half_x, &tan_half_y);

	long screen_width, screen_height, centre_x, centre_y;
	GetScreenDimensions(&screen_width, &screen_height);
	GetProjectionCentre(&centre_x, &centre_y);

	// Focal lengths in screen pixels, then the four frustum edges as the pixel distance
	// from the principal point out to each screen edge.
	const float zn = 0.5f;
	const float focal_x = (screen_width  * 0.5f) / tan_half_x;
	const float focal_y = (screen_height * 0.5f) / tan_half_y;

	// b/t follow D3DXMatrixPerspectiveFovLH's flipped y (dx_linux.cpp): b is the screen
	// TOP edge, t the screen BOTTOM one. With a centred principal point these come out
	// as the +fh / -fh that function uses.
	const float l = -(centre_x                  / focal_x) * zn;
	const float r =  ((screen_width  - centre_x) / focal_x) * zn;
	const float b =  (centre_y                  / focal_y) * zn;
	const float t = -((screen_height - centre_y) / focal_y) * zn;

	D3DXMATRIX matProj;
	D3DXMatrixPerspectiveOffCenterLH( &matProj, l, r, b, t, zn, FURTHEST_Z );
	pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
}

//--------------------------------------------------------------------------------------
// Colours
//--------------------------------------------------------------------------------------

#define NUM_PALETTE_ENTRIES     (42+6)
//#define	PALETTE_COMPONENT_BITS	(8)		// bits per colour r/g/b component

static PALETTEENTRY SCPalette[NUM_PALETTE_ENTRIES] =
	{
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00},

		// car colours 1
		{0x00, 0x00, 0x00},
		{0x88, 0x00, 0x22},
		{0xaa, 0x00, 0x33},
		{0xcc, 0x00, 0x44},
		{0xee, 0x00, 0x55},
		{0x22, 0x22, 0x33},
		{0x44, 0x44, 0x44},
		{0x33, 0x33, 0x33},

		// car colours 2
		{0x00, 0x00, 0x00},
		{0x22, 0x00, 0x88},
		{0x33, 0x00, 0xaa},
		{0x44, 0x00, 0xcc},
		{0x55, 0x00, 0xee},
		{0x22, 0x22, 0x33},
		{0x44, 0x44, 0x44},
		{0x33, 0x33, 0x33},

		// track colours (i.e. Stunt Car Racer car colours)
		{0x00, 0x00, 0x00},
		{0x99, 0x99, 0x77},
		{0xbb, 0xbb, 0x99},
		{0xff, 0xff, 0x00},
		{0x99, 0xbb, 0x33},
		{0x55, 0x77, 0x77},
		{0x55, 0xbb, 0xff},
		{0x55, 0x99, 0xff},
		{0x33, 0x55, 0x77},
		{0x55, 0x00, 0x00}, // 9
		{0x77, 0x33, 0x33},	//10
		{0x99, 0x55, 0x55},
		{0xdd, 0x99, 0x99}, //12
		{0x77, 0x77, 0x55},
		{0xbb, 0xbb, 0xbb},
		{0xff, 0xff, 0xff},

		// extra track colours (altered super league)
		{ 51,   51,  119},	// SCR_BASE_COLOUR+16
		{119,  153,  119},
		{ 85,  153,   85},
		{0x00, 0x00, 0x55}, //19
		{0x33, 0x33, 0x77},	//20
		{0x99, 0x99, 0xdd}, //21
	};


DWORD SCRGB (long colour_index)		// return full RGB value
	{
	return(D3DCOLOR_XRGB(SCPalette[colour_index].peRed,
					SCPalette[colour_index].peGreen,
					SCPalette[colour_index].peBlue));
	}


/*
 * As SCRGB, but scaled by a lighting factor (0..1).  The palette has no usable shade ramp
 * (index+1 is a different hue as often as it is a lighter one), so we scale the RGB rather
 * than shift the index - the same trick the old reducedSCPalette used with its flat 5/8.
 * The caller quantises the factor, so the result still reads as flat-shaded faces.
 */
DWORD SCRGBShaded (long colour_index, float shade)
	{
	long r = static_cast<long>(SCPalette[colour_index].peRed   * shade + 0.5f);
	long g = static_cast<long>(SCPalette[colour_index].peGreen * shade + 0.5f);
	long b = static_cast<long>(SCPalette[colour_index].peBlue  * shade + 0.5f);

	if (r > 255) r = 255;	if (r < 0) r = 0;
	if (g > 255) g = 255;	if (g < 0) g = 0;
	if (b > 255) b = 255;	if (b < 0) b = 0;

	return(D3DCOLOR_XRGB(r, g, b));
	}

DWORD Fill_Colour, Line_Colour;

void SetSolidColour (long colour_index)
	{
/*
    static DWORD reducedSCPalette[NUM_PALETTE_ENTRIES];
    static long first_time = TRUE;

    // make all reduced palette values on first call
    if (first_time)
        {
        long i;
        for (i = 0; i < NUM_PALETTE_ENTRIES; i++)
            {
            // reduce R/G/B to 5/8 of original
            reducedSCPalette[i] = D3DCOLOR_XRGB((5*SCPalette[i].peRed)/8,
			                               (5*SCPalette[i].peGreen)/8,
			                               (5*SCPalette[i].peBlue)/8);
            }

        first_time = FALSE;
        }

	Fill_Colour = reducedSCPalette[colour_index];
*/
	Fill_Colour = SCRGB(colour_index);
	}


void SetLineColour (long colour_index)
	{
	Line_Colour = SCRGB(colour_index);
	}


void SetTextureColour (long colour_index)
	{
	Fill_Colour = SCRGB(colour_index);
	}

#ifdef NOT_USED
/*	======================================================================================= */
/*	Function:		EnforceConstantFrameRate												*/
/*																							*/
/*	Description:	Attempt to keep frame rate close to MAX_FRAME_RATE						*/
/*	======================================================================================= */

static void EnforceConstantFrameRate( long max_frame_rate )
	{
	static long first_time = TRUE;

	static DWORD last_time_ms;
	DWORD this_time_ms, frame_time_ms;
	DWORD min_frame_time_ms = (1000/max_frame_rate);
	long remaining_ms;	// use long because it is signed (DWORD isn't)


	if (first_time)
		{
		first_time = FALSE;
		last_time_ms = timeGetTime();
		}
	else
	{
	this_time_ms = timeGetTime();
	frame_time_ms = this_time_ms - last_time_ms;

	remaining_ms = static_cast<long>(min_frame_time_ms) - static_cast<long>(frame_time_ms);
	last_time_ms = this_time_ms;	if (remaining_ms > 0)
		{
		Sleep(remaining_ms);
		last_time_ms += static_cast<DWORD>(remaining_ms);
		}
	}	return;
	}
#endif

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
#ifdef SCR_PORTABLE
TTF_Font *g_pFont = NULL;
TTF_Font *g_pFontLarge = NULL;
float GetTextScale() {
	// Must match the cockpit's scaling (DrawCockpit) so the dashboard readouts stay in
	// their boxes at any window size.
	long current_width, current_height;
	GetScreenDimensions(&current_width, &current_height);
	float base_width = wideScreen ? static_cast<float>(BASE_WIDTH_WIDESCREEN)
								  : static_cast<float>(BASE_WIDTH_STANDARD);
	return static_cast<float>(current_width) / base_width;
}
GLuint   g_pSprite = 0;	// Texture for batching text calls
#else
ID3DXFont *g_pFont = NULL;         // Font for drawing text
ID3DXFont *g_pFontLarge = NULL;    // Font for drawing large text

/*	======================================================================================= */
/*	Function:		GetTextScale															*/
/*																									*/
/*	Description:	Calculate text scaling factor based on current vs base resolution		*/
/*					Used to scale font sizes and text positions for different window sizes	*/
/*																									*/
/*	Returns:		Scaling factor (1.0 = base resolution, 2.0 = double size, etc.)		*/
/*	======================================================================================= */

// Helper function to get text scale based on current resolution
float GetTextScale()
{
	long current_width, current_height;
	GetScreenDimensions(&current_width, &current_height);
	float base_width = wideScreen ? static_cast<float>(BASE_WIDTH_WIDESCREEN) : static_cast<float>(BASE_WIDTH_STANDARD);
	return static_cast<float>(current_width) / base_width;
}
ID3DXSprite *g_pSprite = NULL;       // Sprite for batching draw text calls
#endif

#ifndef SCR_PORTABLE
//--------------------------------------------------------------------------------------
// Rejects any devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9 *pCaps, D3DFORMAT AdapterFormat, 
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void *pUserContext )
{
    // Typically want to skip backbuffer formats that don't support alpha blending
    IDirect3D9 *pD3D = DXUTGetD3DObject(); 
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                    AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, 
                    D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Before a device is created, modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings *pDeviceSettings, const D3DCAPS9 *pCaps, void *pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning();
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3DPOOL_MANAGED resources here 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9 *pd3dDevice, const D3DSURFACE_DESC *pBackBufferSurfaceDesc, void *pUserContext )
{
    HRESULT hr;

//    V_RETURN( g_DialogResourceManager.OnCreateDevice( pd3dDevice ) );
//    V_RETURN( g_SettingsDlg.OnCreateDevice( pd3dDevice ) );

    // Initialize the fonts with scaled sizes
	float textScale = GetTextScale();
    V_RETURN( D3DXCreateFont( pd3dDevice, static_cast<int>(15 * textScale), 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                              L"Arial", &g_pFont ) );

    V_RETURN( D3DXCreateFont( pd3dDevice, static_cast<int>(25 * textScale), 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                              L"Arial", &g_pFontLarge ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3DPOOL_DEFAULT resources here 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9 *pd3dDevice, 
                                const D3DSURFACE_DESC *pBackBufferSurfaceDesc, void *pUserContext )
{
    HRESULT hr;

//    V_RETURN( g_DialogResourceManager.OnResetDevice() );
//    V_RETURN( g_SettingsDlg.OnResetDevice() );

    // Recreate fonts with proper scaling for new resolution
	if( g_pFont )
	{
		g_pFont->Release();
		g_pFont = NULL;
	}
	if( g_pFontLarge )
	{
		g_pFontLarge->Release();
		g_pFontLarge = NULL;
	}
	
	float textScale = GetTextScale();
	V_RETURN( D3DXCreateFont( pd3dDevice, static_cast<int>(15 * textScale), 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
	                          OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
	                          L"Arial", &g_pFont ) );
	
	V_RETURN( D3DXCreateFont( pd3dDevice, static_cast<int>(25 * textScale), 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
	                          OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
	                          L"Arial", &g_pFontLarge ) );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite ) );

	if (FAILED(D3DXCreateTextureFromResource(pd3dDevice, NULL, L"ATLAS", &g_pAtlas)))
	{
		OutputDebugStringW(L"ERROR: Failed to create texture from ATLAS resource\n");
		return E_FAIL;
	}

	InitAtlasCoord();

	// D3DX would round the 10x116 chain strip up to a power of two and leave the UVs
	// pointing at the padding, so ask for the size as authored.
	if (FAILED(D3DXCreateTextureFromResourceEx(pd3dDevice, NULL, L"CHAIN",
	                                           D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2,
	                                           1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
	                                           D3DX_DEFAULT, D3DX_DEFAULT, 0,
	                                           NULL, NULL, &g_pChain)))
	{
		OutputDebugStringW(L"ERROR: Failed to create texture from CHAIN resource\n");
		return E_FAIL;
	}

	if ((hr = CreatePolygonVertexBuffer(pd3dDevice)) != S_OK)
	{
		OutputDebugStringW(L"ERROR: Failed to create polygon vertex buffer\n");
		return hr;
	}
	if ((hr = CreateTrackVertexBuffer(pd3dDevice)) != S_OK)
	{
		OutputDebugStringW(L"ERROR: Failed to create track vertex buffer\n");
		return hr;
	}
	if ((hr = CreateShadowVertexBuffer(pd3dDevice)) != S_OK)
	{
		OutputDebugStringW(L"ERROR: Failed to create shadow vertex buffer\n");
		return hr;
	}
	if ((hr = CreateCarVertexBuffer(pd3dDevice)) != S_OK)
	{
		OutputDebugStringW(L"ERROR: Failed to create car vertex buffer\n");
		return hr;
	}
	if ((hr = CreateCockpitVertexBuffer(pd3dDevice)) != S_OK)
	{
		OutputDebugStringW(L"ERROR: Failed to create cockpit vertex buffer\n");
		return hr;
	}

	// Set the projection transform (view and world are updated per frame)
	SetSceneProjection( pd3dDevice );

    pd3dDevice->SetRenderState( D3DRS_ZENABLE,      TRUE );
    pd3dDevice->SetRenderState( D3DRS_SHADEMODE,    D3DSHADE_FLAT );
    pd3dDevice->SetRenderState( D3DRS_LIGHTING,     FALSE );

	// Disable texture mapping by default (only DrawTrack() enables it)
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );

	return S_OK;
}
#else
// some helper functions....
void CreateFonts()
{
	if(!TTF_WasInit() && TTF_Init()==-1) {
		printf("TTF_Init: %s\n", TTF_GetError());
		exit(1);
	}

	if (g_pFont==NULL)
	{
		g_pFont = TTF_OpenFont("DejaVuSans-Bold.ttf", 15);
	}
	if (g_pFontLarge==NULL)
	{
		g_pFontLarge = TTF_OpenFont("DejaVuSans-Bold.ttf", 25);
	}
	printf("Font created (%p / %p)\n", g_pFont, g_pFontLarge);
}
void CloseFonts()
{
	if (g_pFont!=NULL)
	{
		TTF_CloseFont(g_pFont);
		g_pFont = 0;
	}
	if (g_pFontLarge!=NULL)
	{
		TTF_CloseFont(g_pFontLarge);
		g_pFontLarge = NULL;
	}
}
void LoadTextures()
{
	if (!g_pAtlas) g_pAtlas = new IDirect3DTexture9();
	g_pAtlas->LoadTexture("Bitmap/atlas.png");
	InitAtlasCoord();

	// Not LoadTexture(): that flips the image vertically, so the chain's UVs would have
	// to be worked out upside down here and the right way up in the DirectX build.
	// CreateFromMemory uploads the rows as they are, which is what the tiling in
	// DrawCockpit assumes.
	{
		int cw, ch, cn;
		unsigned char *chain = stbi_load("Bitmap/ChainLeft.png", &cw, &ch, &cn, STBI_rgb_alpha);
		if (chain == NULL)
			printf("Warning, image \"Bitmap/ChainLeft.png\" not loaded\n");
		else
		{
			if (!g_pChain) g_pChain = new IDirect3DTexture9();
			g_pChain->CreateFromMemory(chain, cw, ch, 4, false /*nearest*/, false /*repeat V*/);
			stbi_image_free(chain);
		}
	}
#ifdef SCR_ROAD_TEXTURE
	// Reads the road cells back out of atlas.png, so it has to follow InitAtlasCoord()
	CreateRoadTextures();
#endif
	printf("Texture loaded\n");
}
void CreateBuffers(IDirect3DDevice9 *pd3dDevice)
{
	if (CreatePolygonVertexBuffer(pd3dDevice) != S_OK)
		printf("Error creating PolygonVertexBuffer\n");
	if (CreateTrackVertexBuffer(pd3dDevice) != S_OK)
		printf("Error creating TrackVertexBuffer\n");
	if (CreateShadowVertexBuffer(pd3dDevice) != S_OK)
		printf("Error creating ShadowVertexBuffer\n");
	if (CreateCarVertexBuffer(pd3dDevice) != S_OK)
		printf("Error creating CarVertexBuffer\n");
	if (CreateCockpitVertexBuffer(pd3dDevice) != S_OK)
		printf("Error creating CarVertexBuffer\n");

}
#endif	//!SCR_PORTABLE
/*	======================================================================================= */
/*	Function:		CalcTrackMenuViewpoint													*/
/*																							*/
/*	Description:	*/
/*	======================================================================================= */

static void CalcTrackMenuViewpoint( void )
{
static long circle_y_angle = 0;

short sin, cos;
long centre = (NUM_TRACK_CUBES * CUBE_SIZE)/2;
long radius = ((NUM_TRACK_CUBES - 2) * CUBE_SIZE)/PRECISION;

	// Target orientation - centre of world
	target_x = (NUM_TRACK_CUBES * CUBE_SIZE)/2;
	target_y = 0;
	target_z = (NUM_TRACK_CUBES * CUBE_SIZE)/2;

	// camera moves in a circle around the track
	if (!bPaused) circle_y_angle += 128;
	circle_y_angle &= (MAX_ANGLE - 1);

	GetSinCos(circle_y_angle, &sin, &cos);

	viewpoint1_x = centre + (sin * radius);
	viewpoint1_y = -CUBE_SIZE * 3;
	viewpoint1_z = centre + (cos * radius);

	LockViewpointToTarget(viewpoint1_x,
						  viewpoint1_y,
						  viewpoint1_z,
						  target_x,
						  target_y,
						  target_z,
						  &viewpoint1_x_angle,
						  &viewpoint1_y_angle);
	viewpoint1_z_angle = 0;
}

/*	======================================================================================= */
/*	Function:		CalcTrackPreviewViewpoint												*/
/*																							*/
/*	Description:	*/
/*	======================================================================================= */

#define NUM_PREVIEW_CAMERAS (9)

/*	The Amiga's track preview (R.604b4, "Reference only/StuntCarRacer.s":13603) is not a
	chase camera at all.  It is FOUR FIXED viewpoints, one at the middle of each edge of the
	16 x 16 map, each looking straight in along its axis, and you cycle round them with fire.
	No car is shown and nothing moves.

	    TAB.60552	world.x	 = 4, 0, 4, 8		(bytes; see below)
			world.z	 = 0, 4, 8, 4
			y.angle	 = $00, $40, $80, $c0	(0, 90, 180, 270 degrees)

	Those x/z bytes are written to the TOP byte of a longword world coordinate, and a map
	square is $800000, so a byte of 4 means 8 map squares - i.e. the four positions are
	(8,0), (0,8), (8,16), (16,8) in map squares, the mid-point of each edge of the map.

	Height is players.world.y = $03f00000.  Amiga world y is up-positive with 0 at ground
	level and a $0400 (1024) ceiling on the player - see "limit player's height" at
	"Reference only/StuntCarRacer.s":15714 - so $03f0 = 1008 is as high as the world goes,
	and at 128 per map square that is 7.875 map squares up.  Ours is the same scale but
	y-DOWN, hence the negation.

	Pitch: the preview sets y.shift = 1792 where the game's y.shift is -players.x.angle in
	degrees*256, so the camera is tilted 7 degrees down.

	Not reproduced: the preview also swaps in a different projection (calculate.screen.x /
	calculate.screen.y at ":16684"), which halves x, quarters z and adds a large constant
	depth, and scales y by 19483/32768.  That is a per-axis hack with a DIFFERENT depth for
	x than for y - not a projective transform, so no single frustum can express it.  We keep
	the game's field of view.									*/

#define NUM_AMIGA_PREVIEW_VIEWS	(4)

/*	Which of the four the preview is currently showing - the Amiga's B.1bb57 & 3.			*/
long gTrackPreviewView = 0;

/*	Off restores the PC port's own preview (a camera parked near the centre of the map,
	locked onto the opponent's car as it drives round).									*/
bool bAmigaTrackPreview = true;

/*	========================================================================================
	The Amiga's preview SCREEN.  The original decrunched a full-screen picture into chip RAM
	(preview.crunched, set.and.preview.road at "Reference only/StuntCarRacer.s":10137) and
	drew the road into a window cut out of it: a checkerboard-framed view of an arena with
	mountains, grandstands and a dirt floor, with the course title on a panel underneath.

	Bitmap/trackpreview.png is that picture, at the Amiga's own 320x200.  These are its
	measurements in surface pixels.  (Bitmap/trackpreview_raw.png is the original grab it
	was cleaned from - see tools/clean_trackpreview.py.)
	======================================================================================== */

#define PREVIEW_SCREEN_IMAGE	"trackpreview.png"

/*	The picture window inside the checkerboard border, which the 3D road is drawn into.	*/
#define PREVIEW_WINDOW_X		12
#define PREVIEW_WINDOW_Y		10
#define PREVIEW_WINDOW_W		296
#define PREVIEW_WINDOW_H		134

/*	The bevelled title panel below it, and the row the prompt line sits on.				*/
#define PREVIEW_PANEL_X			80
#define PREVIEW_PANEL_Y			168
#define PREVIEW_PANEL_W			160
#define PREVIEW_PANEL_H			16
#define PREVIEW_PROMPT_ROW		24

/*	The dirt arena floor inside the picture, which is where the track has to end up.  The
	painted floor runs from y=67 (behind the grandstands) to the bottom of the window at
	y=143, and x=14 to x=307 at its widest; this is that area inset a little, so the track
	sits on the dirt with a margin the way the original's does.  SetPreviewWindowProjection
	fits the whole 16 x 16 map into this rectangle.										*/
#define PREVIEW_FLOOR_X			22
#define PREVIEW_FLOOR_Y			78
#define PREVIEW_FLOOR_W			276
#define PREVIEW_FLOOR_H			60

bool bAmigaPreviewScreen = true;

/*	How far back from the centre of the map the eye sits, in map squares.  The Amiga's own
	viewpoints are ON the edge (8 squares out), which only works because its preview
	projection quarters z and adds a large constant depth - the whole track is pushed away
	and flattened into a long lens.  We cannot express that as a frustum, so we do the
	geometric equivalent and stand the camera back instead.  The distance no longer sets the
	framing (the projection is fitted to the arena floor, below); all it controls is how much
	perspective there is, and 32 squares gives the flat, long-lens look the original's
	quartered z does - the near edge of the map comes out 1.7x the far edge.				*/
#define PREVIEW_EYE_DISTANCE	(32)

/*	How steeply the eye looks down, in degrees.  The Amiga's is 44.5: players.world.y =
	$03f00000 = 1008, and at 128 per map square that is 7.875 squares up, over the 8 squares
	its viewpoints stand out from the centre (world y is up-positive there with 0 at ground
	level - see the $0400 ceiling at "Reference only/StuntCarRacer.s":15714).  Steep enough
	to look down ON the road rather than along it, which is what shows the loop as a loop;
	the flattening that made the original look like a poster rather than a plan view comes
	from the projection, not from lowering the eye.  Ours is a little shallower so the road
	still reads as road.																	*/
#define PREVIEW_EYE_ELEVATION	(38.0)

static void CalcAmigaTrackPreviewViewpoint( void )
{
	// Which way the eye faces, from TAB.60552's y.angle bytes $00/$40/$80/$c0.  y_angle is
	// atan2(dx, dz) here (see LockViewpointToTarget), so the heading is (sin, cos) and the
	// eye stands back along the opposite of it.
	static const long view_a[NUM_AMIGA_PREVIEW_VIEWS] = { 0, 64, 128, 192 };

	const long view    = gTrackPreviewView & (NUM_AMIGA_PREVIEW_VIEWS - 1);
	const long centre  = (NUM_TRACK_CUBES / 2) * CUBE_SIZE;

	const double radians = (static_cast<double>(view_a[view]) * 2.0 * PI) / 256.0;
	const char *dist_env = getenv("SCR_PREVIEW_DIST");
	const double back    = static_cast<double>(dist_env ? atof(dist_env) : PREVIEW_EYE_DISTANCE) * static_cast<double>(CUBE_SIZE);

	// Aim at the centre of the map, as the Amiga's four axis-aligned views all do.
	target_x = centre;
	target_y = 0;					// road level, as CalcTrackMenuViewpoint uses
	target_z = centre;

	const char *elev_env = getenv("SCR_PREVIEW_ELEV");
	const double elevation = (elev_env ? atof(elev_env) : PREVIEW_EYE_ELEVATION) * PI / 180.0;

	viewpoint1_x = centre - static_cast<long>(sin(radians) * back);
	viewpoint1_z = centre - static_cast<long>(cos(radians) * back);
	viewpoint1_y = -static_cast<long>(back * tan(elevation));

	// Pitch and heading follow from the two points; the view matrix is built with LookAt, but
	// DrawBackdrop reads these angles.
	LockViewpointToTarget(viewpoint1_x,
						  viewpoint1_y,
						  viewpoint1_z,
						  target_x,
						  target_y,
						  target_z,
						  &viewpoint1_x_angle,
						  &viewpoint1_y_angle);
	viewpoint1_z_angle = 0;
}

static void CalcTrackPreviewViewpoint( void )
{
	if (bAmigaTrackPreview)
	{
		CalcAmigaTrackPreviewViewpoint();
		if (getenv("SCR_PREVIEW_DEBUG"))
			printf("preview viewpoint: %ld,%ld,%ld target %ld,%ld,%ld\n",
				   viewpoint1_x, viewpoint1_y, viewpoint1_z, target_x, target_y, target_z);
		return;
	}

	// Target orientation - opponent
	target_x = opponent_x,
	target_y = opponent_y,
	target_z = opponent_z;

#ifndef  PREVIEW_METHOD1
	long centre = (NUM_TRACK_CUBES * CUBE_SIZE)/2;

	viewpoint1_x = centre;

	if (TrackID == DRAW_BRIDGE)
		viewpoint1_y = opponent_y - (CUBE_SIZE*5)/2;	// Draw Bridge requires a higher viewpoint
	else
		viewpoint1_y = opponent_y - CUBE_SIZE/2;

	viewpoint1_z = centre;

    viewpoint1_x += (target_x-viewpoint1_x)/2;
    viewpoint1_z += (target_z-viewpoint1_z)/2;

	// lock viewpoint y angle to target
	LockViewpointToTarget(viewpoint1_x,
						  viewpoint1_y,
						  viewpoint1_z,
						  target_x,
						  target_y,
						  target_z,
						  &viewpoint1_x_angle,
						  &viewpoint1_y_angle);
#else
    // cameras - four at corners, four half way along, one at centre
    long camera_x[NUM_PREVIEW_CAMERAS] =
                                        {CUBE_SIZE,
                                         CUBE_SIZE,
                                         (NUM_TRACK_CUBES-1) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES-1) * CUBE_SIZE,
                                         //
                                         0,
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE,
                                         //
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE
                                        };
    long camera_z[NUM_PREVIEW_CAMERAS] =
                                        {CUBE_SIZE,
                                         (NUM_TRACK_CUBES-1) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES-1) * CUBE_SIZE,
                                         CUBE_SIZE,
                                         //
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES) * CUBE_SIZE,
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE,
                                         0,
                                         //
                                         (NUM_TRACK_CUBES/2) * CUBE_SIZE
                                        };

    // calculate nearest camera
    long camera, distance, shortest_distance = 0, nearest = 0;
    double o, a;
    for (camera = 0; camera < NUM_PREVIEW_CAMERAS; camera++)
        {
        o = double(camera_x[camera] - target_x);
        a = double(camera_z[camera] - target_z);
        distance = static_cast<long>(sqrt((o*o) + (a*a)));

        if (camera == 0)
            {
            shortest_distance = distance;
            nearest = camera;
            }
        else if (distance < shortest_distance)
            {
            shortest_distance = distance;
            nearest = camera;
            }
        }

	viewpoint1_x = camera_x[nearest];
	viewpoint1_y = player1_y - CUBE_SIZE/2;
	viewpoint1_z = camera_z[nearest];

	LockViewpointToTarget(viewpoint1_x,
						  viewpoint1_y,
						  viewpoint1_z,
						  target_x,
						  target_y,
						  target_z,
						  &viewpoint1_x_angle,
						  &viewpoint1_y_angle);
#endif

	viewpoint1_z_angle = 0;
}

/*	======================================================================================= */
/*	Function:		CalcGameViewpoint														*/
/*																							*/
/*	Description:	*/
/*	======================================================================================= */

/*	======================================================================================= */
/*	Function:		CalcAmigaYPerspectiveShift												*/
/*																							*/
/*	Description:	Height of the in-car camera above the car body, the way the Amiga		*/
/*					original did it (`y.pers.shift`, set.road.position.values in			*/
/*					"Reference only/StuntCarRacer.s":13396).								*/
/*																							*/
/*					The Amiga never clamped the camera against the road. Instead it			*/
/*					RAISES the eye point in proportion to how far the wheels are below		*/
/*					the road surface, so the view climbs out of the road exactly as the		*/
/*					suspension compresses. Above a compression of $500 the slope doubles	*/
/*					(hard landings and corner loading lift the camera fast). Nose-down		*/
/*					pitch adds a further half-angle of lift.								*/
/*																							*/
/*					The 68k source, verbatim:												*/
/*						move.w	#$780,d3												*/
/*						move.w	average.amount.below.road,d0							*/
/*						cmpi.w	#$500,d0												*/
/*						bcs	srpv1														*/
/*						asl.w	#1,d0													*/
/*						move.w	#$280,d3												*/
/*					srpv1	add.w	d3,d0												*/
/*						move.w	players.x.angle,d3											*/
/*						bpl	srpv2														*/
/*						asr.w	#1,d3													*/
/*						sub.w	d3,d0													*/
/*					srpv2	asr.w	#4,d0												*/
/*						add.w	players.smaller.y,d0										*/
/*						move.w	d0,y.pers.shift											*/
/*																							*/
/*					players.smaller.y is the car's own Y (players.world.y >> 11), which		*/
/*					here is player1_y, so we return only the offset term.					*/
/*																							*/
/*					Units: the Amiga shift counts 2^11 of players.world.y; player1_y is		*/
/*					player_y * LOCAL_Y_FACTOR, so one Amiga unit is 2^13 here. Sanity		*/
/*					check: with no compression and level pitch the offset is				*/
/*					($780 >> 4) = 120 units = 120 << 13 = 60 << LOG_PRECISION -- exactly		*/
/*					the HEIGHT_ABOVE_ROAD 60 this replaces. The old fixed 100 was a			*/
/*					port-era fudge standing in for the missing dynamic term.				*/
/*	======================================================================================= */

#define AMIGA_Y_SHIFT_LOG	13		// see units note above

static long CalcAmigaYPerspectiveShift( void )
{
	// average.amount.below.road, as CarCollisionDetection computes it. Taken from
	// the per-wheel globals so this works under both the legacy and FloatV2 paths
	// (CopyFloatV2ToLegacy writes them).
	long average_front = (front_left_amount_below_road + front_right_amount_below_road) >> 1;
	long below         = (average_front + rear_amount_below_road) >> 1;

	// ---- Not Amiga; needed because we no longer tick at the Amiga's rate -----
	// amount.below.road is set to 0 the instant a wheel leaves the road. That is
	// faithful -- the Amiga (front.left.above.road, StuntCarRacer.s:15987), the
	// FloatV2 C# and our port all do it. But FloatV2's contact test carries a
	// predictive term, (d - oldDiff) * 1.078125/dtRatio, whose divisor makes it
	// 6.47x the per-step delta at 60Hz. A wheel barely unloading for a single
	// step is then enough to drive the test negative, zero the average, and drop
	// the camera by the whole compression lift for exactly one frame -- the
	// one-frame flash through the road. The Amiga never saw this because its
	// camera sampled a ~10Hz value where one step spanned a whole 1/10s.
	//
	// So smooth only the camera's copy, asymmetrically: rise instantly, so
	// impacts still lift the view with no lag and the original feel is kept, but
	// fall gradually, over roughly one Amiga frame. Physics is untouched.
	static double heldBelow = 0.0;
	if (static_cast<double>(below) >= heldBelow)
		heldBelow = static_cast<double>(below);
	else
		heldBelow += (static_cast<double>(below) - heldBelow) * 0.25;	// ~1/10s at 60Hz
	below = static_cast<long>(heldBelow);
	// -------------------------------------------------------------------------

	long base = 0x780;
	if (below >= 0x500)
	{
		below <<= 1;		// slope doubles once the suspension is well compressed
		base = 0x280;
	}
	long shift = below + base;

	// Nose-down pitch lifts the eye further. The Amiga tests the sign of the
	// 16-bit angle, so convert our unsigned 0..65535 global to signed first
	// (the same trap as the FloatV2 angle boundary).
	short x_angle = static_cast<short>(player1_x_angle & (MAX_ANGLE - 1));
	if (x_angle < 0)
		shift -= (x_angle >> 1);	// subtracting a negative: adds |angle| / 2

	shift >>= 4;

	return (shift << AMIGA_Y_SHIFT_LOG);
}

/*	======================================================================================= */
/*	Render interpolation helpers - see the PLAYER_STATE comment above.						*/
/*	======================================================================================= */

static void PlayerStateCapture( PLAYER_STATE *s )
{
	s->x = player1_x;			s->y = player1_y;			s->z = player1_z;
	s->x_angle = player1_x_angle;	s->y_angle = player1_y_angle;	s->z_angle = player1_z_angle;
}

static void PlayerStateApply( const PLAYER_STATE *s )
{
	player1_x = s->x;			player1_y = s->y;			player1_z = s->z;
	player1_x_angle = s->x_angle;	player1_y_angle = s->y_angle;	player1_z_angle = s->z_angle;
}

/*	Blend one angle the short way round.  These are 16-bit angles: y_angle wraps through
	zero freely, so a car crossing north goes 65500 -> 40 and the straight average would
	spin it the long way round the compass.  Take the difference into signed range first,
	then step a fraction of it.  x_angle and z_angle are clamped well inside the range and
	never wrap, but the same arithmetic is correct for them anyway.					*/
static long LerpAngle( long from, long to, double alpha )
{
	long delta = (to - from) & (MAX_ANGLE - 1);
	if (delta >= (MAX_ANGLE / 2))
		delta -= MAX_ANGLE;

	long result = from + static_cast<long>(static_cast<double>(delta) * alpha);

	/*	x_angle and z_angle are held as signed values around zero (the +-11264 pitch/roll
		clamps), while y_angle is a 0..65535 compass bearing that the game masks.  Masking
		here would turn a small negative pitch into a value near 65536, so only bring the
		result back into range when the input was already in it.					*/
	if ((from >= 0) && (to >= 0))
		result &= (MAX_ANGLE - 1);

	return result;
}

static void PlayerStateLerp( PLAYER_STATE *out, const PLAYER_STATE *a, const PLAYER_STATE *b, double alpha )
{
	/*	A teleport, not motion - the crane setting the car down, a respawn, the 'R'
		turnaround.  Blending across it would drag the car through the scenery, so take
		the new state whole.													*/
	long long dx = static_cast<long long>(b->x) - a->x;
	long long dy = static_cast<long long>(b->y) - a->y;
	long long dz = static_cast<long long>(b->z) - a->z;
	if ((llabs(dx) > INTERP_TELEPORT_LIMIT) ||
		(llabs(dy) > INTERP_TELEPORT_LIMIT) ||
		(llabs(dz) > INTERP_TELEPORT_LIMIT))
	{
		*out = *b;
		return;
	}

	out->x = a->x + static_cast<long>(static_cast<double>(dx) * alpha);
	out->y = a->y + static_cast<long>(static_cast<double>(dy) * alpha);
	out->z = a->z + static_cast<long>(static_cast<double>(dz) * alpha);

	out->x_angle = LerpAngle(a->x_angle, b->x_angle, alpha);
	out->y_angle = LerpAngle(a->y_angle, b->y_angle, alpha);
	out->z_angle = LerpAngle(a->z_angle, b->z_angle, alpha);
}


/*	The same for the opponent - see OPPONENT_STATE.  Its angles are radians rather than
	Amiga angle units (OpponentBehaviour builds them with atan2, and the netplay path
	converts into the same form), so this is the float twin of the code above.		*/

/*	Set by anything that moves the opponent; consumed once per render frame by
	OpponentStateAdvance().  Sampling per frame rather than per step is deliberate: at a
	physics rate near the display rate a frame takes two steps now and then, and
	timestamping each step separately would leave the pair a fraction of a millisecond
	apart, which the blend would cross in one frame - the stepping we are removing.  What
	the display actually needs is where the car was when it was last drawn and where it
	is now, spanning however long that frame took.								*/
static bool gOppStepped = false;

static void OpponentStateMark( void )
{
	gOppStepped = true;
}

/*	Rolls the previous sample down and timestamps the new one, which is what paces
	the blend.  Called once per render frame, after all of that frame's stepping.	*/
static void OpponentStateAdvance( void )
{
	if (!gOppStepped)
		return;			// no new state - hold the pair, and let alpha run out to 1
	gOppStepped = false;

	gOppPrev     = gOppCurr;
	gOppPrevTime = gOppCurrTime;

	gOppCurr.x = opponent_x;					gOppCurr.y = opponent_y;					gOppCurr.z = opponent_z;
	gOppCurr.x_angle = opponent_x_angle;		gOppCurr.y_angle = opponent_y_angle;		gOppCurr.z_angle = opponent_z_angle;
	gOppCurrTime = DXUTGetTime();

	// Two samples and a positive gap between them, or there is nothing to blend along.
	if (gOppCurrTime > gOppPrevTime)
		gOppStatesValid = true;
}

/*	The car has been put somewhere outright rather than driven there - a new race, the
	crane.  There is no motion to blend, so start the pair again.					*/
static void OpponentStateReset( void )
{
	gOppStatesValid = false;
	gOppStepped = false;
	gOppPrevTime = gOppCurrTime = 0.0;
}

/*	Blend one angle the short way round, radians.  Same wrap problem as LerpAngle: a car
	crossing north goes from just under 2pi to just over 0.						*/
static float LerpAngleF( float from, float to, double alpha )
{
	const double kTwoPi = 2.0 * D3DX_PI;

	double delta = static_cast<double>(to) - static_cast<double>(from);
	while (delta >  D3DX_PI) delta -= kTwoPi;
	while (delta < -D3DX_PI) delta += kTwoPi;

	return static_cast<float>(static_cast<double>(from) + delta * alpha);
}

/*	Where to draw the opponent this render frame: the two samples, blended by how far the
	display clock has run past the newer one.  Falls back to the newest state whenever
	there is nothing sane to interpolate along, so the car is never left undrawn.	*/
static void OpponentStateForRender( OPPONENT_STATE *out )
{
	if (!gOppStatesValid || !gRenderInterpolation || bPaused || scr::gSimTraceEnabled)
	{
		out->x = opponent_x;					out->y = opponent_y;					out->z = opponent_z;
		out->x_angle = opponent_x_angle;		out->y_angle = opponent_y_angle;		out->z_angle = opponent_z_angle;
		return;
	}

	/*	The gap between the last two updates is the step length, whichever clock produced
		it.  Clamping alpha to 1 means a stalled opponent simply holds its newest state
		rather than being extrapolated off into the distance.					*/
	const double interval = gOppCurrTime - gOppPrevTime;
	double alpha = (DXUTGetTime() - gOppCurrTime) / interval;
	if (alpha < 0.0) alpha = 0.0;
	if (alpha > 1.0) alpha = 1.0;

	// A teleport, not motion - take the new state whole.  Same test as PlayerStateLerp.
	long long dx = static_cast<long long>(gOppCurr.x) - gOppPrev.x;
	long long dy = static_cast<long long>(gOppCurr.y) - gOppPrev.y;
	long long dz = static_cast<long long>(gOppCurr.z) - gOppPrev.z;
	if ((llabs(dx) > INTERP_TELEPORT_LIMIT) ||
		(llabs(dy) > INTERP_TELEPORT_LIMIT) ||
		(llabs(dz) > INTERP_TELEPORT_LIMIT))
	{
		*out = gOppCurr;
		return;
	}

	out->x = gOppPrev.x + static_cast<long>(static_cast<double>(dx) * alpha);
	out->y = gOppPrev.y + static_cast<long>(static_cast<double>(dy) * alpha);
	out->z = gOppPrev.z + static_cast<long>(static_cast<double>(dz) * alpha);

	out->x_angle = LerpAngleF(gOppPrev.x_angle, gOppCurr.x_angle, alpha);
	out->y_angle = LerpAngleF(gOppPrev.y_angle, gOppCurr.y_angle, alpha);
	out->z_angle = LerpAngleF(gOppPrev.z_angle, gOppCurr.z_angle, alpha);
}

static void CalcGameViewpoint( void )
{
long x_offset, y_offset, z_offset;

	if (bOutsideView)
	{
		// set Viewpoint 1 to behind Player 1
		// 04/11/1998 - would probably need to do a final rotation (i.e. of the trig. coefficients)
		//			    to allow a viewpoint with e.g. a different X angle to that of the player.
		//				For the car this would mean the following rotations: Y,X,Z, Y,X,Z, X
		//				For the viewpoint this would mean the following rotations: Y,X,Z, X (possibly!)
		CalcYXZTrigCoefficients(player1_x_angle,
								player1_y_angle,
								player1_z_angle);

		// vector from centre of car
		x_offset = 0;
		y_offset = 0xc0;
		z_offset = 0x300;
		WorldOffset(&x_offset, &y_offset, &z_offset);
		viewpoint1_x = (player1_x - x_offset);
		viewpoint1_y = (player1_y - y_offset);
		viewpoint1_z = (player1_z - z_offset);

		viewpoint1_x_angle = player1_x_angle;
		//viewpoint1_x_angle = (player1_x_angle + (48<<6)) & (MAX_ANGLE-1);
		viewpoint1_y_angle = player1_y_angle;
		//viewpoint1_y_angle = (player1_y_angle - (64<<6)) & (MAX_ANGLE-1);
		viewpoint1_z_angle = player1_z_angle;
		//viewpoint1_x_angle = 0;
		//viewpoint1_z_angle = 0;
	}
	else
	{
		viewpoint1_x = player1_x;
		viewpoint1_y = player1_y - CalcAmigaYPerspectiveShift();
//		viewpoint1_y = player1_y - (HEIGHT_ABOVE_ROAD << LOG_PRECISION);	// old fixed height
		viewpoint1_z = player1_z;

		viewpoint1_x_angle = player1_x_angle;
		viewpoint1_y_angle = player1_y_angle;
		viewpoint1_z_angle = player1_z_angle;
	}
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
static D3DXMATRIX matWorldTrack, matWorldCar, matWorldOpponentsCar;


/*	======================================================================================= */
/*	Function:		WorldF																	*/
/*																							*/
/*	Description:	A position in world units, keeping its fraction.							*/
/*																							*/
/*					Positions carry LOG_PRECISION fractional bits, and everything feeding a	*/
/*					transform used to throw them away with >>LOG_PRECISION - putting the		*/
/*					eye point and the cars on a whole-unit lattice. A world unit is roughly	*/
/*					a screen pixel in the near field, so the view snapped sideways a pixel	*/
/*					at a time. On a straight that barely shows: the eye's x hardly changes,	*/
/*					so its rounding sits still and the track edges hold their place. Through	*/
/*					a corner x and z are both moving and both roundings toggle every frame	*/
/*					or two, which is the shimmer along the edge of the road.					*/
/*																							*/
/*					The rotations were always continuous - only the translations were not.	*/
/*					Division in double first: the fixed-point values reach ~2^29 near the	*/
/*					far corner of the map, past what a float mantissa holds exactly.			*/
/*	======================================================================================= */

static inline float WorldF( long fixed_point_position )
{
	return static_cast<float>(static_cast<double>(fixed_point_position)
							  / static_cast<double>(1L << LOG_PRECISION));
}


static void SetCarWorldTransform( void )
{
D3DXMATRIX matRot, matTemp, matTrans;

	D3DXMatrixIdentity(&matRot);
	float xa = ((static_cast<float>(player1_x_angle) * 2 * D3DX_PI) / 65536.0f);
	float ya = ((static_cast<float>(player1_y_angle) * 2 * D3DX_PI) / 65536.0f);
	float za = ((static_cast<float>(player1_z_angle) * 2 * D3DX_PI) / 65536.0f);
	// Produce and combine the rotation matrices
	D3DXMatrixRotationZ(&matTemp, za);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	D3DXMatrixRotationX(&matTemp, xa);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	D3DXMatrixRotationY(&matTemp, ya);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	// Produce the translation matrix
	// Position car slightly higher than wheel height (VCAR_HEIGHT/4) so wheels are fully visible
	D3DXMatrixTranslation( &matTrans, WorldF(player1_x), WorldF(-player1_y)+VCAR_HEIGHT/3, WorldF(player1_z) );
	// Combine the rotation and translation matrices to complete the world matrix
	D3DXMatrixMultiply(&matWorldCar, &matRot, &matTrans);
}


/*	The opponent is the one thing you always want to be able to pick out, however far ahead	*/
/*	(or behind) it is. The renderer never culls it by distance, but the volumetric fog will	*/
/*	blend it fully into the haze long before then, which reads as a draw distance. So cap	*/
/*	the fog just for its draw call - the car keeps enough of its own colour to stay visible	*/
/*	all the way down the track, while the track around it still fades away as before.		*/
static void DrawOpponentsCar( IDirect3DDevice9 *pd3dDevice )
{
#ifdef SCR_FOG_SHADER
	const float savedFogMax = gFogMaxAmount;
	gFogMaxAmount = OPPONENT_MAX_FOG;
#endif

	DrawOpponentCar(pd3dDevice);

#ifdef SCR_FOG_SHADER
	gFogMaxAmount = savedFogMax;
#endif
}


/*	======================================================================================= */
/*	Netplay: stepping both cars																*/
/*																							*/
/*	Description:	In a head-to-head race the AI opponent does not run at all - the other	*/
/*					car is another player, driven by the same physics as this one.  So the	*/
/*					step runs CarBehaviour twice, once per car, with SelectCar swapping		*/
/*					Car_Behaviour.cpp's file-scope state in and out around each (see			*/
/*					CAR_STATE_FIELDS there).												*/
/*																							*/
/*					Two things about this are load-bearing:									*/
/*																							*/
/*					- The local player's car is always in the PLAYER slot, because the		*/
/*					  camera, the HUD, the engine sound and the damage readout all read the	*/
/*					  player globals directly and must see this machine's car.				*/
/*					- The *order* the two are stepped in is fixed by network role, host		*/
/*					  first, NOT by which one is local.  Order is part of the simulation:	*/
/*					  if the two peers stepped them in opposite orders they would not be	*/
/*					  running the same simulation, which is the one thing lockstep needs.	*/
/*	======================================================================================= */

/*	The remote car's position and orientation, in CarBehaviour's own units.  Kept across
	steps because CarBehaviour reads these back when INITIALISE_PLAYER is set - they are
	the remote car's equivalent of player1_x..player1_z_angle.							*/
static long remote_x = 0, remote_y = 0, remote_z = 0;
static long remote_x_angle = 0, remote_y_angle = 0, remote_z_angle = 0;

/*	The two cars' road sections.  UpdateLapData reads them per car, and with the AI gone
	the opponent's is the remote player's.												*/
extern long player_current_piece;		// Car_Behaviour.cpp
extern long opponents_current_piece;	// Opponent_Behaviour.cpp

/*	Each car's across-road position as of its own step, for the trace below.  Reading it
	after both steps would report the local car twice - it is per-car context.		*/
extern long players_road_x_position;	// Car_Behaviour.cpp
static long net_trace_road_x[NUM_CARS] = { 0, 0 };

static void StepOneCar( long slot, DWORD carInput )
{
	SelectCar(slot);

	/*	Same reason as the single-player path: FloatV2 treats the road section, the
		distance into it and the road-x as inputs, and this is their only writer.	*/
	if (scr::gUseFloatV2Physics)
		CalculatePlayersRoadPosition();

	if (slot == PLAYER)
		{
		CarBehaviour(carInput,
					 &player1_x, &player1_y, &player1_z,
					 &player1_x_angle, &player1_y_angle, &player1_z_angle);
		}
	else
		{
		CarBehaviour(carInput,
					 &remote_x, &remote_y, &remote_z,
					 &remote_x_angle, &remote_y_angle, &remote_z_angle);

		/*	Hand the result to the slot the renderer already draws a second car from.
			The positions are in the same units the player's are (both go through
			WorldF with the same sign), so they copy straight across.  The angles do
			not: OpponentBehaviour produced radians from atan2, while CarBehaviour
			produces Amiga angle units, 65536 to the turn.						*/
		opponent_x = remote_x;
		opponent_y = remote_y;
		opponent_z = remote_z;

		opponent_x_angle = (float)((double)remote_x_angle * 2.0 * D3DX_PI / 65536.0);
		opponent_y_angle = (float)((double)remote_y_angle * 2.0 * D3DX_PI / 65536.0);
		opponent_z_angle = (float)((double)remote_z_angle * 2.0 * D3DX_PI / 65536.0);

		OpponentStateMark();		// the other car has moved - see OPPONENT_STATE

		/*	Lap counting reads this for the OPPONENT car (UpdateLapData), and it is
			the remote player's piece now, not the AI's.						*/
		opponents_current_piece = player_current_piece;
		}

	net_trace_road_x[slot] = players_road_x_position;
}

/*	One lockstep simulation step: both cars, in role order.  Returns the state hash of the
	step, which is what the two peers compare to notice they have diverged.				*/
static uint64_t NetStepBothCars( DWORD hostInput, DWORD joinerInput )
{
	const bool localIsHost = scr::NetGameLocalIsHost();
	const long hostSlot    = localIsHost ? PLAYER : OPPONENT;
	const long joinerSlot  = localIsHost ? OPPONENT : PLAYER;

	StepOneCar(hostSlot, hostInput);
	const uint64_t hostHash = scr::SimTrace_HashState(scr::FloatV2_State());

	StepOneCar(joinerSlot, joinerInput);
	const uint64_t joinerHash = scr::SimTrace_HashState(scr::FloatV2_State());

	/*	Leave the local car selected: everything downstream of the step - rendering,
		sound, the HUD - reads the globals and expects this machine's car.			*/
	SelectCar(PLAYER);

	/*	Both cars in the digest, in role order, so a divergence in either one is caught
		and both peers compute the same number from the same two cars.  Hashing only
		"my car" would compare two different cars and report a desync every step.	*/
	uint64_t h = hostHash;
	h ^= joinerHash + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);

	/*	SCR_NET_TRACE=1 prints both cars once a second - the quickest way to tell a car
		that is not being drawn from one that is being drawn in the wrong place.		*/
	if (getenv("SCR_NET_TRACE"))
		{
		const uint32_t s = scr::NetGameStep();
		if ((s % 60) == 0)
			{
			printf("net step %5u  local xyz %8ld %8ld %8ld  piece %ld roadx %ld"
				   "   remote xyz %8ld %8ld %8ld  piece %ld roadx %ld  oppID %ld\n",
				   (unsigned)s, player1_x, player1_y, player1_z, player_current_piece,
				   net_trace_road_x[PLAYER],
				   opponent_x, opponent_y, opponent_z, opponents_current_piece,
				   net_trace_road_x[OPPONENT], opponentsID);
			fflush(stdout);
			}
		}

	return h;
}

/*	Hang one of the two cars on the crane at the start of a head-to-head race.  `swingFromLeft`
	is picked from the network role by the caller, never from which car is local, so both
	peers hang both cars the same way - and the two take opposite sides, which is what keeps
	the cars off one another.  This is the Amiga's own link-up arrangement: the SLAVE sets
	swing.from.left before set.players.restart.position and the MASTER leaves it clear
	(StuntCarRacer.s:10268).  See SetCarSwingFromLeft.									*/
static void PlaceNetCarOnChains( long slot, long swingFromLeft )
{
	SelectCar(slot);
	SetCarSwingFromLeft(swingFromLeft);

	if (slot == PLAYER)
		{
		PlaceCarOnChainsForNewGame(&player1_x, &player1_y, &player1_z,
								   &player1_x_angle, &player1_y_angle, &player1_z_angle);
		// The car has been put somewhere new outright - there is no motion between the
		// old state and this one for the renderer to blend.
		gPlayerStatesValid = false;
		}
	else
		{
		PlaceCarOnChainsForNewGame(&remote_x, &remote_y, &remote_z,
								   &remote_x_angle, &remote_y_angle, &remote_z_angle);

		/*	So the remote car is drawn in the right place on the very first frame,
			before any step has run.  Same conversion as StepOneCar.				*/
		opponent_x = remote_x;
		opponent_y = remote_y;
		opponent_z = remote_z;
		opponent_x_angle = (float)((double)remote_x_angle * 2.0 * D3DX_PI / 65536.0);
		opponent_y_angle = (float)((double)remote_y_angle * 2.0 * D3DX_PI / 65536.0);
		opponent_z_angle = (float)((double)remote_z_angle * 2.0 * D3DX_PI / 65536.0);
		opponents_current_piece = player_current_piece;

		// Placed, not driven - nothing to blend from the old race's last position.
		OpponentStateReset();
		}
}

static void SetOpponentsCarWorldTransform( void )
{
D3DXMATRIX matRot, matTemp, matTrans;

	/*	Drawn at a blend of the last two physics states, not at the newest one - see
		OPPONENT_STATE.  Only the transform is built from the blend; the globals the
		simulation reads are left alone.										*/
	OPPONENT_STATE opp;
	OpponentStateForRender(&opp);

	D3DXMatrixIdentity(&matRot);
	// Produce and combine the rotation matrices
	D3DXMatrixRotationZ(&matTemp, opp.z_angle);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	D3DXMatrixRotationX(&matTemp, opp.x_angle);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	D3DXMatrixRotationY(&matTemp, opp.y_angle);
	D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
	// Produce the translation matrix
	// Position car at wheel height (VCAR_HEIGHT/4)
	D3DXMatrixTranslation( &matTrans, WorldF(opp.x), WorldF(-opp.y)+VCAR_HEIGHT/4, WorldF(opp.z) );
	// Combine the rotation and translation matrices to complete the world matrix
	D3DXMatrixMultiply(&matWorldOpponentsCar, &matRot, &matTrans);
}


/*	The Amiga track preview draws the road only.  The opponent's car is drawn unconditionally
	by the render pass, so put it somewhere the camera can never see rather than adding a
	branch to the renderer.																*/
static void HideOpponentsCar( void )
{
	D3DXMatrixTranslation( &matWorldOpponentsCar,
						   0.0f,
						   static_cast<float>(1000 * NUM_TRACK_CUBES * (CUBE_SIZE>>LOG_PRECISION)),
						   0.0f );
}


static void StopEngineSound( void )
{
	if (engineSoundPlaying)
	{
		for (int i = 0; i < 8; i++)
			EngineSoundBuffers[i]->Stop();

		engineSoundPlaying = FALSE;
	}
}


/*	Menu key codes.  Defined up here rather than next to HandleTrackMenu because
	OnFrameMove needs STARTMENU too: --simtrace presses it to start the race.	*/
#ifdef SCR_PORTABLE
#define FIRSTMENU SDLK_1
#define STARTMENU SDLK_s
#define LEAGUEMENU SDLK_l
#define PREVIEWVIEW SDLK_SPACE
#define PREVIEWENTER SDLK_RETURN
#define PREVIEWENTER2 SDLK_KP_ENTER
#define PREVIEWLEFT SDLK_LEFT
#define PREVIEWRIGHT SDLK_RIGHT
#else
#define FIRSTMENU '1'
#define STARTMENU 'S'
#define LEAGUEMENU 'L'
#define PREVIEWVIEW ' '
#define PREVIEWENTER VK_RETURN
#define PREVIEWENTER2 VK_RETURN
#define PREVIEWLEFT VK_LEFT
#define PREVIEWRIGHT VK_RIGHT
#endif

void CALLBACK OnFrameMove( IDirect3DDevice9 *pd3dDevice, double fTime, float fElapsedTime, void *pUserContext )
{
static D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
static long frameCount = 0;
// Set by OnFrameMove's accumulators, consumed by the physics/draw body below.
static long PlayerPhysicsSteps = 0;
static bool bOpponentStepDue = false;
static bool bDrawBridgeStepDue = true;	// track menu / preview keep the old per-frame rate
DWORD input = lastInput;	// take copy of user input
D3DXMATRIX matRot, matTemp, matTrans, matView;

#ifndef SCR_PORTABLE
// crude 60fps cap method...
static float lastFrame = 0.0f;
#define FPSMAX (1.0f/60.f)
	lastFrame += fElapsedTime;
	if (lastFrame < FPSMAX)
		return;
	lastFrame -= FPSMAX;
#endif
	bFrameMoved = FALSE;
//	VALUE3 = frameGap;

	/*	Pump the network before anything is simulated, so this frame's steps see the
		peer's inputs as soon as they have arrived rather than a frame later.  This is
		also what drives a session that is still listening or connecting, and what
		starts the race when the handshake lands - so it runs whether the menus are up
		or a race is in progress.												*/
	MenuScreensTick( DXUTGetTime() );

	/*	Determinism trace (--simtrace).  Drive the menus straight into a race, then take
		the controls off the keyboard: two hand-driven runs can never be diffed against
		each other.  See Sim_Trace.h.												*/
	if (scr::gSimTraceEnabled)
	{
		if (GameMode == TRACK_MENU)
		{
			// A named driver, not RANDOM_OPPONENT: which car we race against has to be
			// the same on both platforms, and it must be settled before the RNG is
			// seeded rather than by drawing from it.
			SetRaceOpponent(scr::gSimTraceSolo ? NO_OPPONENT : 0);
			// Deactivate as well as start: while the Amiga menus are up OnFrameRender
			// returns before the track-preview input is reached, so the race would
			// never be entered. Same pair as the screenshot path below.
			if (MenuStartTrack(scr::gSimTraceTrack))
				MenuScreensDeactivate();
		}
		else if (GameMode == TRACK_PREVIEW)
		{
			keyPress = STARTMENU;		// "hit fire to continue" starts the race
		}
		else if (GameMode == GAME_IN_PROGRESS)
		{
			// Idempotent - opens the log and seeds the sim RNG on the first race frame.
			scr::SimTrace_Begin();
			input = scr::SimTrace_ScriptedInput();
		}
	}

	if (GameMode == GAME_OVER)
	{
		StopEngineSound();
		return;
	}

	if (bPaused)
	{
		StopEngineSound();
	}

	if (TrackID == NO_TRACK)
		return;

	// Track preview and game mode run at reduced frame rate.
	// Original Amiga: vsync-locked at 50Hz PAL, physics every frameGap frames
	// (default 6 -> ~8.3Hz). Here we decouple from render rate using a wall-clock
	// accumulator so behaviour matches the Amiga on any display refresh.
	if ((GameMode == TRACK_PREVIEW) || (GameMode == GAME_IN_PROGRESS))
	{
		// Two independent clocks:
		//
		//  - The 50Hz clock drives FramesWheelsEngine (engine sound and wheel
		//    animation, which the Amiga ran at the PAL vsync rate) and counts
		//    down frameGap for the *legacy* physics and the opponent.
		//
		//  - The player clock drives CarBehaviour. On the legacy path that is
		//    still 50Hz/frameGap (~8.3Hz), which is why the game looks jerky:
		//    the world only changes 8 times a second. With the FloatV2 port
		//    enabled it runs at 1/gFloatV2Dt instead, because that physics is
		//    timestep-parameterised and can be stepped as often as we like.
		//
		// The opponent stays on the legacy clock either way — its AI has no
		// timestep, so stepping it faster would simply make it drive faster.
		static double engineAccum = 0.0;
		static double playerAccum = 0.0;
		static double lastPhysicsT = DXUTGetTime();
		const double STEP_50HZ = 1.0 / 50.0;

		const bool   bFloatV2   = scr::gUseFloatV2Physics;
		const double playerStep = bFloatV2 ? scr::gFloatV2Dt : STEP_50HZ;

		double nowT = DXUTGetTime();
		double elapsed = nowT - lastPhysicsT;
		lastPhysicsT = nowT;
		// Clamp to avoid spiral-of-death after pauses / stalls
		if (elapsed > 0.25) elapsed = 0.25;

		// Determinism trace: pin the clock. Otherwise the number of physics steps
		// this frame depends on how long the last frame took to draw, which is the
		// one input guaranteed to differ between a Mac and a PC. One player step per
		// render frame, and the 50Hz engine/opponent clock still derived from it, so
		// the two stay in exactly the relationship they have at run time.
		if (scr::gSimTraceEnabled) elapsed = playerStep;
		engineAccum += elapsed;
		playerAccum += elapsed;

		bool ranLegacyStep = false;
		while (engineAccum >= STEP_50HZ)
		{
			engineAccum -= STEP_50HZ;

			if (GameMode == GAME_IN_PROGRESS)
			{
				// Should run at 50Hz
				if (!bPaused) FramesWheelsEngine(EngineSoundBuffers);
			}

			if (frameCount > 0)
				--frameCount;
			if (frameCount == 0)
			{
				frameCount = frameGap;
				ranLegacyStep = true;
			}
		}

		// How many player physics steps are due this render frame.
		PlayerPhysicsSteps = 0;
		/*	Take the step slightly early.  The physics rate and the display rate are both
			60Hz, so the accumulator sits exactly on its threshold and a frame that runs
			a fraction of a millisecond long takes two steps while the next takes none.
			The world is then advancing 0, 2, 1, 1, 0, 2... and a rotating view shears
			by a double-sized angle every few frames.  The margin absorbs that: accum
			goes marginally negative instead, and the next frame's elapsed pays it back,
			so no time is created or lost over any span longer than a frame.

			2% is well inside the slop we are trying to swallow and well below the point
			where a genuinely different frame rate would be mistaken for this one.  The
			trace clock feeds in exactly playerStep, so it still takes exactly one step
			per frame and the determinism run is unaffected.									*/
		const double stepMargin = playerStep * 0.02;
		while (playerAccum >= playerStep - stepMargin)
		{
			playerAccum -= playerStep;
			++PlayerPhysicsSteps;
			if (PlayerPhysicsSteps >= 8) { playerAccum = 0.0; break; }	// sanity cap
		}

		if (!bFloatV2)
		{
			// Legacy: the player moves on the frameGap clock, as before.
			PlayerPhysicsSteps = ranLegacyStep ? 1 : 0;
		}

		/*	Whatever the accumulator still holds is how far past the last physics state
			the display clock has reached - which is exactly the blend factor for the
			render.  The step margin above can leave it marginally negative; clamp.	*/
		gInterpAlpha = playerAccum / playerStep;
		if (gInterpAlpha < 0.0) gInterpAlpha = 0.0;
		if (gInterpAlpha > 1.0) gInterpAlpha = 1.0;

		bOpponentStepDue = ranLegacyStep;

		// Latch rather than assign: the 60fps cap above can skip whole FrameMove calls,
		// and a tick that has not been drawn yet must not be lost.
		if (ranLegacyStep) bWorldStepDue = TRUE;

		// The drawbridge advances one animation frame per *world* step, not per
		// render frame: on the Amiga move.draw.bridge is called once per race.loop
		// iteration, right alongside car.movement (StuntCarRacer.s:10310), and the
		// pre-FloatV2 code got the same effect by returning early on non-frameGap
		// frames. Now that we fall through every render frame the bridge would
		// animate ~7x too fast, so gate it on the legacy clock explicitly.
		bDrawBridgeStepDue = ranLegacyStep;

		/*	Nothing to do at all this render frame?  With interpolation there still is:
			no new physics state, but a new point in time to draw the old ones at, which
			is the whole reason the display can run above the physics rate.  The step
			loops below are all count-gated, so falling through costs nothing.	*/
		if ((PlayerPhysicsSteps == 0) && !ranLegacyStep)
		{
			if (!(gRenderInterpolation && (GameMode == GAME_IN_PROGRESS) && gPlayerStatesValid))
				return;
		}
	}
	else if (GameMode == TRACK_MENU)
	{
		// Stop engine sound if at track menu or if game has finished
		StopEngineSound();
		bDrawBridgeStepDue = true;
	}

	if ((GameMode == GAME_IN_PROGRESS) && (keyPress == 'R'))
	{
		// point car in opposite direction
		player1_y_angle += _180_DEGREES;
		player1_y_angle &= (MAX_ANGLE-1);
		INITIALISE_PLAYER = TRUE;
		keyPress = '\0';
	}

	if (!bPaused && bDrawBridgeStepDue)
		MoveDrawBridge();

	// Car behaviour
	if ((GameMode == TRACK_PREVIEW) || (GameMode == GAME_IN_PROGRESS))
	{
		if (!bPaused)
		{
			if ((GameMode == GAME_IN_PROGRESS) && scr::NetGameRacing())
			{
				// Head-to-head. Both cars are players, and the step is gated on
				// the network rather than on the wall clock alone: a step whose
				// inputs are not both in hand cannot be simulated at all.
				for (long step = 0; step < PlayerPhysicsSteps; ++step)
				{
					const uint32_t s = scr::NetGameStep();

					// Sampled now, consumed at s + kInputDelay. That is what the
					// input delay IS, not an off-by-one.
					scr::NetSubmitInput(s, (uint32_t)input);

					if (!scr::NetStepReady(s))
						break;		// the peer's input has not arrived - do NOT simulate

					// Snapshot before the step, so the pair the renderer blends is
					// always the last two states - see PLAYER_STATE.
					PlayerStateCapture(&gPlayerPrev);

					const uint32_t localIn  = scr::NetLocalInput(s);
					const uint32_t remoteIn = scr::NetRemoteInput(s);
					const bool     isHost   = scr::NetGameLocalIsHost();

					const uint64_t hash = NetStepBothCars(
						(DWORD)(isHost ? localIn  : remoteIn),
						(DWORD)(isHost ? remoteIn : localIn));

					scr::NetReportDigest(s, hash);
					scr::NetGameAdvanceStep();

					PlayerStateCapture(&gPlayerCurr);
					gPlayerStatesValid = true;
				}
			}
			else if ((GameMode == GAME_IN_PROGRESS) && (!bPlayerPaused))
			{
				// May be more than one step per render frame if the FloatV2
				// rate is above the render rate, or if a frame ran long.
				for (long step = 0; step < PlayerPhysicsSteps; ++step)
				{
				// FloatV2 treats road section / distance-into-section / road-x
				// as *inputs*, but their only writer, CalculatePlayersRoadPosition(),
				// sits inside OpponentBehaviour() on the 8.3Hz frameGap clock.
				// At 60Hz that leaves the road-height lookup frozen for ~8 steps
				// and then snapping, and ProcessWheel's (1.078125 / dtRatio)
				// predictive term turns each snap into a fake impact that rolls
				// the car over. Refresh per player step. Gated so the legacy
				// path keeps its original call pattern exactly.
				if (scr::gUseFloatV2Physics)
					CalculatePlayersRoadPosition();

				// Snapshot before the step, so the pair the renderer blends is always
				// the last two states - see PLAYER_STATE.
				PlayerStateCapture(&gPlayerPrev);

				CarBehaviour(input,
							 &player1_x,
							 &player1_y,
							 &player1_z,
							 &player1_x_angle,
							 &player1_y_angle,
							 &player1_z_angle);

				PlayerStateCapture(&gPlayerCurr);
				gPlayerStatesValid = true;
				}
			}

			// The FloatV2 opponent is timestep-parameterised too, so it rides
			// the player's clock. Its once-per-Amiga-frame decisions (steering
			// randomisation, player interaction, car-to-car collision) are
			// gated internally by _framePhase, so stepping it faster makes it
			// smoother without making it drive faster or react sooner.
			// The legacy opponent has no timestep and stays on the 8.3Hz clock.
			/*	A practise run is solo: draw.world's no.opponent4/no.opponent5 branches
				skip opponent.movement and everything hanging off it (~line 20280).	*/
			if (scr::NetGameRacing())
			{
				// The other car is the remote player, already stepped above.
				// The AI must not run at all: it would fight the remote player
				// for the same slot, and it is not part of the shared simulation.
			}
			else if (opponentsID == NO_OPPONENT)
			{
				// nothing to step
			}
			else if (scr::gUseFloatV2Physics && scr::gUseFloatV2Opponent)
			{
				for (long step = 0; step < PlayerPhysicsSteps; ++step)
				{
					OpponentBehaviour(&opponent_x,
								  &opponent_y,
								  &opponent_z,
								  &opponent_x_angle,
								  &opponent_y_angle,
								  &opponent_z_angle,
								  bOpponentPaused);
					OpponentStateMark();		// see OPPONENT_STATE
				}
			}
			else if (bOpponentStepDue)
			{
				OpponentBehaviour(&opponent_x,
							  &opponent_y,
							  &opponent_z,
							  &opponent_x_angle,
							  &opponent_y_angle,
							  &opponent_z_angle,
							  bOpponentPaused);
				/*	The legacy AI moves 8.3 times a second.  Without the blend the car
					holds still for seven frames out of eight and then jumps.	*/
				OpponentStateMark();
			}

			/*	All of this frame's opponent motion is in - take the render sample.
				Does nothing on a frame where the car did not move.				*/
			OpponentStateAdvance();
		}

		// LimitViewpointY(&player1_y);
		// Disabled 2026-08-02. This was a PC-port invention: it clamped the car
		// against the road to stop the camera tearing through it. The Amiga did
		// no such clamp -- it raised the eye point with suspension compression
		// instead, which CalcAmigaYPerspectiveShift() now implements. Leaving
		// both active would double-correct, and this one also displaces the
		// drawn car (it edits player1_y, which SetCarWorldTransform reads).
	}

	if ((GameMode == TRACK_MENU) || (GameMode == TRACK_PREVIEW))
	{
		if (GameMode == TRACK_MENU)
			CalcTrackMenuViewpoint();
		else
		{
			CalcTrackPreviewViewpoint();

			// Set the car's world transform matrix.  The Amiga preview shows no cars at
			// all - it draws the road and nothing else - so park the opponent out of
			// sight rather than driving it round the track.
			if (bAmigaTrackPreview)
				HideOpponentsCar();
			else
				SetOpponentsCarWorldTransform();
		}

		// Set Direct3D transforms, ready for OnFrameRender
		viewpoint1_x >>= LOG_PRECISION;
		// NOTE: viewpoint1_y must be preserved for use by DrawBackdrop
		viewpoint1_z >>= LOG_PRECISION;

		target_x >>= LOG_PRECISION;
		target_y = -target_y;
		target_y >>= LOG_PRECISION;
		target_z >>= LOG_PRECISION;

		// Set the track's world transform matrix
		D3DXMatrixIdentity( &matWorldTrack );

		//
		// Set the view transform matrix
		//
		// Set the eye point
		D3DXVECTOR3 vEyePt( static_cast<float>(viewpoint1_x), static_cast<float>(-viewpoint1_y>>LOG_PRECISION), static_cast<float>(viewpoint1_z) );
		// Set the lookat point
		D3DXVECTOR3 vLookatPt( static_cast<float>(target_x), static_cast<float>(target_y), static_cast<float>(target_z) );
		D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
		pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
	}
	else if (GameMode == GAME_IN_PROGRESS)
	{
		/*	Draw at a blend of the last two physics states rather than at the newest one.
			Everything below - the camera basis, the eye point, the car's own transform -
			is derived from these globals, so writing the blended state here interpolates
			all of it at once.  The real state goes back at the end of the branch, before
			anything else can see it: the simulation must never read a blended value.	*/
		PLAYER_STATE trueState;
		const bool bInterp = gRenderInterpolation && gPlayerStatesValid && !bPaused
							 && !scr::gSimTraceEnabled;
		if (bInterp)
		{
			PLAYER_STATE blended;
			PlayerStateCapture(&trueState);
			PlayerStateLerp(&blended, &gPlayerPrev, &gPlayerCurr, gInterpAlpha);
			PlayerStateApply(&blended);
		}

		CalcGameViewpoint();

		// The eye point keeps its fraction here -- see WorldF(). The track preview
		// branch above still shifts in place because SetPreviewWindowProjection()
		// rebuilds its camera basis from the shifted globals afterwards.
		// NOTE: viewpoint1_y must be preserved unshifted for use by DrawBackdrop

		// Set the track's world transform matrix
		D3DXMatrixIdentity( &matWorldTrack );

		// Set the opponent's car world transform matrix
		/*
		// temp set opponent's position to same as player
		if ((opponent_x == 0) && (opponent_y == 0) && (opponent_z == 0))
		{
			opponent_x = player1_x;
			opponent_y = player1_y + (0xc00 * 256 * 4);	// Subtract amount above road, added by PositionCarAbovePiece()
			opponent_z = player1_z;
			opponent_x_angle = player1_x_angle;
			opponent_y_angle = player1_y_angle;
			opponent_z_angle = player1_z_angle;
		}
		*/
		/*	Practise has no opponent car to place - park it where the camera can never
			see it, the same trick the track preview uses.							*/
		if (opponentsID == NO_OPPONENT)
			HideOpponentsCar();
		else
			SetOpponentsCarWorldTransform();

		if (bOutsideView)
		{
			// Set the car's world transform matrix
			SetCarWorldTransform();
		}

		//
		// Set the view transform matrix
		//
		// Produce the translation matrix
		D3DXMatrixTranslation( &matTrans, WorldF(-viewpoint1_x), WorldF(viewpoint1_y), WorldF(-viewpoint1_z) );
		D3DXMatrixIdentity(&matRot);
		float xa = ((static_cast<float>(-viewpoint1_x_angle) * 2 * D3DX_PI) / 65536.0f);
		float ya = ((static_cast<float>(-viewpoint1_y_angle) * 2 * D3DX_PI) / 65536.0f);
		float za = ((static_cast<float>(-viewpoint1_z_angle) * 2 * D3DX_PI) / 65536.0f);
		// Produce and combine the rotation matrices
#ifdef SCR_PORTABLE
		D3DXMatrixRotationY(&matTemp, ya + D3DX_PI);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
		D3DXMatrixRotationX(&matTemp, -xa);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
		D3DXMatrixRotationZ(&matTemp, -za);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
#else
		D3DXMatrixRotationY(&matTemp, ya);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
		D3DXMatrixRotationX(&matTemp, xa);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
		D3DXMatrixRotationZ(&matTemp, za);
		D3DXMatrixMultiply(&matRot, &matRot, &matTemp);
#endif
		// Combine the rotation and translation matrices to complete the world matrix
		D3DXMatrixMultiply(&matView, &matTrans, &matRot);
#ifdef SCR_PORTABLE
		D3DXMatrixScaling(&matTrans, +1, -1, +1);
		D3DXMatrixMultiply(&matView, &matView, &matTrans);
#endif
		pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

		// PLAYER_STATE_RESTORE.  The transforms are built; put the simulation's own
		// state back before anything else reads it.
		if (bInterp)
			PlayerStateApply(&trueState);
	}

	if (!bPaused)
		bFrameMoved = TRUE;
}


/*	======================================================================================= */
/*	Function:		HandleTrackMenu															*/
/*																							*/
/*	Description:	Output track menu text													*/
/*	======================================================================================= */

/*	======================================================================================= */
/*	Function:		MenuStartTrack															*/
/*																							*/
/*	Description:	Called by the Amiga menus when a race is chosen: convert the track,		*/
/*					build its vertex buffer and drop into the track preview, exactly as		*/
/*					the old text track menu did.  Also picks up the Super League setting		*/
/*					from the career, since that changes the car and the track colours.		*/
/*	======================================================================================= */

bool MenuStartTrack( int trackID )
	{
	/*	Not gLeagueSuperLeague directly: the Single Race screen runs a race in whichever	*/
	/*	league it was asked for, and everything downstream - engine, boost, colours and	*/
	/*	the opponent speed table - keys off this one flag.								*/
	if (bSuperLeague != MenuScreensRaceSuperLeague())
		{
		bSuperLeague = MenuScreensRaceSuperLeague();
		CreateCarVertexBuffer(DXUTGetD3DDevice());		// recreate car in league colours
		}

	if (! ConvertAmigaTrack(trackID))
		{
		OutputDebugStringW(L"ERROR: Failed to convert track\n");
		return false;
		}

	if (CreateTrackVertexBuffer(DXUTGetD3DDevice()) != S_OK)
		{
		OutputDebugStringW(L"ERROR: Failed to create track vertex buffer\n");
		return false;
		}

	/*	Damage is cumulative across a league season and nowhere else: a season race is	*/
	/*	started in the car the last one left behind, everything else in a fresh one.		*/
	/*	Set before ResetPlayer, which is what puts the carried holes into the bar.		*/
	carried_nholes = MenuScreensRaceIsLeague() ? gLeagueDamageHoles : 0;

	bNewGame = TRUE;		// resets the opponent's car, shown during the preview
	ResetPlayer();
	GameMode = TRACK_PREVIEW;
	bPlayerPaused = bOpponentPaused = FALSE;
	bQuitConfirm = FALSE;
	keyPress = '\0';
	return true;
	}

/*	======================================================================================= */
/*	Function:		ReturnToMenus															*/
/*																							*/
/*	Description:	Leave the race (or its preview) without scoring it and put the Amiga		*/
/*					menus back up.  Shared by the 'M' key and the Escape prompt so the		*/
/*					two cannot drift apart.													*/
/*	======================================================================================= */

static void ReturnToMenus( void )
	{
	if (GameMode == TRACK_MENU)
		return;

	GameMode    = TRACK_MENU;
	opponentsID = NO_OPPONENT;

	// reset all animated objects
	ResetDrawBridge();

	bQuitConfirm  = FALSE;
	bPaused       = FALSE;
	bPlayerPaused = bOpponentPaused = FALSE;

	MenuScreensAbandonRace();
	}

static void HandleTrackMenu( CDXUTTextHelper &txtHelper )
	{
	long i, track_number;
	UINT firstMenuOption, lastMenuOption;
	float textScale = GetTextScale();
	txtHelper.SetInsertionPos( static_cast<int>((2+(wideScreen?10:0)) * textScale), static_cast<int>(15*8*textScale) );
	txtHelper.DrawTextLine( L"Choose track :-" );

	for (i = 0, firstMenuOption = FIRSTMENU; i < NUM_TRACKS; i++)
		{
		txtHelper.DrawFormattedTextLine( L"'%d' -  " STRING, (i+1), GetTrackName(i) );
		}
	lastMenuOption = i + FIRSTMENU - 1;

	// output instructions
	const D3DSURFACE_DESC *pd3dsdBackBuffer = DXUTGetBackBufferSurfaceDesc();
	txtHelper.SetInsertionPos( static_cast<int>((2+(wideScreen?10:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-15*8*textScale) );
	txtHelper.DrawFormattedTextLine( L"Current track - " STRING L".  Press 'S' to select, Escape for the menu", (TrackID == NO_TRACK ? L"None" : GetTrackName(TrackID)));
	txtHelper.DrawTextLine( L"'L' to switch Super League On/Off");

	if (((keyPress >= firstMenuOption) && (keyPress <= lastMenuOption)) || (keyPress == LEAGUEMENU))
		{
		if(keyPress == LEAGUEMENU) {
			bSuperLeague = !bSuperLeague;
			track_number = TrackID;
			CreateCarVertexBuffer(DXUTGetD3DDevice());	// recreate car
		} else 
			track_number = keyPress - firstMenuOption;	// start at 0

		if (! ConvertAmigaTrack(track_number))
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "Failed to convert track %d\n", track_number);
#endif
			MessageBox(NULL, L"Failed to convert track", L"Error", MB_OK);	//temp
			return;
			}

		if (CreateTrackVertexBuffer(DXUTGetD3DDevice()) != S_OK)
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "Failed to create track vertex buffer %d\n", track_number);
#endif
			MessageBox(NULL, L"Failed to create track vertex buffer", L"Error", MB_OK);	//temp
			return;
			}

		keyPress = '\0';
		}

	if ((keyPress == STARTMENU) && (TrackID != NO_TRACK))
		{
		SetRaceOpponent(RANDOM_OPPONENT);	// no fixture behind this menu to name one
		bNewGame = TRUE;	// Used here just to reset the opponent's car, which is then shown during the track preview
		ResetPlayer();		// Also reset player to clear values if there was a previous game (CarBehaviour normally does this, but isn't called for track preview)
        GameMode = TRACK_PREVIEW;
		bPlayerPaused = bOpponentPaused = FALSE;
		keyPress = '\0';
		}
	

	return;
	}


/*	======================================================================================= */
/*	Function:		HandleTrackPreview														*/
/*																							*/
/*	Description:	Output track preview text												*/
/*	======================================================================================= */

static void HandleTrackPreviewInput( void );

static void HandleTrackPreview( CDXUTTextHelper &txtHelper )
	{
	// output instructions
	const D3DSURFACE_DESC *pd3dsdBackBuffer = DXUTGetBackBufferSurfaceDesc();
	float textScale = GetTextScale();
	txtHelper.SetInsertionPos( static_cast<int>((2+(wideScreen?10:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-15*9*textScale) );
	txtHelper.DrawFormattedTextLine( L"Selected track - " STRING L".  Press 'S' to start game", (TrackID == NO_TRACK ? L"None" : GetTrackName(TrackID)));
	txtHelper.DrawTextLine( bAmigaTrackPreview
							? L"'M' for track menu, steer to rotate view, Escape for the menu"
							: L"'M' for track menu, Escape for the menu");
	txtHelper.DrawTextLine( L"(Press F4 to change scenery, F9 / F10 to adjust frame rate)" );

	txtHelper.SetInsertionPos( static_cast<int>((2+(wideScreen?10:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-15*6*textScale) );
	txtHelper.DrawTextLine( L"Keyboard controls during game :-" );
	#if defined(PANDORA) || defined(PYRA)
	txtHelper.DrawTextLine( L"  DPad = Steer, (X) = Accelerate, (B) = Brake, (R) = Nitro" );
	#else
	txtHelper.DrawTextLine( L"  Arrow left = Steer left, Arrow right = Steer right, Space = Accelerate, Arrow Down = Brake" );
	#endif
	txtHelper.DrawTextLine( L"  R = Point car in opposite direction, P = Pause, O = Unpause" );
	txtHelper.DrawTextLine( L"  M = Back to track menu, Escape = Back to menu" );

	HandleTrackPreviewInput();

	return;
	}

/*	Preview keys, kept apart from the text above: the Amiga preview screen draws itself in
	OnFrameRender and returns before RenderText is reached, so the input has to be reachable
	from there too.																			*/
static void HandleTrackPreviewInput( void )
	{
	// Amiga: "steer to rotate view or fire to continue" - R.604b4 is re-entered on each
	// press, stepping B.1bb57 through the four fixed viewpoints
	// ("Reference only/StuntCarRacer.s":13603).
	if (bAmigaTrackPreview &&
		((keyPress == PREVIEWLEFT) || (keyPress == PREVIEWRIGHT)))
		{
		gTrackPreviewView = (gTrackPreviewView + ((keyPress == PREVIEWLEFT) ? -1 : 1))
							& (NUM_AMIGA_PREVIEW_VIEWS - 1);
		keyPress = '\0';
		}

	// "Hit fire to continue" - fire (space) starts the race, as on the Amiga.
	// Enter is accepted as well, since it's the natural "continue" key here.
	if (bAmigaTrackPreview && ((keyPress == PREVIEWVIEW)
							   || (keyPress == PREVIEWENTER) || (keyPress == PREVIEWENTER2)))
		keyPress = STARTMENU;

	/*	Netplay: the handshake dropped both machines into the track, so there is nobody
		to "hit fire to continue" - and a player who lingered on the preview would just
		stall the other one at step 0.  Start as soon as the track is up; the lockstep
		step counter, not the wall clock, is what keeps the two races together.		*/
	if ((GameMode == TRACK_PREVIEW) &&
		scr::NetGameSessionActive() && !scr::NetGameRacing() && !scr::NetGameFailed())
		keyPress = STARTMENU;

	if (keyPress == STARTMENU)
		{
		bNewGame = TRUE;
        GameMode = GAME_IN_PROGRESS;
		// initialise game data
		ResetLapData(OPPONENT);
		ResetLapData(PLAYER);
		gameStartTime = DXUTGetTime();
		gameEndTime = 0;
		if(bSuperLeague) {
			boostReserve = SuperBoost;
			road_cushion_value = 1;
			engine_power = 320;
			boost_unit_value = 12;
			opp_engine_power = 314;
		} else {
			boostReserve = StandardBoost;	// SuperBoost for super league
			road_cushion_value = 0;
			engine_power = 240;
			boost_unit_value = 16;
			opp_engine_power = 236;
		}
		boostUnit = 0;
		bPlayerPaused = bOpponentPaused = FALSE;
		keyPress = '\0';

		// New race: the opponent is about to be placed, not driven, so throw away the
		// pair of states the renderer blends between.
		OpponentStateReset();

		// Hang the car on the chains now, not on the first physics step: CarBehaviour
		// only runs when a step is due, so the first render frame (or two) of the race
		// would otherwise still be drawn from the track preview's car position, and the
		// car would appear to be sitting on the ground and then snap up onto the crane.
		if (scr::NetGameSessionActive())
			{
			// Two cars to hang, and both peers must hang them identically. Side is
			// picked from the network role, never from which car is local, so the
			// host's car is on the same side of the road on both machines.
			const bool isHost     = scr::NetGameLocalIsHost();
			const long hostSlot   = isHost ? PLAYER : OPPONENT;
			const long joinerSlot = isHost ? OPPONENT : PLAYER;

			// Zeroes the step counter, seeds the shared RNG and locks the sim
			// settings. FIRST, before either car is placed: the crane's hang time
			// (car_on_chains_countdown, Car_Behaviour.cpp) is drawn from that RNG,
			// so placing a car before the seed is agreed would have the two peers
			// roll different drop times and desync on the very first step.
			scr::NetGameRaceBegun();

			// Host is the MASTER's side, joiner the SLAVE's, as on the Amiga.
			PlaceNetCarOnChains(hostSlot,   FALSE);
			PlaceNetCarOnChains(joinerSlot, TRUE);

			SelectCar(PLAYER);

			// The AI opponent step normally clears this; it does not run here, so
			// clear it by hand or CarBehaviour keeps seeing a new game.
			bNewGame = FALSE;
			}
		else
			{
			PlaceCarOnChainsForNewGame(&player1_x,
									   &player1_y,
									   &player1_z,
									   &player1_x_angle,
									   &player1_y_angle,
									   &player1_z_angle);
			gPlayerStatesValid = false;
			}
		}

	return;
	}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.  Also render text specific to GameMode.
//--------------------------------------------------------------------------------------
extern long new_damage;
extern long opponentsID;
extern WCHAR *opponentNames[];

//--------------------------------------------------------------------------------------
// Draw one "M:SS.hh" stopwatch readout in the dashboard, following print.lap.time in
// "Reference only/StuntCarRacer.s".  Each piece gets its own position because the
// original walks the print column and the sub-character fine.x between them - see the
// HUD_TIME_* constants in Car.h.  bShowHundredths false blanks the last two digits,
// which is what the running clock does.
//--------------------------------------------------------------------------------------
static void DrawLapTime( CDXUTTextHelper &txtHelper, double timeSeconds, float rowY,
						 bool bShowHundredths, float wide, float textScale, float scaleY )
{
	if (timeSeconds < 0.0) timeSeconds = 0.0;

	long hundredths = static_cast<long>(timeSeconds * 100.0 + 0.5);
	long mins = (hundredths / 6000) % 10;
	long secs = (hundredths / 100) % 60;
	long frac = hundredths % 100;

	#define HUD_X(ax)	static_cast<int>((wide + (ax)) * 2.0f * textScale)
	#define HUD_Y(ay)	static_cast<int>((ay) * 2.4f * scaleY)

	txtHelper.SetInsertionPos( HUD_X(HUD_TIME_MINS_X), HUD_Y(rowY) );
	txtHelper.DrawFormattedTextLine( L"%d", mins );

	txtHelper.SetInsertionPos( HUD_X(HUD_TIME_COLON_X), HUD_Y(rowY) );
	txtHelper.DrawTextLine( L":" );

	txtHelper.SetInsertionPos( HUD_X(HUD_TIME_SECS_X), HUD_Y(rowY) );
	txtHelper.DrawFormattedTextLine( L"%02ld", secs );

	txtHelper.SetInsertionPos( HUD_X(HUD_TIME_POINT_X), HUD_Y(rowY + HUD_TIME_POINT_Y_OFFSET) );
	txtHelper.DrawTextLine( L"." );

	if (bShowHundredths)
	{
		txtHelper.SetInsertionPos( HUD_X(HUD_TIME_HUNDREDTHS_X), HUD_Y(rowY) );
		txtHelper.DrawFormattedTextLine( L"%02ld", frac );
	}

	#undef HUD_X
	#undef HUD_Y
}

void RenderText( double fTime )
{
	// SCR_AUTOSTART=1 drives the menus for a non-interactive run: track 1, select, go.
	{
	static long autostart = -1;
	static long frames = 0;
	if (autostart < 0)
		autostart = (getenv("SCR_AUTOSTART") != NULL) ? 1 : 0;
	if (autostart)
		{
		++frames;
		if (frames == 60)  keyPress = FIRSTMENU;
		if (frames == 120) keyPress = STARTMENU;
		if (frames == 180) keyPress = STARTMENU;
		}
	}

    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work fine however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improve perf.
	float textScale = GetTextScale();
#ifdef SCR_PORTABLE
	static
#endif
    CDXUTTextHelper txtHelper( g_pFont, g_pSprite, static_cast<int>(15 * textScale) );

    // Output statistics
    txtHelper.Begin();
	txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
	if (bShowStats)
	{
		txtHelper.SetInsertionPos( static_cast<int>((2+(wideScreen?10:0)) * textScale), 0 );
#ifndef SCR_PORTABLE
		txtHelper.DrawTextLine( DXUTGetFrameStats(true) );
		txtHelper.DrawTextLine( DXUTGetDeviceStats() );
#else
		
		txtHelper.DrawFormattedTextLine( L"fTime: %0.1f  sin(fTime): %0.4f", fTime, sin(fTime) );
#endif

#if defined(DEBUG) || defined(_DEBUG)
		// Output VALUE1, VALUE, VALUE3
		txtHelper.DrawFormattedTextLine( L"V1: %08x, V2: %08x, V3: %08x", VALUE1, VALUE2, VALUE3 );
#else
		// Output version
		txtHelper.DrawTextLine( L"Version 1.0" );
#endif
	}

	switch (GameMode)
		{
		case TRACK_MENU:
			// The Amiga menus draw themselves over the whole display in OnFrameRender;
			// the old text track menu is only reached if they are somehow not up.
			if (!MenuScreensActive())
				HandleTrackMenu(txtHelper);
			txtHelper.End();
			break;

		case TRACK_PREVIEW:
			HandleTrackPreview(txtHelper);
			txtHelper.End();
			break;

		case GAME_IN_PROGRESS:
		case GAME_OVER:
			// Show car speed, damage and race details
			const D3DSURFACE_DESC *pd3dsdBackBuffer = DXUTGetBackBufferSurfaceDesc();
			// The Amiga names your opponent on the RACE n fixture screen and nowhere
			// else - it prints nothing over the cockpit at the start of a race.
			txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ) );

			// The dashboard readouts sit in the four grey boxes of the cockpit bitmap, which
			// is 320x200 art blown up by 2 horizontally and 2.4 vertically.  Each field is
			// placed at the exact spot the Amiga prints it (print.lap.boost.text, boost.print
			// and display.opponents.distance in "Reference only/StuntCarRacer.s"), converted
			// from its column/row + fine.x/fine.y to 320x200 pixels.  They have to be drawn
			// individually - the original nudges each field by a different sub-character
			// offset, so no single padded string lines them all up.
			{
			float scaleY = static_cast<float>(pd3dsdBackBuffer->Height) / static_cast<float>(BASE_HEIGHT);
			float wide = wideScreen ? COCKPIT_WIDESCREEN_OFFSET : 0.0f;
			#define HUD_X(ax)	static_cast<int>((wide + (ax)) * 2.0f * textScale)
			#define HUD_Y(ay)	static_cast<int>((ay) * 2.4f * scaleY)

			txtHelper.SetInsertionPos( HUD_X(HUD_LAP_LABEL_X), HUD_Y(HUD_TOP_Y) );
			txtHelper.DrawTextLine( L"L" );
			if (lapNumber[PLAYER] > 0)
			{
				txtHelper.SetInsertionPos( HUD_X(HUD_LAP_VALUE_X), HUD_Y(HUD_TOP_Y) );
				txtHelper.DrawFormattedTextLine( L"%d", lapNumber[PLAYER] );
			}
			txtHelper.SetInsertionPos( HUD_X(HUD_BOOST_LABEL_X), HUD_Y(HUD_TOP_Y) );
			txtHelper.DrawTextLine( L"B" );
			txtHelper.SetInsertionPos( HUD_X(HUD_BOOST_VALUE_X), HUD_Y(HUD_TOP_Y) );
			txtHelper.DrawFormattedTextLine( L"%02d", boostReserve );

			// Distance carries a leading '-' when the player is behind, a blank when ahead
			long distance = CalculateOpponentsDistance();
			txtHelper.SetInsertionPos( HUD_X(HUD_DIST_X), HUD_Y(HUD_DIST_Y) );
			txtHelper.DrawFormattedTextLine( L"%c%04ld", (distance < 0) ? L'-' : L' ', labs(distance) );

			// The stopwatch, laid out piece by piece as print.lap.time does it.  The top
			// row runs the current lap with the hundredths blanked out (show.lap.time only
			// prints them while B.1bbcc is counting down), and freezes on the lap just
			// completed - hundredths and all - for a moment after crossing the line.
			// The row below holds the best lap, once there is one.
			{
			bool bHolding = (lapTimeHoldRemaining > 0.0);
			double showTime = bHolding ? lastLapTime : currentLapTime;

			DrawLapTime( txtHelper, showTime, HUD_TIME_Y, bHolding, wide, textScale, scaleY );

			if (bBestLapTimeSet)
				DrawLapTime( txtHelper, bestLapTime, HUD_BEST_TIME_Y, true, wide, textScale, scaleY );
			}
			#undef HUD_X
			#undef HUD_Y
			}

			txtHelper.End();

			// The Amiga's WRECKED prompt, up for the sixty frames between the damage bar
			// filling and the race ending, flashing white on the (countdown>>2)&1 beat
			// that dlt6 sets white.prompts from (StuntCarRacer.s:10826).
			if (!raceFinished && (wreck_countdown > 0))
			{
				#ifdef SCR_PORTABLE
				static
				#endif
				CDXUTTextHelper txtHelperWreck( g_pFontLarge, g_pSprite, static_cast<int>(25 * textScale) );

				txtHelperWreck.Begin();
				if ((wreck_countdown >> 2) & 1)
					txtHelperWreck.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
				else
					txtHelperWreck.SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ) );

				txtHelperWreck.SetInsertionPos( static_cast<int>((250+(wideScreen?80:0)) * textScale),
												static_cast<int>(pd3dsdBackBuffer->Height-25*12*textScale) );
				txtHelperWreck.DrawTextLine( L"WRECKED" );
				txtHelperWreck.End();
			}

			if (raceFinished)
			{
				#ifdef SCR_PORTABLE
				static
				#endif
				CDXUTTextHelper txtHelperLarge( g_pFontLarge, g_pSprite, static_cast<int>(25 * textScale) );

				txtHelperLarge.Begin();

				double currentTime = DXUTGetTime(), diffTime;
				if (gameEndTime == 0.0)
					gameEndTime = currentTime;

				// Show race finished text for six seconds, then end the game
				diffTime = currentTime - gameEndTime;
				if (diffTime > 6.0)
				{
					GameMode = GAME_OVER;
				}

				if (GameMode == GAME_OVER)
				{
#ifdef SCR_PORTABLE
					txtHelperLarge.SetInsertionPos( static_cast<int>((250+(wideScreen?80:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-25*13*textScale) );
					txtHelperLarge.DrawTextLine( L"GAME OVER" );
					txtHelperLarge.SetInsertionPos( static_cast<int>((132+(wideScreen?80:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-25*11*textScale) );
					txtHelperLarge.DrawTextLine( L"Press 'M' for track menu" );
#else
					txtHelperLarge.SetInsertionPos( static_cast<int>((124+(wideScreen?80:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-25*12*textScale) );
					txtHelperLarge.DrawTextLine( L"GAME OVER: Press 'M' for track menu" );
#endif
				}
				else
				{
					long intTime = static_cast<long>(diffTime);
					// Text flashes white/black, changing every half second
					if ((diffTime - (double)intTime) < 0.5)
						txtHelperLarge.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
					else
						txtHelperLarge.SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ) );

					txtHelperLarge.SetInsertionPos( static_cast<int>((250+(wideScreen?80:0)) * textScale), static_cast<int>(pd3dsdBackBuffer->Height-25*12*textScale) );

					if (raceWon)
						txtHelperLarge.DrawTextLine( L"RACE WON" );
					else
						txtHelperLarge.DrawTextLine( L"RACE LOST" );
				}

				txtHelperLarge.End();
			}

			if (bQuitConfirm)
			{
				// Keep the prompt small and a quarter of the way down the screen so it
				// lands in the sky above the horizon rather than over the cockpit.
				const int confirmSize = static_cast<int>(15 * textScale);
				#ifdef SCR_PORTABLE
				static
				#endif
				CDXUTTextHelper txtHelperConfirm( g_pFont, g_pSprite, confirmSize );

				// The 7-bit font advances a fixed 7 pixels per character at its integer
				// scale (see CDXUTTextHelper in dx_linux.cpp), so the width is exact.
				int glyphScale = (confirmSize + 4) / 8;
				if (glyphScale < 1) glyphScale = 1;
				const int advance = 7 * glyphScale;
				#define CONFIRM_X(text)	((static_cast<int>(pd3dsdBackBuffer->Width) \
										  - static_cast<int>(wcslen(text)) * advance) / 2)

				const WCHAR *line1 = L"LEAVE THE RACE?";
				const WCHAR *line2 = L"ESC TO CANCEL, ENTER FOR THE MENU";

				txtHelperConfirm.Begin();
				txtHelperConfirm.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
				const int confirmY = static_cast<int>(pd3dsdBackBuffer->Height) / 4;
				const int lineStep = static_cast<int>(confirmSize * 1.5f);

				txtHelperConfirm.SetInsertionPos( CONFIRM_X(line1), confirmY );
				txtHelperConfirm.DrawTextLine( line1 );
				txtHelperConfirm.SetInsertionPos( CONFIRM_X(line2), confirmY + lineStep );
				txtHelperConfirm.DrawTextLine( line2 );
				txtHelperConfirm.End();
				#undef CONFIRM_X
			}
			break;
		}
//	VALUE2 = raceFinished ? 1 : 0;
//	VALUE3 = (long)gameEndTime;
}


#ifdef NOT_USED
//-----------------------------------------------------------------------------
// Name: SetupLights()
// Desc: Sets up the lights and materials for the scene.
//-----------------------------------------------------------------------------
void SetupLights( IDirect3DDevice9 *pd3dDevice )
{
D3DXVECTOR3 vecDir;
D3DLIGHT9 light;

    // Set up a material. The material here just has the diffuse and ambient
    // colors set to white. Note that only one material can be used at a time.
    D3DMATERIAL9 mtrl;
    ZeroMemory( &mtrl, sizeof(D3DMATERIAL9) );
    mtrl.Diffuse.r = mtrl.Ambient.r = 1.0f;
    mtrl.Diffuse.g = mtrl.Ambient.g = 1.0f;
    mtrl.Diffuse.b = mtrl.Ambient.b = 1.0f;
    mtrl.Diffuse.a = mtrl.Ambient.a = 1.0f;
    pd3dDevice->SetMaterial( &mtrl );

	/*
    // Set up a white spotlight
    ZeroMemory( &light, sizeof(D3DLIGHT9) );
    light.Type       = D3DLIGHT_SPOT;
    light.Diffuse.r  = 1.0f;
    light.Diffuse.g  = 1.0f;
    light.Diffuse.b  = 1.0f;
	// Set position vector
//	light.Position = D3DXVECTOR3(32768.0f, 1000.0f, 32768.0f);
	if (GameMode == TRACK_MENU)
	{
		light.Position.x = 32768.0f;
		light.Position.y = 16384.0f;
		light.Position.z = 32768.0f;
	}
	else
	{
		light.Position.x = (player1_x>>LOG_PRECISION);
		light.Position.y = 16384.0f;
		light.Position.z = (player1_z>>LOG_PRECISION);
	}
	// Set direction vector to simulate sunlight
    vecDir = D3DXVECTOR3(0.0f, -1.0f, 0.0f);
    D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
    light.Range       = 32768;//((float)sqrt(FLT_MAX));
	light.Falloff = 1.0f;
	light.Attenuation0 = 1.0f;
	light.Attenuation1 = 0.0f;
	light.Attenuation2 = 0.0f;
	light.Theta = PI/3;
	light.Phi = PI/2;
	pd3dDevice->SetLight( 0, &light );
    pd3dDevice->LightEnable( 0, TRUE );
	*/

	/**/
    // Set up four white, directional lights
    ZeroMemory( &light, sizeof(D3DLIGHT9) );
    light.Type       = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r  = 0.33f;
    light.Diffuse.g  = 0.33f;
    light.Diffuse.b  = 0.33f;
	// Set direction vector to simulate sunlight
    vecDir = D3DXVECTOR3(0.2f, -0.7f, 0.5f);
    D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
    light.Range       = 10000.0f;
    pd3dDevice->SetLight( 1, &light );
    pd3dDevice->LightEnable( 1, TRUE );
	/**/
    vecDir = D3DXVECTOR3(0.2f, -0.7f, -0.5f);
    D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
    pd3dDevice->SetLight( 2, &light );
    pd3dDevice->LightEnable( 2, TRUE );
	/**/
    vecDir = D3DXVECTOR3(-0.2f, -0.7f, 0.5f);
    D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
    pd3dDevice->SetLight( 3, &light );
    pd3dDevice->LightEnable( 3, TRUE );
	/**/
    vecDir = D3DXVECTOR3(-0.2f, -0.7f, -0.5f);
    D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
    pd3dDevice->SetLight( 4, &light );
    pd3dDevice->LightEnable( 4, TRUE );
	/**/

    // Finally, turn on some ambient light and turn lighting on
    pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00303030 );
	pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
}
#endif


/*	======================================================================================= */
/*	Function:		DrawAmigaPreviewScreen													*/
/*																							*/
/*	Description:	Put up the Amiga's preview picture and the course title under it.		*/
/*					set.and.preview.road decrunched the picture, then called R.61260 to		*/
/*					print the road title and printed 'Broken by QUARTEX...' underneath		*/
/*					("Reference only/StuntCarRacer.s":10137).  We print the title and the	*/
/*					prompt the original showed in its place.								*/
/*	======================================================================================= */

static void DrawAmigaPreviewScreen( IDirect3DDevice9 *pd3dDevice )
	{
	static const AmigaPen INK_YELLOW = { 240, 240, 0 };

	AmigaMenuBlit(PREVIEW_SCREEN_IMAGE, 0, 0);

	/*	The title goes in the bevelled panel.  Centre it there in pixels rather than on the	*/
	/*	character grid - the panel does not sit on an 8-pixel row boundary.					*/
	if (TrackID != NO_TRACK)
		{
		char name[64];
		const WCHAR *wide = GetTrackName(TrackID);
		int n = 0;
		while (wide[n] && (n < (int)sizeof(name) - 1))
			{
			name[n] = (char)wide[n];
			n++;
			}
		name[n] = '\0';

		const int text_w = n * AMIGA_CHAR_WIDTH;

		AmigaMenuSetInk(AMIGA_INK_WHITE);
		AmigaMenuPrintPixel(PREVIEW_PANEL_X + (PREVIEW_PANEL_W - text_w) / 2,
							PREVIEW_PANEL_Y + (PREVIEW_PANEL_H - AMIGA_CHAR_HEIGHT) / 2,
							name);
		}

	/*	Centre the prompt on the screen in pixels - twenty characters at the seven-pixel	*/
	/*	column step does not land on a whole character cell.									*/
	{
	static const char *prompt = "Hit fire to continue";
	const int prompt_w = (int)strlen(prompt) * AMIGA_CHAR_WIDTH;

	AmigaMenuSetInk(INK_YELLOW);
	AmigaMenuPrintPixel((AMIGA_SCREEN_WIDTH - prompt_w) / 2,
						PREVIEW_PROMPT_ROW * AMIGA_CHAR_HEIGHT, prompt);
	}

	AmigaMenuPresent(pd3dDevice);
	}


/*	======================================================================================= */
/*	Function:		SetPreviewWindowProjection / SetPreviewWindowClip						*/
/*																							*/
/*	Description:	Aim the 3D projection at the picture window rather than the whole		*/
/*					screen, and clip to it.												*/
/*																							*/
/*					The game's own field of view cannot be used here.  The preview window is	*/
/*					a 296 x 134 letterbox and the arena floor painted inside it is flatter	*/
/*					still, so a 45 x 22.5-degree frustum shows a band across the middle of	*/
/*					the map and throws the rest off the top and bottom of the window - which	*/
/*					is exactly what the Amiga's own preview projection existed to avoid.  It	*/
/*					halved x, quartered z and scaled y by 19483/32768 (calculate.screen.x /	*/
/*					calculate.screen.y at "Reference only/StuntCarRacer.s":16684): a per-axis	*/
/*					squash that flattened the whole map onto the arena floor.				*/
/*																							*/
/*					We do the same thing the honest way.  The four ground corners of the		*/
/*					16 x 16 map are taken into camera space, and the frustum is built to		*/
/*					land their bounding box exactly on the picture's dirt floor				*/
/*					(PREVIEW_FLOOR_*).  x and y get their own focal lengths, so this is		*/
/*					anamorphic in the same way the original was, and the framing is right by	*/
/*					construction whatever eye distance or height is chosen.  Raised pieces	*/
/*					stand above the fitted box, as they do on the original.					*/
/*																							*/
/*					The frustum still has to reach the edges of the render target, because	*/
/*					that is what the viewport covers; the scissor is what actually confines	*/
/*					the road to the window.												*/
/*	======================================================================================= */

#ifndef SCR_DEG_TO_RAD
#define SCR_DEG_TO_RAD(d)	((d) * 3.14159265358979323846f / 180.0f)
#endif

static void SetPreviewWindowProjection( IDirect3DDevice9 *pd3dDevice,
									    float shift_x = 0.0f, float shift_y = 0.0f )
	{
	long screen_width, screen_height;
	GetScreenDimensions(&screen_width, &screen_height);

	float floor_x, floor_y, floor_w, floor_h;
	AmigaMenuGetScreenRect(PREVIEW_FLOOR_X, PREVIEW_FLOOR_Y,
						   PREVIEW_FLOOR_W, PREVIEW_FLOOR_H,
						   &floor_x, &floor_y, &floor_w, &floor_h);

	/*	The camera basis, rebuilt from exactly the eye, look-at and up vector that the view	*/
	/*	matrix was built from in OnFrameMove, so the two cannot disagree.					*/
	const float eye[3]  = { (float)viewpoint1_x,
							(float)(-viewpoint1_y >> LOG_PRECISION),
							(float)viewpoint1_z };
	const float look[3] = { (float)target_x - eye[0], (float)target_y - eye[1], (float)target_z - eye[2] };

	const float look_len = sqrtf((look[0]*look[0]) + (look[1]*look[1]) + (look[2]*look[2]));
	if (look_len < 1.0f)
		return;
	const float fwd[3] = { look[0]/look_len, look[1]/look_len, look[2]/look_len };

	// right = forward x up, camera up = right x forward (vUpVec is (0,1,0)).
	float right[3] = { -fwd[2], 0.0f, fwd[0] };			// fwd x (0,1,0)
	const float right_len = sqrtf((right[0]*right[0]) + (right[2]*right[2]));
	if (right_len < 0.0001f)
		return;
	right[0] /= right_len; right[2] /= right_len;

	const float up[3] = { (right[1]*fwd[2]) - (right[2]*fwd[1]),
						  (right[2]*fwd[0]) - (right[0]*fwd[2]),
						  (right[0]*fwd[1]) - (right[1]*fwd[0]) };

	/*	The map's ground corners, and the extent of their projection.  Ground level is y=0	*/
	/*	in the space the view matrix works in (see the target_y negation in OnFrameMove).	*/
	const float map = (float)((NUM_TRACK_CUBES * CUBE_SIZE) >> LOG_PRECISION);
	float tan_x_min = 0.0f, tan_x_max = 0.0f, tan_y_min = 0.0f, tan_y_max = 0.0f;
	float max_depth = 0.0f, min_depth = 0.0f;

	for (int corner = 0; corner < 4; corner++)
		{
		const float p[3] = { (corner & 1) ? map : 0.0f, 0.0f, (corner & 2) ? map : 0.0f };
		const float d[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };

		const float depth = (d[0]*fwd[0]) + (d[1]*fwd[1]) + (d[2]*fwd[2]);
		if (depth < 1.0f)
			return;								// the map is not wholly in front of the eye
		if (depth > max_depth)
			max_depth = depth;
		if ((corner == 0) || (depth < min_depth))
			min_depth = depth;

		const float tx = ((d[0]*right[0]) + (d[1]*right[1]) + (d[2]*right[2])) / depth;
		const float ty = ((d[0]*up[0])    + (d[1]*up[1])    + (d[2]*up[2]))    / depth;

		if ((corner == 0) || (tx < tan_x_min)) tan_x_min = tx;
		if ((corner == 0) || (tx > tan_x_max)) tan_x_max = tx;
		if ((corner == 0) || (ty < tan_y_min)) tan_y_min = ty;
		if ((corner == 0) || (ty > tan_y_max)) tan_y_max = ty;
		}

	if ((tan_x_max - tan_x_min < 0.0001f) || (tan_y_max - tan_y_min < 0.0001f))
		return;

	/*	Focal lengths that map that box onto the floor rectangle, and the principal point	*/
	/*	that lands it in the right place.  Screen x grows with tan_x; screen y grows with	*/
	/*	tan_y too, because of the b/t flip these frustums are built with.					*/
	/*	The whole map is tens of squares away, so the near plane can be pushed right out to	*/
	/*	meet it.  It has to be: the game's zn of 0.5 against a far plane out at the far side	*/
	/*	of the map leaves the depth buffer with no resolution at all at this range, and the	*/
	/*	far half of the track quietly fails the depth test against the cleared buffer.		*/
	const float zn = min_depth * 0.25f;
	const float focal_x = floor_w / (tan_x_max - tan_x_min);
	const float focal_y = floor_h / (tan_y_max - tan_y_min);

	/*	The principal point is what puts the fitted box in the right place, so nudging it	*/
	/*	slides the whole projected track by that many screen-space units without touching	*/
	/*	its shape or size.  PreviewFattenRoad uses that to dilate the road (see there).		*/
	const float centre_x = floor_x - (tan_x_min * focal_x) + shift_x;
	const float centre_y = floor_y - (tan_y_min * focal_y) + shift_y;

	// b/t follow D3DXMatrixPerspectiveFovLH's flipped y, as in SetSceneProjection.
	const float l = -(centre_x / focal_x) * zn;
	const float r =  (((float)screen_width  - centre_x) / focal_x) * zn;
	const float b =  (centre_y / focal_y) * zn;
	const float t = -(((float)screen_height - centre_y) / focal_y) * zn;

	/*	The game's FURTHEST_Z is a draw distance for a car on a track, and the far side of	*/
	/*	the map is well beyond it from here - left at that, most of the track is clipped		*/
	/*	away and only the nearest few pieces are drawn.  Take the far plane from the map		*/
	/*	instead, with room for the pieces standing above the ground corners.				*/
	const float zf = max_depth * 1.5f;

	if (getenv("SCR_PREVIEW_DEBUG"))
		printf("preview eye (%.0f,%.0f,%.0f) fwd (%.3f,%.3f,%.3f) right (%.3f,%.3f,%.3f) up (%.3f,%.3f,%.3f) map %.0f\n",
			   eye[0], eye[1], eye[2], fwd[0], fwd[1], fwd[2], right[0], right[1], right[2], up[0], up[1], up[2], map);
	if (getenv("SCR_PREVIEW_DEBUG"))
		printf("preview fit: floor base (%.1f,%.1f %.1fx%.1f) tanx %.3f..%.3f tany %.3f..%.3f "
			   "focal %.1f,%.1f centre %.1f,%.1f zf %.0f\n",
			   floor_x, floor_y, floor_w, floor_h,
			   tan_x_min, tan_x_max, tan_y_min, tan_y_max,
			   focal_x, focal_y, centre_x, centre_y, zf);

	D3DXMATRIX matProj;
	D3DXMatrixPerspectiveOffCenterLH( &matProj, l, r, b, t, zn, zf );
	pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
	}

/*	Confine drawing to a rectangle given in screen space (the space GetScreenDimensions
	reports: base space on the SDL build, back buffer pixels on the D3D one).				*/

static void SetScreenSpaceClip( float win_x, float win_y, float win_w, float win_h )
	{
#ifdef SCR_PORTABLE
	// The frame's viewport is the whole 640x480 (or 800x480) base space, letterboxed into
	// the drawable and vertically squashed to PAL's pixel aspect.  Read it back rather than
	// recomputing it, so this cannot drift out of step with SetupViewport.
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);

	long screen_width, screen_height;
	GetScreenDimensions(&screen_width, &screen_height);

	const float sx = (float)vp[2] / (float)screen_width;
	const float sy = (float)vp[3] / (float)screen_height;

	// glScissor's origin is bottom left, base space's is top left.
	glScissor(vp[0] + (GLint)(win_x * sx),
			  vp[1] + (GLint)(((float)screen_height - win_y - win_h) * sy),
			  (GLsizei)(win_w * sx + 0.5f),
			  (GLsizei)(win_h * sy + 0.5f));
	glEnable(GL_SCISSOR_TEST);
#else
	IDirect3DDevice9 *pd3dDevice = DXUTGetD3DDevice();

	RECT rect;
	rect.left   = (LONG)win_x;
	rect.top    = (LONG)win_y;
	rect.right  = (LONG)(win_x + win_w);
	rect.bottom = (LONG)(win_y + win_h);
	pd3dDevice->SetScissorRect(&rect);
	pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
#endif
	}

static void ClearScreenSpaceClip( void )
	{
#ifdef SCR_PORTABLE
	glDisable(GL_SCISSOR_TEST);
#else
	DXUTGetD3DDevice()->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
#endif
	}

static void SetPreviewWindowClip( bool enable )
	{
	if (!enable)
		{
		ClearScreenSpaceClip();
		return;
		}

	float win_x, win_y, win_w, win_h;
	AmigaMenuGetScreenRect(PREVIEW_WINDOW_X, PREVIEW_WINDOW_Y,
						   PREVIEW_WINDOW_W, PREVIEW_WINDOW_H,
						   &win_x, &win_y, &win_w, &win_h);

	SetScreenSpaceClip(win_x, win_y, win_w, win_h);
	}


/*	======================================================================================= */
/*	Function:		PreviewFattenRoad														*/
/*																							*/
/*	Description:	Draw the preview track thick enough to read as the Amiga's did.			*/
/*																							*/
/*					The whole 16 x 16 map is squeezed into a 296 x 134 window, so a road		*/
/*					piece on the far side is a good deal narrower than one Amiga pixel.  Drawn	*/
/*					once it is a hairline that breaks into dashes wherever a piece happens to	*/
/*					miss a pixel centre - the original was a solid, blocky ribbon all the way	*/
/*					round, because its road had real width in the low-resolution bitmap it was	*/
/*					plotted into.															*/
/*																							*/
/*					Widening the geometry is not an option: Track[].coords is the physics'		*/
/*					track as well, and the preview must not have its own.  So the road is		*/
/*					dilated instead - drawn once per offset in a ring one Amiga pixel out,		*/
/*					which fattens every part of it by a pixel in every direction and closes	*/
/*					the gaps, then once more centred so the true position is what lands on top.*/
/*					The offset goes into the projection's principal point, which slides the	*/
/*					image without changing its shape, so each copy is the same track seen from	*/
/*					the same eye - not a scaled or re-aimed one.								*/
/*																							*/
/*					PREVIEW_ROAD_FATTEN is a radius in Amiga pixels: 0 is the bare hairline,	*/
/*					1 gives the three-pixel-wide minimum the original reads as.				*/
/*	======================================================================================= */

#define PREVIEW_ROAD_FATTEN		1

static void PreviewFattenRoad( IDirect3DDevice9 *pd3dDevice )
	{
	long screen_width, screen_height;
	GetScreenDimensions(&screen_width, &screen_height);

	/*	One Amiga pixel, in the screen space the projection above is built in.				*/
	const float px = (float)screen_width  / (float)AMIGA_SCREEN_WIDTH;
	const float py = (float)screen_height / (float)AMIGA_SCREEN_HEIGHT;

	for (int dy = -PREVIEW_ROAD_FATTEN; dy <= PREVIEW_ROAD_FATTEN; dy++)
		for (int dx = -PREVIEW_ROAD_FATTEN; dx <= PREVIEW_ROAD_FATTEN; dx++)
			{
			if ((dx == 0) && (dy == 0))
				continue;					// centred pass goes last, below
			SetPreviewWindowProjection(pd3dDevice, (float)dx * px, (float)dy * py);
			DrawTrack(pd3dDevice);
			}

	SetPreviewWindowProjection(pd3dDevice);
	DrawTrack(pd3dDevice);
	}


/*	======================================================================================= */
/*	Function:		SetCockpitWindowClip													*/
/*																							*/
/*	Description:	Clip the 3D scene to the windscreen aperture of the cockpit.				*/
/*																							*/
/*					The Amiga drew the world into a 320x200 playfield behind a cockpit that	*/
/*					covered everything outside the screen window, so nothing of the track	*/
/*					was ever visible past the frame.  Widescreen here keeps the same cockpit	*/
/*					art but hands the scene the extra width, so sky and scenery show up		*/
/*					beside the A-pillars and above the side panels.  With this on, the scene	*/
/*					is scissored to the aperture and the surround is left black, as it was	*/
/*					on the Amiga.															*/
/*																							*/
/*					The rectangle is the hole in the cockpit art (COCKPIT_WINDOW_*), not		*/
/*					the Amiga playfield SCR_WINDOW_* describes - the frame's inner bevel		*/
/*					is transparent for another ten pixels each side, and clipping to the		*/
/*					playfield instead leaves black bands inside the frame.  In widescreen	*/
/*					the whole 640-wide panel shifts right by COCKPIT_WIDESCREEN_OFFSET * 2.	*/
/*	======================================================================================= */

static void SetCockpitWindowClip( bool enable )
	{
	if (!enable)
		{
		ClearScreenSpaceClip();
		return;
		}

	long screen_width, screen_height;
	GetScreenDimensions(&screen_width, &screen_height);

	const float base_width = wideScreen ? (float)BASE_WIDTH_WIDESCREEN
									    : (float)BASE_WIDTH_STANDARD;
	const float scaleX = (float)screen_width  / base_width;
	const float scaleY = (float)screen_height / (float)BASE_HEIGHT;

	// The art is authored in 320x200; base space is that doubled across and x2.4 down.
	const float wide = wideScreen ? COCKPIT_WIDESCREEN_OFFSET : 0.0f;

	SetScreenSpaceClip((wide + COCKPIT_WINDOW_X) * 2.0f   * scaleX,
					   COCKPIT_WINDOW_Y         * 2.4f   * scaleY,
					   COCKPIT_WINDOW_WIDTH     * 2.0f   * scaleX,
					   COCKPIT_WINDOW_HEIGHT    * 2.4f   * scaleY);
	}


#ifdef SCR_PORTABLE
/*	Set by the SCR_PREVIEW_SHOT development aid below: the file the next completed preview
	frame should be written to, before the program quits.									*/
static const char *gPreviewShotDue = NULL;

extern SDL_Window *window;

static void WriteFramebufferPPM( const char *path )
	{
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);

	const int x = 0, y = 0;
	int w = 0, h = 0;
	SDL_GL_GetDrawableSize(window, &w, &h);
	if ((w <= 0) || (h <= 0))
		{
		w = vp[2];
		h = vp[3];
		}

	unsigned char *pixels = (unsigned char *)malloc((size_t)w * h * 3);
	if (pixels == NULL)
		return;

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	FILE *f = fopen(path, "wb");
	if (f != NULL)
		{
		fprintf(f, "P6\n%d %d\n255\n", w, h);
		for (int row = h - 1; row >= 0; row--)		// glReadPixels is bottom-up
			fwrite(pixels + (size_t)row * w * 3, 1, (size_t)w * 3, f);
		fclose(f);
		printf("preview shot: wrote %s (%dx%d)\n", path, w, h);
		}
	free(pixels);
	}


/*	======================================================================================= */
/*	Function:		PreviewLowResBegin / PreviewLowResEnd									*/
/*																							*/
/*	Description:	Render the whole preview screen at the Amiga's own 320x200 and blow it	*/
/*					up, so the road is made of the same pixels as the picture behind it.		*/
/*																							*/
/*					Everything else on this screen is 320x200 art scaled to the window, but	*/
/*					the road is real geometry and comes out at the window's resolution: a	*/
/*					hairline-thin, hard-edged HD line laid over a chunky pixel picture.  The	*/
/*					Amiga drew both into the same 320x200 bitplanes, so its road was as		*/
/*					coarse as its scenery - a chain of fat two-pixel blocks, not a wire.		*/
/*																							*/
/*					So the whole screen - picture and road - is rasterised into the bottom	*/
/*					left 320x200 of the back buffer (the ortho projection is untouched, so	*/
/*					base space simply lands on a smaller grid), copied to a texture and		*/
/*					drawn back as one quad.  Going through the device's own textured-quad	*/
/*					path means the upscale gets the sharp-bilinear filter the menus and		*/
/*					cockpit already use, so it matches them rather than out-crisping them.	*/
/*																							*/
/*					D3D build unaffected: this is the SDL/GL path only.						*/
/*	======================================================================================= */

/*	Off gives the sharp, full-resolution road the port drew before.							*/
bool bAmigaPreviewLowRes = true;

static IDirect3DTexture9	*pPreviewLowResTexture	= NULL;
static IDirect3DVertexBuffer9 *pPreviewLowResVB	= NULL;
static GLint				 gPreviewSavedViewport[4];
static bool					 gPreviewLowResActive	= false;
static bool					 gPreviewSavedMultisample = false;

struct PREVIEWVERTEX
	{
	float x, y, z, rhw;
	float u, v;
	};
#define D3DFVF_PREVIEWVERTEX	(D3DFVF_XYZRHW|D3DFVF_TEX1)

static bool PreviewLowResBegin( void )
	{
	gPreviewLowResActive = false;
	if (!bAmigaPreviewLowRes)
		return false;

	glGetIntegerv(GL_VIEWPORT, gPreviewSavedViewport);

	/*	A window small enough that the presented picture is under 320x200 has nothing to	*/
	/*	gain here, and the copy below would read outside the viewport.					*/
	if ((gPreviewSavedViewport[2] < AMIGA_SCREEN_WIDTH) ||
		(gPreviewSavedViewport[3] < AMIGA_SCREEN_HEIGHT))
		return false;

	/*	Multisampling off.  The Amiga had none, and its preview road was hard-edged blocks	*/
	/*	of flat colour; smoothed coverage at this size just greys the ribbon's edges into	*/
	/*	the dirt.  What made multisampling look necessary before was the road breaking into	*/
	/*	a dotted line where pieces are thinner than a pixel - PreviewFattenRoad fixes that	*/
	/*	at the source instead, by dilating the road until it is always a solid ribbon.		*/
	gPreviewSavedMultisample = glIsEnabled(GL_MULTISAMPLE) ? true : false;
	if (gPreviewSavedMultisample)
		glDisable(GL_MULTISAMPLE);

	glViewport(0, 0, AMIGA_SCREEN_WIDTH, AMIGA_SCREEN_HEIGHT);

	gPreviewLowResActive = true;
	return true;
	}

static void PreviewLowResEnd( IDirect3DDevice9 *pd3dDevice )
	{
	if (!gPreviewLowResActive)
		return;
	gPreviewLowResActive = false;

	const int w = AMIGA_SCREEN_WIDTH, h = AMIGA_SCREEN_HEIGHT;

	static unsigned char *pixels = NULL;
	if (pixels == NULL)
		pixels = (unsigned char *)malloc((size_t)w * h * 3);
	if (pixels == NULL)
		return;

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	glViewport(gPreviewSavedViewport[0], gPreviewSavedViewport[1],
			   gPreviewSavedViewport[2], gPreviewSavedViewport[3]);
	if (gPreviewSavedMultisample)
		glEnable(GL_MULTISAMPLE);

	if (pPreviewLowResTexture == NULL)
		pPreviewLowResTexture = new IDirect3DTexture9();
	/*	Linear, not point: the sharp-bilinear shader does the sampling, and it needs the	*/
	/*	filter left alone (same as the menu surface).									*/
	pPreviewLowResTexture->CreateFromMemory(pixels, w, h, 3, false, false);

	if (pPreviewLowResVB == NULL)
		{
		if (FAILED(pd3dDevice->CreateVertexBuffer(4 * sizeof(PREVIEWVERTEX), D3DUSAGE_WRITEONLY,
												  D3DFVF_PREVIEWVERTEX, D3DPOOL_DEFAULT,
												  &pPreviewLowResVB, NULL)))
			return;
		}

	/*	Back to where the Amiga's screen sits in base space - the same rectangle every		*/
	/*	other menu is presented in, so nothing shifts as the low-res pass is toggled.		*/
	float x0, y0, qw, qh;
	AmigaMenuGetScreenRect(0, 0, AMIGA_SCREEN_WIDTH, AMIGA_SCREEN_HEIGHT, &x0, &y0, &qw, &qh);

	PREVIEWVERTEX *pVertices;
	if (FAILED(pPreviewLowResVB->Lock(0, 0, (void **)&pVertices, 0)))
		return;

	// glReadPixels is bottom-up, so v = 0 is the bottom of the picture.
	pVertices[0].x = x0;      pVertices[0].y = y0;      pVertices[0].u = 0.0f; pVertices[0].v = 1.0f;
	pVertices[1].x = x0 + qw; pVertices[1].y = y0;      pVertices[1].u = 1.0f; pVertices[1].v = 1.0f;
	pVertices[2].x = x0 + qw; pVertices[2].y = y0 + qh; pVertices[2].u = 1.0f; pVertices[2].v = 0.0f;
	pVertices[3].x = x0;      pVertices[3].y = y0 + qh; pVertices[3].u = 0.0f; pVertices[3].v = 0.0f;
	for (int i = 0; i < 4; i++)
		{
		pVertices[i].z   = 0.5f;
		pVertices[i].rhw = 1.0f;
		}
	pPreviewLowResVB->Unlock();

	// The 320x200 pass painted the corner of the back buffer; wipe it before it shows.
	pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTSS_COLORARG1);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	pd3dDevice->SetTexture(0, pPreviewLowResTexture);
	pd3dDevice->SetStreamSource(0, pPreviewLowResVB, 0, sizeof(PREVIEWVERTEX));
	pd3dDevice->SetFVF(D3DFVF_PREVIEWVERTEX);
	pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	}
#endif

//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------

void CALLBACK OnFrameRender( IDirect3DDevice9 *pd3dDevice, double fTime, float fElapsedTime, void *pUserContext )
{
HRESULT hr;

//    // Clear the render target and the zbuffer
//    V( pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0, 45, 50, 170), 1.0f, 0) );

    // Clear the zbuffer
    V( pd3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0) );

#ifdef SCR_PORTABLE
	/*	SCR_PREVIEW_SHOT=<file.ppm> jumps straight into the track preview for the track in
		SCR_PREVIEW_TRACK, grabs the framebuffer and quits.  A development aid for working
		on the preview screen without driving the menus by hand.							*/
	{
	static const char *shotPath = getenv("SCR_PREVIEW_SHOT");
	static long shotFrame = 0;
	if (shotPath)
		{
		++shotFrame;
		if (shotFrame == 3)
			{
			const char *t = getenv("SCR_PREVIEW_TRACK");
			if (MenuStartTrack(t ? atoi(t) : 0))
				MenuScreensDeactivate();
			}
		else if (shotFrame > 90)		// long enough for the ~8Hz world clock to run
			gPreviewShotDue = shotPath;
		}
	}
#endif

    // A finished race hands control straight back to the menus, which score it and put up
    // the RESULT screen.  MenuScreensRaceFinished reactivates them, so this fires once.
    if ((GameMode == GAME_OVER) && !MenuScreensActive())
    {
        const bool playerBestLap = bBestLapTimeSet &&
                                   (!bOppBestLapTimeSet || (bestLapTime < oppBestLapTime));
        const double raceTime = (gameEndTime > gameStartTime) ? (gameEndTime - gameStartTime) : 0.0;

        GameMode = TRACK_MENU;
        MenuScreensRaceFinished( raceWon != FALSE, playerBestLap,
                                 bBestLapTimeSet ? bestLapTime : 0.0, raceTime,
                                 bRaceMarginSet ? raceMarginTime : 0.0,
                                 CarIsWreckedNow() );
    }

    // The Amiga menus replace the display entirely, exactly as they did on the Amiga, so
    // when they are up none of the scene is drawn.
    if (MenuScreensActive())
    {
        V( pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                             D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0) );
        if( SUCCEEDED( pd3dDevice->BeginScene() ) )
        {
            MenuScreensRender( pd3dDevice );
            pd3dDevice->EndScene();
        }
        return;
    }

	// The Amiga's preview screen: a picture with the road drawn into a window cut out of it.
	// Nothing of the normal scene presentation applies - no backdrop (the picture has its own
	// mountains and grandstands), no help text, and the road is clipped to the window.
	const bool previewScreen = (GameMode == TRACK_PREVIEW) && bAmigaTrackPreview && bAmigaPreviewScreen;

	// Amiga windscreen: the scene is confined to the cockpit's window, so the cockpit is the
	// only thing drawn outside it.  Only worth doing when the cockpit is actually up.
	const bool cockpitWindow = bAmigaWindscreen && !bOutsideView && !previewScreen &&
							   ((GameMode == GAME_IN_PROGRESS) || (GameMode == GAME_OVER));

	// Normally DrawBackdrop covers every pixel, so the target is never cleared.  The preview
	// picture is letterboxed, so the bands round it have to be wiped - and so is the surround
	// the clipped scene no longer paints over.
	if (previewScreen || cockpitWindow)
		V( pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
		if (previewScreen)
		{
#ifdef SCR_PORTABLE
			// Everything up to PreviewLowResEnd goes into a 320x200 corner of the back
			// buffer, so the road is drawn at the Amiga's resolution rather than the
			// window's.  See PreviewLowResBegin.
			PreviewLowResBegin();
#endif
			DrawAmigaPreviewScreen( pd3dDevice );

			pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
			pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

			// The projection is PreviewFattenRoad's to set - it needs a different one per
			// pass - so only the clip is established here.
			SetPreviewWindowClip( true );

			// The whole map is 30-odd squares away here, deep into the haze, and the Amiga's
			// preview had no distance shading at all - it drew the road in flat colour. Turn
			// the fog off for it, or the track comes out as a grey silhouette.
#ifdef SCR_FOG_SHADER
			const bool savedFog = gFogEnabled;
			gFogEnabled = false;
#endif
			// AmigaMenuBlit leaves the picture bound as texture 0 with a live colour op, and
			// DrawTrack's preview path never sets one of its own, so without this the road
			// comes out sampled from the preview picture instead of its own flat colours.
			pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );

			pd3dDevice->SetTransform( D3DTS_WORLD, &matWorldTrack );
			PreviewFattenRoad(pd3dDevice);
#ifdef SCR_FOG_SHADER
			gFogEnabled = savedFog;
#endif

			SetPreviewWindowClip( false );
			pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
#ifdef SCR_PORTABLE
			PreviewLowResEnd( pd3dDevice );
#endif
			pd3dDevice->EndScene();

#ifdef SCR_PORTABLE
			if (gPreviewShotDue)
				{
				WriteFramebufferPPM(gPreviewShotDue);
				exit(0);
				}
#endif

			// This path returns without reaching RenderText, so the preview keys
			// ("hit fire to continue", steer to rotate) are handled here instead.
			HandleTrackPreviewInput();
			return;
		}

		// Cheap, and means the FOV toggle takes effect immediately
		SetSceneProjection( pd3dDevice );

		// Disable Z buffer and polygon culling, ready for DrawBackdrop()
		pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
		pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

		if (cockpitWindow) SetCockpitWindowClip( true );

		// Draw Backdrop
		DrawBackdrop(viewpoint1_y, viewpoint1_x_angle, viewpoint1_y_angle, viewpoint1_z_angle);

//		SetupLights(pd3dDevice);

		// Draw Track
		pd3dDevice->SetTransform( D3DTS_WORLD, &matWorldTrack );
		DrawTrack(pd3dDevice);

		// Ride heights for this frame, before either car is drawn
		UpdateCarSuspension(pd3dDevice, fElapsedTime);

		switch (GameMode)
			{
			case TRACK_MENU:
				break;

			case TRACK_PREVIEW:
				// Draw Opponent's Car
				pd3dDevice->SetTransform( D3DTS_WORLD, &matWorldOpponentsCar );
				DrawOpponentsCar(pd3dDevice);
				break;

			case GAME_IN_PROGRESS:
			case GAME_OVER:
				// Draw Opponent's Car
				pd3dDevice->SetTransform( D3DTS_WORLD, &matWorldOpponentsCar );
				DrawOpponentsCar(pd3dDevice);

				// Sparks and dust go into the scene, so they must be drawn before the
				// cockpit is laid over the top of it
				if (GameMode == GAME_IN_PROGRESS) DrawSceneParticles();

				if (bOutsideView)
				{
				// Draw Player1's Car
				pd3dDevice->SetTransform( D3DTS_WORLD, &matWorldCar );
				DrawCar(pd3dDevice);
				}
				else
				{
				// The cockpit is what fills the surround, so it must not be clipped to
				// the window it is drawing the frame of.
				if (cockpitWindow) SetCockpitWindowClip( false );

				// draw cockpit...
				DrawCockpit(pd3dDevice);
				}
				break;
			}

		if (cockpitWindow) SetCockpitWindowClip( false );

		if (GameMode == GAME_IN_PROGRESS)
		{
			//jsr	display.speed.bar
			if (bFrameMoved) UpdateDamage();

			// The lap stopwatch runs on the wall clock rather than a step count, so feed
			// UpdateLapData the real time since the last render frame.  A large gap means
			// we were stalled (or came back from the menu), so it does not count.
			{
			static double lastLapClockT = 0.0;
			double nowT = DXUTGetTime();
			double lapClockElapsed = (lastLapClockT > 0.0) ? (nowT - lastLapClockT) : 0.0;
			lastLapClockT = nowT;
			if ((lapClockElapsed < 0.0) || (lapClockElapsed > 0.25) || bPaused)
				lapClockElapsed = 0.0;

			UpdateLapData( lapClockElapsed );
			}
			//jsr	display.opponents.distance
		}

		RenderText( fTime );

		// End the scene
		pd3dDevice->EndScene();

#ifdef SCR_PORTABLE
		/*	SCR_RACE_SHOT=<file.ppm> grabs the race view once the race has been running
			for SCR_RACE_SHOT_FRAME frames (default 240, ~4s) and quits.  The sibling of
			SCR_PREVIEW_SHOT above, for the race rather than the preview - it is how a
			head-to-head session gets checked without two people at two keyboards.	*/
		{
		static const char *raceShotPath  = getenv("SCR_RACE_SHOT");
		static long        raceShotFrame = 0;
		if (raceShotPath && (GameMode == GAME_IN_PROGRESS))
			{
			/*	SCR_RACE_SHOT_OUTSIDE=1 takes the shot from the chase camera rather
				than the cockpit, which is the only way to see both cars at once -
				and so the only way to check a head-to-head session from a script. */
			if (getenv("SCR_RACE_SHOT_OUTSIDE"))
				bOutsideView = TRUE;

			const char *n = getenv("SCR_RACE_SHOT_FRAME");
			if (++raceShotFrame > (n ? atol(n) : 240))
				{
				WriteFramebufferPPM(raceShotPath);
				exit(0);
				}
			}
		}
#endif
	}
}

#ifndef SCR_PORTABLE
//--------------------------------------------------------------------------------------
// Handle messages to the application 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
                          bool *pbNoFurtherProcessing, void *pUserContext )
{
	// Handle window resizing to preserve aspect ratio
	if (uMsg == WM_SIZING)
	{
		RECT* pRect = (RECT*)lParam;
		int width = pRect->right - pRect->left;
		int height = pRect->bottom - pRect->top;
		
		// Preserve aspect ratio based on wideScreen setting
		// wideScreen=1 means 16:10 (800:480), wideScreen=0 would be 4:3
		float targetAspect = wideScreen ? (800.0f / 480.0f) : (4.0f / 3.0f);
		float currentAspect = static_cast<float>(width) / static_cast<float>(height);
		
		// Adjust based on which edge is being dragged
		if (currentAspect > targetAspect)
		{
			// Too wide, adjust width
			int newWidth = static_cast<int>(height * targetAspect);
			if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
				pRect->left = pRect->right - newWidth;
			else
				pRect->right = pRect->left + newWidth;
		}
		else if (currentAspect < targetAspect)
		{
			// Too tall, adjust height
			int newHeight = static_cast<int>(width / targetAspect);
			if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
				pRect->top = pRect->bottom - newHeight;
			else
				pRect->bottom = pRect->top + newHeight;
		}
		
		*pbNoFurtherProcessing = true;
		return 0;
	}
	
	return 0;
}

//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke 
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void *pUserContext )
{
    if( bKeyDown )
    {
		keyPress = nChar;
        switch( nChar )
        {
#if defined(DEBUG) || defined(_DEBUG)
        case VK_F1:
            bTestKey = !bTestKey;
            break;
#endif
        case VK_F2:
            ++bTrackDrawMode;
			if (bTrackDrawMode > 1) bTrackDrawMode = 0;
			DXUTReset3DEnvironment();
            break;

        case VK_F4:
            NextSceneryType();
            break;

        case VK_F5:
            bShowStats = !bShowStats;
            break;

        case VK_F6:
			if (scr::NetGamePauseBlocked("Pausing the player")) break;
            bPlayerPaused = !bPlayerPaused;
            break;

        case VK_F7:
			if (scr::SimSettingLocked("Pausing the opponent")) break;
            bOpponentPaused = !bOpponentPaused;
            break;

		case VK_F9:
			if (scr::SimSettingLocked("The opponent clock (frameGap)")) break;
			if (frameGap > 1) frameGap--;
			break;

		case VK_F10:
			if (scr::SimSettingLocked("The opponent clock (frameGap)")) break;
			frameGap++;
			break;

		case 'V':
			// Toggle the FloatV2 physics port (see Physics_FloatV2.h).
			if (scr::SimSettingLocked("The physics path (V)")) break;
			scr::gUseFloatV2Physics = !scr::gUseFloatV2Physics;
			break;

		case 'U':
			// Toggle the FloatV2 opponent step (see Physics_FloatV2.h).
			if (scr::SimSettingLocked("The opponent step (U)")) break;
			scr::gUseFloatV2Opponent = !scr::gUseFloatV2Opponent;
			break;

		case 'B':
			// Cycle the FloatV2 timestep: 10Hz (Amiga rate) -> 25Hz -> 60Hz.
			// At 10Hz this should behave like the legacy path; the higher
			// rates are the point of the port.
			if (scr::SimSettingLocked("The physics timestep (B)")) break;
			if      (scr::gFloatV2Dt > 0.05)  scr::gFloatV2Dt = 1.0 / 25.0;
			else if (scr::gFloatV2Dt > 0.025) scr::gFloatV2Dt = 1.0 / 60.0;
			else                              scr::gFloatV2Dt = 0.1;
			break;

		case 'N':
			// Dump the next 20 FloatV2 steps to stdout (see Physics_FloatV2.h).
			scr::gFloatV2DebugSteps = (int)(2.0 / scr::gFloatV2Dt);	// ~2s of steps at any rate
			break;

		case 'K':
			scr::gFloatV2DumpOnCurves = !scr::gFloatV2DumpOnCurves;
			break;

		case 'J':
			// EXPERIMENT: un-reverse distance-into-section on opposite-direction
			// curves (see gFloatV2UnreverseCurveDist in Physics_FloatV2.h).
			if (scr::SimSettingLocked("The curve distance experiment (J)")) break;
			scr::gFloatV2UnreverseCurveDist = !scr::gFloatV2UnreverseCurveDist;
			break;

#if defined(DEBUG) || defined(_DEBUG)
		case VK_BACK:
			bOutsideView = !bOutsideView;
            break;
#endif
		case 'M':
			ReturnToMenus();
            break;

		case 'O':
			if (scr::NetGamePauseBlocked("Pausing")) break;
			bPaused = FALSE;
            break;

		case 'P':
			if (scr::NetGamePauseBlocked("Pausing")) break;
			bPaused = TRUE;
            break;

		case 'Z':
			bNewGame = TRUE;		// for testing to try stopping car positioning bug
            break;

		// controls for Car Behaviour, Player 1
        case VK_LEFT:
            lastInput |= KEY_P1_LEFT;
            break;

        case VK_RIGHT:
            lastInput |= KEY_P1_RIGHT;
            break;

        case VK_SPACE:
            lastInput |= KEY_P1_BOOST;
            break;

		case VK_DOWN:
            lastInput |= KEY_P1_BRAKE;
            break;

        case VK_UP:
            lastInput |= KEY_P1_ACCEL;
            break;
        }

#ifdef NOT_USED
        switch( nChar )
        {
            case VK_LEFT: fEyeX -= 1000.0f; break;
            case VK_RIGHT: fEyeX += 1000.0f; break;
            case VK_UP: fEyeY -= 1000.0f; break;
            case VK_DOWN: fEyeY += 1000.0f; break;
            case VK_PRIOR: fEyeZ -= 1000.0f; break;	// pgup
            case VK_NEXT: fEyeZ += 1000.0f; break;	// pgdn
        }
#endif
    }
	else
	{
		keyPress = '\0';
        switch( nChar )
        {
		// controls for Car Behaviour, Player 1
        case VK_LEFT:
            lastInput &= ~KEY_P1_LEFT;
            break;

        case VK_RIGHT:
            lastInput &= ~KEY_P1_RIGHT;
            break;

        case VK_SPACE:	// couldn't find VK_ definition for HASH key
            lastInput &= ~KEY_P1_BOOST;
            break;

		case VK_DOWN:
            lastInput &= ~KEY_P1_BRAKE;
            break;

        case VK_UP:
            lastInput &= ~KEY_P1_ACCEL;
            break;
		}
	}
}


//--------------------------------------------------------------------------------------
// Release resources created in the OnResetDevice callback here 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void *pUserContext )
{
//    g_DialogResourceManager.OnLostDevice();
//    g_SettingsDlg.OnLostDevice();
//    CDXUTDirectionWidget::StaticOnLostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pFontLarge )
        g_pFontLarge->OnLostDevice();
    SAFE_RELEASE(g_pSprite);

	FreePolygonVertexBuffer();
	FreeTrackVertexBuffer();
	FreeShadowVertexBuffer();
	FreeCarVertexBuffer();
	FreeCockpitVertexBuffer();

	if (g_pAtlas) g_pAtlas->Release(), g_pAtlas = NULL;
	if (g_pChain) g_pChain->Release(), g_pChain = NULL;
#ifdef SCR_ROAD_TEXTURE
	FreeRoadTextures();
#endif
}


//--------------------------------------------------------------------------------------
// Release resources created in the OnCreateDevice callback here
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void *pUserContext )
{
//    g_DialogResourceManager.OnDestroyDevice();
//    g_SettingsDlg.OnDestroyDevice();
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pFontLarge);

	FreePolygonVertexBuffer();
	FreeTrackVertexBuffer();
	FreeShadowVertexBuffer();
	FreeCarVertexBuffer();
	FreeCockpitVertexBuffer();
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
{
    // Enable run-time memory check for debug builds.
	wchar_t maintitle[50] = {0};
	wsprintf(maintitle, L"StuntCarRemake v%d.%02d.%02d", V_MAJOR, V_MINOR, V_PATCH);
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set the callback functions
    DXUTSetCallbackDeviceCreated( OnCreateDevice );
    DXUTSetCallbackDeviceReset( OnResetDevice );
    DXUTSetCallbackDeviceLost( OnLostDevice );
    DXUTSetCallbackDeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameRender( OnFrameRender );
    DXUTSetCallbackFrameMove( OnFrameMove );
   
    // Perform any application-level initialization here
	if (!InitialiseData())
	    return DXUTGetExitCode();

    // Initialize DXUT and create the desired Win32 window and Direct3D device for the application
    DXUTInit( true, true, true, false ); // Parse the command line, handle the default hotkeys, show msgboxes, don't handle Alt-Enter
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( maintitle );
	
	// Get screen resolution and set initial window size to roughly half
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int initialWidth = screenWidth / 2;
	int initialHeight = screenHeight / 2;
	// Maintain 16:10 aspect ratio
	if (initialWidth * 10 != initialHeight * 16) {
		initialHeight = (initialWidth * 10) / 16;
	}
	
    DXUTCreateDevice( D3DADAPTER_DEFAULT, true, initialWidth, initialHeight, IsDeviceAcceptable, ModifyDeviceSettings );
	wideScreen = 1;

//	DXUTSetConstantFrameTime( true, 0.033f );	// Doesn't seem to work

	//
	//	Initialise sound objects
	//
	if (!DSInit())
		return FALSE;

	if (!DSSetMode())
		return FALSE;

    // Start the render loop
    DXUTMainLoop();

    // Perform any application-level cleanup here
	FreeData();

    return DXUTGetExitCode();
}

#else

#ifdef USE_SDL2
// Recompute the GL viewport from the window's current drawable size. Needed at
// runtime as well as at startup: dragging the window between displays with
// different backing scales (Retina <-> external) changes the drawable size
// without any resize of our own, and a stale viewport stretches the raster.
void ApplyViewport();
#endif

bool process_events()
{
    SDL_Event event;
    while( SDL_PollEvent( &event ) ) {
        switch( event.type ) {
        case SDL_KEYDOWN:
			keyPress = event.key.keysym.sym;
			// some special cases for French keyboards
			if((event.key.keysym.mod & KMOD_SHIFT) == 0)
			switch(event.key.keysym.sym) {
				case SDLK_AMPERSAND:	keyPress = SDLK_1; break;
				case 233:				keyPress = SDLK_2; break;
				case SDLK_QUOTEDBL:		keyPress = SDLK_3; break;
				case SDLK_QUOTE:		keyPress = SDLK_4; break;
				case SDLK_LEFTPAREN:	keyPress = SDLK_5; break;
				case SDLK_MINUS:		keyPress = SDLK_6; break;
				case 232:				keyPress = SDLK_7; break;
				case SDLK_UNDERSCORE:	keyPress = SDLK_8; break;
				case 231:				keyPress = SDLK_9; break;
				case 224:				keyPress = SDLK_0; break;
			}

			// While the Amiga menus are up they own the keyboard: navigation, name entry
			// and menu selection all go to them, and none of the in-game keys apply.
			if (MenuScreensActive())
			{
				// Escape backs out one screen, and only quits from the top of the
				// menu tree - otherwise there is no way back to a screen once it
				// has been left.
				if (keyPress == SDLK_ESCAPE)
				{
					if (!MenuScreensBack())
						return false;
					keyPress = '\0';
					break;
				}
				MenuScreensKey( keyPress );
				keyPress = '\0';
				break;
			}

			// The quit prompt owns the keyboard while it is up: Escape backs out,
			// Enter leaves the race, everything else is swallowed so the car cannot
			// be driven.
			if (bQuitConfirm)
			{
				if (keyPress == SDLK_RETURN || keyPress == SDLK_KP_ENTER)
				{
					ReturnToMenus();		// clears the prompt and the freeze
					keyPress = '\0';
					break;
				}
				if (keyPress == SDLK_ESCAPE)
				{
					bQuitConfirm = FALSE;
					bPaused = bQuitConfirmWasPaused;
				}
				keyPress = '\0';
				break;
			}

            switch( keyPress ) {
#if defined(DEBUG) || defined(_DEBUG)
				case SDLK_F1:
					bTestKey = !bTestKey;
					break;
#endif
				case SDLK_F2:
					++bTrackDrawMode;
					if (bTrackDrawMode > 1) bTrackDrawMode = 0;
					DXUTReset3DEnvironment();
					break;

				case SDLK_F3:
					// Render interpolation on/off, for an A/B against the raw
					// physics rate.  Display only - see PLAYER_STATE.
					gRenderInterpolation = !gRenderInterpolation;
					printf("Render interpolation: %s\n", gRenderInterpolation ? "on" : "off");
					break;

				case SDLK_F4:
					NextSceneryType();
					break;

				case SDLK_F5:
					bShowStats = !bShowStats;
					break;

				case SDLK_F6:
					if (scr::NetGamePauseBlocked("Pausing the player")) break;
					bPlayerPaused = !bPlayerPaused;
					break;

				case SDLK_F7:
					if (scr::SimSettingLocked("Pausing the opponent")) break;
					bOpponentPaused = !bOpponentPaused;
					break;

				case SDLK_F9:
					if (scr::SimSettingLocked("The opponent clock (frameGap)")) break;
					if (frameGap > 1) frameGap--;
					break;

				case SDLK_F10:
					if (scr::SimSettingLocked("The opponent clock (frameGap)")) break;
					frameGap++;
					break;

				case SDLK_v:
					// Toggle the FloatV2 physics port (see Physics_FloatV2.h).
					// Letter keys, not F11/F12 — those collide with macOS.
					if (scr::SimSettingLocked("The physics path (V)")) break;
					scr::gUseFloatV2Physics = !scr::gUseFloatV2Physics;
					printf("FloatV2 physics %s (dt=%.4f, %.0fHz)\n",
						   scr::gUseFloatV2Physics ? "ON" : "OFF",
						   scr::gFloatV2Dt, 1.0 / scr::gFloatV2Dt);
					fflush(stdout);
					break;

				case SDLK_a:
					// Toggle the display pixel aspect the raster is presented with:
					// PAL 1.0667 (authentic, 1.707 picture, bars top and bottom) or
					// NTSC 0.8333 (4:3 picture, bars at the sides).  Presentation only.
					gPresentPixelAspect = (gPresentPixelAspect == AMIGA_PAL_PIXEL_ASPECT)
						? AMIGA_NTSC_PIXEL_ASPECT : AMIGA_PAL_PIXEL_ASPECT;
					printf("Display aspect: %s (pixel aspect %.4f, picture %.3f:1)\n",
						   (gPresentPixelAspect == AMIGA_PAL_PIXEL_ASPECT) ? "PAL" : "4:3",
						   gPresentPixelAspect,
						   (wideScreen ? 800.f : 640.f) / (480.f * ScrPresentSquash()));
					fflush(stdout);
#ifdef USE_SDL2
					ApplyViewport();
#endif
					break;

				case SDLK_f:
					// Toggle the Amiga field of view (see GetProjectionTangents,
					// 3D_Engine.cpp).
					gAmigaFov = !gAmigaFov;
					ReportFieldOfView();
					break;

				case SDLK_COMMA:
					// Less vertical stretch (1.0 = geometrically square).
					gAmigaFovStretch -= 0.05f;
					if (gAmigaFovStretch < 1.0f) gAmigaFovStretch = 1.0f;
					ReportFieldOfView();
					break;

				case SDLK_PERIOD:
					// More vertical stretch (1.437 = the Amiga's exact 45 x 22.5 angles).
					gAmigaFovStretch += 0.05f;
					if (gAmigaFovStretch > 1.45f) gAmigaFovStretch = 1.45f;
					ReportFieldOfView();
					break;

				case SDLK_u:
					// Toggle the FloatV2 opponent step (see Physics_FloatV2.h).
					// O is taken (unpause), hence U.
					if (scr::SimSettingLocked("The opponent step (U)")) break;
					scr::gUseFloatV2Opponent = !scr::gUseFloatV2Opponent;
					printf("FloatV2 opponent %s%s\n",
						   scr::gUseFloatV2Opponent ? "ON" : "OFF (8.3Hz legacy)",
						   scr::gUseFloatV2Physics ? "" : "  [physics still OFF - press V]");
					fflush(stdout);
					break;

#ifdef SCR_FOG_SHADER
				case SDLK_g:
					// Toggle the volumetric fog (see dx_linux.h).
					gFogEnabled = !gFogEnabled;
					printf("Fog %s (density=%g, heightScale=%g)\n",
						   gFogEnabled ? "ON" : "OFF", gFogDensity, gFogHeightScale);
					fflush(stdout);
					break;

				case SDLK_h:
					// Cycle fog density, so it can be eyeballed against our world scale.
					gFogDensity *= 2.0f;
					if (gFogDensity > 0.0001f) gFogDensity = 0.000001f;
					printf("Fog density=%g%s\n", gFogDensity,
						   gFogEnabled ? "" : "  [fog still OFF - press G]");
					fflush(stdout);
					break;
#endif

#ifdef SCR_SHARP_PIXEL
				case SDLK_y:
					// Toggle sharp-bilinear filtering of the 2D art (see dx_linux.h).
					gSharpPixelEnabled = !gSharpPixelEnabled;
					printf("Sharp-pixel 2D filtering %s\n", gSharpPixelEnabled ? "ON" : "OFF (plain bilinear)");
					fflush(stdout);
					break;
#endif

				case SDLK_i:
					// Toggle the fixed-sun shading of the track faces (see FaceShade in Track.cpp).
					// The shade is baked into the vertex colours, so the track vertex buffer has
					// to be refilled.  Note we can't use DXUTReset3DEnvironment() for this - it is
					// a no-op outside Windows (see dx_linux.cpp).
					gTrackLighting = !gTrackLighting;
					printf("Track lighting %s\n", gTrackLighting ? "ON" : "OFF (flat colours)");
					fflush(stdout);
					if (CreateTrackVertexBuffer(DXUTGetD3DDevice()) != S_OK)
						printf("Track lighting: failed to rebuild the track vertex buffer\n");
					break;

				case SDLK_b:
					// Cycle the FloatV2 timestep: 10Hz (Amiga rate) -> 25Hz -> 60Hz.
					if (scr::SimSettingLocked("The physics timestep (B)")) break;
					if      (scr::gFloatV2Dt > 0.05)  scr::gFloatV2Dt = 1.0 / 25.0;
					else if (scr::gFloatV2Dt > 0.025) scr::gFloatV2Dt = 1.0 / 60.0;
					else                              scr::gFloatV2Dt = 0.1;
					printf("FloatV2 dt=%.4f (%.0fHz)%s\n", scr::gFloatV2Dt,
						   1.0 / scr::gFloatV2Dt,
						   scr::gUseFloatV2Physics ? "" : "  [physics still OFF - press V]");
					fflush(stdout);
					break;

				case SDLK_n:
					// Dump the next 20 FloatV2 steps to stdout.
					scr::gFloatV2DebugSteps = (int)(2.0 / scr::gFloatV2Dt);	// ~2s of steps at any rate
					break;

				case SDLK_j:
					// EXPERIMENT: un-reverse distance-into-section on
					// opposite-direction curves (Physics_FloatV2.h). M is taken
					// by the track menu, so this lives on J.
					if (scr::SimSettingLocked("The curve distance experiment (J)")) break;
					scr::gFloatV2UnreverseCurveDist = !scr::gFloatV2UnreverseCurveDist;
					printf("FloatV2 un-mirror NormalDistanceIntoSection: %s\n",
						   scr::gFloatV2UnreverseCurveDist ? "ON" : "OFF");
					fflush(stdout);
					break;

				case SDLK_k:
					// Dump every step spent on a curved piece.
					scr::gFloatV2DumpOnCurves = !scr::gFloatV2DumpOnCurves;
					printf("FloatV2 dump-on-curves: %s\n",
						   scr::gFloatV2DumpOnCurves ? "ON" : "OFF");
					fflush(stdout);
					break;

#if defined(DEBUG) || defined(_DEBUG)
				case SDLK_BACK:
					bOutsideView = !bOutsideView;
					break;
#endif
				case SDLK_w:
					// Amiga windscreen: clip the scene to the cockpit window and leave
					// the widescreen surround black, as the Amiga's playfield did.
					bAmigaWindscreen = !bAmigaWindscreen;
					printf("Amiga windscreen (black surround): %s\n",
						   bAmigaWindscreen ? "ON" : "OFF");
					fflush(stdout);
					break;

				case SDLK_m:
					ReturnToMenus();
					break;

				case SDLK_o:
					if (scr::NetGamePauseBlocked("Pausing")) break;
					bPaused = FALSE;
					break;

				case SDLK_p:
					if (scr::NetGamePauseBlocked("Pausing")) break;
					bPaused = TRUE;
					break;

				case SDLK_z:
					bNewGame = TRUE;		// for testing to try stopping car positioning bug
					break;

				// controls for Car Behaviour, Player 1
				case SDLK_LEFT:
					lastInput |= KEY_P1_LEFT;
					break;

				case SDLK_RIGHT:
					lastInput |= KEY_P1_RIGHT;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_RCTRL:
#else
				case SDLK_SPACE:
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
#endif
					lastInput |= KEY_P1_BOOST;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_END:
#else
				case SDLK_DOWN:
#endif
					lastInput |= KEY_P1_BRAKE;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_PAGEDOWN:
#else
				case SDLK_UP:
#endif
					lastInput |= KEY_P1_ACCEL;
					break;

				case SDLK_ESCAPE:
					// Dropping out from under a race is too easy to do by accident,
					// so put a confirmation up and freeze the race behind it.
					if ((GameMode == GAME_IN_PROGRESS) || (GameMode == GAME_OVER))
					{
						bQuitConfirm = TRUE;
						bQuitConfirmWasPaused = bPaused;
						// The confirmation still goes up in a two-player race, but
						// without the freeze: stopping the simulation here would
						// stall the other player's game for as long as this one
						// took to answer it.  The race carries on behind the prompt.
						if (!scr::NetGameRacing())
							bPaused = TRUE;
						break;
					}
					// Anywhere else in the game (the track preview) escape just goes
					// back to the menus, which is where quitting now lives.
					ReturnToMenus();
					break;
				}
            break;
        case SDL_KEYUP:
			keyPress = 0;
            switch( event.key.keysym.sym ) {
				// controls for Car Behaviour, Player 1
				case SDLK_LEFT:
					lastInput &= ~KEY_P1_LEFT;
					break;

				case SDLK_RIGHT:
					lastInput &= ~KEY_P1_RIGHT;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_RCTRL:
#else
				case SDLK_SPACE:
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
#endif
					lastInput &= ~KEY_P1_BOOST;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_END:
#else
				case SDLK_DOWN:
#endif
					lastInput &= ~KEY_P1_BRAKE;
					break;

#if defined(PANDORA) || defined(PYRA)
				case SDLK_PAGEDOWN:
#else
				case SDLK_UP:
#endif
					lastInput &= ~KEY_P1_ACCEL;
					break;
				}
			break;
#ifdef USE_SDL2
		case SDL_WINDOWEVENT:
			switch(event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			case SDL_WINDOWEVENT_MOVED:
			case SDL_WINDOWEVENT_EXPOSED:
				ApplyViewport();
				break;
			}
			break;
#endif
        case SDL_QUIT:
            return false;
        }
    }
	return true;
}

IDirect3DDevice9 pd3dDevice;
#ifdef USE_SDL2
SDL_Window *window = NULL;

void ApplyViewport()
{
	if(!window)
		return;
	int drawW = 0, drawH = 0, pointW = 0, pointH = 0;
	SDL_GL_GetDrawableSize(window, &drawW, &drawH);
	SDL_GetWindowSize(window, &pointW, &pointH);
	if(drawW <= 0 || drawH <= 0)
		return;
	float dpiFactor = (pointW > 0) ? static_cast<float>(drawW) / static_cast<float>(pointW) : 1.0f;
	if(dpiFactor <= 0.0f)
		dpiFactor = 1.0f;

	// automatic guess the scale or use custom scale
	float screenScale;
	if(gCustomScale > 0.0f) {
		// Use custom scale factor, in points, so it matches the requested size
		screenScale = gCustomScale * dpiFactor;
	} else {
		// Automatic scaling based on window size.  Only 480*ScrPresentSquash() of the base
		// space is ever presented (see below), so fit against that, not against 480 - else
		// the squash would be paid for twice and the picture would sit in a letterbox
		// inside a letterbox.
		const double presentH = 480. * ScrPresentSquash();
		screenScale = (drawW/640. < drawH/presentH) ? drawW/640. : drawH/presentH;
	}
	// is it a Wide screen ratio?
	// Detect widescreen if width is significantly wider than 4:3 aspect ratio.
	// Decided once, at startup: the whole 2D layout is built around it, so it
	// must not flip when the window is dragged to another display.
	// Measured against the PAL presentation (the constant, not ScrPresentSquash) so that
	// the A toggle changes only how the raster is presented, never the 2D layout.
	static bool aspectChosen = false;
	if(!aspectChosen) {
		const double palScale = (drawW/640. < drawH/(480.*SCR_PRESENT_SQUASH))
			? drawW/640. : drawH/(480.*SCR_PRESENT_SQUASH);
		if((drawW/palScale - 640)>=80)
			wideScreen=1;
		aspectChosen = true;
	}
	int viewW = static_cast<int>((wideScreen?800:640)*screenScale);
	int fullH = static_cast<int>(480*screenScale);
	int viewX = (drawW - viewW)/2;
	int baseY = (drawH - fullH)/2;
	// The 640x480 base holds the Amiga's 320x200 at (2.0, 2.4), i.e. 1.2x taller than wide.
	// Undo that here, once, for the whole raster - geometry and 2D art alike - and replace it
	// with the display's own pixel aspect, exactly as the monitor did on real hardware.
	// PAL (1.0667) gives the authentic 1.707 picture, letterboxed top and bottom as the 200
	// lines were inside PAL's 256-line display window; NTSC (0.8333) gives a 4:3 picture,
	// pillarboxed on a wide display.  A toggles.  See 3D_Engine.h for the derivation.
	int viewH = static_cast<int>(fullH * ScrPresentSquash() + 0.5f);
	int viewY = baseY + (fullH - viewH)/2;
	static int lastW = 0, lastH = 0;
	if(viewW != lastW || viewH != lastH) {
		lastW = viewW; lastH = viewH;
		printf("Display mode: %s, Scale: %.2f, Resolution: %dx%d (DPI factor %.2f)\n",
			   wideScreen ? "Widescreen" : "Standard", screenScale, viewW, fullH, dpiFactor);
	}
	glViewport(viewX, viewY, viewW, viewH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, wideScreen?800:640, 480, 0, 0, FURTHEST_Z);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
#endif

#ifdef __EMSCRIPTEN__
extern "C"
void initialize_gl4es();
double fLastTime;
void em_main_loop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	double fTime = DXUTGetTime();
	process_events();	// no quit...
	OnFrameMove( &pd3dDevice, fTime, fTime - fLastTime, NULL );
	OnFrameRender( &pd3dDevice, fTime, fTime - fLastTime, NULL );
#ifdef USE_SDL2
	SDL_GL_SwapWindow(window);
#else
	SDL_GL_SwapBuffers();
#endif

	int32_t timetowait = (1.0f/50.0f - (fTime-fLastTime))*1000;
	//int32_t timetowait = (1.0f/60.0f - (fTime-fLastTime))*1000;
	if (timetowait>0)
		SDL_Delay(timetowait);

	fLastTime = fTime;
}
#endif

int GL_MSAA = 0;
// argv is char** rather than const char**: on Windows SDL_main.h does
// "#define main SDL_main" and declares SDL_main as taking char *argv[]. A const
// parameter mangles to a different symbol, so libSDL2main's WinMain cannot find
// it and the link fails with "undefined reference to SDL_main".
int main(int argc, char** argv)
{
#ifdef __EMSCRIPTEN__
	initialize_gl4es();
#endif
	char maintitle[50] = {0};
	sprintf(maintitle, "StuntCarRemake v%d.%02d.%02d", V_MAJOR, V_MINOR, V_PATCH);
	printf("%s\n", maintitle);
	// get executable folder and cd into it, so relative asset paths work
#ifdef USE_SDL2
	// SDL knows how to do this on every platform we care about: GetModuleFileName on
	// Windows, _NSGetExecutablePath on macOS, /proc/self/exe on Linux.
	if(char* basePath = SDL_GetBasePath()) {
		chdir(basePath);
		printf("chdir(\"%s\")\n", basePath);
		SDL_free(basePath);
	}
#else
	char buf[500];
	ssize_t bufsized = readlink("/proc/self/exe", buf, sizeof(buf)-1);
	if(bufsized>0) {
		buf[bufsized] = 0;		// readlink() does not terminate
		char* p = strrchr(buf, '/');
		if(p) {
			*p=0;
			chdir(buf);
			printf("chdir(\"%s\")\n", buf);
		}
	}
#endif
#ifdef USE_SDL2
	SDL_GLContext context = NULL;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK)==-1) {
		printf("Could not initialise SDL2: %s\n", SDL_GetError());
		exit(-1);
	}
#else
	SDL_Surface *screen = NULL;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTTHREAD)==-1) {
		printf("Could not initialise SDL: %s\n", SDL_GetError());
		exit(-1);
	}
#endif
	atexit(SDL_Quit);

	TTF_Init();

	// crude command line parameter reading
	int nomsaa = 0;
	int fullscreen = 0;
	int desktop = 0;
	int givehelp = 0;
	int customWidth = 0;
	int customHeight = 0;
	float customScale = 0.0f;

	for (int i=1; i<argc; i++) {
		if(!strcmp(argv[i], "-f"))
			fullscreen = 1;
		else if(!strcmp(argv[i], "--fullscreen"))
			fullscreen = 1;
		else if(!strcmp(argv[i], "-d"))
			desktop = 1;
		else if(!strcmp(argv[i], "--desktop"))
			desktop = 1;
		else if(!strcmp(argv[i], "-n"))
			nomsaa = 1;
		else if(!strcmp(argv[i], "--nomsaa"))
			nomsaa = 1;
		else if((!strcmp(argv[i], "-w") || !strcmp(argv[i], "--width")) && i+1 < argc) {
			customWidth = atoi(argv[++i]);
			if(customWidth <= 0) {
				printf("Error: Invalid width value\n");
				givehelp = 1;
			}
		}
		else if((!strcmp(argv[i], "-h") || !strcmp(argv[i], "--height")) && i+1 < argc) {
			customHeight = atoi(argv[++i]);
			if(customHeight <= 0) {
				printf("Error: Invalid height value\n");
				givehelp = 1;
			}
		}
		else if((!strcmp(argv[i], "-s") || !strcmp(argv[i], "--scale")) && i+1 < argc) {
			customScale = static_cast<float>(atof(argv[++i]));
			if(customScale <= 0.0f) {
				printf("Error: Invalid scale value\n");
				givehelp = 1;
			}
		}
		/*	Netplay shortcuts.  They do nothing the Multiplayer menu cannot, but they
			skip the menu-driving, which makes a two-machine session one command per
			machine instead of a sequence of keypresses on each - and makes a
			loopback session scriptable.									*/
		else if(!strcmp(argv[i], "--net-host") && i+1 < argc) {
			gNetAutoHostTrack = atoi(argv[++i]);
		}
		else if(!strcmp(argv[i], "--net-join") && i+1 < argc) {
			snprintf(gNetAutoJoinAddress, sizeof(gNetAutoJoinAddress), "%s", argv[++i]);
		}
		else if(int used = scr::SimTrace_ParseArg(argc, argv, i)) {
			i += used - 1;		// the loop's own ++i accounts for the first
		}
		else givehelp = 1;
	}
	if(givehelp) {
		printf("Unrecognized parameter.\nOptions are:\n");
		printf("\t-f|--fullscreen\t\tUse fullscreen\n");
		printf("\t-d|--desktop\t\tUse desktop fullscreen\n");
		printf("\t-n|--nomsaa\t\tDisable MSAA\n");
		printf("\t-w|--width <pixels>\tSet window width (e.g., 640, 800, 1280)\n");
		printf("\t-h|--height <pixels>\tSet window height (e.g., 480, 600, 720)\n");
		printf("\t-s|--scale <factor>\tSet scale factor (e.g., 1.0, 1.5, 2.0)\n");
		printf("\t--simtrace [steps]\tRecord a per-step physics checksum to simtrace.log and quit\n");
		printf("\t--simtrace-verbose\tAs --simtrace, but dump every state field too\n");
		printf("\t--simtrace-track <n>\tTrack to trace on (0-7, default 0)\n");
		printf("\t--simtrace-seed <n>\tSimulation RNG seed (default 0x12345678)\n");
		printf("\t--simtrace-out <file>\tLog file to write (default simtrace.log)\n");
		printf("\t--net-host <track>\tHost a two-player race on the given track (0-7)\n");
		printf("\t--net-join <address>\tJoin a two-player race hosted at <address>\n");
		exit(0);
	}

    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
	/*	24 bits of depth, not 16.  Depth resolution falls off as z^2: the world-space gap
		between two representable depths is about z^2 / (zn * 2^bits), and this frustum is
		zn 0.5 to FURTHEST_Z 131072 - a 262144:1 ratio, which spends nearly all of a 16-bit
		buffer inside the first few units.  At 3000 units out the gap was ~275 world units
		and the opponent's car is only ~240 long, so the car and the road it stands on
		landed in the same bucket and rounding decided which one you saw.  That is the
		opponent flickering and vanishing through a 180: the car is far away and there is
		road behind it in every direction to lose against.  8 more bits is 256x finer.
		Asked for, not required - SDL will hand back whatever the driver has.			*/
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

#if defined(PANDORA)
	int revision = 5;
	FILE *f = fopen("/etc/powervr-esrev", "r");
	if (f) {
		fscanf(f, "%d", &revision);
		fclose(f);
		printf("Pandora Model detected = %d\n", revision);
	}
	if(revision==5 && !nomsaa) {
		// only do MSAA for Gigahertz model
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 2);
		GL_MSAA=1;
	}
#else
	if(!nomsaa) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 4);
		GL_MSAA=1;
	}
#endif
	int flags = 0;
	float dpiFactor = 1.0f;	// drawable pixels per logical point (>1 on HiDPI)
	wideScreen = 0;
	int screenH, screenW, screenX, screenY;
#ifdef USE_SDL2
	// ALLOW_HIGHDPI: render at native pixel density on Retina/HiDPI displays
	// rather than letting the OS upscale a low-res drawable. Safe here because
	// nothing in the game consumes mouse coordinates (which stay in points).
	flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
#else
	flags = SDL_OPENGL | SDL_DOUBLEBUF;
#endif
	if(fullscreen)
#ifdef USE_SDL2
		flags |= SDL_WINDOW_FULLSCREEN;
#else
		flags |= SDL_FULLSCREEN;
#endif
#ifdef PANDORA
#ifdef USE_SDL2
		flags |= SDL_WINDOW_FULLSCREEN;
#else
	flags |= SDL_FULLSCREEN;
#endif
	screenW = 800; screenH = 480;
#elif defined(CHIP)
#ifdef USE_SDL2
		flags |= SDL_WINDOW_FULLSCREEN;
#else
	flags |= SDL_FULLSCREEN;
#endif
	screenW = 480; screenH = 272;
#else
	// Use custom dimensions if provided
	if(customWidth > 0 && customHeight > 0) {
		screenW = customWidth;
		screenH = customHeight;
	} else if(desktop || fullscreen) {
#ifdef USE_SDL2
		flags |= (desktop)?SDL_WINDOW_FULLSCREEN_DESKTOP:SDL_WINDOW_FULLSCREEN;
#else
		flags |= SDL_FULLSCREEN;
#endif
		if(desktop) {
#ifdef USE_SDL2
			screenW = customWidth > 0 ? customWidth : 640;
			screenH = customHeight > 0 ? customHeight : 480;
#else
			const SDL_VideoInfo* infos = SDL_GetVideoInfo();
			screenW = customWidth > 0 ? customWidth : infos->current_w;
			screenH = customHeight > 0 ? customHeight : infos->current_h;
#endif
		} else {
			screenW = customWidth > 0 ? customWidth : 640;
			screenH = customHeight > 0 ? customHeight : 480;
		}
	} else {
		// Windowed. Pick a sensible default size instead of the old fixed 800x480,
		// which is tiny on a modern display.
		int defW = 800, defH = 480;
		if(customScale > 0.0f) {
			// An explicit scale needs a window big enough to hold the scaled
			// viewport, otherwise the render gets clipped.
			defW = static_cast<int>(800 * customScale);
			defH = static_cast<int>(480 * customScale);
		}
#ifdef USE_SDL2
		else {
			// Largest half-step scale of the 800x480 base that still leaves
			// room for the menu bar / dock / window chrome.
			SDL_Rect usable;
			if(SDL_GetDisplayUsableBounds(0, &usable)==0 && usable.w>0 && usable.h>0) {
				double scale = floor(fmin(usable.w*0.9/800., usable.h*0.9/480.) * 2.0) / 2.0;
				if(scale < 1.0) scale = 1.0;
				defW = (int)(800 * scale);
				defH = (int)(480 * scale);
			}
		}
#endif
		screenW = customWidth > 0 ? customWidth : defW;
		screenH = customHeight > 0 ? customHeight : defH;
	}
#endif
#ifdef USE_SDL2
	window = SDL_CreateWindow(maintitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenW, screenH, flags);
	if(window==NULL && GL_MSAA) {
		// fallback to no MSAA
		GL_MSAA=0;
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0);
		window = SDL_CreateWindow(maintitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenW, screenH, flags);
	}
	if(window==NULL) {
		printf("Couldn't create Window (%dx%d): %s\n", screenW, screenH, SDL_GetError());
		exit(-2);
	}
	context = SDL_GL_CreateContext(window);
	if(context==NULL) {
			printf("Couldn't create OpenGL Context: %s\n", SDL_GetError());
			exit(-3);
	}
	// Resolve the post-GL-1.1 entry points now the context is current. Failure is not
	// fatal: the game falls back to fixed-function, minus fog and sharp-pixel filtering.
	SCR_LoadGLProcs();
	{
		int r=0,g=0,b=0,d=0,ms=0,msb=0;
		SDL_GL_GetAttribute(SDL_GL_RED_SIZE,&r);
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE,&g);
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE,&b);
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE,&d);
		SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&msb);
		SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES,&ms);
		printf("GL framebuffer: colour %d/%d/%d  depth %d  msaa %dx%d\n", r,g,b,d,msb,ms);
	}
	// Drawable size, not window size: on a HiDPI display these differ and the
	// GL viewport is in pixels. dpiFactor rescales the point-based -s/-w/-h
	// options so a requested size still means the same physical size.
	{
		int pointW = screenW, pointH = screenH;
		SDL_GetWindowSize(window, &pointW, &pointH);
		SDL_GL_GetDrawableSize(window, &screenW, &screenH);
		if(pointW > 0)
			dpiFactor = static_cast<float>(screenW) / static_cast<float>(pointW);
		if(dpiFactor <= 0.0f)
			dpiFactor = 1.0f;
	}
	SDL_SetWindowTitle(window, maintitle);
	/*	Frame pacing.  The physics is driven by a wall-clock accumulator now (see
		OnFrameMove), so a faster display no longer makes the world run fast, and vsync
		is the better clock: SDL_Delay only has millisecond granularity plus scheduling
		slop, so a self-paced loop swaps at arbitrary points in the scanout and every
		frame is a few ms long or short.  That jitter is what shows up as judder when
		the whole view rotates - on the crane swing, say - because then every pixel on
		screen is moving rather than just the car.

		On a high-refresh display take every 2nd (120Hz) or 3rd (180Hz) refresh so the
		presented rate still lands near 60: drawing at 120 with 60Hz physics would just
		show each state twice.  If the driver refuses the interval, fall back to the
		wall-clock cap.												*/
	{
		int interval = 1;
#ifdef USE_SDL2
		SDL_DisplayMode mode;
		if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode) == 0 && mode.refresh_rate > 0)
		{
			interval = (mode.refresh_rate + 30) / 60;	// nearest multiple of 60Hz
			if (interval < 1) interval = 1;
			if (interval > 4) interval = 4;
		}
#endif
		bVsyncPacing = (SDL_GL_SetSwapInterval(interval) == 0);
		if (!bVsyncPacing && interval != 1)
			bVsyncPacing = (SDL_GL_SetSwapInterval(1) == 0);
		if (!bVsyncPacing)
			SDL_GL_SetSwapInterval(0);
	}
#endif
	{
		// icon...
		int x,y,n;
		unsigned char *img = stbi_load("Bitmap/icon.png", &x, &y, &n, STBI_rgb_alpha);
		if(img) {
			SDL_Surface *icon = 
			#ifdef USE_SDL2
				SDL_CreateRGBSurfaceWithFormatFrom(img, x, y, 32, x*4, SDL_PIXELFORMAT_RGBA32);
			#else
				SDL_CreateRGBSurfaceFrom(img, x, y, 32, x*4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
			#endif
			if(icon) {
				#ifdef USE_SDL2
				SDL_SetWindowIcon(window, icon);
				SDL_FreeSurface(icon);
				#else
				SDL_WM_SetIcon(icon, NULL);
				#endif
			}
			free(img);
		}
	}
#ifndef USE_SDL2
	screen = SDL_SetVideoMode( screenW, screenH, 32, flags );
    if ( screen == NULL ) {
		// fallback to no MSAA
		GL_MSAA=0;
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0);
		screen = SDL_SetVideoMode( screenW, screenH, 32, flags );
    	if ( screen == NULL ) {
#ifdef PANDORA
			printf("Couldn't set 800x480x16 video mode: %s\n", SDL_GetError());
#else
			printf("Couldn't set %dx%dx32 video mode: %s\n", screenW, screenH, SDL_GetError());
#endif
        	exit(-2);
		}
    } else {
		glEnable(GL_MULTISAMPLE);
	}
	SDL_WM_SetCaption(maintitle, NULL);
	// See the USE_SDL2 branch above - resolve the post-GL-1.1 entry points.
	SCR_LoadGLProcs();
#endif
#ifdef USE_SDL2
	if(flags&SDL_WINDOW_FULLSCREEN || flags&SDL_WINDOW_FULLSCREEN_DESKTOP)
		SDL_ShowCursor(SDL_DISABLE);
	gCustomScale = customScale;
	ApplyViewport();
	screenH = 480;
	screenW = wideScreen?800:640;
#else
	// automatic guess the scale or use custom scale
	float screenScale = 1.;
	if(customScale > 0.0f) {
		// Use custom scale factor, in points, so it matches the requested size
		screenScale = customScale * dpiFactor;
	} else {
		// Automatic scaling based on window size.  Only 480*ScrPresentSquash() of the base
		// space is ever presented (see below), so fit against that, not against 480.
		const double presentH = 480. * ScrPresentSquash();
		if(screenW/640. < screenH/presentH)
			screenScale = screenW/640.;
		else
			screenScale = screenH/presentH;
	}
	// is it a Wide screen ratio?
	// Detect widescreen if width is significantly wider than 4:3 aspect ratio.
	// Measured against the PAL presentation (the constant, not ScrPresentSquash) so that
	// the display-aspect choice changes only presentation, never the 2D layout.
	{
		const double palPresentH = 480. * SCR_PRESENT_SQUASH;
		const double palScale = (screenW/640. < screenH/palPresentH)
			? screenW/640. : screenH/palPresentH;
		if((screenW/palScale - 640)>=80)
			wideScreen=1;
	}
	screenX = (screenW-(wideScreen?800.:640.)*screenScale)/2.;
	screenY = (screenH-480.*screenScale)/2.;
	screenW = (wideScreen?800:640)*screenScale;
	screenH = 480*screenScale;
	printf("Display mode: %s, Scale: %.2f, Resolution: %dx%d (DPI factor %.2f)\n",
		   wideScreen ? "Widescreen" : "Standard", screenScale, screenW, screenH, dpiFactor);
	if(flags&SDL_FULLSCREEN)
		SDL_ShowCursor(SDL_DISABLE);
	// The 640x480 base holds the Amiga's 320x200 at (2.0, 2.4), i.e. 1.2x taller than wide.
	// Undo that here, once, for the whole raster - geometry and 2D art alike - and replace it
	// with the display's own pixel aspect, exactly as the monitor did on real hardware.
	// See ScrPresentSquash() / AMIGA_PAL_PIXEL_ASPECT in 3D_Engine.h for the derivation.
	long viewH = static_cast<long>(screenH * ScrPresentSquash() + 0.5f);
	long viewY = screenY + (screenH - viewH) / 2;
	glViewport(screenX, viewY, screenW, viewH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	screenH = 480;
	screenW = wideScreen?800:640;
	glOrtho(0, screenW, screenH, 0, 0, FURTHEST_Z);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
#endif
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	SetSceneProjection( &pd3dDevice );

	glEnable(GL_DEPTH_TEST);
	glAlphaFunc(GL_NOTEQUAL, 0);
//	glEnable(GL_ALPHA_TEST);
//	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	// Disable texture mapping by default (only DrawTrack() enables it)
	pd3dDevice.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );

	sound_init();


	CreateFonts();
	LoadTextures();

	// Bring up the Amiga menus, starting at the name entry screen
	MenuScreensInit();

	if (!InitialiseData()) {
		printf("Error initialising data\n");
		exit(-3);
	}

	CreateBuffers(&pd3dDevice);

	DSInit();
	DSSetMode();

	glClearColor(0,0,0,1);
#ifdef __EMSCRIPTEN__
	fLastTime = DXUTGetTime();
	emscripten_set_main_loop(em_main_loop, 0, 1);
#else
	bool run = true;
	double fLastTime = DXUTGetTime();
    while( run ) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		double fTime = DXUTGetTime();
		run = process_events();
		OnFrameMove( &pd3dDevice, fTime, fTime - fLastTime, NULL );
        OnFrameRender( &pd3dDevice, fTime, fTime - fLastTime, NULL );
#ifdef USE_SDL2
		SDL_GL_SwapWindow(window);
#else
		SDL_GL_SwapBuffers();
#endif

		// Determinism trace: stop as soon as the requested steps are in the log,
		// and never throttle - the trace clock is fixed, so running flat out just
		// finishes sooner without changing a single number.
		if (scr::gSimTraceEnabled)
		{
			if (scr::SimTrace_Finished())
			{
				scr::SimTrace_End();
				run = false;
			}
			fLastTime = fTime;
			continue;
		}

		// Vsync is pacing the loop, so there is nothing to wait for: sleeping on top of
		// it would only push the next swap past a refresh and drop a frame.
		if (bVsyncPacing)
		{
			fLastTime = fTime;
			continue;
		}

		// Cap the render rate. 50Hz matches the Amiga's PAL vsync, but the
		// FloatV2 physics can step faster than that, and rendering slower than
		// the physics just throws those steps away — so follow it when it is
		// running above 50Hz.
		double renderStep = 1.0/50.0;
		if (scr::gUseFloatV2Physics && scr::gFloatV2Dt < renderStep)
			renderStep = scr::gFloatV2Dt;

		// Sleep until this frame's deadline. (fTime-fLastTime) is the *previous*
		// frame's start-to-start time, which the previous sleep had already padded
		// out to renderStep - so subtracting it left roughly nothing to wait for,
		// this frame ran short, and the frame after it over-slept to compensate.
		// That alternation is what the physics accumulator sees as 0 steps one
		// frame and 2 the next, and it shows up as judder in anything moving
		// across the screen rather than with the camera.
		int32_t timetowait = static_cast<int32_t>((fTime + renderStep - DXUTGetTime())*1000);
		if (timetowait>0)
			SDL_Delay(timetowait);

		fLastTime = fTime;
    }
#endif
	FreeData();

	CloseFonts();

	sound_destroy();
	TTF_Quit();
	SDL_Quit();
	
	exit(0);
}
#endif
