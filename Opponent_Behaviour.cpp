/**************************************************************************

  Opponent Behaviour.cpp - Functions relating to opponents's car behaviour

 **************************************************************************/


/*	============= */
/*	Include files */
/*	============= */
#include "dxstdafx.h"

#include <stdlib.h>
#include <stdio.h>

#include "StuntCarRacer.h"
#include "Profile.h"	// ProfileDataPath - where the tuning offsets are kept
#include "Opponent_Behaviour.h"
#include "Car_Behaviour.h"
#include "Track.h"
#include "Car.h"		// visible car dimensions, which the shadow is built to match
#include "3D_Engine.h"
#include "Physics_FloatV2.h"
#include "Det_Rand.h"

/*	===== */
/*	Debug */
/*	===== */
//#define	TEST_AMIGA_RWP
//#define	TEST_AMIGA_OPI
//#define	TEST_AMIGA_MOTOS
//#define	TEST_AMIGA_ROS

#define	OPPONENT_SHADOW
//#define USE_OPP_CENTRE_POS
//#define CALC_FRONT_X_Z_FROM_SCRATCH

#if defined(DEBUG) || defined(_DEBUG)
extern FILE *out;
extern bool bTestKey;
#endif

/*	========= */
/*	Constants */
/*	========= */
#ifdef SCR_PORTABLE
#undef FALSE
#undef TRUE
#endif
#define	FALSE	0
#define	TRUE	1

#define	NUM_OPPONENTS	(11)
#define	NUM_X_SPANS		(32)

#define LOCAL_Y_FACTOR	4

typedef enum
	{
	REAR_LEFT = 0,
	REAR_RIGHT,
	FRONT,
	NUM_OPP_WHEEL_POSITIONS
	} OppWheelPositionType;

/*	=========== */
/*	Global data */
/*	=========== */
long opponentsID = NO_OPPONENT;	// 0 to 10, or NO_OPPONENT for a solo practise run
static long gRaceOpponent = RANDOM_OPPONENT;	// what the next race asked for
long opponents_current_piece = 0;	// use as opponents_road_section

bool player_close_to_opponent = FALSE;
bool opponent_behind_player = FALSE;

extern bool bSuperLeague;
extern unsigned char sections_car_can_be_put_on[]; 				// both array are used for opponents speed values computation
extern char Piece_Angle_And_Template[MAX_PIECES_PER_TRACK];

// SEB: The opponents_speed_values, that is pre-computed, is not used anymore and Opponents_Speed_Value function is used now
// Values for each piece of each track (Global because MoveDrawBridge() modifies the Draw Bridge values)
// NOTE: These are for the Standard league.  Super league values are different
unsigned char opponents_speed_values[NUM_TRACKS][MAX_PIECES_PER_TRACK] =
{
	{
	/* Little Ramp data */
	0x76,0x6c,0x62,0x58,0x7a,0x7a,0x70,0x66,0x5c,0x52,0x48,0x48,0x48,0x7a,0x7a,0x7a,
	0x7a,0x7a,0x7a,0x7a,0x70,0x66,0x5c,0x52,0x48,0x48,0x48,0x48,0x78,0x6e,0x64,0x5a,
	0x50,0x46,0x7a,0x70,0x66,0x5c,0x52,0x48,0x48,0x48,0x48,0x7c
	},
	{
	/* Stepping Stones data */
	0xf2,0xe8,0xde,0xd4,0x67,0x5d,0x53,0x49,0x3f,0x4b,0x41,0x41,0xc1,0xd2,0xc8,0xbe,
	0xc7,0xbd,0xc5,0xbb,0xc4,0xba,0x55,0x4b,0x41,0x41,0x41,0x60,0x56,0x4c,0x42,0x7d,
	0x7d,0x73,0x69,0x5f,0x55,0x4b,0x41,0x41,0x41,0xfd,0xfd,0xfd,0xf3,0x7d,0x7d,0x73,
	0x69,0x5f,0x55,0x4b,0x41,0x41,0x41,0x7c
	},
	{
	/* Hump Back data */
	0x52,0x4d,0x77,0x77,0x77,0x6d,0x63,0x59,0x4f,0x45,0x45,0x45,0x77,0x77,0x77,0x77,
	0x77,0x77,0x77,0x6d,0x63,0x59,0x4f,0x45,0x45,0x45,0x56,0x4c,0x77,0x77,0x6d,0x63,
	0x59,0x4f,0x45,0x45,0x45,0x4f,0x61,0x57,0x4d,0x45,0x4f,0x45,0x45,0x63,0x59,0x4f,
	0x45,0x45,0x45,0x66,0x5c
	},
	{
	/* Big Ramp data */
	0x7a,0x7a,0x7a,0x7a,0x7a,0x7a,0x70,0x66,0x5c,0x52,0x48,0x48,0x48,0x58,0x4e,0x4b,
	0x69,0x5f,0x55,0x4b,0x46,0x66,0x5c,0x52,0x48,0x48,0x48,0x48,0x7e,0xf4,0xea,0xe0,
	0xd6,0x7a,0x7a,0x70,0x66,0x5c,0x52,0x48,0x48,0x48,0x48,0x7c
	},
	{
	/* Ski Jump data */
	0x42,0xec,0xe2,0xd8,0x77,0x77,0x77,0x6d,0x63,0x59,0x4f,0x4f,0x4f,0x4f,0x63,0x59,
	0x4f,0x4f,0x72,0x68,0x5e,0x54,0x4a,0x40,0x36,0x4f,0x4f,0x4f,0x6a,0xe0,0xd6,0xcc,
	0xc2,0x63,0x59,0x4f,0xcf,0xcf,0xcf,0xc9,0x56,0x56,0x56,0x7e,0x7e,0x7e,0x7e,0x7e,
	0x7e,0x74,0x6a,0x60,0x56,0x56,0x56,0x56,0x56,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x74,
	0x6a,0x60,0x56,0x56,0x56,0x7e,0x7e,0x74,0x6a,0x60,0x56,0x56,0x5a,0x52
	},
	{
	/* Draw Bridge data */
	0x76,0x76,0x6c,0x62,0x69,0x5f,0x55,0x50,0x58,0x58,0x58,0x76,0x76,0x76,0x6c,0x62,
	0x58,0x58,0x58,0x4d,0x43,0x76,0x76,0x76,0x76,0x76,0x6c,0x62,0x58,0x58,0x58,0x58,
	0x58,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0xf8,0xee,0xe4,0x5a,0x50,0xc6,
	0x76,0x76,0x76,0xbb,0xbb,0x76,0x76,0x76,0x6c,0x62,0xd8,0xd8,0xd8,0xe4,0xf6,0xec,
	0xe2,0xd8,0x76,0x76,0x76,0x76,0x6c,0x62,0x58,0x58,0x58,0x58,0x58,0x7c
	},
	{
	/* High Jump data */
	0xe7,0xdd,0xd3,0x77,0x77,0x77,0x77,0x6d,0x63,0x59,0x4f,0x4f,0x4f,0x7a,0x7a,0x7a,
	0x7a,0x7a,0x70,0x66,0x5c,0x52,0x52,0x55,0x59,0x4f,0x4f,0x77,0x77,0x77,0x77,0x6d,
	0x63,0x59,0x4f,0x4f,0xcf,0xe7,0xdd,0xd3,0xce,0x77,0x77,0x77,0x77,0x6d,0x63,0x59,
	0x4f,0x4f,0x4f,0x7c,0x41,0x41,0x41,0x7c
	},
	{
	/* Roller Coaster data */
	0x66,0x5c,0x52,0x48,0x3e,0x34,0x2a,0x29,0x6a,0x60,0x56,0x56,0x56,0x40,0x36,0x7e,
	0x7e,0x7e,0x7e,0x7e,0x7e,0x74,0x6a,0x60,0x56,0x56,0x54,0x4a,0x7e,0x7e,0x7e,0x7e,
	0x7e,0x7e,0x7e,0x74,0x6a,0x60,0x56,0x56,0x56,0x56,0x56,0x7e,0x7e,0x7e,0x7e,0x7e,
	0x7e,0x74,0x6a,0x60,0x56,0x56,0x56,0x56,0x56,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x74,
	0x6a,0x60,0x56,0x56,0x56,0x7e,0x7e,0x74,0x6a,0x60,0x56,0x56,0x5a,0x52
	}
};

WCHAR *opponentNames[NUM_OPPONENTS] =
{
	L"Hot Rod     ",
	L"Whizz Kid   ",
	L"Bad Guy     ",
	L"The Dodger  ",
	L"Big Ed      ",
	L"Max Boost   ",
	L"Dare Devil  ",
	L"High Flyer  ",
	L"Bully Boy   ",
	L"Jumping Jack",
	L"Road Hog    "
};

extern IDirectSoundBuffer8 *HitCarSoundBuffer;

/*	=========== */
/*	Static data */
/*	=========== */
// Opponent attributes
#define OBSTRUCTS_PLAYER	2
#define	WHEELIE		4
#define	DRIVES_NEAR_EDGE	8
#define UNUSED4	16
#define PUSH_PLAYER	32
#define UNUSED6	64

static unsigned char opponent_attributes[NUM_OPPONENTS] =
{
// Hot Rod
PUSH_PLAYER|OBSTRUCTS_PLAYER,
// Whizz Kid
PUSH_PLAYER,
// Bad Guy
UNUSED6|PUSH_PLAYER|OBSTRUCTS_PLAYER,
// The Dodger
PUSH_PLAYER,
// Big Ed
PUSH_PLAYER|UNUSED4|DRIVES_NEAR_EDGE|WHEELIE|OBSTRUCTS_PLAYER,
// Max Boost
WHEELIE,
// Dare Devil
PUSH_PLAYER|UNUSED4,
// High Flyer
UNUSED4|WHEELIE,
// Bully Boy
UNUSED6|DRIVES_NEAR_EDGE|OBSTRUCTS_PLAYER,
// Jumping Jack
UNUSED4,
// Road Hog
DRIVES_NEAR_EDGE
};

// Values for each track
static unsigned char opp_track_speed_values[] =	//DAT.1fe2c
{
	// Standard league
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x4f,0x3a,0x3e,0x41,0x48,0x51,0x48,0x4f,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	// used when creating opponents.speed.values
	0x48,0x41,0x45,0x48,0x4f,0x58,0x4f,0x56,	// used when creating opponents.speed.values

	// Super league
	0x07,0x03,0x03,0x03,0x03,0x03,0x07,0x03,
	0x66,0x57,0x57,0x59,0x59,0x69,0x62,0x64,
	0x07,0x03,0x03,0x03,0x03,0x01,0x03,0x03,	// used when creating opponents.speed.values
	0x61,0x55,0x53,0x56,0x58,0x5b,0x5a,0x62		// used when creating opponents.speed.values
};

/*	The two groups above that this port lets a tester move: the base a track's speeds are	*/
/*	built up from, once for the max speed the opponent will hold and once for the target	*/
/*	speed of an individual piece.  The random masks either side of them are left alone -	*/
/*	they are what makes two races on the same track differ, and widening them would change	*/
/*	the character of the driving rather than its pace.										*/
#define OPP_SPEED_GROUP_MAX_SPEED	8
#define OPP_SPEED_GROUP_PER_PIECE	24

/*	Tuning offsets, [league][track], applied to both bases.  Kept in their own file next		*/
/*	to the profile rather than inside it: finding a number takes more than one sitting, so	*/
/*	the offsets have to outlive the run that found them, but they are a developer's			*/
/*	workings and have no business in a player's career file - and they should survive a		*/
/*	new driver, which wipes that file.  What does not change is that any offset at all		*/
/*	stops times being recorded, so a tuned session still cannot set a record.				*/
static signed char gOppSpeedTuning[2][NUM_TRACKS] = { { 0 }, { 0 } };

#define OPP_TUNING_FILE		"tuning.txt"
#define OPP_TUNING_VERSION	1

/*	Bumped on every edit, so Opponent_Speed_Value's one-entry cache cannot answer from		*/
/*	before a change - the tuning screen is reached between races, exactly when the piece		*/
/*	and track it caches on are unchanged.													*/
static long gOppSpeedTuningEdits = 0;

/*	Set while the file is being read back, so the writes that reading does are not each		*/
/*	written straight out again over the file still being read.								*/
static bool gOppSpeedTuningLoading = false;

/*	Write the offsets out, or remove the file once they are all back to zero - a reset		*/
/*	should leave nothing behind to be loaded next time.  Silent on failure, as the profile	*/
/*	is: a bench that cannot save is still a bench.											*/
static void OpponentTuningSave( void )
	{
	if (gOppSpeedTuningLoading)
		return;

	const char *path = ProfileDataPath(OPP_TUNING_FILE);
	if (path == NULL)
		return;

	if (!OpponentTuningActive())
		{
		remove(path);
		return;
		}

	FILE *f = fopen(path, "w");
	if (f == NULL)
		return;

	fprintf(f, "StuntCarRacerTuning %d\n", OPP_TUNING_VERSION);

	/*	Only the tracks that have been moved, so the file reads as a list of what was	*/
	/*	changed rather than a table of mostly zeroes.									*/
	for (int league = 0; league < 2; league++)
		for (int track = 0; track < NUM_TRACKS; track++)
			if (gOppSpeedTuning[league][track] != 0)
				fprintf(f, "tuning %d %d %d\n", league, track,
						(int)gOppSpeedTuning[league][track]);

	fclose(f);
	}

void OpponentTuningLoad( void )
	{
	gOppSpeedTuningLoading = true;

	/*	Start from stock, so a second call cannot add to what the first one read.		*/
	OpponentTuningClear();

	const char *path = ProfileDataPath(OPP_TUNING_FILE);
	FILE *f = (path != NULL) ? fopen(path, "r") : NULL;

	/*	Nothing saved is the normal case: stock speeds, and no file to write until		*/
	/*	something is actually tuned.													*/
	if (f != NULL)
		{
		char line[256];
		int  version = 0;

		if ((fgets(line, sizeof(line), f) != NULL) &&
			(sscanf(line, "StuntCarRacerTuning %d", &version) == 1) &&
			(version >= 1) && (version <= OPP_TUNING_VERSION))
			{
			while (fgets(line, sizeof(line), f) != NULL)
				{
				int league = 0, track = 0, offset = 0;
				if (sscanf(line, "tuning %d %d %d", &league, &track, &offset) != 3)
					continue;	// a line from a build that knows more than this one

				if ((league < 0) || (league > 1))
					continue;

				/*	Through OpponentTuningSet for the range clamp, so a hand-edited	*/
				/*	file cannot put a base somewhere the table cannot hold.			*/
				OpponentTuningSet(track, league != 0, offset);
				}
			}

		fclose(f);
		}

	gOppSpeedTuningLoading = false;
	}

long OpponentTuningGet( long trackID, bool superLeague )
	{
	if ((trackID < 0) || (trackID >= NUM_TRACKS))
		return 0;
	return gOppSpeedTuning[superLeague ? 1 : 0][trackID];
	}

void OpponentTuningSet( long trackID, bool superLeague, long offset )
	{
	if ((trackID < 0) || (trackID >= NUM_TRACKS))
		return;

	if (offset < OPPONENT_TUNING_MIN) offset = OPPONENT_TUNING_MIN;
	if (offset > OPPONENT_TUNING_MAX) offset = OPPONENT_TUNING_MAX;

	gOppSpeedTuning[superLeague ? 1 : 0][trackID] = (signed char)offset;
	gOppSpeedTuningEdits++;

	/*	Written out on every edit rather than on the way out of the game: the bench is	*/
	/*	used by racing, and a race is left by whatever route the tester feels like,		*/
	/*	including closing the window.  One small file per keypress is nothing.			*/
	OpponentTuningSave();
	}

void OpponentTuningClear( void )
	{
	for (int league = 0; league < 2; league++)
		for (int track = 0; track < NUM_TRACKS; track++)
			gOppSpeedTuning[league][track] = 0;

	gOppSpeedTuningEdits++;
	OpponentTuningSave();		// removes the file - a reset leaves nothing behind
	}

bool OpponentTuningActive( void )
	{
	for (int league = 0; league < 2; league++)
		for (int track = 0; track < NUM_TRACKS; track++)
			if (gOppSpeedTuning[league][track] != 0)
				return TRUE;
	return FALSE;
	}

/*	A track's base speed for one of the two groups, with the tuning offset folded in and		*/
/*	held inside the byte the table stores - the values are used as unsigned 7-bit speeds,	*/
/*	and a base that wrapped would make the opponent crawl rather than slow down.				*/
long OpponentTuningBase( long trackID, long group, bool superLeague )
	{
	if ((trackID < 0) || (trackID >= NUM_TRACKS))
		trackID = 0;

	long value = static_cast<long>(opp_track_speed_values[trackID + group + (superLeague ? 32 : 0)]);
	value += OpponentTuningGet(trackID, superLeague);

	if (value < 0)     value = 0;
	if (value > 0x7f)  value = 0x7f;
	return value;
	}

static long opponents_distance_into_section;
static long opponents_road_x_position;

// Three co-ordinates needed for opponent behaviour (as per original Amiga StuntCarRacer)
static COORD_3D opp_rear_left_road_pos;
static COORD_3D opp_rear_right_road_pos;
static long opp_front_road_pos_y;	//X,Z not needed

// Additional co-ordinates needed for PC StuntCarRacer (for calculating opponent orientation)
static COORD_3D opp_front_left_road_pos;
static COORD_3D opp_front_right_road_pos;
#ifdef USE_OPP_CENTRE_POS
COORD_XZ opp_centre_road_pos;
#endif

static COORD_3D opp_shadow_rear_left;
static COORD_3D opp_shadow_rear_right;
static COORD_3D opp_shadow_front_left;
static COORD_3D opp_shadow_front_right;

// wheel heights
static long opp_actual_height[NUM_OPP_WHEEL_POSITIONS];

static long opp_smallest_difference;

static long opp_old_rear_left_difference;
static long opp_old_rear_right_difference;
static long opp_old_front_difference;

static long opp_new_rear_left_difference;
static long opp_new_rear_right_difference;
static long opp_new_front_difference;

static long opp_touching_road;

static long opp_y_acceleration[NUM_OPP_WHEEL_POSITIONS];
static long opp_y_speed[NUM_OPP_WHEEL_POSITIONS];

long opp_engine_power = 236;		// (236 standard, 314 super)
static long opponents_engine_z_acceleration;
static long opponents_max_speed;
static long opponents_z_speed;
static bool opponents_required_z_speed_reached;


/*	===================== */
/*	Function declarations */
/*	===================== */
static void ResetOpponent (void);
static void CalculateOpponentsRoadWheelPositions( void );
static void GetSurfaceCoords( long piece, long segment );
static long CalcSurfacePosition( long *next_segment, long distance, long z_shift );
static void CalculateOpponentsRoadWheelHeight( long sx, long sz, long *y_out );
static void OpponentMovement( void );

static void UpdateOpponentsActualWheelHeights( void );
static void CalculateWheelDifference( long road_height,
									  long actual_height,
									  long height_adjust,
									  long *old_difference_in_out,
									  long *new_difference_out,
									  long *touching_road);
static long LimitOpponentWheels( long max_difference, long wheel1, long wheel2 );
static void AverageWheelYSpeeds( long wheel1, long wheel2 );

static void RandomizeOpponentsSteering( void );

static void GetOpponentsEngineAcceleration( void );
static void AdjustOpponentsEngineAcceleration( void );
static void UpdateOpponentsZSpeed( void );

static void CalculateDistancesBetweenPlayers( void );

static void OpponentPlayerInteraction( bool applySteering = true );
static void SteerTowardSuggested( void );
static void MoveOpponentToOneSide( void );
static void OpponentPushPlayer( void );

// FloatV2 opponent step (OpponentStepF in PhysicsFloatV2.cs). Defined at the
// bottom of this file, next to the legacy functions it drives.
static void OpponentStepFloatV2( double dt );

extern long fourteen_frames_elapsed;	// Car_Behaviour.cpp


/*	======================================================================================= */
/*	Function:		ResetOpponent															*/
/*																							*/
/*	Description:	Reset all opponent behaviour variables to their initial state			*/
/*	======================================================================================= */

/*	A league race is against the driver the fixture named.  Only the dev track menu,		*/
/*	which has no fixture behind it, still draws one at random - which is what this port		*/
/*	used to do for every race.																*/

static void ChooseOpponent (void)
	{
	if (gRaceOpponent == NO_OPPONENT)
		opponentsID = NO_OPPONENT;
	else if ((gRaceOpponent >= 0) && (gRaceOpponent < NUM_OPPONENTS))
		opponentsID = gRaceOpponent;
	else
		opponentsID = SCR_Rand() % NUM_OPPONENTS;
//	opponentsID = 9;	// Jumping Jack
	}

static void ResetOpponent (void)
	{
	ChooseOpponent();

	opp_old_rear_left_difference = 0;
	opp_old_rear_right_difference = 0;
	opp_old_front_difference = 0;

	for (long i = 0; i < NUM_OPP_WHEEL_POSITIONS; i++)
	{
		opp_y_speed[i] = 0;
	}

	opponents_z_speed = 0;
	opponents_required_z_speed_reached = FALSE;

	player_close_to_opponent = FALSE;
	opponent_behind_player = FALSE;

	scr::gFloatV2OpponentNeedsSeed = true;	// re-seed the FloatV2 sub-state
	return;
	}

/*	======================================================================================= */
/*	Function:		OpponentBehaviour														*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

extern bool bNewGame;
extern long TrackID;
extern TRACK_PIECE Track[MAX_PIECES_PER_TRACK];
extern long Track_Map[NUM_TRACK_CUBES][NUM_TRACK_CUBES];	// [x][z]
extern long NumTrackPieces;
extern long PlayersStartPiece;
extern bool drop_start_done;

//#define USE_ROAD_Y
#define NEW_OPP_METHOD
// current surface co-ords
static long sx1, sy1, sz1, sx2, sy2, sz2, sx3, sy3, sz3, sx4, sy4, sz4;

// ---------------------------------------------------------------------------
// Continuous mirrors of the opponent's road/wheel positions.
//
// The gameplay state stays in Amiga integer units (OppFloatV2Sync rounds the
// doubles back into it every step), which is what the AI, the collision code
// and the TEST_AMIGA_RWP comparisons need. But rebuilding the *visuals* from
// those integers makes the opponent stutter wherever height is steeply coupled
// to lateral position -- i.e. on banked corners, where a one-unit rounding of
// the road x position moves the wheel heights by several units and the roll
// angle is an atan2 of that integer difference. Straights and airborne stretches
// are insensitive to the same rounding, which is why they look fine.
//
// So the FloatV2 opponent additionally derives a double-precision copy of the
// wheel positions straight from gOppF, and the renderer uses that. Display only:
// nothing here feeds back into the simulation.
struct COORD_3D_F { double x, y, z; };

static COORD_3D_F oppf_rear_left_road_pos;
static COORD_3D_F oppf_rear_right_road_pos;
static COORD_3D_F oppf_front_left_road_pos;
static COORD_3D_F oppf_front_right_road_pos;
static double oppf_front_road_pos_y;
static double oppf_act[NUM_OPP_WHEEL_POSITIONS];

static void CalculateOpponentsRoadWheelPositionsF( void );
static void ComputeOpponentRenderStateF( long *x, long *y, long *z,
										 float *x_angle, float *y_angle, float *z_angle );

void OpponentBehaviour (long *x,
						long *y,
						long *z,
						float *x_angle,
						float *y_angle,
						float *z_angle,
						bool bOpponentPaused)
{
	long opponent_x, opponent_y, opponent_z;
	float opponent_x_angle = 0.0f, opponent_y_angle = 0.0f, opponent_z_angle = 0.0f;

	// reset opponent
	if (bNewGame)
		{
		ResetOpponent();

		opponents_current_piece = PlayersStartPiece;
		opponents_distance_into_section = 0x400;	// half way into section
		opponents_road_x_position = 0x4c;
//temp		opponents_road_x_position = 0x1c;
//temp		opponents_road_x_position = 0xe4;

		// initialise.opponent.data
		CalculateOpponentsRoadWheelPositions();
		// Position the opponent a random amount above the road
		int r = SCR_Rand();
		r &= 0x7f;
		r += 0x68;
		opp_actual_height[REAR_LEFT] = opp_rear_left_road_pos.y + r;
		opp_actual_height[REAR_RIGHT] = opp_rear_right_road_pos.y + r;
		opp_actual_height[FRONT] = opp_front_road_pos_y + r;
		// end initialise.opponent.data

		// Set opponent_max_speed
		long s = static_cast<long>(SCR_Rand()) & static_cast<long>(opp_track_speed_values[TrackID+(bSuperLeague?32:0)]);
		s += OpponentTuningBase(TrackID, OPP_SPEED_GROUP_MAX_SPEED, bSuperLeague);
		opponents_max_speed = s;
//temp		opponents_max_speed = 10;

		bNewGame = FALSE;
		}


	CalculatePlayersRoadPosition();
	if (!bOpponentPaused)
	{
		if (scr::gUseFloatV2Physics && scr::gUseFloatV2Opponent)
		{
			// Timestep-parameterised opponent. Called once per *player* physics
			// step rather than once per frameGap tick, so the opponent moves as
			// smoothly as the player does.
			OpponentStepFloatV2(scr::gFloatV2Dt);
		}
		else
		{
			scr::gFloatV2OpponentNeedsSeed = true;
			// Only the FloatV2 step maintains this; make sure it cannot be
			// left latched on if the opponent path is toggled mid-race.
			fourteen_frames_elapsed = 0;
			OpponentMovement();
			CalculateDistancesBetweenPlayers();
			OpponentPlayerInteraction();
		}
	}
	else
		CalculateDistancesBetweenPlayers();

	CalculateOpponentsRoadWheelPositions();

	// Once the FloatV2 opponent is running (and seeded), take the render state
	// from the continuous mirror instead of the integer globals -- see the note
	// on COORD_3D_F above. The integer positions computed just now are still
	// what the AI and collision code read next step.
	if (scr::gUseFloatV2Physics && scr::gUseFloatV2Opponent &&
		drop_start_done && !scr::gFloatV2OpponentNeedsSeed)
	{
		CalculateOpponentsRoadWheelPositionsF();
		ComputeOpponentRenderStateF(x, y, z, x_angle, y_angle, z_angle);
		return;
	}

	//
	// Calculate opponent's new centre point ...
	//

	/*
	 * Calculate opponent's x position
	 */
	opponent_x = (opp_front_left_road_pos.x + opp_front_right_road_pos.x + opp_rear_left_road_pos.x + opp_rear_right_road_pos.x) / 4;
	opponent_x <<= LOG_PRECISION;

#ifdef USE_OPP_CENTRE_POS
//	VALUE1 = (bTestKey ? 1 : 0);
	VALUE2 = VALUE3 = 0;
	if (bTestKey)
	{
		VALUE2 = opponent_x;
		opponent_x = opp_centre_road_pos.x;
		opponent_x <<= LOG_PRECISION;
		VALUE3 = opponent_x;
	}
#endif

	/*
	 * Calculate opponent's y position
	 */
#ifdef NEW_OPP_METHOD
	long vis_rear_left_y, vis_rear_right_y, vis_front_y;
	vis_rear_left_y = opp_rear_left_road_pos.y > opp_actual_height[REAR_LEFT] ? opp_rear_left_road_pos.y : opp_actual_height[REAR_LEFT];
	vis_rear_right_y = opp_rear_right_road_pos.y > opp_actual_height[REAR_RIGHT] ? opp_rear_right_road_pos.y : opp_actual_height[REAR_RIGHT];
	vis_front_y = opp_front_road_pos_y > opp_actual_height[FRONT] ? opp_front_road_pos_y : opp_actual_height[FRONT];
	long rear_y = (vis_rear_left_y + vis_rear_right_y) / 2;
	opponent_y = (rear_y + vis_front_y) / 2;
#else
	long road_y = (opp_rear_left_road_pos.y + opp_rear_right_road_pos.y) / 2;
	road_y = (road_y + opp_front_road_pos_y) / 2;
	#ifdef USE_ROAD_Y
	long rear_y = (opp_rear_left_road_pos.y + opp_rear_right_road_pos.y) / 2;
	opponent_y = (rear_y + opp_front_road_pos_y) / 2;
	#else
	long rear_y = (opp_actual_height[REAR_LEFT] + opp_actual_height[REAR_RIGHT]) / 2;
	opponent_y = (rear_y + opp_actual_height[FRONT]) / 2;
	#endif
	if (opponent_y < road_y) opponent_y = road_y;
#endif

	// Raise the opponent slightly (to stop them sinking into road due to inaccurate heights)
	opponent_y += 20;
	opponent_y <<= (LOG_PRECISION-3);

	/*
	 * Calculate opponent's z position
	 */
	opponent_z = (opp_front_left_road_pos.z + opp_front_right_road_pos.z + opp_rear_left_road_pos.z + opp_rear_right_road_pos.z) / 4;
	opponent_z <<= LOG_PRECISION;

#ifdef USE_OPP_CENTRE_POS
	if (bTestKey)
	{
		opponent_z = opp_centre_road_pos.z;
		opponent_z <<= LOG_PRECISION;
	}
#endif

	//
	// Calculate opponent's new angles
	//

	// Along car's x axis, only use y and z components
#ifdef USE_ROAD_Y
	double yd = static_cast<double>(rear_y - opp_front_road_pos_y) / 2;	// Note y is halved because of unit differences between y and x,z
#else
	#ifdef NEW_OPP_METHOD
	double yd = static_cast<double>(rear_y - vis_front_y) / 2;	// Note y is halved because of unit differences between y and x,zn y and x,z
	#else
	double yd = static_cast<double>(rear_y - opp_actual_height[FRONT]) / 2;	// Note y is halved because of unit differences between y and x,z
	#endif
#endif
	long rear_x = (opp_rear_left_road_pos.x + opp_rear_right_road_pos.x) / 2;
	long rear_z = (opp_rear_left_road_pos.z + opp_rear_right_road_pos.z) / 2;
	long front_x = (opp_front_left_road_pos.x + opp_front_right_road_pos.x) / 2;
	long front_z = (opp_front_left_road_pos.z + opp_front_right_road_pos.z) / 2;
	double xd = static_cast<double>(rear_x - front_x);
	double zd = static_cast<double>(rear_z - front_z);
	double carzd = sqrt((xd*xd) + (zd*zd));
	opponent_x_angle = static_cast<float>(atan2(yd, carzd));

	// Along car's y axis, only use x and z components
	xd = static_cast<double>(opp_rear_left_road_pos.x - opp_rear_right_road_pos.x);
	zd = static_cast<double>(opp_rear_left_road_pos.z - opp_rear_right_road_pos.z);
//	opponent_y_angle = static_cast<float>(atan2(xd, zd)) + D3DX_PI/2;
	opponent_y_angle = static_cast<float>(atan2(zd, -xd));

	// Along car's z axis, only use x and y components
#ifdef USE_ROAD_Y
	yd = static_cast<double>(opp_rear_left_road_pos.y - opp_rear_right_road_pos.y) / 2;	// Note y is halved because of unit differences between y and x,z
#else
	#ifdef NEW_OPP_METHOD
	yd = static_cast<double>(vis_rear_left_y - vis_rear_right_y) / 2;	// Note y is halved because of unit differences between y and x,zn y and x,z
	#else
	yd = static_cast<double>(opp_actual_height[REAR_LEFT] - opp_actual_height[REAR_RIGHT]) / 2;	// Note y is halved because of unit differences between y and x,z
	#endif
#endif
	double carxd = sqrt((xd*xd) + (zd*zd));
	opponent_z_angle = static_cast<float>(atan2(-yd, carxd));

	// output opponent values for use by functions that draw the world
	*x = opponent_x;
	*y = -(opponent_y * LOCAL_Y_FACTOR);
	*z = opponent_z;
	*x_angle = opponent_x_angle;
	*y_angle = opponent_y_angle;
	*z_angle = opponent_z_angle;
}

/*	======================================================================================= */
/*	Function:		CalculateOpponentsRoadWheelPositions									*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

// current surface co-ords
//static long sx1, sy1, sz1, sx2, sy2, sz2, sx3, sy3, sz3, sx4, sy4, sz4;

static long B1bbbe[3] = {0,0,0};	// set by randomize.opponents.steering
									// third value is opponents.random.steering.count

static long opponents_x_spans[NUM_X_SPANS] =
{27,27,27,27,27,26,26,26,25,25,25,24,23,23,22,21,20,19,18,17,15,14,11,9,7,7,7,7,7,7,7,7};

#ifdef OPPONENT_SHADOW
extern void RemoveShadowTriangles( void );
extern void StoreShadowTriangle( D3DXVECTOR3 v1, D3DXVECTOR3 v2, D3DXVECTOR3 v3, long other_colour );
#endif


// All three road heights tested against Amiga
static void CalculateOpponentsRoadWheelPositions( void )
{
long distance, segment, surface_position;
long piece = opponents_current_piece, next_segment;
long left_side_x, left_side_z, right_side_x, right_side_z;
bool draw_shadow = TRUE;


	/*
	 * Rear wheels
	 */
#ifdef	TEST_AMIGA_RWP
	if (GetRecordedAmigaWord(&piece))
		++VALUE2;
	GetRecordedAmigaWord(&opponents_distance_into_section);
	GetRecordedAmigaWord(&opponents_road_x_position);
	GetRecordedAmigaWord(&opp_actual_height[REAR_LEFT]);
	GetRecordedAmigaWord(&opp_actual_height[REAR_RIGHT]);
	GetRecordedAmigaWord(&opp_actual_height[FRONT]);
#endif
	// Rear wheel position
	distance = opponents_distance_into_section - 64;
	if (distance < 0)
	{
		// DIRECTION DEPENDANT

		// go to previous piece
		piece--; if (piece < 0) piece = (NumTrackPieces - 1);

		distance += (Track[piece].numSegments * 256);
	}
#ifdef	TEST_AMIGA_RWP
	CompareRecordedAmigaWord("opponents.road.section.m64", &piece);
	CompareRecordedAmigaWord("opponents.distance.into.section.minus64", &distance);
#endif
	// Fetch 4 surface co-ords surrounding rear wheels (opponents.distance.into.section.minus64 / 256)
	segment = distance >> 8;
	GetSurfaceCoords(piece, segment);
	// Don't draw opponent's shadow on black road segments
	if (Track[piece].roadColour[segment] == SCR_BASE_COLOUR + 0)
		draw_shadow = FALSE;

	// Calculate segment left side x,z at opponents.distance.into.section.minus64
	surface_position = CalcSurfacePosition(&next_segment, distance, B1bbbe[0]);
	if (!next_segment)
	{
		left_side_x = sx2 + ((surface_position * (sx1-sx2)) >> 8);
		left_side_z = sz2 + ((surface_position * (sz1-sz2)) >> 8);
	}
	else
	{
		// Use other end's value as base
		// (Amiga StuntCarRacer does this, but not correct as should really use next segment's values)
		left_side_x = sx1 + ((surface_position * (sx1-sx2)) >> 8);
		left_side_z = sz1 + ((surface_position * (sz1-sz2)) >> 8);
	}

	// Calculate segment right side x,z at opponents.distance.into.section.minus64
	surface_position = CalcSurfacePosition(&next_segment, distance, B1bbbe[2]);
	if (!next_segment)
	{
		right_side_x = sx3 + ((surface_position * (sx4-sx3)) >> 8);
		right_side_z = sz3 + ((surface_position * (sz4-sz3)) >> 8);
	}
	else
	{
		// Use other end's value as base
		// (Amiga StuntCarRacer does this, but not correct as should really use next segment's values)
		right_side_x = sx4 + ((surface_position * (sx4-sx3)) >> 8);
		right_side_z = sz4 + ((surface_position * (sz4-sz3)) >> 8);
	}

	next_segment = FALSE;
	long i = abs(opp_actual_height[REAR_LEFT] - opp_actual_height[REAR_RIGHT]) >> 4;
	// Get half the x distance that the opponent's rear wheels span
	if (i < 0) i = 0;
	if (i >= NUM_X_SPANS) i = NUM_X_SPANS-1;
	long opponents_x_span = opponents_x_spans[i];

	// For calculating the opponent's shadow co-ordinates, the original StuntCarRacer spans are slightly
	// too big, so need to be reduced to take into account the greater width of sloped segments
	long xd = right_side_x - left_side_x;
	long yd = (sy3 - sy2) / LOCAL_Y_FACTOR;
	long zd = right_side_z - left_side_z;
	long base_width = static_cast<long>(sqrt(static_cast<double>((xd*xd) + (zd*zd))));
	long slope_width = static_cast<long>(sqrt(static_cast<double>((base_width*base_width) + (yd*yd))));
	long opponents_shadow_x_span = (opponents_x_span * base_width) / slope_width;
//	if (!bTestKey)
//		opponents_shadow_x_span = opponents_x_span;

	long sx, sz;
	sz = distance & 0xff;	// z position of rear wheels

	// Calculate rear left road co-ordinate (as per Amiga opp.rear.left.road.height)
	sx = opponents_road_x_position - opponents_x_span;	// x position of rear left wheel
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_rear_left_road_pos.y);
	opp_rear_left_road_pos.x = left_side_x + ((sx * xd) >> 8);
	opp_rear_left_road_pos.z = left_side_z + ((sx * zd) >> 8);
#ifdef	TEST_AMIGA_RWP
	CompareRecordedAmigaWord("opp.rear.left.road.height", &opp_rear_left_road_pos.y);
#endif
	// Calculate rear left shadow co-ordinate
	sx = opponents_road_x_position - opponents_shadow_x_span;
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_shadow_rear_left.y);
	opp_shadow_rear_left.x = left_side_x + ((sx * xd) >> 8);
	opp_shadow_rear_left.z = left_side_z + ((sx * zd) >> 8);

	// Calculate rear right road co-ordinate (as per Amiga opp.rear.right.road.height)
	sx = opponents_road_x_position + opponents_x_span;	// x position of rear right wheel
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_rear_right_road_pos.y);
	opp_rear_right_road_pos.x = left_side_x + ((sx * xd) >> 8);
	opp_rear_right_road_pos.z = left_side_z + ((sx * zd) >> 8);
#ifdef	TEST_AMIGA_RWP
	CompareRecordedAmigaWord("opp.rear.right.road.height", &opp_rear_right_road_pos.y);
#endif
	// Calculate rear right shadow co-ordinate
	sx = opponents_road_x_position + opponents_shadow_x_span;
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_shadow_rear_right.y);
	opp_shadow_rear_right.x = left_side_x + ((sx * xd) >> 8);
	opp_shadow_rear_right.z = left_side_z + ((sx * zd) >> 8);


	long piece_x, piece_y, piece_z;
	// Calculate position of piece's bottom front left corner, within world
	piece_x = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_y = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_z = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);
	// Position rear road co-ordinates within world
	opp_rear_left_road_pos.x += piece_x;
	opp_rear_right_road_pos.x += piece_x;
	opp_rear_left_road_pos.z += piece_z;
	opp_rear_right_road_pos.z += piece_z;
	// Position rear shadow co-ordinates within world
	opp_shadow_rear_left.x += piece_x;
	opp_shadow_rear_right.x += piece_x;
	opp_shadow_rear_left.z += piece_z;
	opp_shadow_rear_right.z += piece_z;


//****************


#ifdef USE_OPP_CENTRE_POS
	long cdistance = opponents_distance_into_section;
	// Fetch 4 surface co-ords surrounding centre point
	segment = cdistance >> 8;
	GetSurfaceCoords(opponents_current_piece, segment);

	surface_position = cdistance & 0xff;
	left_side_x = sx2 + ((surface_position * (sx1-sx2)) >> 8);
	left_side_z = sz2 + ((surface_position * (sz1-sz2)) >> 8);

	right_side_x = sx3 + ((surface_position * (sx4-sx3)) >> 8);
	right_side_z = sz3 + ((surface_position * (sz4-sz3)) >> 8);

	sx = opponents_road_x_position & 0xff;	// x position of front wheel

	// Calculate centre road x,z co-ordinates
	opp_centre_road_pos.x = left_side_x + ((sx * (right_side_x-left_side_x)) >> 8);
	opp_centre_road_pos.z = left_side_z + ((sx * (right_side_z-left_side_z)) >> 8);

	// Calculate position of piece's bottom front left corner, within world
	piece_x = Track[opponents_current_piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_z = Track[opponents_current_piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);
	// Position centre road co-ordinates within world
	opp_centre_road_pos.x += piece_x;
	opp_centre_road_pos.z += piece_z;
#endif


//****************


	/*
	 * Front wheels
	 */
	long diff, xdiff, zdiff;

	// Calculate front left and right road x,z co-ordinates
	diff = opp_rear_right_road_pos.x - opp_rear_left_road_pos.x;
	xdiff = diff + (diff >> 1);	// car length is 1.5 times width
	diff = opp_rear_right_road_pos.z - opp_rear_left_road_pos.z;
	zdiff = diff + (diff >> 1);	// car length is 1.5 times width
	opp_front_left_road_pos.x = opp_rear_left_road_pos.x - zdiff;
	opp_front_left_road_pos.z = opp_rear_left_road_pos.z + xdiff;
	opp_front_right_road_pos.x = opp_rear_right_road_pos.x - zdiff;
	opp_front_right_road_pos.z = opp_rear_right_road_pos.z + xdiff;
	/* Don't need to add piece_x,piece_z as already have world position (from rear wheels) */

	// Calculate front left and right shadow x,z co-ordinates
	diff = opp_shadow_rear_right.x - opp_shadow_rear_left.x;
	xdiff = diff + (diff >> 1);	// car length is 1.5 times width
	diff = opp_shadow_rear_right.z - opp_shadow_rear_left.z;
	zdiff = diff + (diff >> 1);	// car length is 1.5 times width
	opp_shadow_front_left.x = opp_shadow_rear_left.x - zdiff;
	opp_shadow_front_left.z = opp_shadow_rear_left.z + xdiff;
	opp_shadow_front_right.x = opp_shadow_rear_right.x - zdiff;
	opp_shadow_front_right.z = opp_shadow_rear_right.z + xdiff;
	/* Don't need to add piece_x,piece_z as already have world position (from rear wheels) */


	// Add 128 to get z of opponent's front
	distance += 128;
	if (distance >= (Track[piece].numSegments * 256))
	{
		// DIRECTION DEPENDANT

		distance -= (Track[piece].numSegments * 256);

		// go to next piece
		piece++; if (piece > (NumTrackPieces - 1)) piece = 0;
	}
	// Fetch 4 surface co-ords surrounding front wheels
	segment = distance >> 8;
	GetSurfaceCoords(piece, segment);
	// Don't draw opponent's shadow on black road segments
	if (Track[piece].roadColour[segment] == SCR_BASE_COLOUR + 0)
		draw_shadow = FALSE;

	/* temporary code
#ifdef	TEST_AMIGA_RWP
	long tsx, tsz, ty1, ty2, ty3, ty4;
	GetRecordedAmigaWord(&tsx);
	GetRecordedAmigaWord(&tsz);
	GetRecordedAmigaWord(&ty1);
	GetRecordedAmigaWord(&ty2);
	GetRecordedAmigaWord(&ty3);
	GetRecordedAmigaWord(&ty4);
#endif
	*/

	sz = distance & 0xff;	// z position of front wheel
	sx = opponents_road_x_position & 0xff;	// x position of front wheel

	// Calculate front road y co-ordinate (as per Amiga opp.front.road.height)
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_front_road_pos_y);
#ifdef	TEST_AMIGA_RWP
	CompareRecordedAmigaWord("opp.front.road.height", &opp_front_road_pos_y);
#endif

	// Calculate front left road y co-ordinate
	sx = opponents_road_x_position - opponents_x_span;	// x position of front left wheel
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_front_left_road_pos.y);

	// Calculate front right road y co-ordinate
	sx = opponents_road_x_position + opponents_x_span;	// x position of front right wheel
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_front_right_road_pos.y);

#ifdef CALC_FRONT_X_Z_FROM_SCRATCH
	VALUE1 = VALUE2 = 0;
	if (bTestKey)
	{
		VALUE1 = opp_front_right_road_pos.x;
		surface_position = sz;
		left_side_x = sx2 + ((surface_position * (sx1-sx2)) >> 8);
		left_side_z = sz2 + ((surface_position * (sz1-sz2)) >> 8);

		right_side_x = sx3 + ((surface_position * (sx4-sx3)) >> 8);
		right_side_z = sz3 + ((surface_position * (sz4-sz3)) >> 8);

		xd = right_side_x - left_side_x;
		zd = right_side_z - left_side_z;

		sx = opponents_road_x_position - opponents_x_span;	// x position of front left wheel
		opp_front_left_road_pos.x = left_side_x + ((sx * xd) >> 8);
		opp_front_left_road_pos.z = left_side_z + ((sx * zd) >> 8);

		sx = opponents_road_x_position + opponents_x_span;	// x position of front right wheel
		opp_front_right_road_pos.x = left_side_x + ((sx * xd) >> 8);
		opp_front_right_road_pos.z = left_side_z + ((sx * zd) >> 8);

		// Calculate position of piece's bottom front left corner, within world
		piece_x = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
		piece_y = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
		piece_z = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);
		// Position front road co-ordinates within world
		opp_front_left_road_pos.x += piece_x;
		opp_front_left_road_pos.z += piece_z;
		opp_front_right_road_pos.x += piece_x;
		opp_front_right_road_pos.z += piece_z;
		VALUE2 = opp_front_right_road_pos.x;
	}
#endif

#ifdef OPPONENT_SHADOW
	// Calculate front left shadow y co-ordinate
	sx = opponents_road_x_position - opponents_shadow_x_span;
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_shadow_front_left.y);

	// Calculate front right shadow y co-ordinate
	sx = opponents_road_x_position + opponents_shadow_x_span;
	CalculateOpponentsRoadWheelHeight(sx, sz, &opp_shadow_front_right.y);

	// Y co-ordinates need to be divided by 4 for display, but they're
	// already /2 because are in Amiga format (i.e. not * PC_FACTOR).
	// Also add 7 to y so that shadow is slightly above road and isn't clipped as much
	D3DXVECTOR3 v1, v2, v3, v4;
	v2 = D3DXVECTOR3( static_cast<float>(opp_shadow_rear_left.x), 7 + static_cast<float>(opp_shadow_rear_left.y)/2, static_cast<float>(opp_shadow_rear_left.z) );
	v3 = D3DXVECTOR3( static_cast<float>(opp_shadow_rear_right.x), 7 + static_cast<float>(opp_shadow_rear_right.y)/2, static_cast<float>(opp_shadow_rear_right.z) );

	v1 = D3DXVECTOR3( static_cast<float>(opp_shadow_front_left.x), 7 + static_cast<float>(opp_shadow_front_left.y)/2, static_cast<float>(opp_shadow_front_left.z) );
	v4 = D3DXVECTOR3( static_cast<float>(opp_shadow_front_right.x), 7 + static_cast<float>(opp_shadow_front_right.y)/2, static_cast<float>(opp_shadow_front_right.z) );

	RemoveShadowTriangles();
	if (draw_shadow)
	{
		StoreShadowTriangle(v2, v1, v3, 0);
		StoreShadowTriangle(v1, v4, v3, 0);
	}
#endif

//	VALUE1 = opp_rear_left_road_pos.y;
//	VALUE2 = opp_front_road_pos_y;
//	VALUE3 = opp_rear_right_road_pos.y;
	return;
}


static void GetSurfaceCoords( long piece, long segment )
{
	if ((segment < 0) || (segment >= Track[piece].numSegments))
	{
		MessageBox(NULL, L"GetSurfaceCoords(opponent) segment out of range", L"Error", MB_OK);
#if defined(DEBUG) || defined(_DEBUG)
		fprintf(out, "GetSurfaceCoords(opponent) piece %d, segment %d, numSegments %d\n", piece, segment, Track[piece].numSegments);
#endif
	}

	sx2 = Track[piece].coords[(segment*4)].x;
	sy2 = Track[piece].coords[(segment*4)].y;
	sz2 = Track[piece].coords[(segment*4)].z;

	sx3 = Track[piece].coords[(segment*4)+1].x;
	sy3 = Track[piece].coords[(segment*4)+1].y;
	sz3 = Track[piece].coords[(segment*4)+1].z;

	segment++;
	sx1 = Track[piece].coords[(segment*4)].x;
	sy1 = Track[piece].coords[(segment*4)].y;
	sz1 = Track[piece].coords[(segment*4)].z;

	sx4 = Track[piece].coords[(segment*4)+1].x;
	sy4 = Track[piece].coords[(segment*4)+1].y;
	sz4 = Track[piece].coords[(segment*4)+1].z;
	return;
}


static long CalcSurfacePosition( long *next_segment, long distance, long z_shift )
{
long surface_position = distance & 0xff;

	*next_segment = FALSE;
	surface_position += z_shift;
	if (surface_position >= 256)
	{
		*next_segment = TRUE;
		surface_position &= 0xff;
	}

	return(surface_position);
}


static void CalculateOpponentsRoadWheelHeight( long sx, long sz, long *y_out )
{
long sya, syb, y;

	// calculate height of surface at x,z position using linear interpolation

	// i.e. calculate y at offset (sx, sz)
	//		given (sx1, sy1, sz1)
	//			  (sx2, sy2, sz2)
	//			  (sx3, sy3, sz3)
	//			  (sx4, sy4, sz4)

	// first do x interpolation
	sya = sy1 + ((sx * (sy4-sy1)) >> 8);
	syb = sy2 + ((sx * (sy3-sy2)) >> 8);

	// now do z interpolation
	y = (syb << 8) + (sz * (sya-syb));

	*y_out = y >> 9;
	return;
}


/*	======================================================================================= */
/*	Function:		OpponentMovement														*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

extern bool drop_start_done;


// Tested against Amiga
static void OpponentMovement( void )
{
static long byte_count = 0;

	if (!drop_start_done)
		return;

	UpdateOpponentsActualWheelHeights();
	/*
	//temp
	opp_actual_height[REAR_LEFT] = opp_rear_left_road_pos.y;
	opp_actual_height[REAR_RIGHT] = opp_rear_right_road_pos.y;
	opp_actual_height[FRONT] = opp_front_road_pos_y;
	*/
	RandomizeOpponentsSteering();
	GetOpponentsEngineAcceleration();
	AdjustOpponentsEngineAcceleration();
#ifdef TEST_AMIGA_AOEA
	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		bool flag = temp & 0x80 ? TRUE : FALSE;
		if (flag != opponents_required_z_speed_reached)
		{
			++VALUE1;	// Count differences
			fprintf(out, "%s different %d %d (VALUE2 %d)\n", "opponents.required.z.speed.reached", temp, opponents_required_z_speed_reached, VALUE2);
			opponents_required_z_speed_reached = flag;	// Use Amiga value when different
		}
	}
	CompareRecordedAmigaWord("opponents.engine.z.acceleration", &opponents_engine_z_acceleration);
#endif
	UpdateOpponentsZSpeed();


#ifdef TEST_AMIGA_OM
	if (GetRecordedAmigaWord(&opponents_z_speed))
		++VALUE2;

	GetRecordedAmigaWord(&opponents_current_piece);
	GetRecordedAmigaWord(&byte_count);
	GetRecordedAmigaWord(&opponents_distance_into_section);
#endif
	// TO DO: Tidy up
	long value = (opponents_z_speed * (Track[opponents_current_piece].lengthReduction << 7)) << 1;
	value >>= 16;
	value *= REDUCTION;
	value >>= 8;
	value <<= 3;
	long byte = value & 0xff;
	value >>= 8;
	byte_count += byte;
	if (byte_count > 0xff)
	{
		++value;
		byte_count &= 0xff;
	}
	opponents_distance_into_section += value;
	

	if (opponents_distance_into_section >= (Track[opponents_current_piece].numSegments * 256))
	{
		// DIRECTION DEPENDANT

		opponents_distance_into_section -= (Track[opponents_current_piece].numSegments * 256);

		// go to next piece
		opponents_current_piece++;
		if (opponents_current_piece > (NumTrackPieces - 1)) opponents_current_piece = 0;
	}

#ifdef TEST_AMIGA_OM
	CompareRecordedAmigaWord("opponents.road.section", &opponents_current_piece);
	CompareRecordedAmigaWord("opponents.distance.into.section", &opponents_distance_into_section);
#endif
}


/*	======================================================================================= */
/*	Function:		UpdateOpponentsActualWheelHeights										*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

// Tested against Amiga
static void UpdateOpponentsActualWheelHeights( void )
{
long height_adjust, touching_road, total_diff, i, acceleration, speed;

	opp_smallest_difference = -32768;

#ifdef	TEST_AMIGA_AWH
	if (GetRecordedAmigaWord(&opponentsID))
		++VALUE2;
	GetRecordedAmigaWord(&opponents_current_piece);
	GetRecordedAmigaWord(&opp_rear_left_road_pos.y);
	GetRecordedAmigaWord(&opp_rear_right_road_pos.y);
	GetRecordedAmigaWord(&opp_front_road_pos_y);

	GetRecordedAmigaWord(&opp_actual_height[REAR_LEFT]);
	GetRecordedAmigaWord(&opp_actual_height[REAR_RIGHT]);
	GetRecordedAmigaWord(&opp_actual_height[FRONT]);

	GetRecordedAmigaWord(&opp_old_rear_left_difference);
	GetRecordedAmigaWord(&opp_old_rear_right_difference);
	GetRecordedAmigaWord(&opp_old_front_difference);
#endif

	if (Track[opponents_current_piece].type & 0x80)	// curve
		height_adjust = 124;	// increase collision when on a curve
	else
		height_adjust = 40;

#ifdef	TEST_AMIGA_AWH
	CompareRecordedAmigaWord("height.adjust", &height_adjust);
#endif

	touching_road = 0;

	CalculateWheelDifference(opp_rear_left_road_pos.y,
							 opp_actual_height[REAR_LEFT],
							 height_adjust,
							 &opp_old_rear_left_difference,
							 &opp_new_rear_left_difference,
							 &touching_road);

	CalculateWheelDifference(opp_rear_right_road_pos.y,
							 opp_actual_height[REAR_RIGHT],
							 height_adjust,
							 &opp_old_rear_right_difference,
							 &opp_new_rear_right_difference,
							 &touching_road);

	CalculateWheelDifference(opp_front_road_pos_y,
							 opp_actual_height[FRONT],
							 height_adjust,
							 &opp_old_front_difference,
							 &opp_new_front_difference,
							 &touching_road);

	if (touching_road)
		opp_touching_road = TRUE;
	else
		opp_touching_road = FALSE;

#ifdef	TEST_AMIGA_AWH
	CompareRecordedAmigaWord("opp.old.rear.left.difference", &opp_old_rear_left_difference);
	CompareRecordedAmigaWord("opp.old.rear.right.difference", &opp_old_rear_right_difference);
	CompareRecordedAmigaWord("opp.old.front.difference", &opp_old_front_difference);

	CompareRecordedAmigaWord("opp.new.rear.left.difference", &opp_new_rear_left_difference);
	CompareRecordedAmigaWord("opp.new.rear.right.difference", &opp_new_rear_right_difference);
	CompareRecordedAmigaWord("opp.new.front.difference", &opp_new_front_difference);

	CompareRecordedAmigaWord("touching.road", &touching_road);
	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		if ((temp && !opp_touching_road) ||
			(!temp && opp_touching_road))
		{
			++VALUE1;	// Count differences
			fprintf(out, "%s different %d %d (VALUE2 %d)\n", "opp.touching.road", temp, opp_touching_road, VALUE2);
			opp_touching_road = temp ? TRUE : FALSE;	// Use Amiga value when different
		}
	}
#endif


	// Make accelerations from 6 parts wheel difference in question and 1 part
	// of the other two wheels (only one central front wheel is considered)
	total_diff = opp_new_rear_left_difference + opp_new_rear_right_difference + opp_new_front_difference;

	opp_y_acceleration[REAR_LEFT] = (total_diff + opp_new_rear_left_difference + (opp_new_rear_left_difference << 2)) >> 3;
	opp_y_acceleration[REAR_RIGHT] = (total_diff + opp_new_rear_right_difference + (opp_new_rear_right_difference << 2)) >> 3;
	opp_y_acceleration[FRONT] = (total_diff + opp_new_front_difference + (opp_new_front_difference << 2)) >> 3;


	// Randomly make opponent do a wheelie (if they have that attribute)
	if (opponent_attributes[opponentsID] & WHEELIE)
		{
		i = opp_y_speed[FRONT] | opp_y_acceleration[FRONT];
		if ((i & 0xfffc) == 0)		// If front of car isn't moving much vertically
			{
			i = SCR_Rand() & 0xf;
			if (i == 0)
				opp_y_speed[FRONT] = 160;	// Make opponent do a wheelie
			}
		}


	// Update rear left wheel y speed and height
	acceleration = ((opp_y_acceleration[REAR_LEFT] * REDUCTION) >> 8);
	opp_y_speed[REAR_LEFT] += acceleration;

	speed = ((opp_y_speed[REAR_LEFT] * REDUCTION) >> 9);
	opp_actual_height[REAR_LEFT] += speed;

	// Update rear right wheel y speed and height
	acceleration = ((opp_y_acceleration[REAR_RIGHT] * REDUCTION) >> 8);
	opp_y_speed[REAR_RIGHT] += acceleration;

	speed = ((opp_y_speed[REAR_RIGHT] * REDUCTION) >> 9);
	opp_actual_height[REAR_RIGHT] += speed;

	// Update front wheel y speed and height
	acceleration = ((opp_y_acceleration[FRONT] * REDUCTION) >> 8);
	opp_y_speed[FRONT] += acceleration;

	speed = ((opp_y_speed[FRONT] * REDUCTION) >> 9);
	opp_actual_height[FRONT] += speed;

	// Limit movement of opponent's wheels
	long diff = LimitOpponentWheels(296, REAR_LEFT, REAR_RIGHT);

	if (diff < 0)
		// Use rear right wheel (because this is higher than rear left)
		LimitOpponentWheels(368, REAR_RIGHT, FRONT);
	else
		LimitOpponentWheels(368, REAR_LEFT, FRONT);

#ifdef	TEST_AMIGA_AWH
	CompareRecordedAmigaWord("opp.rear.left.y.acceleration", &opp_y_acceleration[REAR_LEFT]);
	CompareRecordedAmigaWord("opp.rear.right.y.acceleration", &opp_y_acceleration[REAR_RIGHT]);
	CompareRecordedAmigaWord("opp.front.y.acceleration", &opp_y_acceleration[FRONT]);

	CompareRecordedAmigaWord("opp.rear.left.y.speed", &opp_y_speed[REAR_LEFT]);
	CompareRecordedAmigaWord("opp.rear.right.y.speed", &opp_y_speed[REAR_RIGHT]);
	CompareRecordedAmigaWord("opp.front.y.speed", &opp_y_speed[FRONT]);

	CompareRecordedAmigaWord("opp.rear.left.actual.height", &opp_actual_height[REAR_LEFT]);
	CompareRecordedAmigaWord("opp.rear.right.actual.height", &opp_actual_height[REAR_RIGHT]);
	CompareRecordedAmigaWord("opp.front.actual.height", &opp_actual_height[FRONT]);
#endif
}


/*	How far each wheel has been pushed up into its arch, for the renderer. This is the
	same road height minus actual height that CalculateWheelDifference() opens with,
	before its height_adjust bias and clamping - the raw travel, which is what the
	suspension wants to draw. Both the legacy and FloatV2 paths keep opp_actual_height[]
	current (OppFloatV2Sync writes it), so this reads correctly under either.		*/
/*	The opponent's forward speed, in the same units as the player's player_z_speed, for the
	renderer to roll its wheels at.												*/
long GetOpponentZSpeed( void )
{
	return opponents_z_speed;
}


void GetOpponentWheelCompression( long *rear_left, long *rear_right, long *front )
{
	*rear_left  = opp_rear_left_road_pos.y  - opp_actual_height[REAR_LEFT];
	*rear_right = opp_rear_right_road_pos.y - opp_actual_height[REAR_RIGHT];
	*front      = opp_front_road_pos_y      - opp_actual_height[FRONT];
}


static void CalculateWheelDifference( long road_height,
									  long actual_height,
									  long height_adjust,
									  long *old_difference_in_out,
									  long *new_difference_out,
									  long *touching_road)
{
long new_difference;
long amount_below_road;

	new_difference = road_height - actual_height;
	if (new_difference > opp_smallest_difference)
		opp_smallest_difference = new_difference;

	new_difference += height_adjust;
	if (new_difference < 0)
	{
		// wheel above road
		if (new_difference < -96) new_difference = -96;	// set to maximum amount above road
	}

	amount_below_road = new_difference - *old_difference_in_out;
	amount_below_road = ((amount_below_road * INCREASE) >> 8) + new_difference;

	if (amount_below_road < 0) amount_below_road = 0;
	if (amount_below_road > 1023) amount_below_road = 1023;

	*touching_road |= amount_below_road;

	amount_below_road -= height_adjust;
	*new_difference_out = amount_below_road;
	*old_difference_in_out = new_difference;
}


// Adjusts opponent wheel heights and y speeds to limit car's x and z angle
// Especially important when in the air on more extreme tracks (e.g. Roller Coaster)
static long LimitOpponentWheels( long max_difference, long wheel1, long wheel2 )
{
long diff, drop, speed_diff;

	diff = opp_actual_height[wheel1] - opp_actual_height[wheel2];

	drop = max_difference - abs(diff);
	if (drop < 0)
	{
		// Drop highest wheel
		if (diff >= 0)
			opp_actual_height[wheel1] += drop;
		else
			opp_actual_height[wheel2] += drop;

		if (wheel2 != FRONT)
		{
			// Get here on first call to function
			// Average rear wheel y speeds
			AverageWheelYSpeeds(REAR_LEFT, REAR_RIGHT);
			return(diff);
		}

		// Average rear wheel y speeds
		AverageWheelYSpeeds(REAR_LEFT, REAR_RIGHT);
		// Average front and rear wheels y speeds (both rear wheel values are currently the same)
		AverageWheelYSpeeds(FRONT, REAR_RIGHT);
		// Average rear wheels y speeds again
		AverageWheelYSpeeds(REAR_LEFT, REAR_RIGHT);
	}

	if (wheel2 != FRONT)
		return(diff);	// Finish if first call to function

	// Following is reached on second call to function
	if (opp_touching_road)
		return(diff);

	// Adjust wheel y speeds when opponent in air, possibly to make the car pitch forwards
	speed_diff = opp_y_speed[wheel1] - opp_y_speed[FRONT];
	if (speed_diff < 16)
	{
		static long	y_speed_adjustments[] = {4,4,-4};

		for (long i = 0; i < NUM_OPP_WHEEL_POSITIONS; i++)
		{
			opp_y_speed[i] += y_speed_adjustments[i];
		}
	}

	return(diff);
}


static void AverageWheelYSpeeds( long wheel1, long wheel2 )
{
	long average = (opp_y_speed[wheel1] + opp_y_speed[wheel2]) >> 1;
	opp_y_speed[wheel1] = average;
	opp_y_speed[wheel2] = average;
}

static long Opponent_Speed_Value( long track_id, long pos )
{
/*srd111a	move.l	#road.section.angle.and.piece,a1
	move.b	(a1,d1.w),d0
	andi.b	#$f,d0
	move.b	d0,d2
	move.l	#sections.car.can.be.put.on,a2
	move.b	(a2,d2.w),d0
	bpl	srd112

	move.b	B.63ce1,d0
	subi.b	#10,d0
	move.b	d0,value
	move.b	B.63ce1,d0
	jmp	srd113a

srd112	move.b	value,d0
	addi.b	#10,d0
	bmi	srd113
	move.b	d0,value

srd113	move.b	value,d0

srd113a	move.b	prompt.chars,d2
	beq	srd114

	subq.b	#1,prompt.chars
	ori.b	#$80,d0

srd114	move.l	#opponents.speed.values,a1
	move.b	d0,(a1,d1.w)
*/
	static long oldpos = -1;
	static long oldtrack = -1;
	static bool oldleague = false;
	static long oldspeed = 0;
	static long oldedits = -1;
	if(pos==oldpos && oldtrack==track_id && oldleague==bSuperLeague && oldedits==gOppSpeedTuningEdits)
		return oldspeed;
	oldpos = pos;
	oldleague = bSuperLeague;
	oldtrack = track_id;
	oldedits = gOppSpeedTuningEdits;

	long b = Piece_Angle_And_Template[pos];
	b = sections_car_can_be_put_on[b&0x0f];
	long B63ce1 = static_cast<long>(SCR_Rand()) & static_cast<long>(opp_track_speed_values[track_id+16+(bSuperLeague?32:0)]);
		 B63ce1 += OpponentTuningBase(track_id, OPP_SPEED_GROUP_PER_PIECE, bSuperLeague);
	long /*value,*/ d0;
	if (b<0) {
		//value = B63ce1-10;
		d0 = B63ce1;
	} else {
		if (B63ce1<(0x7f-10))
			d0 = /*value =*/ B63ce1+10;
		else
			d0 = /*value =*/ B63ce1;
	}
	oldspeed = d0;
	return d0;
}


/*	======================================================================================= */
/*	Function:		RandomizeOpponentsSteering												*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

static long B1bb9d = 0;
static long B1bbc2 = 0;
static long B1bbbd = 0;

static long TAB5be34[] =
{0x20,0x50,0x60,0x70,0x70,0x60,0x50,0x20,
-0x20,-0x50,-0x60,-0x70,-0x70,-0x60,-0x50,-0x20};


// Tested against Amiga
static void RandomizeOpponentsSteering( void )
{
// TO DO: Tidy up, rename variables, remove gotos
long d0, d1, d2;
long value;

#ifdef TEST_AMIGA_ROS
	if (GetRecordedAmigaWord(&opponentsID))
		++VALUE2;

	long temp, fourteen_frames_elapsed = 0;
	if (GetRecordedAmigaWord(&temp))
	{
		opp_touching_road = temp ? TRUE : FALSE;
	}

	GetRecordedAmigaWord(&B1bbbe[2]);	//opponents.random.steering.count
	GetRecordedAmigaWord(&B1bb9d);
	GetRecordedAmigaWord(&B1bbc2);
	GetRecordedAmigaWord(&opponents_current_piece);

	if (GetRecordedAmigaWord(&temp))
		opponent_behind_player = temp & 0x80 ? TRUE : FALSE;

	GetRecordedAmigaWord(&fourteen_frames_elapsed);
#endif

	if (!opp_touching_road)
		return;

	d1 = 0;
	B1bbbe[0] = B1bbbe[1] = 0;
	B1bbbd = 0;
	d0 = B1bbbe[2];	//opponents.random.steering.count
	if (!d0) goto ros1;

#ifdef TEST_AMIGA_ROS
	if (!fourteen_frames_elapsed)
#endif
	B1bbbe[2] -= 1;

	d0 += B1bbc2;
	d0 &= 0xf;
	d2 = d0;
	d0 = TAB5be34[d2];
	if (d0 < 0)
	{
		d0 = -d0;
		d1++;
	}
	B1bbbe[d1] = d0;

	d2 += 5;
	d2 &= 0xf;
	d0 = TAB5be34[d2];
	B1bbbd = d0;
	goto ros2;

ros1:
	d2 = opponents_current_piece;
	if (/*opponents_speed_values[TrackID][d2]*/Opponent_Speed_Value(TrackID, d2) < 0)
		goto ros2;

	if (opponent_behind_player)
		goto ros2;

	if (Track[opponents_current_piece].type & 0x80)	// curve
		goto ros2;

	d2 = 8;
	if ((B1bb9d & 0x80) == 0)	// not curved piece
		goto ros2;

	if (B1bb9d & 0x40)	// diagonal piece (45 degrees)
		d2 = 16;

	B1bbc2 = d2;

	value = SCR_Rand() & 0x1f;
#ifdef TEST_AMIGA_ROS
	GetRecordedAmigaWord(&value);
#endif
	if (opponentsID < value)
		goto ros2;

	B1bbbe[2] = 16;	//opponents.random.steering.count

ros2:
	d0 = Track[opponents_current_piece].oppositeDirection ? 0x40 : 0;
	d0 ^= Track[opponents_current_piece].type;
	B1bb9d = d0;

#ifdef TEST_AMIGA_ROS
	CompareRecordedAmigaWord("B1bbbe[0]", &B1bbbe[0]);
	CompareRecordedAmigaWord("B1bbbe[1]", &B1bbbe[1]);
	CompareRecordedAmigaWord("B1bbbe[2]", &B1bbbe[2]);
	CompareRecordedAmigaWord("B1bb9d", &B1bb9d);
	CompareRecordedAmigaWord("B1bbc2", &B1bbc2);

	// Amiga just uses a signed byte
	B1bbbd &= 0xff;
	CompareRecordedAmigaWord("B1bbbd", &B1bbbd);
	if (B1bbbd & 0x80) B1bbbd = B1bbbd - 0x100;	//sign extend again
#endif

	//VALUE3 = B1bbbe[2];	//opponents.random.steering.count
	return;
}


/*	======================================================================================= */
/*	Function:		GetOpponentsEngineAcceleration											*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

// Tested against Amiga
static void GetOpponentsEngineAcceleration( void )
{
long power = opp_engine_power;

#ifdef TEST_AMIGA_GOEA
	if (GetRecordedAmigaWord(&B1bbbe[2]))	//opponents.random.steering.count
		++VALUE2;

	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		opp_touching_road = temp ? TRUE : FALSE;
	}
#endif

	if (B1bbbe[2] != 0)	//opponents.random.steering.count
		power -= 25;

	if (opp_touching_road)
		opponents_engine_z_acceleration = power;
	else
		opponents_engine_z_acceleration = 0;

#ifdef TEST_AMIGA_GOEA
	CompareRecordedAmigaWord("opponents.engine.z.acceleration", &opponents_engine_z_acceleration);
#endif
	return;
}


// Tested against Amiga
static void AdjustOpponentsEngineAcceleration( void )
{
long speed_value, speed, opponents_required_z_speed;

#ifdef TEST_AMIGA_AOEA
	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		++VALUE2;
		opp_touching_road = temp ? TRUE : FALSE;
	}
	GetRecordedAmigaWord(&opponents_current_piece);
	GetRecordedAmigaWord(&opponents_max_speed);
	GetRecordedAmigaWord(&opponents_z_speed);
	GetRecordedAmigaWord(&opponents_engine_z_acceleration);
#endif

	if (!opp_touching_road)
		return;

	speed_value = /*opponents_speed_values[TrackID][opponents_current_piece]*/Opponent_Speed_Value(TrackID, opponents_current_piece);
	speed = speed_value;
	if ((speed & 0x80) == 0)
	{
		if (speed > opponents_max_speed)
			speed = opponents_max_speed;
	}

	opponents_required_z_speed = speed & 0x7f;

	speed = opponents_z_speed >> 8;
	speed -= opponents_required_z_speed;
	if (speed == 0)
	{
		opponents_required_z_speed_reached = TRUE;
		return;
	}
	if (speed > 0)
	{
		// Speed is greater than required speed
		opponents_required_z_speed_reached = TRUE;
		opponents_engine_z_acceleration = -opponents_engine_z_acceleration;
		if (speed < 14)
			return;
	}

	if ((speed_value & 0x80) || (speed >= 0) || (!opponents_required_z_speed_reached))
	{
		opponents_engine_z_acceleration <<= 1;
		return;
	}

	if (speed >= -2)
		return;		// Value is -2 or -1

	opponents_required_z_speed_reached = FALSE;
	opponents_engine_z_acceleration <<= 1;
	return;
}


// Tested against Amiga
static void UpdateOpponentsZSpeed( void )
{
long acceleration_adjust = 0, s, a;

#ifdef TEST_AMIGA_UOZS
	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		++VALUE2;
		opp_touching_road = temp ? TRUE : FALSE;
	}
	GetRecordedAmigaWord(&opponents_z_speed);

	if (GetRecordedAmigaWord(&temp))
		player_close_to_opponent = temp & 0x80 ? TRUE : FALSE;
	if (GetRecordedAmigaWord(&temp))
		opponent_behind_player = temp & 0x80 ? TRUE : FALSE;

	GetRecordedAmigaWord(&opponents_engine_z_acceleration);
	GetRecordedAmigaWord(&opponents_current_piece);

	GetRecordedAmigaWord(&opp_rear_left_road_pos.y);
	GetRecordedAmigaWord(&opp_rear_right_road_pos.y);
	GetRecordedAmigaWord(&opp_front_road_pos_y);
#endif

	if (opponents_z_speed >= 0)
	{
		s = opponents_z_speed >> 7;

		// Reduce speed value if opponent close behind player
		if (player_close_to_opponent && opponent_behind_player)
		{
			s -= 20;
			if (s < 0) s = 0;
		}

		// A fraction of the square of the speed is subtracted from acceleration
		// This only has a small effect
		acceleration_adjust = ((opponents_z_speed >> 8) * s) >> 6;

		// Reduce the acceleration further if on the road
		if (opp_touching_road)
		{
			if (opponents_engine_z_acceleration >= 0)
			{
				// Subtract fraction of speed from acceleration
				s = (opponents_z_speed >> 8);
				a = opponents_engine_z_acceleration - s;

				if (Track[opponents_current_piece].type & 0x80)	// curve
				{
					// Subtract again when on a curve, then reduce further
					a -= s;
					a -= 35;
				}
				opponents_engine_z_acceleration = a;
			}
		}
	}

	a = opponents_engine_z_acceleration - acceleration_adjust;
	if (opp_touching_road)
	{
		long d = (opp_rear_left_road_pos.y + opp_rear_right_road_pos.y) >> 1;
		d -= opp_front_road_pos_y;
		// d is -'ve when opponent pitched backwards, +'ve when pitched forwards

		long pitch = abs(d), adjust;
		if (pitch >= 512) pitch = 510;

		pitch >>= 1;		// pitch value / 2
		adjust = pitch + (pitch >> 2);	// (5 * pitch value) / 8

		if (d < 0) adjust = -adjust;

		// Acceleration is reduced when opponent pitched backwards, increased when pitched forwards
		// i.e. effect of gravity
		a += adjust;
	}

	long acceleration = (a * REDUCTION) >> 8;
	opponents_z_speed += acceleration;
	if (opponents_z_speed < 0) opponents_z_speed = 0;

//	VALUE2 = opponents_engine_z_acceleration;
//	VALUE3 = opponents_z_speed;
#ifdef TEST_AMIGA_UOZS
	CompareRecordedAmigaWord("opponents.engine.z.acceleration", &opponents_engine_z_acceleration);
	CompareRecordedAmigaWord("opponents.z.speed", &opponents_z_speed);
#endif
	return;
}


/*	======================================================================================= */
/*	Function:		CalculateDistancesBetweenPlayers										*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

extern long player_current_piece;
extern long players_distance_into_section;
extern long players_road_x_position;
extern long rear_wheel_surface_x_position;

static long difference_between_players = 0;
static long smallest_distance_between_players = 0;


static void CalculateDistancesBetweenPlayers( void )
{
static long distances_around_road[MAX_PIECES_PER_TRACK], total_road_distance;
static long previousTrackID = NO_TRACK;

//	VALUE1 = player_current_piece;
//	VALUE2 = players_distance_into_section;
//	VALUE3 = opponents_current_piece;

	// Re-calculate road distances when track changes
	if (previousTrackID != TrackID)
	{
		long piece;
		long distance = 0;
		for (piece = 0; piece < NumTrackPieces; piece++)
			{
			distances_around_road[piece] = distance << 5;

			distance += Track[piece].numSegments;
			}
		total_road_distance = distance << 5;

		previousTrackID = TrackID;
	}

	long diff, abs_diff, opposite;
	diff = (opponents_distance_into_section - players_distance_into_section) >> 3;
	diff += distances_around_road[opponents_current_piece] - distances_around_road[player_current_piece];
	// NOTE: following value can only be relied upon when player and opponent are on same piece
	// (it's wrong when they're on different sides of track start/end, e.g. opponent on piece 0, player on piece 43)
	difference_between_players = diff;
//	VALUE1 = diff;
	/*
	fprintf(out, "opponent %2x %3x, player %2x %3x, diff %x\n",
		opponents_current_piece, opponents_distance_into_section,
		player_current_piece, players_distance_into_section, difference_between_players);
	*/

	abs_diff = abs(diff);
	opposite = total_road_distance - abs_diff;	// difference between players in opposite direction

	// compare two road distances
	if (abs_diff < opposite)
	{
		// get smallest distance
		smallest_distance_between_players = abs_diff;
		diff = -diff;
	}
	else
		smallest_distance_between_players = opposite;

//	VALUE2 = smallest_distance_between_players;

	if (diff > 0)
		opponent_behind_player = TRUE;
	else
		opponent_behind_player = FALSE;

//	VALUE3 = opponent_behind_player ? 1 : 0;
	return;
}


/*	======================================================================================= */
/*	Function:		CalculateIfWinning														*/
/*																							*/
/*	Description:	Returns negative if player is winning									*/
/*	======================================================================================= */

extern long lapNumber[];


long CalculateIfWinning( long start_finish_piece )
{
long result, p, o;

	result = lapNumber[OPPONENT] - lapNumber[PLAYER];
	if (result != 0)	// on different laps
		return(result);

	p = player_current_piece - start_finish_piece;
	if (p < 0)
		p += NumTrackPieces;

	o = opponents_current_piece - start_finish_piece;
	if (o < 0)
		o += NumTrackPieces;

	result = o - p;
	if (result != 0)	// on different pieces
		return(result);

	result = difference_between_players;
	return(result);
}


/*	======================================================================================= */
/*	Function:		CarToCarCollisionDetection												*/
/*																							*/
/*	Description:	Calculates opponent collision with player								*/
/*	======================================================================================= */

static long B1bbc3 = 0, B1bbeb = 0;
static long x_difference, player_to_right;
static long cars_collided;
static long car_to_car_x_acceleration, car_to_car_y_acceleration, car_to_car_z_acceleration;

// player's values
extern long touching_road;
extern long player_y;
extern long player_z_speed;
extern long front_left_damage, front_right_damage, rear_damage, damaged;


static void CarToCarCollisionDetection( void )
{
// TO DO: Tidy up, rename variables, remove gotos
long d0, d3, d4;
long players_smaller_y;

	if (!drop_start_done)
		return;

	if (!opp_touching_road)
		goto ctccd1;

	if (touching_road)
		goto ctccd2;

ctccd1:
	players_smaller_y = player_y >> 11;
	d0 = players_smaller_y - opp_actual_height[REAR_LEFT];
	d4 = d0;
	d0 += 40;
	d0 = abs(d0);

	if (d0 >= 192)
	{
		B1bbc3 = 3;
		return;
	}

	if (!B1bbc3)
		goto ctccd2;

	--B1bbc3;
	d3 = 256 - d0;
	if (d4 < 0)
		d3 = -d3;

	d3 <<= 4;
	car_to_car_y_acceleration = d3;

ctccd2:
	d0 = x_difference;
	if (d0 >= 45)
		goto ctccd4;

	d0 = smallest_distance_between_players & 0xff;
	if (d0 > 8)
		goto ctccd4;

	d0 = 0x800;
	if (player_to_right)
		goto ctccd3;

	d0 = -0x800;

ctccd3:
	car_to_car_x_acceleration = d0;

ctccd4:
	if (B1bbeb & 0x80)
		goto ctccd5;

	d3 = 3;
	d0 = opponents_z_speed - player_z_speed;
	if (d0 < 0)
		d3 = -3;

	d0 >>= 1;
	d0 += d3;
	car_to_car_z_acceleration = d0;

ctccd5:
	cars_collided = 0x80;
	B1bbeb = 0x80;

	d3 = 512;
	d0 = abs(car_to_car_x_acceleration);
	d3 += d0;

	d0 = abs(car_to_car_y_acceleration);
	d3 += d0;

	d0 = abs(car_to_car_z_acceleration);
	d3 += d0;

	d3 >>= 8;

	d0 = rear_damage + d3;
	if (d0 > 255) d0 = 255;
	rear_damage = d0;

	d0 = front_right_damage + d3;
	if (d0 > 255) d0 = 255;
	front_right_damage = d0;

	d0 = front_left_damage + d3;
	if (d0 > 255) d0 = 255;
	front_left_damage = d0;

	damaged = 0x80;
	return;
}


static long cars_collided_delay = 0;

// player's values
extern long car_collision_x_acceleration, car_collision_y_acceleration, car_collision_z_acceleration;


void CarToCarCollision( void )
{
long d0;

	if (cars_collided_delay > 0)
		--cars_collided_delay;

	if (!cars_collided)
		return;

	cars_collided = 0;

	d0 = opponents_z_speed - car_to_car_z_acceleration;
	if (d0 < 0) d0 = 0;
	opponents_z_speed = d0;

	d0 = car_to_car_y_acceleration >> 4;
	opp_y_speed[REAR_LEFT] -= d0;
	opp_y_speed[REAR_RIGHT] -= d0;
	opp_y_speed[FRONT] -= d0;

	//VALUE1 = car_to_car_x_acceleration;
	//VALUE2 = car_to_car_y_acceleration;
	//VALUE3 = car_to_car_z_acceleration;
	car_collision_x_acceleration += car_to_car_x_acceleration;
	car_collision_y_acceleration += car_to_car_y_acceleration;
	car_collision_z_acceleration += car_to_car_z_acceleration;

	car_to_car_x_acceleration = 0;
	car_to_car_y_acceleration = 0;
	car_to_car_z_acceleration = 0;

//******** Play collision sound if necessary ********

	if (cars_collided_delay > 0)
		return;

	//HitCarSoundBuffer->SetCurrentPosition(0);
	HitCarSoundBuffer->Play(NULL,NULL,NULL);	// not looping

	cars_collided_delay = 5;
	return;
}


/*	======================================================================================= */
/*	Function:		OpponentPlayerInteraction												*/
/*																							*/
/*	Description:	Calculates opponent movement sideways and collision with player			*/
/*	======================================================================================= */

static long opponents_suggested_road_x_position;

extern unsigned char sections_car_can_be_put_on[];


/*	======================================================================================= */
/*	Function:		SteerTowardSuggested													*/
/*																							*/
/*	Description:	Move opponent one step toward opponents_suggested_road_x_position.		*/
/*					This is the tail of the Amiga's opponent.player.interaction; it is		*/
/*					split out because OpponentStepF (PhysicsFloatV2.cs) calls it on its		*/
/*					own so the resulting road-x change can be scaled by dtRatio.			*/
/*	======================================================================================= */

static void SteerTowardSuggested( void )
{
long d0;

	d0 = B1bbbd;
	if (d0 < 0) goto opi10;
	if (d0) goto opi11;

	d0 = opponents_suggested_road_x_position;
	d0 -= opponents_road_x_position & 0xff;
	if (!d0) return;
	if (d0 >= 0) goto opi11;

opi10:
	if (d0 >= -16)
		return;

	d0 = -9;
	goto opi12;

opi11:
	if (d0 < 16)
		return;

	d0 = 9;

opi12:
	d0 += opponents_road_x_position & 0xff;

	if (!opp_touching_road)
		return;

	if (d0 < 0)	//temp, remove
		MessageBox(NULL, L"Less than 0", L"Error", MB_OK);	//temp
	if (d0 >= 225)
		return;

	if (d0 < 32)
		return;

	opponents_road_x_position = d0;
}


// Tested against Amiga
static void OpponentPlayerInteraction( bool applySteering )
{
// TO DO: Tidy up, rename variables, remove gotos
long d0, d1, d2;
long count, piece;

	//VALUE2 = players_road_x_position;
	//VALUE3 = rear_wheel_surface_x_position;

#ifdef TEST_AMIGA_OPI
	if (GetRecordedAmigaWord(&opponentsID))
		++VALUE2;

	GetRecordedAmigaWord(&opponents_road_x_position);
	GetRecordedAmigaWord(&rear_wheel_surface_x_position);
	GetRecordedAmigaWord(&smallest_distance_between_players);

	long temp;
	if (GetRecordedAmigaWord(&temp))
		opponent_behind_player = temp & 0x80 ? TRUE : FALSE;

	GetRecordedAmigaWord(&players_road_x_position);
	GetRecordedAmigaWord(&opponents_current_piece);
	GetRecordedAmigaWord(&opponents_distance_into_section);
	GetRecordedAmigaWord(&B1bbbd);
	if (B1bbbd & 0x80) B1bbbd = B1bbbd - 0x100;	//sign extend

	if (GetRecordedAmigaWord(&temp))
		opp_touching_road = temp ? TRUE : FALSE;
#endif

	d1 = opponentsID;
	d2 = 0;
	player_close_to_opponent = FALSE;

	d0 = opponents_road_x_position & 0xff;
	opponents_suggested_road_x_position = d0;

	d0 -= rear_wheel_surface_x_position;
	if (d0 < 0)
	{
		d0 = -d0;
		--d2;	// flag that player is to right of opponent
	}
	x_difference = d0;
	player_to_right = d2;

	if ((smallest_distance_between_players >> 8) != 0)
		goto far_away;

	// Player and opponent are within $100 of each other (ahead or behind)
	d0 = smallest_distance_between_players;
	if (smallest_distance_between_players >= 64)
		goto close_checked;

	if (opponent_behind_player)
		goto flag_close;

	if (x_difference >= 50)
		goto close_checked;

	// Either:-
	//  Opponent less than 64 behind player
	//  OR Player less than 64 behind and less then 50 to the left or right of opponent
flag_close:
	player_close_to_opponent = TRUE;

close_checked:
	if (d0 >= 16)
		goto opi3;

	//tst.b	machine
	//beq	opi1

	//if ((opponents_road_x_position & 0xff) != 0)
	//	goto opi3;

//opi1
	if (x_difference >= 50)
		goto opi3;

	d0 = players_road_x_position >> 8;
	if (d0 < 1)	goto opi2;
	if (d0 != 1) goto opi3;

	d0 = players_road_x_position & 0xff;
	if (d0 >= 0x80)
		goto opi3;

opi2:
	CarToCarCollisionDetection();
	goto opi4;

	// not within 16
opi3:
	B1bbc3 = 0;	// clear collision values
	B1bbeb = 0;

	d0 = smallest_distance_between_players & 0xff;
	if (d0 >= 24)
		goto opi6;

opi4:
	if (!(opponent_attributes[opponentsID] & DRIVES_NEAR_EDGE))
		goto opi5;

	if (opponent_behind_player)
		goto opi5;

	d0 = smallest_distance_between_players & 0xff;
	if (d0 >= 14)
		goto far_away;

opi5:
	MoveOpponentToOneSide();
	goto opif;

opi6:
	if (opponent_behind_player)
		goto opi9;

	if (d0 >= 50)
		goto opi7;

	if (!(opponent_attributes[opponentsID] & OBSTRUCTS_PLAYER))
		goto opi8;

	// put opponent at same position as player
	opponents_suggested_road_x_position = rear_wheel_surface_x_position;
	goto opic;

opi7:
	if (d0 >= 200)
		goto far_away;

	if (!(opponent_attributes[opponentsID] & PUSH_PLAYER))	// opponent pushing player off track
		goto far_away;

opi8:
	OpponentPushPlayer();
	goto opic;

opi9:
	OpponentPushPlayer();
	goto opif;

// Player and opponent atleast $100 from each other (ahead or behind)
far_away:
	d2 = 64;
	if (opponent_attributes[opponentsID] & DRIVES_NEAR_EDGE)
		d2 = 110;

	if (opponentsID & 1)
		d2 = 255-d2;	// to other side of road

	opponents_suggested_road_x_position = d2;

opic:
	// move opponent to middle of road if approaching or on a curve
	piece = opponents_current_piece;
	for (count = 2; count > 0; count--)
	{
		d0 = GetPieceAngleAndTemplate(piece);
		d0 &= 0xf;	// templateNum
		if (sections_car_can_be_put_on[d0] & 0x80)
			opponents_suggested_road_x_position = 128;	// middle of road

		// go to next piece
		piece++; if (piece > (NumTrackPieces - 1)) piece = 0;
	}

opif:
	// The steering tail is a separate function (Physics.cs:783). The FloatV2
	// opponent step calls it itself so it can interpolate the road-x change.
	if (applySteering)
		SteerTowardSuggested();

	//VALUE1 = player_close_to_opponent;
#ifdef TEST_AMIGA_OPI
	CompareRecordedAmigaWord("x.difference", &x_difference);

	if (GetRecordedAmigaWord(&temp))
	{
		long val = temp & 0x80 ? -1 : 0;
		CompareAmigaWord("player.to.right", val, &player_to_right);
	}

	if (GetRecordedAmigaWord(&temp))
	{
		long amiga_flag = temp & 0x80 ? TRUE : FALSE;
		long flag = player_close_to_opponent;
		CompareAmigaWord("player.close.to.opponent", amiga_flag, &flag);
	}
	CompareRecordedAmigaWord("opponents.suggested.road.x.position", &opponents_suggested_road_x_position);
	CompareRecordedAmigaWord("opponents.road.x.position", &opponents_road_x_position);
#endif
	return;
}


// Tested against Amiga
// position opponent on left or right
static void MoveOpponentToOneSide( void )
{
#ifdef TEST_AMIGA_MOTOS
	if (GetRecordedAmigaWord(&x_difference))
		++VALUE2;

	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		player_to_right = temp & 0x80 ? -1 : 0;
	}
#endif

long d0 = x_difference;

	if (d0 >= 56)
		return;
//	++VALUE3;
	if (player_to_right & 0x80)
		opponents_suggested_road_x_position = 32;
	else
		opponents_suggested_road_x_position = 256-32;

#ifdef TEST_AMIGA_MOTOS
	CompareRecordedAmigaWord("opponents.suggested.road.x.position", &opponents_suggested_road_x_position);
#endif
	return;
}


// Tested against Amiga
// opponent pushing player off track
static void OpponentPushPlayer( void )
{
long d0 = x_difference;

	if (d0 >= 56)
		return;

	d0 = rear_wheel_surface_x_position;

	if (player_to_right & 0x80)
	{
		if (d0 < 96)
			opponents_suggested_road_x_position = 256-32;
		else
			opponents_suggested_road_x_position = 32;
	}
	else
	{
		if (d0 >= (256-96))
			opponents_suggested_road_x_position = 32;
		else
			opponents_suggested_road_x_position = 256-32;
	}

	return;
}


/*	======================================================================================= */
/*	Function:		CalculateOpponentsDistance												*/
/*																							*/
/*	Description:	Calculate distance between opponent and player							*/
/*	======================================================================================= */

long CalculateOpponentsDistance (void)
	{
	// should do every fourth frame

	long dist = smallest_distance_between_players;
	dist += (dist >> 2);
	dist >>= 2;

	if (opponent_behind_player)
		dist = -dist;

	return(dist);
	}


/*	======================================================================================= */
/*	Function:		SetRaceOpponent															*/
/*																							*/
/*	Description:	Choose who the next race is against - see Opponent_Behaviour.h.			*/
/*					Practise takes NO_OPPONENT, and the race then runs solo, so everything	*/
/*					the opponent leaves behind it has to be cleared here: nothing will		*/
/*					call ResetOpponent to do it.											*/
/*	======================================================================================= */

void SetRaceOpponent( long driver )
	{
	gRaceOpponent = driver;

	/*	Settle on a driver now rather than waiting for ResetOpponent: the race loop asks	*/
	/*	opponentsID whether there is an opponent at all before it calls anything that		*/
	/*	would reset one, so leaving it stale here would race everyone solo.				*/
	ChooseOpponent();

	if (driver == NO_OPPONENT)
		{
		difference_between_players = 0;
		smallest_distance_between_players = 0;
		player_close_to_opponent = FALSE;
		opponent_behind_player = FALSE;
		}
	}


/*	======================================================================================= */
/*	Function:		OpponentStepFloatV2														*/
/*																							*/
/*	Description:	Timestep-parameterised opponent step. Direct port of					*/
/*					OpponentStepF.Tick (PhysicsFloatV2.cs:25).								*/
/*	======================================================================================= */

// The opponent AI itself is *not* re-derived here: FloatV2 keeps the Amiga's
// integer routines (RandomizeOpponentsSteering, Get/AdjustOpponentsEngineAcceleration,
// UpdateOpponentsZSpeed, OpponentPlayerInteraction, CarToCarCollision) exactly as
// they are and wraps them. Everything the AI *integrates* -- z speed, distance
// into section, road-x position, the three wheel heights and their y speeds --
// is held here as a double and advanced by dtRatio, while the AI functions
// still see plain integers via Sync(). Decisions that the Amiga made once per
// frame (steering randomisation, player interaction, car-to-car collision) stay
// on a once-per-frame boundary tracked by _framePhase, so stepping faster makes
// the opponent smoother rather than more reactive.
//
// dtRatio is dt/0.1 -- the same convention the player port uses (see BaseDt in
// Physics_FloatV2.cpp), so at the default dt the opponent and player agree.

namespace {

struct OppFloatV2State
{
	double zSpeed;
	double roadX;
	double distance;
	double act[NUM_OPP_WHEEL_POSITIONS];		// opp_actual_height
	double ySpd[NUM_OPP_WHEEL_POSITIONS];		// opp_y_speed
	double oldDiff[NUM_OPP_WHEEL_POSITIONS];	// opp_old_*_difference
	double framePhase;
	unsigned char frameFractionAccumulator;
};

OppFloatV2State gOppF = {};

inline double ClampShortF( double v )
{
	double r = floor(v + 0.5);
	if (r < -32768.0) r = -32768.0;
	if (r >  32767.0) r =  32767.0;
	return r;
}

} // namespace

// PhysicsFloatV2.cs:57 -- the OpponentStepF constructor.
static void OppFloatV2Seed( void )
{
	gOppF.zSpeed   = static_cast<double>(opponents_z_speed);
	gOppF.roadX    = static_cast<double>(opponents_road_x_position & 0xff);
	gOppF.distance = static_cast<double>(opponents_distance_into_section);

	for (long i = 0; i < NUM_OPP_WHEEL_POSITIONS; i++)
	{
		gOppF.act[i]  = static_cast<double>(opp_actual_height[i]);
		gOppF.ySpd[i] = static_cast<double>(opp_y_speed[i]);
	}

	gOppF.oldDiff[REAR_LEFT]  = static_cast<double>(opp_old_rear_left_difference);
	gOppF.oldDiff[REAR_RIGHT] = static_cast<double>(opp_old_rear_right_difference);
	gOppF.oldDiff[FRONT]      = static_cast<double>(opp_old_front_difference);

	// _framePhase / _frameFractionAccumulator deliberately survive a re-seed:
	// they track the Amiga frame clock, not car state.
}

// PhysicsFloatV2.cs:160 -- Sync(). Publishes the doubles into the integer
// globals the legacy AI functions read and write.
static void OppFloatV2Sync( void )
{
	opponents_z_speed = static_cast<long>(ClampShortF(gOppF.zSpeed));

	double rx = floor(gOppF.roadX + 0.5);
	if (rx < 0.0) rx = 0.0;
	if (rx > 255.0) rx = 255.0;
	opponents_road_x_position = (opponents_road_x_position & ~0xffL) | static_cast<long>(rx);

	double d = floor(gOppF.distance + 0.5);
	if (d < 0.0) d = 0.0;
	if (d > 65535.0) d = 65535.0;
	opponents_distance_into_section = static_cast<long>(d);

	for (long i = 0; i < NUM_OPP_WHEEL_POSITIONS; i++)
	{
		opp_actual_height[i] = static_cast<long>(ClampShortF(gOppF.act[i]));
		opp_y_speed[i]       = static_cast<long>(ClampShortF(gOppF.ySpd[i]));
	}

	opp_old_rear_left_difference  = static_cast<long>(ClampShortF(gOppF.oldDiff[REAR_LEFT]));
	opp_old_rear_right_difference = static_cast<long>(ClampShortF(gOppF.oldDiff[REAR_RIGHT]));
	opp_old_front_difference      = static_cast<long>(ClampShortF(gOppF.oldDiff[FRONT]));
}

// PhysicsFloatV2.cs:182 -- UpdateWheelHeights(). The double counterpart of
// UpdateOpponentsActualWheelHeights() above; keep the two in step.
static void OppFloatV2UpdateWheelHeights( double dtRatio, bool frameBoundary )
{
	const double height_adjust = (Track[opponents_current_piece].type & 0x80) ? 124.0 : 40.0;

	double smallest  = -32768.0;
	double forceBits = 0.0;

	// Local ProcessWheel: identical to CalculateWheelDifference(), in doubles.
	// The predictive term is per-10Hz-step, hence the / dtRatio.
	struct ProcessWheel
	{
		static double Run( double roadH, double actH, double& oldDiff,
						   double height_adjust, double dtRatio,
						   double& smallest, double& forceBits )
		{
			double diff = roadH - actH;
			if (diff > smallest) smallest = diff;

			diff += height_adjust;
			if (diff < -96.0) diff = -96.0;		// maximum amount above road

			double newDiff = diff;
			double below = newDiff + (diff - oldDiff) * (INCREASE / 256.0) / dtRatio;
			oldDiff = newDiff;

			if (below < 0.0)    below = 0.0;
			if (below > 1023.0) below = 1023.0;

			forceBits += below;
			return below - height_adjust;
		}
	};

	double newRL = ProcessWheel::Run(static_cast<double>(opp_rear_left_road_pos.y),
									 gOppF.act[REAR_LEFT],  gOppF.oldDiff[REAR_LEFT],
									 height_adjust, dtRatio, smallest, forceBits);
	double newRR = ProcessWheel::Run(static_cast<double>(opp_rear_right_road_pos.y),
									 gOppF.act[REAR_RIGHT], gOppF.oldDiff[REAR_RIGHT],
									 height_adjust, dtRatio, smallest, forceBits);
	double newF  = ProcessWheel::Run(static_cast<double>(opp_front_road_pos_y),
									 gOppF.act[FRONT],      gOppF.oldDiff[FRONT],
									 height_adjust, dtRatio, smallest, forceBits);

	opp_touching_road = (forceBits > 0.5) ? TRUE : FALSE;
	opp_smallest_difference = static_cast<long>(ClampShortF(smallest));

	opp_new_rear_left_difference  = static_cast<long>(ClampShortF(newRL));
	opp_new_rear_right_difference = static_cast<long>(ClampShortF(newRR));
	opp_new_front_difference      = static_cast<long>(ClampShortF(newF));

	// 6 parts this wheel, 1 part each of the other two
	double total = newRL + newRR + newF;
	double accRL = (total + 5.0 * newRL) / 8.0;
	double accRR = (total + 5.0 * newRR) / 8.0;
	double accF  = (total + 5.0 * newF)  / 8.0;

	opp_y_acceleration[REAR_LEFT]  = static_cast<long>(ClampShortF(accRL));
	opp_y_acceleration[REAR_RIGHT] = static_cast<long>(ClampShortF(accRR));
	opp_y_acceleration[FRONT]      = static_cast<long>(ClampShortF(accF));

	// Randomly make opponent do a wheelie (if they have that attribute).
	// Once per Amiga frame only -- otherwise the chance would scale with dt.
	if (frameBoundary && (opponent_attributes[opponentsID] & WHEELIE))
	{
		long i = static_cast<long>(ClampShortF(gOppF.ySpd[FRONT]))
			   | static_cast<long>(ClampShortF(accF));
		if ((i & 0xfffc) == 0)			// front of car isn't moving much vertically
		{
			if ((SCR_Rand() & 0xf) == 0)
				gOppF.ySpd[FRONT] = 160.0;
		}
	}

	const double reduction = REDUCTION / 256.0;		// 238/256

	gOppF.ySpd[REAR_LEFT]  += accRL * reduction * dtRatio;
	gOppF.ySpd[REAR_RIGHT] += accRR * reduction * dtRatio;
	gOppF.ySpd[FRONT]      += accF  * reduction * dtRatio;

	gOppF.act[REAR_LEFT]  += gOppF.ySpd[REAR_LEFT]  * reduction / 2.0 * dtRatio;
	gOppF.act[REAR_RIGHT] += gOppF.ySpd[REAR_RIGHT] * reduction / 2.0 * dtRatio;
	gOppF.act[FRONT]      += gOppF.ySpd[FRONT]      * reduction / 2.0 * dtRatio;

	// Limit movement of opponent's wheels (LimitOpponentWheels, in doubles)
	double rearDiff = gOppF.act[REAR_LEFT] - gOppF.act[REAR_RIGHT];

	// Rear pair, max 296 apart
	{
		double drop = fabs(rearDiff) - 296.0;
		if (drop > 0.0)
		{
			if (rearDiff >= 0.0) gOppF.act[REAR_LEFT]  -= drop;
			else                 gOppF.act[REAR_RIGHT] -= drop;

			double avg = (gOppF.ySpd[REAR_LEFT] + gOppF.ySpd[REAR_RIGHT]) / 2.0;
			gOppF.ySpd[REAR_LEFT] = gOppF.ySpd[REAR_RIGHT] = avg;
		}
	}

	// Higher rear wheel against the front wheel, max 368 apart
	{
		long rear = (rearDiff < 0.0) ? REAR_RIGHT : REAR_LEFT;
		double diff = gOppF.act[rear] - gOppF.act[FRONT];
		double drop = fabs(diff) - 368.0;
		if (drop > 0.0)
		{
			if (diff >= 0.0) gOppF.act[rear]  -= drop;
			else             gOppF.act[FRONT] -= drop;

			double avg = (gOppF.ySpd[REAR_LEFT] + gOppF.ySpd[REAR_RIGHT]) / 2.0;
			gOppF.ySpd[REAR_LEFT] = gOppF.ySpd[REAR_RIGHT] = avg;
			gOppF.ySpd[FRONT] = (gOppF.ySpd[FRONT] + avg) / 2.0;
			gOppF.ySpd[REAR_LEFT] = gOppF.ySpd[REAR_RIGHT] = (avg + gOppF.ySpd[FRONT]) / 2.0;
		}
	}

	// Adjust wheel y speeds when opponent is in the air, to pitch the car forwards
	if (!opp_touching_road)
	{
		double rearSpd = (rearDiff < 0.0) ? gOppF.ySpd[REAR_RIGHT] : gOppF.ySpd[REAR_LEFT];
		if ((rearSpd - gOppF.ySpd[FRONT]) < 16.0)
		{
			gOppF.ySpd[FRONT]      -= 4.0 * dtRatio;
			gOppF.ySpd[REAR_LEFT]  += 4.0 * dtRatio;
			gOppF.ySpd[REAR_RIGHT] += 4.0 * dtRatio;
		}
	}
}

// PhysicsFloatV2.cs:73 -- OpponentStepF.Tick()
static void OpponentStepFloatV2( double dt )
{
	if (!drop_start_done)
		return;

	if (scr::gFloatV2OpponentNeedsSeed)
	{
		OppFloatV2Seed();
		scr::gFloatV2OpponentNeedsSeed = false;
	}

	const double dtRatio = dt / 0.1;		// BaseDt, as in Physics_FloatV2.cpp

	// One Amiga frame's worth of decisions per 1/10s of simulated time.
	bool frameBoundary = false;
	gOppF.framePhase += dtRatio;
	if (gOppF.framePhase >= 0.999999999)
	{
		gOppF.framePhase -= 1.0;
		frameBoundary = true;
	}

	OppFloatV2UpdateWheelHeights(dtRatio, frameBoundary);

	if (frameBoundary)
	{
		// The 14-frame clock the Amiga used to hold off boost drain and damage
		// for one frame in fourteen. It lives on the opponent's frame boundary
		// in FloatV2 (PhysicsFloatV2.cs:87) and the player reads it.
		int acc = gOppF.frameFractionAccumulator + 238;
		gOppF.frameFractionAccumulator = static_cast<unsigned char>(acc);
		fourteen_frames_elapsed = (acc <= 255) ? -1 : 0;

		RandomizeOpponentsSteering();
	}

	OppFloatV2Sync();

	GetOpponentsEngineAcceleration();
	AdjustOpponentsEngineAcceleration();

	long prevZSpeed = opponents_z_speed;
	UpdateOpponentsZSpeed();
	gOppF.zSpeed += static_cast<double>(opponents_z_speed - prevZSpeed) * dtRatio;
	if (gOppF.zSpeed < 0.0) gOppF.zSpeed = 0.0;

	// Advance along the section. Same chain as OpponentMovement(), but the
	// low byte that OpponentMovement carries in byte_count is simply the
	// fractional part of the double here.
	{
		long lengthReduction = (static_cast<long>(Track[opponents_current_piece].lengthReduction) << 7) & 0x7fff;
		short zs = static_cast<short>(ClampShortF(gOppF.zSpeed));
		short v1 = static_cast<short>((static_cast<long>(zs) * lengthReduction * 2) >> 16);
		short v2 = static_cast<short>((static_cast<long>(v1) * REDUCTION) >> 8);
		double advance = static_cast<double>(static_cast<long>(v2) << 3) / 256.0;

		gOppF.distance += advance * dtRatio;

		double sectionLength = static_cast<double>(Track[opponents_current_piece].numSegments * 256);
		if (gOppF.distance >= sectionLength)
		{
			gOppF.distance -= sectionLength;

			// go to next piece
			opponents_current_piece++;
			if (opponents_current_piece > (NumTrackPieces - 1)) opponents_current_piece = 0;
		}
	}

	OppFloatV2Sync();

	if (frameBoundary)
	{
		CalculateDistancesBetweenPlayers();
		OpponentPlayerInteraction(false);	// steering applied separately, below

		// Car-to-car collision is an impulse: it is applied once per frame and
		// its effect on the opponent is taken whole, not scaled by dtRatio.
		long prevZ  = opponents_z_speed;
		long prevRL = opp_y_speed[REAR_LEFT];
		long prevRR = opp_y_speed[REAR_RIGHT];
		long prevF  = opp_y_speed[FRONT];

		CarToCarCollision();

		gOppF.zSpeed += static_cast<double>(opponents_z_speed - prevZ);
		if (gOppF.zSpeed < 0.0) gOppF.zSpeed = 0.0;
		gOppF.ySpd[REAR_LEFT]  += static_cast<double>(opp_y_speed[REAR_LEFT]  - prevRL);
		gOppF.ySpd[REAR_RIGHT] += static_cast<double>(opp_y_speed[REAR_RIGHT] - prevRR);
		gOppF.ySpd[FRONT]      += static_cast<double>(opp_y_speed[FRONT]      - prevF);
	}
	else
	{
		// Distances are needed every step (the HUD and the player's own
		// collision test read them), the decisions are not.
		CalculateDistancesBetweenPlayers();
	}

	OppFloatV2Sync();

	/*	Steering. The integer AI still takes its whole step, so the globals the interaction
		and collision code read stay Amiga-exact; what the drawn car follows is gOppF.roadX,
		advanced here (the Sync below writes it straight back over the integer value, so
		there is still only one source of truth).

		Spreading that step across the timestep is not enough on its own. When the car is
		chasing opponents_suggested_road_x_position, the Amiga steers at a flat 9 per frame
		and stops the moment it is within 16 of the target - and the target itself is only
		recomputed on a frame boundary, 10 times a second. So the lateral drift runs at full
		rate for a step or two and then stops dead until the next decision. At the Amiga's
		8.3Hz that was invisible, because everything moved in jumps; at 60Hz it is the only
		thing that does, and it reads as the opponent stuttering its way round a corner,
		where the suggested position is moving fastest.

		So close the last 16 continuously instead: same 9-per-frame rate, but easing onto
		the target rather than stopping short of it.									*/
	{
		long before = opponents_road_x_position & 0xff;
		SteerTowardSuggested();
		long after = opponents_road_x_position & 0xff;
		double step = static_cast<double>(static_cast<signed char>(after - before)) * dtRatio;

		// B1bbbd == 0 is the branch that chases the suggested position; non-zero is the
		// random steering wobble, which is open loop and already moves every frame.
		if ((B1bbbd == 0) && opp_touching_road)
		{
			const double rate  = 9.0 * dtRatio;
			double       error = static_cast<double>(opponents_suggested_road_x_position)
							   - gOppF.roadX;

			step = (error >  rate) ?  rate
				 : (error < -rate) ? -rate
				 : error;

			// The integer path's edge guards, applied to where this would land.
			double dest = gOppF.roadX + step;
			if ((dest < 32.0) || (dest >= 225.0))
				step = 0.0;
		}

		gOppF.roadX += step;
	}

	OppFloatV2Sync();
}


/*	======================================================================================= */
/*	Function:		CalculateOpponentsRoadWheelPositionsF									*/
/*																							*/
/*	Description:	Double-precision counterpart of CalculateOpponentsRoadWheelPositions().	*/
/*					Same geometry, same Amiga quirks, but fed from gOppF rather than from	*/
/*					the rounded integer globals, and with the fixed-point >>8 steps done	*/
/*					in floating point. Display only -- keep the two in step.				*/
/*	======================================================================================= */

// Height of the current surface quad at continuous (sx, sz), in the same
// (halved) units CalculateOpponentsRoadWheelHeight() returns.
static double CalculateOpponentsRoadWheelHeightF( double sx, double sz )
{
	// first do x interpolation
	double sya = sy1 + ((sx * (sy4-sy1)) / 256.0);
	double syb = sy2 + ((sx * (sy3-sy2)) / 256.0);

	// now do z interpolation, then halve as the integer version's >>9 does
	return (syb + ((sz * (sya-syb)) / 256.0)) / 2.0;
}

static double CalcSurfacePositionF( long *next_segment, double distance, double z_shift )
{
	double surface_position = distance - (floor(distance / 256.0) * 256.0);

	*next_segment = FALSE;
	surface_position += z_shift;
	if (surface_position >= 256.0)
	{
		*next_segment = TRUE;
		surface_position -= 256.0;
	}

	return(surface_position);
}

// Half the x distance the opponent's rear wheels span, interpolated between the
// opponents_x_spans[] entries instead of snapping to one of 32 buckets. The
// bucket edges are the loop that turns a sub-unit height wobble into a visible
// jump: the span moves the wheels, which moves the heights, which can move the
// span back.
static double OpponentsXSpanF( double height_difference )
{
	double t = fabs(height_difference) / 16.0;
	if (t < 0.0) t = 0.0;
	if (t > static_cast<double>(NUM_X_SPANS-1)) t = static_cast<double>(NUM_X_SPANS-1);

	long i0 = static_cast<long>(floor(t));
	if (i0 > NUM_X_SPANS-2) i0 = NUM_X_SPANS-2;
	double frac = t - static_cast<double>(i0);

	return static_cast<double>(opponents_x_spans[i0]) +
		   (frac * static_cast<double>(opponents_x_spans[i0+1] - opponents_x_spans[i0]));
}

static void CalculateOpponentsRoadWheelPositionsF( void )
{
long piece = opponents_current_piece, next_segment;
long segment;
double distance, surface_position;
double left_side_x, left_side_z, right_side_x, right_side_z;
bool draw_shadow = TRUE;

	// Continuous road x position, clamped the way OppFloatV2Sync() clamps it
	double road_x = gOppF.roadX;
	if (road_x <   0.0) road_x =   0.0;
	if (road_x > 255.0) road_x = 255.0;

	for (long i = 0; i < NUM_OPP_WHEEL_POSITIONS; i++)
		oppf_act[i] = gOppF.act[i];

	/*
	 * Rear wheels
	 */
	distance = gOppF.distance - 64.0;
	if (distance < 0.0)
	{
		// DIRECTION DEPENDANT

		// go to previous piece
		piece--; if (piece < 0) piece = (NumTrackPieces - 1);

		distance += static_cast<double>(Track[piece].numSegments * 256);
	}

	// Fetch 4 surface co-ords surrounding rear wheels
	segment = static_cast<long>(floor(distance / 256.0));
	if (segment < 0) segment = 0;
	if (segment >= Track[piece].numSegments) segment = Track[piece].numSegments - 1;
	GetSurfaceCoords(piece, segment);
	// Don't draw opponent's shadow on black road segments
	if (Track[piece].roadColour[segment] == SCR_BASE_COLOUR + 0)
		draw_shadow = FALSE;

	// Calculate segment left side x,z
	surface_position = CalcSurfacePositionF(&next_segment, distance, static_cast<double>(B1bbbe[0]));
	if (!next_segment)
	{
		left_side_x = sx2 + ((surface_position * (sx1-sx2)) / 256.0);
		left_side_z = sz2 + ((surface_position * (sz1-sz2)) / 256.0);
	}
	else
	{
		// Use other end's value as base (Amiga quirk, mirrored from the integer path)
		left_side_x = sx1 + ((surface_position * (sx1-sx2)) / 256.0);
		left_side_z = sz1 + ((surface_position * (sz1-sz2)) / 256.0);
	}

	// Calculate segment right side x,z
	surface_position = CalcSurfacePositionF(&next_segment, distance, static_cast<double>(B1bbbe[2]));
	if (!next_segment)
	{
		right_side_x = sx3 + ((surface_position * (sx4-sx3)) / 256.0);
		right_side_z = sz3 + ((surface_position * (sz4-sz3)) / 256.0);
	}
	else
	{
		// Use other end's value as base (Amiga quirk, mirrored from the integer path)
		right_side_x = sx4 + ((surface_position * (sx4-sx3)) / 256.0);
		right_side_z = sz4 + ((surface_position * (sz4-sz3)) / 256.0);
	}

	double opponents_x_span = OpponentsXSpanF(oppf_act[REAR_LEFT] - oppf_act[REAR_RIGHT]);

	// Reduce the span for the shadow, to take into account the greater width of
	// sloped segments
	double xd = right_side_x - left_side_x;
	double yd = static_cast<double>(sy3 - sy2) / LOCAL_Y_FACTOR;
	double zd = right_side_z - left_side_z;
	double base_width = sqrt((xd*xd) + (zd*zd));
	double slope_width = sqrt((base_width*base_width) + (yd*yd));
	double opponents_shadow_x_span = (slope_width > 0.0)
		? ((opponents_x_span * base_width) / slope_width)
		: opponents_x_span;

	/*	The footprint above is the car's road width - what the wheels ran on when the car
		was as wide as its contact patch at both ends. The drawn car isn't: the rears are
		pulled in a little and the fronts a long way, so a shadow of that footprint spills
		out from under the car, worst at the nose. Narrow each end to the wheels that are
		actually there and the shadow becomes the car's own outline: a trapezium, wide at
		the back, tucked in at the front.											*/
	double shadow_span_rear  = opponents_shadow_x_span
							 * (2.0 * WHEEL_REAR_OUTER)  / static_cast<double>(VCAR_WIDTH);
	double shadow_span_front = opponents_shadow_x_span
							 * (2.0 * WHEEL_FRONT_OUTER) / static_cast<double>(VCAR_WIDTH);

	double sx, sz;
	sz = distance - (floor(distance / 256.0) * 256.0);	// z position of rear wheels

	COORD_3D_F shadow_rear_left, shadow_rear_right, shadow_front_left, shadow_front_right;

	// Rear left road co-ordinate
	sx = road_x - opponents_x_span;
	oppf_rear_left_road_pos.y = CalculateOpponentsRoadWheelHeightF(sx, sz);
	oppf_rear_left_road_pos.x = left_side_x + ((sx * xd) / 256.0);
	oppf_rear_left_road_pos.z = left_side_z + ((sx * zd) / 256.0);

	// Rear left shadow co-ordinate
	sx = road_x - shadow_span_rear;
	shadow_rear_left.y = CalculateOpponentsRoadWheelHeightF(sx, sz);
	shadow_rear_left.x = left_side_x + ((sx * xd) / 256.0);
	shadow_rear_left.z = left_side_z + ((sx * zd) / 256.0);

	// Rear right road co-ordinate
	sx = road_x + opponents_x_span;
	oppf_rear_right_road_pos.y = CalculateOpponentsRoadWheelHeightF(sx, sz);
	oppf_rear_right_road_pos.x = left_side_x + ((sx * xd) / 256.0);
	oppf_rear_right_road_pos.z = left_side_z + ((sx * zd) / 256.0);

	// Rear right shadow co-ordinate
	sx = road_x + shadow_span_rear;
	shadow_rear_right.y = CalculateOpponentsRoadWheelHeightF(sx, sz);
	shadow_rear_right.x = left_side_x + ((sx * xd) / 256.0);
	shadow_rear_right.z = left_side_z + ((sx * zd) / 256.0);

	// Position rear co-ordinates within world
	double piece_x = static_cast<double>(Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION));
	double piece_z = static_cast<double>(Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION));
	oppf_rear_left_road_pos.x  += piece_x;	oppf_rear_left_road_pos.z  += piece_z;
	oppf_rear_right_road_pos.x += piece_x;	oppf_rear_right_road_pos.z += piece_z;
	shadow_rear_left.x  += piece_x;			shadow_rear_left.z  += piece_z;
	shadow_rear_right.x += piece_x;			shadow_rear_right.z += piece_z;

	/*
	 * Front wheels
	 */
	double diff, xdiff, zdiff;

	// Front left and right road x,z co-ordinates
	diff = oppf_rear_right_road_pos.x - oppf_rear_left_road_pos.x;
	xdiff = diff * 1.5;	// car length is 1.5 times width
	diff = oppf_rear_right_road_pos.z - oppf_rear_left_road_pos.z;
	zdiff = diff * 1.5;	// car length is 1.5 times width
	oppf_front_left_road_pos.x  = oppf_rear_left_road_pos.x  - zdiff;
	oppf_front_left_road_pos.z  = oppf_rear_left_road_pos.z  + xdiff;
	oppf_front_right_road_pos.x = oppf_rear_right_road_pos.x - zdiff;
	oppf_front_right_road_pos.z = oppf_rear_right_road_pos.z + xdiff;

	// Front left and right shadow x,z co-ordinates. The wheelbase is 1.5 times the car's
	// full width, not the narrowed rear track, so the length still comes off the footprint
	// - only the width at each end is the drawn car's.
	double footprint_x = (2.0 * opponents_shadow_x_span * xd) / 256.0;
	double footprint_z = (2.0 * opponents_shadow_x_span * zd) / 256.0;
	xdiff = footprint_x * 1.5;
	zdiff = footprint_z * 1.5;

	double half_front_x = (shadow_span_front * xd) / 256.0;
	double half_front_z = (shadow_span_front * zd) / 256.0;
	double front_mid_x = ((shadow_rear_left.x + shadow_rear_right.x) / 2.0) - zdiff;
	double front_mid_z = ((shadow_rear_left.z + shadow_rear_right.z) / 2.0) + xdiff;

	shadow_front_left.x  = front_mid_x - half_front_x;
	shadow_front_left.z  = front_mid_z - half_front_z;
	shadow_front_right.x = front_mid_x + half_front_x;
	shadow_front_right.z = front_mid_z + half_front_z;

	// Add 128 to get z of opponent's front
	distance += 128.0;
	if (distance >= static_cast<double>(Track[piece].numSegments * 256))
	{
		// DIRECTION DEPENDANT

		distance -= static_cast<double>(Track[piece].numSegments * 256);

		// go to next piece
		piece++; if (piece > (NumTrackPieces - 1)) piece = 0;
	}
	// Fetch 4 surface co-ords surrounding front wheels
	segment = static_cast<long>(floor(distance / 256.0));
	if (segment < 0) segment = 0;
	if (segment >= Track[piece].numSegments) segment = Track[piece].numSegments - 1;
	GetSurfaceCoords(piece, segment);
	// Don't draw opponent's shadow on black road segments
	if (Track[piece].roadColour[segment] == SCR_BASE_COLOUR + 0)
		draw_shadow = FALSE;

	sz = distance - (floor(distance / 256.0) * 256.0);	// z position of front wheels

	// Front road y co-ordinate (centre)
	oppf_front_road_pos_y = CalculateOpponentsRoadWheelHeightF(road_x, sz);

	// Front left / right road y co-ordinates
	oppf_front_left_road_pos.y  = CalculateOpponentsRoadWheelHeightF(road_x - opponents_x_span, sz);
	oppf_front_right_road_pos.y = CalculateOpponentsRoadWheelHeightF(road_x + opponents_x_span, sz);

#ifdef OPPONENT_SHADOW
	shadow_front_left.y  = CalculateOpponentsRoadWheelHeightF(road_x - shadow_span_front, sz);
	shadow_front_right.y = CalculateOpponentsRoadWheelHeightF(road_x + shadow_span_front, sz);

	// Y co-ordinates need to be divided by 4 for display, but they're
	// already /2 because are in Amiga format (i.e. not * PC_FACTOR).
	// SHADOW_ABOVE_ROAD keeps the shadow off the road so it isn't clipped as much
	D3DXVECTOR3 v1, v2, v3, v4;
	const float lift = static_cast<float>(SHADOW_ABOVE_ROAD);
	v2 = D3DXVECTOR3( static_cast<float>(shadow_rear_left.x),   lift + static_cast<float>(shadow_rear_left.y/2),   static_cast<float>(shadow_rear_left.z) );
	v3 = D3DXVECTOR3( static_cast<float>(shadow_rear_right.x),  lift + static_cast<float>(shadow_rear_right.y/2),  static_cast<float>(shadow_rear_right.z) );
	v1 = D3DXVECTOR3( static_cast<float>(shadow_front_left.x),  lift + static_cast<float>(shadow_front_left.y/2),  static_cast<float>(shadow_front_left.z) );
	v4 = D3DXVECTOR3( static_cast<float>(shadow_front_right.x), lift + static_cast<float>(shadow_front_right.y/2), static_cast<float>(shadow_front_right.z) );

	RemoveShadowTriangles();
	if (draw_shadow)
	{
		StoreShadowTriangle(v2, v1, v3, 0);
		StoreShadowTriangle(v1, v4, v3, 0);
	}
#else
	(void)draw_shadow;
	(void)shadow_front_left;
	(void)shadow_front_right;
#endif
}


/*	======================================================================================= */
/*	Function:		ComputeOpponentRenderStateF												*/
/*																							*/
/*	Description:	The tail of OpponentBehaviour() -- centre point and orientation --		*/
/*					computed from the continuous mirror. Implements the NEW_OPP_METHOD		*/
/*					semantics only, which is what the integer path is built with.			*/
/*	======================================================================================= */

static void ComputeOpponentRenderStateF( long *x, long *y, long *z,
										 float *x_angle, float *y_angle, float *z_angle )
{
	/*
	 * Centre x, z
	 */
	double opponent_x = (oppf_front_left_road_pos.x + oppf_front_right_road_pos.x +
						 oppf_rear_left_road_pos.x  + oppf_rear_right_road_pos.x) / 4.0;
	double opponent_z = (oppf_front_left_road_pos.z + oppf_front_right_road_pos.z +
						 oppf_rear_left_road_pos.z  + oppf_rear_right_road_pos.z) / 4.0;

	/*
	 * Centre y -- the car rides on whichever is higher, road or actual height
	 */
	double vis_rear_left_y  = (oppf_rear_left_road_pos.y  > oppf_act[REAR_LEFT])  ? oppf_rear_left_road_pos.y  : oppf_act[REAR_LEFT];
	double vis_rear_right_y = (oppf_rear_right_road_pos.y > oppf_act[REAR_RIGHT]) ? oppf_rear_right_road_pos.y : oppf_act[REAR_RIGHT];
	double vis_front_y      = (oppf_front_road_pos_y      > oppf_act[FRONT])      ? oppf_front_road_pos_y      : oppf_act[FRONT];

	double rear_y = (vis_rear_left_y + vis_rear_right_y) / 2.0;
	double opponent_y = (rear_y + vis_front_y) / 2.0;

	/*	Raise the opponent slightly, so it doesn't sink into the road. The integer path uses
		20 because its heights are coarse; these are not, so lift the car by exactly what the
		shadow is lifted by. The two then share a plane - the wheels stand on their own
		shadow instead of hovering a third of a wheel above it.						*/
	opponent_y += static_cast<double>(CAR_LIFT_ABOVE_ROAD);

	/*
	 * Angles
	 */
	// Along car's x axis, only use y and z components
	// (y is halved because of unit differences between y and x,z)
	double yd = (rear_y - vis_front_y) / 2.0;
	double rear_x  = (oppf_rear_left_road_pos.x  + oppf_rear_right_road_pos.x)  / 2.0;
	double rear_z  = (oppf_rear_left_road_pos.z  + oppf_rear_right_road_pos.z)  / 2.0;
	double front_x = (oppf_front_left_road_pos.x + oppf_front_right_road_pos.x) / 2.0;
	double front_z = (oppf_front_left_road_pos.z + oppf_front_right_road_pos.z) / 2.0;
	double xd = rear_x - front_x;
	double zd = rear_z - front_z;
	double carzd = sqrt((xd*xd) + (zd*zd));
	*x_angle = static_cast<float>(atan2(yd, carzd));

	// Along car's y axis, only use x and z components
	xd = oppf_rear_left_road_pos.x - oppf_rear_right_road_pos.x;
	zd = oppf_rear_left_road_pos.z - oppf_rear_right_road_pos.z;
	*y_angle = static_cast<float>(atan2(zd, -xd));

	// Along car's z axis, only use x and y components
	yd = (vis_rear_left_y - vis_rear_right_y) / 2.0;
	double carxd = sqrt((xd*xd) + (zd*zd));
	*z_angle = static_cast<float>(atan2(-yd, carxd));

	/*
	 * Output. Rounding happens only after the shift into world units, so the
	 * sub-unit precision that the integer path threw away survives.
	 */
	*x = static_cast<long>(floor((opponent_x * (1 << LOG_PRECISION)) + 0.5));
	*z = static_cast<long>(floor((opponent_z * (1 << LOG_PRECISION)) + 0.5));
	*y = -static_cast<long>(floor((opponent_y * (1 << (LOG_PRECISION-3)) * LOCAL_Y_FACTOR) + 0.5));
}
