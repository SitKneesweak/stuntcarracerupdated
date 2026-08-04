/**************************************************************************

    Car Behaviour.cpp - Functions relating to player's car behaviour

	NOTE: Best to always start a car off on a straight (non-diagonal) piece

	NOTE: All statics and globals that have been initialised will need to be
		  reinitialised when the car is repositioned on the track (e.g. at
		  start of race and when put back on track after coming off)

	NOTE: player_x and player_z are in PC StuntCarRacer format
		  player_y is currently in Amiga StuntCarRacer format
		  Angles have the same magnitude but are unsigned.
		  And sin/cos/tan also need alteration (divide by 2)


/* OUTSTANDING ISSUES / IMPROVEMENTS :-

	1.  SOME FUNCTIONS CONTAIN LOGIC THAT IS DEPENDANT UPON THE ORDER THAT THE
		TRACK PIECES ARE STORED IN (I.E. WHETHER THE PIECE NUMBERS INCREMENT
		AROUND THE TRACK IN A CLOCKWISE OR ANTI-CLOCKWISE MANNER)
	
		LINES THAT HAVE ALREADY BEEN IDENTIFIED HAVE THE COMMENT :-

				// DIRECTION DEPENDANT

		- NEED TO IMPLEMENT A TRACK DIRECTION VALUE (1 OR -1) THAT IS EITHER
		STORED WITH THE TRACK DEFINITION OR CALCULATED AT THE START OF A RACE.
		THE LOGIC WOULD THEN USE THIS VALUE RATHER THAN JUST ADDING OR
		SUBTRACTING 1.

 **************************************************************************/

/*
 * Unfortunately these HIGHER_FRAME_RATE changes didn't work...
 *
//#define	HIGHER_FRAME_RATE	// to cater for four times frame rate
// 10/12/1998 - engine_z_acceleration not now reduced (to enable car to jump as before)
//			  - could try limiting the top speed instead (should be better)
//	or try REDUCTION of 202, INCREASE of 306
*/

/*	============= */
/*	Include files */
/*	============= */
#include "dxstdafx.h"

#include <stdlib.h>

#include "StuntCarRacer.h"
#include "Car.h"
#include "Car_Behaviour.h"
#include "Opponent_Behaviour.h"
#include "Track.h"
#include "3D_Engine.h"
#include "XBOXController.h"
#include "Physics_FloatV2.h"
#include "Det_Rand.h"

/*	===== */
/*	Debug */
/*	===== */
//#define USE_AMIGA_RECORDING
//#define TEST_AMIGA_UER

//#define PLAY_AMIGA_RECORDING

#if defined(DEBUG) || defined(_DEBUG)
extern FILE *out;
extern bool bTestKey;
#endif

/*	========= */
/*	Constants */
/*	========= */
#ifndef SCR_PORTABLE
#define	FALSE	0
#define	TRUE	1
#endif

#define	MAX_AMIGA_VOLUME	64
#define DIRECTX_VOLUME_FACTOR	100		// Volume units are in hundredths of decibels

// car behaviour definitions
#define	GRAVITY_ACCELERATION	317		// used to be called CAR_WEIGHT

#define	ROAD_WIDTH	0x0180

#define	SURFACE_SIZE	1024	// used when interpolating (Amiga StuntCarRacer used 256, but 1024 is smoother!)
#define	LOG_SURFACE_SIZE	10	// to base 2

#define	OFF_ROAD_HEIGHT	0x1000

#define	OFF_TRACK_LIMIT	64		// count after which player is put back on track

#define	WRECKED			(wreck_wheel_height_reduction != 0)
#define	NOT_WRECKED		(wreck_wheel_height_reduction == 0)

#define LOCAL_Y_FACTOR	4

/*	=========== */
/*	Global data */
/*	=========== */
long player_current_piece = 0;	// use as players_road_section
long player_current_segment = 0;
long players_distance_into_section = 0;
long players_road_x_position = 0;
long rear_wheel_surface_x_position = 0;
bool drop_start_done = TRUE;
long touching_road = FALSE;
long player_y;
long player_z_speed = 0;

long front_left_damage = 0,
	 front_right_damage = 0,
	 rear_damage = 0;
long damaged = 0;
long new_damage = 0;
long nholes = 0;

long car_collision_x_acceleration,
	 car_collision_y_acceleration,
	 car_collision_z_acceleration;

long boostReserve = 0, boostUnit = 0;
long playerLapNumber;

static CXBOXController P1Controller(1);


#if defined(DEBUG) || defined(_DEBUG)
//long CCPIECE, CCSEGMENT, CCSURFACEX, CCSURFACEZ;
bool debug_position = FALSE;
#endif

extern IDirectSoundBuffer8 *WreckSoundBuffer;
extern IDirectSoundBuffer8 *GroundedSoundBuffer;
extern IDirectSoundBuffer8 *CreakSoundBuffer;
extern IDirectSoundBuffer8 *SmashSoundBuffer;
extern IDirectSoundBuffer8 *OffRoadSoundBuffer;

extern bool bSuperLeague;
extern int wideScreen;

/*	=========== */
/*	Static data */
/*	=========== */
static long player_x,
			player_z;

static long player_x_angle = 0,
			player_y_angle = 0,
			player_z_angle = 0;

static long player_world_x_speed = 0,
			player_world_y_speed = 0,
			player_world_z_speed = 0;

static long player_x_speed = 0,
			player_y_speed = 0;

static long accelerate, brake;

static long accelerating = FALSE;	// to remember previous control state

long engine_power = 240;		// (240 standard, 320 super)
long boost_unit_value = 16;	// (16 standard, 12 super)

static long left_right_value;
static long engine_z_acceleration;
/*static*/ long boost_activated;

static long rear_wheel_x_offset, rear_wheel_z_offset;
static long front_left_wheel_x_offset, front_left_wheel_z_offset;
static long front_right_wheel_x_offset, front_right_wheel_z_offset;

static long front_left_road_height = OFF_ROAD_HEIGHT;
static long front_right_road_height = OFF_ROAD_HEIGHT;
static long rear_road_height = OFF_ROAD_HEIGHT;

// wheel heights
static long front_left_actual_height;
static long front_right_actual_height;
static long rear_actual_height;

static long off_left, off_right;
static long wheel_off_road, distance_off_road;
static long at_side_byte, which_side_byte;
static long smaller_limit_required = FALSE;


static long wreck_wheel_height_reduction = 0;		// 0x200 if wrecked

	// Amiga StuntCarRacer's crane sequence (lift.car.onto.track, StuntCarRacer.s:7869).
	// car_on_chains_countdown is kept in the Amiga's 0..255 byte form because the
	// release test is a *signed* byte one - values >= 128 mean "still hanging" - and
	// because the countdown doubles as left.right.value while the car is chained.
static long car_on_chains_countdown = 0;
#define	ON_CHAINS	(car_on_chains_countdown != 0)

	// swing.car state.  The car does not oscillate: it hangs rolled to one side and
	// the roll decays towards +/-16 (high byte) before the crane lets go.
static long swing_from_left = FALSE;
static long swing_magnitude = 0;
static long required_raise_height = 0;

	// Set in CarControl, read by the "press fire to be dropped" path of a re-lift.
static long chain_fire_pressed = FALSE;

	// The crane runs on Amiga frames, not physics steps: the countdowns below are
	// frame counts, so FloatV2's variable dt has to be accumulated back into them.
static double chain_frame_phase = 0.0;

	// The Amiga gated the drop-start countdown on fourteen.frames.elapsed, the 238/256
	// clock built in display.lap.time (StuntCarRacer.s:10775), so the countdown skips
	// roughly one frame in fourteen.  The port's fourteen_frames_elapsed is maintained
	// by the FloatV2 opponent step, which does not run until drop_start_done - so the
	// crane keeps its own accumulator at the same rate rather than waiting on a clock
	// that is stopped for exactly as long as the car is hanging.
static long chain_frame_fraction = 0;

	// Touchdown, an addition to the Amiga sequence.  Its raise amounts run 3, 4, 2
	// (StuntCarRacer.s:7899/7912/7953), so the car already descends as the hang
	// begins - just not far enough to reach the road.  These frames extend that dip
	// to a landing: the crane sets the car down on the track, holds it there, and
	// then hoists it back to the hang height before letting go.  Frame counts, so
	// like the rest of the crane they are rate-independent.
#define	CHAIN_TOUCHDOWN_FRAMES	14		// ~1.4s: the descent, plus a beat on the deck
	// Raise amount while planted.  raise.car.off.ground's lift is 256 - d3/8, and
	// with the car at road level d3 is -(amount << 8) - so at -8 the lift is exactly
	// zero and the car rests on its springs under its own weight, at the same ride
	// height it would have parked at with no crane attached.  Shallower than that and
	// the crane is still taking part of the weight and the suspension sits half
	// extended; deeper and it presses the car down harder than gravity does.
	// SCR_CRANE_TOUCHDOWN=amount[,frames] overrides both for tuning by eye.
#define	CHAIN_TOUCHDOWN_AMOUNT	(-8)
static long chain_touchdown_frames = 0;

	// Release guard, see lift.car.stage3.  The Amiga lets go the instant its random
	// timer expires; this port additionally refuses to let go while the car is out
	// over the edge of the road, for a bounded number of frames.
static long chain_release_hold = 0;
static long chain_last_road_x  = -1;

static long player_distance_off_road;	// used to determine the value below
static long off_map_status = 0;	// not set exactly like Amiga StuntCarRacer

static long off_track_count = 0;

static long gravity_x_acceleration,
			gravity_y_acceleration,
			gravity_z_acceleration;

static long grounded_delay = 0;
static long grounded_count = 0;
static long damage_value = 0;
static long damaged_count = 0;

long front_left_amount_below_road = 0,
			front_right_amount_below_road = 0,
			rear_amount_below_road = 0;

long front_left_wheel_speed = 0,
	 		front_right_wheel_speed = 0;
long leftwheel_angle = 0, rightwheel_angle = 0;

static long old_front_left_difference = 0,
			old_front_right_difference = 0,
			old_rear_difference = 0;

static long smashed_countdown = 0;

static long car_to_road_collision_z_acceleration;

static long player_x_acceleration,
			player_y_acceleration,
			player_z_acceleration;

static long total_world_x_acceleration,
			total_world_y_acceleration,
			total_world_z_acceleration;

static long player_x_rotation_speed = 0,
			player_y_rotation_speed = 0,
			player_z_rotation_speed = 0;

static long player_final_x_rotation_speed,
			player_final_y_rotation_speed,
			player_final_z_rotation_speed;

static long player_x_rotation_acceleration,
			player_y_rotation_acceleration,
			player_z_rotation_acceleration;

static long Replay = FALSE, ReplayRequested = FALSE, ReplayLooping = FALSE, ReplayFinished = FALSE;

#ifdef USE_AMIGA_RECORDING
static bool ReplayAmigaRecording = FALSE;
static bool StartOfAmigaRecording = FALSE;
static long AmigaRecordingFrame = 0;
#endif

/*	===================== */
/*	Function declarations */
/*	===================== */
static void CarControl (DWORD input);
static void BoostPower (long boost_flag,
						long accelerate,
						long brake);

// Defined further down with the other legacy<->FloatV2 adapter helpers; the
// debug dump in CarMovement prints the value FloatV2 actually receives.
namespace scr { static long FV2_PlayersRoadXPosition (long roadX); }

static void CarMovement (void);
static void UpdateOffMapStatus (void);
static long GetPieceUsingMap (long x, long z, long *piece_out);
static void CalcXZRelativeToPiece (long x, long z, long piece, long *rx_out, long *rz_out);

static void CalculateWheelXZOffsets (void);

static void CalculateRoadWheelHeights (void);
static void CalculateRoadWheelHeight (long height, long *height_out);
static void CalculateIfCarOffRoad (long *height);
static void CalculateWorldRoadHeight (long wheel, long x, long z, long *y_out);

static void GetSurfaceCoords (long piece, long segment);

// Front-left surface position as legacy computes it, for the FloatV2 dump
// (written in CalculateWorldRoadHeight).
static long gDbgLegacyPiece = 0, gDbgLegacySeg = 0, gDbgLegacyZFrac = 0, gDbgLegacyXFrac = 0;
static long CalcDistanceOffRoad (long x, long z,
								 long ox, long oz,
								 long ux, long uz,
								 long vx, long vz,
								 long *ex, long *ez);
static void CalcSurfacePosition (long piece,
								 long x, long z,
								 long ox, long oz,
								 long ux, long uz,
								 long vx, long vz,
								 long *sx, long *sz,
								 long *road_x,
								 long *segment_out);

static void CalculateActualWheelHeights (void);
static void CalculateXZSpeeds (void);
static void CalculateGravityAcceleration (void);
static void CarCollisionDetection (void);
static void PlayGroundedSound (void);
static void CalculateWheelCollision (long road_height,
									 long actual_height,
									 long *height_difference_out,
									 long *old_difference_in_out,
									 long *amount_below_road_in_out,
									 long *damage_in_out);
static void CalculateCarCollisionAcceleration (long average_amount_below_road);
static void CalculateInclinationSinCos (long inclination_in,
										long *inclination_sin_out,
										long *inclination_cos_out);
static void LiftCarOntoTrack (void);
static void LiftCarOntoTrackFloatV2 (double dt);

static void CalculateTotalAcceleration (void);
static long GetTwiceCollisionYAcceleration (void);
static void CalculateXAcceleration (void);

static void CalculateSteering (void);
static void CalculateSteeringAcceleration (long steering_amount);
static void AlignCarWithRoad (void);
static void AdjustSteeringAcceleration (void);
static void IdentifyPiece (long x, long z, long *piece_in_out);
static void GetPieceCoords (long piece);

static void CalculateWorldAcceleration (void);
static void ReduceWorldAcceleration (void);

static void CalculateXZRotationAcceleration (void);
static void UpdatePlayersRotationSpeed (void);
static void CalculateFinalRotationSpeed (void);
static void UpdatePlayersWorldSpeed (void);
static void UpdatePlayersPosition (void);

static long CalcSectionYAngle (long piece,
							   long x,
							   long z);
static void CalcCurveMeasurements (long piece,
								   long x,
								   long z,
								   long *y_angle_out,
								   long *radius_out,
								   double *distance_from_centre_out);

static void PositionCarAbovePiece (long piece);
static void UpdateEngineRevs (void);
static bool DrawDustClouds (void);
static bool DrawSparks (void);
static void UpdateSparks (bool emit_sparks, bool emit_dust);
static void InitialiseSparksTable (void);

// Stands in for main.loop.count, which only the dust clouds use (to cycle the puff shape).
static long spark_step_count = 0;
static void SetWheelRotationSpeed();

#ifdef NOT_USED
static void RewindRecording (void);
static void Record (DWORD input);
static void PlayBack (DWORD *input);
static void ReadRecordedFile (void);
#endif

#ifdef USE_AMIGA_RECORDING
static bool OpenAmigaRecording( void );
#endif

/*	======================================================================================= */
/*	Function:		ResetPlayer																*/
/*																							*/
/*	Description:	Reset all car behaviour variables to their initial state				*/
/*	======================================================================================= */

void ResetPlayer (void)
	{
	// resets almost everything at the moment, just to make sure
	player_x = 0;
	player_y = 0;
	player_z = 0;

	player_x_angle = 0;
	player_y_angle = 0;
	player_z_angle = 0;

	player_world_x_speed = 0;
	player_world_y_speed = 0;
	player_world_z_speed = 0;

	// calculated
	player_x_speed = 0;
	player_y_speed = 0;
	player_z_speed = 0;

	accelerating = FALSE;

	engine_power = (bSuperLeague)?320:240;		// (240 standard, 320 super)
	boost_unit_value = (bSuperLeague)?12:16;	// (16 standard, 12 super)

	// calculated
	left_right_value = 0;
	engine_z_acceleration = 0;
	boost_activated = 0;

	// calculated
	rear_wheel_x_offset = 0, rear_wheel_z_offset = 0;
	front_left_wheel_x_offset = 0, front_left_wheel_z_offset = 0;
	front_right_wheel_x_offset = 0, front_right_wheel_z_offset = 0;

	front_left_road_height = OFF_ROAD_HEIGHT;
	front_right_road_height = OFF_ROAD_HEIGHT;
	rear_road_height = OFF_ROAD_HEIGHT;

	// calculated
	front_left_actual_height = 0;
	front_right_actual_height = 0;
	rear_actual_height = 0;

	// calculated
	off_left = 0, off_right = 0;
	wheel_off_road = 0, distance_off_road = 0;
	at_side_byte = 0, which_side_byte = 0;

	// initialise.sparks.table - the car has been repositioned, so any sparks or dust still
	// in flight belong to wherever it used to be
	InitialiseSparksTable();

	smaller_limit_required = FALSE;

	wreck_wheel_height_reduction = 0;		// 0x200 if wrecked

	drop_start_done = TRUE;
	touching_road = FALSE;

	// Crane state.  PositionCarAbovePiece puts the car back on the chains; until
	// then it is simply not hanging.
	// swing_from_left is deliberately not cleared here: it records which side the
	// car went off, and UpdateOffMapStatus set it well before this reset runs.
	car_on_chains_countdown = 0;
	swing_magnitude = 0;
	required_raise_height = 0;
	chain_fire_pressed = FALSE;
	chain_frame_phase = 0.0;
	chain_frame_fraction = 0;
	chain_release_hold = 0;
	chain_last_road_x = -1;

	// calculated
	player_distance_off_road = 0;
	off_map_status = 0;
	//off_track_count = 0;	// now done in CarBehaviour

	// calculated
	gravity_x_acceleration = 0;
	gravity_y_acceleration = 0;
	gravity_z_acceleration = 0;

	grounded_delay = 0;

	// calculated
	grounded_count = 0;
	damage_value = 0;

	damaged_count = 0;
	damaged = 0;

	front_left_amount_below_road = 0;
	front_right_amount_below_road = 0;
	rear_amount_below_road = 0;

	old_front_left_difference = 0;
	old_front_right_difference = 0;
	old_rear_difference = 0;

	front_left_damage = 0;
	front_right_damage = 0;
	rear_damage = 0;

	new_damage = 0;
	smashed_countdown = 0;
	nholes = 0;

	// calculated
	car_collision_x_acceleration = 0;
	car_collision_y_acceleration = 0;
	car_collision_z_acceleration = 0;
	car_to_road_collision_z_acceleration = 0;

	// calculated
	player_x_acceleration = 0;
	player_y_acceleration = 0;
	player_z_acceleration = 0;

	// calculated
	total_world_x_acceleration = 0;
	total_world_y_acceleration = 0;
	total_world_z_acceleration = 0;

	player_x_rotation_speed = 0;
	player_y_rotation_speed = 0;
	player_z_rotation_speed = 0;

	// calculated
	player_final_x_rotation_speed = 0;
	player_final_y_rotation_speed = 0;
	player_final_z_rotation_speed = 0;

	// calculated
	player_x_rotation_acceleration = 0;
	player_y_rotation_acceleration = 0;
	player_z_rotation_acceleration = 0;

	// The FloatV2 state is a copy of these globals, not a view of them, so it
	// has to be re-seeded — otherwise the step immediately overwrites the reset
	// position with its own and the car never actually moves. Callers reposition
	// the car after this returns; the seed happens on the next step, so it picks
	// up the final position.
	scr::gFloatV2NeedsSeed = true;
	return;
	}


/*	======================================================================================= */
/*	Function:		CarBehaviour															*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

// eventually make access functions for the following, remove extern definitions
extern bool bNewGame;

extern long TrackID;
extern TRACK_PIECE Track[MAX_PIECES_PER_TRACK];
extern long Track_Map[NUM_TRACK_CUBES][NUM_TRACK_CUBES];	// [x][z]
extern long NumTrackPieces;
extern long PlayersStartPiece;
extern long StartLinePiece;
extern long StandardBoost, SuperBoost;	// league boost maxima, from the track data
extern long HalfALapPiece;


long INITIALISE_PLAYER = TRUE;


// Set once PlaceCarOnChainsForNewGame has hung the car on the chains, so that
// CarBehaviour's own bNewGame reset does not immediately do it all again (which
// would throw away the crane frames that have run in the meantime).
static long new_game_placement_done = FALSE;


/*	======================================================================================= */
/*	Function:		PlaceCarOnChainsForNewGame												*/
/*																							*/
/*	Description:	The new-game reset, pulled out of CarBehaviour so it can be done at		*/
/*					the moment the race starts rather than on the first physics step.		*/
/*																							*/
/*					CarBehaviour only runs when a physics step is due, and the render		*/
/*					frame that starts the race is neither guaranteed to run one nor even	*/
/*					to call FrameMove (the 60fps cap can skip it).  That left the first		*/
/*					frame or two of the race drawn from the track preview's car position -	*/
/*					on the ground, beside the start piece - before the car snapped up onto	*/
/*					the crane.																*/
/*	======================================================================================= */

void PlaceCarOnChainsForNewGame (long *x,
								 long *y,
								 long *z,
								 long *x_angle,
								 long *y_angle,
								 long *z_angle)
	{
	ResetPlayer();

	ResetDrawBridge();
	ReplayFinished = FALSE;

	// A start-of-race drop start, not a re-lift: from high up, on a timer.
	drop_start_done = FALSE;
	PositionCarAbovePiece(PlayersStartPiece);

	off_track_count = 0;
	new_game_placement_done = TRUE;

	// The car is where it is going to be, so CarBehaviour must not overwrite it
	// from the values passed in.
	INITIALISE_PLAYER = FALSE;

	*x = player_x;
	*y = -(player_y * LOCAL_Y_FACTOR);
	*z = player_z;

	// Same angle conventions as CarBehaviour's output block below.
	*x_angle = (-player_x_angle & (MAX_ANGLE - 1));
	*y_angle = (player_y_angle & (MAX_ANGLE - 1));
	*z_angle = (-player_z_angle & (MAX_ANGLE - 1));
	}


void CarBehaviour (DWORD input,
				   long *x,
				   long *y,
				   long *z,
				   long *x_angle,
				   long *y_angle,
				   long *z_angle)
	{
	static long first_time = TRUE;

	// temporarily set player values to values provided when required
	if (INITIALISE_PLAYER)
		{
		INITIALISE_PLAYER = FALSE;

		if (! Replay)
			{
			player_x = *x;
			player_y = -(*y / LOCAL_Y_FACTOR);
			player_z = *z;
			player_x_angle = (*x_angle);
			player_y_angle = (*y_angle);
			player_z_angle = (*z_angle);
			}
		}


	// off_track_count counts physics steps, and OFF_TRACK_LIMIT is in Amiga
	// 50Hz frames, so scale it when FloatV2 is running at some other rate.
	long off_track_limit = OFF_TRACK_LIMIT;
	if (scr::gUseFloatV2Physics && (scr::gFloatV2Dt > 0.0))
		off_track_limit = lround(OFF_TRACK_LIMIT * (0.02 / scr::gFloatV2Dt));

	if (! bNewGame)
		new_game_placement_done = FALSE;

	// reset player and control action replay as required.  A new game whose car has
	// already been hung on the chains (PlaceCarOnChainsForNewGame, at the moment the
	// race started) is skipped here - redoing it would restart the crane sequence.
	if ((off_track_count > off_track_limit) ||
	    (bNewGame && ! new_game_placement_done) ||
		(ReplayRequested))
		{
		// Going off the track and being craned back on is not a fresh car: the
		// damage taken so far has to survive the reset.  ResetPlayer clears the
		// lot (it is also the new-game reset), so carry the accumulated damage
		// across by hand.  Everything else - the per-step damage flags,
		// damaged_count, the fractional remainders - is transient and should
		// start clean.
		const bool relift = (off_track_count > off_track_limit) && ! bNewGame && ! ReplayRequested;

		long saved_front_left_damage  = front_left_damage;
		long saved_front_right_damage = front_right_damage;
		long saved_rear_damage        = rear_damage;
		long saved_new_damage         = new_damage;
		long saved_nholes             = nholes;

		ResetPlayer();

		if (relift)
			{
			front_left_damage  = saved_front_left_damage;
			front_right_damage = saved_front_right_damage;
			rear_damage        = saved_rear_damage;
			new_damage         = saved_new_damage;
			nholes             = saved_nholes;
			}

		if (bNewGame || ReplayRequested)
			{
			// reset all animated objects
			ResetDrawBridge();
			ReplayFinished = FALSE;
			}

		// PositionCarAbovePiece puts the car back on the crane, and reads
		// drop_start_done to tell a start-of-race drop start (from high up, on a
		// timer) from a re-lift after going off the track (from just above the road,
		// waiting for fire).  So it has to be right before the car is positioned.
		if (off_track_count > off_track_limit)
			{
			PositionCarAbovePiece(player_current_piece);
			}
		else
			{
			drop_start_done = FALSE;
			PositionCarAbovePiece(PlayersStartPiece);
			}

		if (bNewGame)
			{
			// reset action replay recording
//			Replay = FALSE;
//			RewindRecording();

#ifdef USE_AMIGA_RECORDING
			CloseAmigaRecording();		// So that play will start from beginning
			ReplayAmigaRecording = TRUE;
#endif
			}

#ifdef NOT_USED
		if (ReplayRequested)
			{
			// begin action replay if requested
			Replay = TRUE;
			}
#endif

		off_track_count = 0;
//		ReplayRequested = FALSE;
		}

	//VALUE1 = Replay;

#ifdef NOT_USED
	// replay doesn't currently work on DrawBridge - doesn't know starting DrawBridge frame

	if (ReplayFinished)
        {
        if (ReplayLooping)
            {
        	RewindRecording();
        	ReplayRequested = TRUE;
            }
	    return;
        }

	if (! Replay)
		{
		Record(input);
		}
	else
		{
		// override user input with recorded value
		PlayBack(&input);
		}
#endif

	CarControl(input);
	CarMovement();
	UpdateEngineRevs();

	// drop_start_done used to be set here, on first contact with the road, because
	// there was no crane to set it.  LiftCarOntoTrack now sets it where the Amiga
	// did, at car.off.chains - the moment the chains let go.


	// output player values for use by functions that draw the world
	*x = player_x;
	*y = -(player_y * LOCAL_Y_FACTOR);
	*z = player_z;

	// following checks angles don't exceed limits
	*x_angle = (player_x_angle & (MAX_ANGLE - 1));
	*y_angle = (player_y_angle & (MAX_ANGLE - 1));
	*z_angle = (player_z_angle & (MAX_ANGLE - 1));

	// Reverse x and z angle because StuntCarRacer's DrawWorld rotates around x and z in
	// the opposite direction to the trig. coefficients calculated by StuntCarRacer's
	// CarBehaviour (i.e. clockwise becomes anti-clockwise or vice-versa)
	*x_angle = (-player_x_angle & (MAX_ANGLE - 1));
	*z_angle = (-player_z_angle & (MAX_ANGLE - 1));

	first_time = FALSE;

	//VALUE1++;		// frame counter
	}

/*	======================================================================================= */
/*	Function:		LimitViewpointY															*/
/*																							*/
/*	Description:	Limit viewpoint Y value to prevent it going below or too close to road	*/
/*					(i.e. prevent road 'tearing')											*/
/*	======================================================================================= */

// NOTE (2026-08-02): this whole function is a PC-port invention, not an Amiga
// mechanism, and it is no longer called — see CalcGameViewpoint() in
// StuntCarRacer.cpp, which now implements the Amiga's own `y.pers.shift` rule
// from set.road.position.values ("Reference only/StuntCarRacer.s":13396).
// Kept intact, and restored to its original form, in case it is ever wanted as
// a backstop. Note it adjusts player1_y itself, which also moves the drawn car,
// not just the camera — one reason to prefer the Amiga rule.
#define Y_ADJUSTMENT_THRESHOLD 0x480

void LimitViewpointY (long *y)
	{
	long saved_player_z_speed = player_z_speed;
	short sin_x, cos_x;
	short sin_z, cos_z;
	long ry = 0, ly = 0;

//	VALUE1 = (bTestKey ? 1 : 0);
	// calculate required sin.cos values using player x, y and z angles
	CalcYXZTrigCoefficients(player_x_angle,
							player_y_angle,
							player_z_angle);

	player_z_speed = 0xA00;	// prevent CalculateRoadWheelHeight from averaging current and previous heights

	CalculateWheelXZOffsets();
	CalculateRoadWheelHeights();
	CalculateActualWheelHeights();

	player_z_speed = saved_player_z_speed;	// restore original value

	/*
	VALUE1 = front_left_road_height;
	VALUE2 = front_left_actual_height;
	VALUE3 = front_left_road_height - front_left_actual_height;
	*/

	/*
	VALUE1 = front_right_road_height;
	VALUE2 = front_right_actual_height;
	VALUE3 = front_right_road_height - front_right_actual_height;
	*/

	GetSinCos(player_x_angle, &sin_x, &cos_x);	// cosine not used
	GetSinCos(player_z_angle, &sin_z, &cos_z);	// cosine not used

//	VALUE1 = player_y;
//	VALUE1 = front_right_road_height - front_right_actual_height;
//	VALUE2 = VALUE3 = 0;
	if ((front_right_road_height - front_right_actual_height) > Y_ADJUSTMENT_THRESHOLD)
		{
		// Use the CalculateActualWheelHeights() calculations in reverse, with road height, to calculate adjusted player_y
		/*
		front_right_actual_height = player_y;
		front_right_actual_height += (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
		front_right_actual_height -= (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
		front_right_actual_height >>= 8;
		*/
#if 0
		if (bTestKey)
			ry = front_right_road_height << 8;
		else
#endif
			ry = (front_right_road_height - Y_ADJUSTMENT_THRESHOLD) << 8;

		ry += (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
		ry -= (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
//		VALUE2 = ry;
		}

//	VALUE1 = front_left_road_height - front_left_actual_height;
//	VALUE2 = VALUE3 = 0;
	if ((front_left_road_height - front_left_actual_height) > Y_ADJUSTMENT_THRESHOLD)
		{
		// Use the CalculateActualWheelHeights() calculations in reverse, with road height, to calculate adjusted player_y
		/*
		front_left_actual_height = player_y;
		front_left_actual_height += (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
		front_left_actual_height += (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
		front_left_actual_height >>= 8;
		*/
#if 0
		if (bTestKey)
			ly = front_left_road_height << 8;
		else
#endif
			ly = (front_left_road_height - Y_ADJUSTMENT_THRESHOLD) << 8;

		ly -= (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
		ly -= (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
//		VALUE3 = ly;
		}

	if (ry)
		{
		if (ly)
		{
			*y = -(((ry + ly) * LOCAL_Y_FACTOR) / 2);	// use average of two values
//			VALUE1 = 1;
		}
		else
		{
			*y = -(ry * LOCAL_Y_FACTOR);
//			VALUE1 = 2;
		}
		}
	else if (ly)
		{
		*y = -(ly * LOCAL_Y_FACTOR);
//		VALUE1 = 3;
		}
	}

	/*
	// Old method...
	// 19/09/2007 attempt to limit player_y to prevent road disappearing.  Doesn't work completely on Draw Bridge track
	long front_road_height = (front_left_road_height + front_right_road_height) << (8-1);
//	long front_road_height = 0;
	VALUE1 = player_y;
	VALUE2 = front_road_height;
	if (player_y > front_road_height)
	{
	//*y = -(player_y * LOCAL_Y_FACTOR);
	VALUE3 = 0;
	}
	else
	{
	*y = -(front_road_height * LOCAL_Y_FACTOR);
	VALUE3 = 1;
	}
	*/

/*	======================================================================================= */
/*	Function:		CarControl																*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

static void CarControl (DWORD input)
	{
//	Keys that control car are :-
//			Left = left, Right = right
//			Up = Accelerate, Down = Brake
//			(X) = Accelerate, (B) = Brake on Pandora
//			SPACE = boost
//			(R) = boost on Pandora
//
//  Note: Can't accelerate without boost when using keyboard,
//		  because HASH key is changed to be brake - see below

	long left = (input & KEY_P1_LEFT),
		 right = (input & KEY_P1_RIGHT),
		 boost = (input & KEY_P1_BOOST);

	accelerate = (input & KEY_P1_ACCEL);
	brake = (input & KEY_P1_BRAKE);

	// if none of the resulting keys are pressed then read joystick
#ifdef SCR_PORTABLE
#warning TODO
#else
	if( !input )
	{
		if(P1Controller.IsConnected())
		{
			// easier to read...
			const XINPUT_GAMEPAD &pad = P1Controller.GetState().Gamepad;
			if(pad.bRightTrigger)
			{
				accelerate = TRUE;
			}

			if(pad.wButtons & XINPUT_GAMEPAD_A)
			{
				boost = TRUE;
			}

			if(pad.wButtons & XINPUT_GAMEPAD_B || pad.bLeftTrigger)
			{
				brake = TRUE;	// select brake
				accelerate = FALSE;
			}

			if( abs(pad.sThumbLX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
			{
				(pad.sThumbLX < 0) ? (left = TRUE) : (right = TRUE);
			}
		}
	}
#endif
	left_right_value = 0;
	if (touching_road)
		{
		if (ON_CHAINS)
			{
			// car.control (StuntCarRacer.s:10578) tests the countdown *after*
			// loading it into d0 and never reloads it, so while the car is on the
			// chains the countdown itself is what reaches left.right.value.  Odd,
			// but the FloatV2 reference kept it (PhysicsFloatV2.cs:829), so it is
			// part of how the car settles as it is lowered.
			left_right_value = static_cast<signed char>(car_on_chains_countdown);
			}
		else
			{
			if (left)
				left_right_value = -15;
			if (right)
				left_right_value = 15;
			}
		}

	long boost_flag;	// active low
	if (boost)
		boost_flag = FALSE;
	else
		boost_flag = TRUE;

	chain_fire_pressed = (boost ? TRUE : FALSE);

	if ((player_z_speed < 120*256) && (! ON_CHAINS) && (NOT_WRECKED))
		{
		if (accelerate)
			{
			engine_z_acceleration = engine_power;
			accelerating = TRUE;
			}
		else
			if (brake)
				{
				engine_z_acceleration = -240;
				accelerating = FALSE;
				}
			else
				if (accelerating)
					{
					// car keeps accelerating even when control released
					engine_z_acceleration = engine_power;
					accelerating = TRUE;	// already TRUE
					}
				else
					engine_z_acceleration = 0;
		}
	else
		engine_z_acceleration = 0;

	BoostPower(boost_flag,
			   accelerate,
			   brake);

#ifdef PLAY_AMIGA_RECORDING
	GetRecordedAmigaWord(&left_right_value);
	GetRecordedAmigaWord(&engine_z_acceleration);
	//VALUE1 = engine_z_acceleration;
#endif
	return;
	}


static void BoostPower (long boost_flag,
						long accel_flag,
						long brake_flag)
	{
	boost_activated = 0;

	if ((! boost_flag) && (NOT_WRECKED))
		{
		if (accelerating || (accel_flag || brake_flag))
			{
			if (boostReserve > 0)
				{
				--boostUnit;
				if (boostUnit < 0)
					{
					boostUnit = boost_unit_value;
					--boostReserve;
					}

				boost_activated = 0x80;
				engine_z_acceleration *= 2;
				}
			}
		}

	return;
	}


/*	======================================================================================= */
/*	Function:		CarMovement																*/
/*																							*/
/*	Description:							*/
/*	======================================================================================= */

static void CarMovement (void)
	{
	// currently uses player_x/y/z and player_x/y/z_angle

	//calculate required sin.cos values using player x, y and z angles
	CalcYXZTrigCoefficients(player_x_angle,
							player_y_angle,
							player_z_angle);

	//fprintf(out, "player_x_angle %d\n", player_x_angle);
	//fprintf(out, "player_y_angle %d\n", player_y_angle);
	//fprintf(out, "player_z_angle %d\n", player_z_angle);

	CalculateWheelXZOffsets();
	CalculateRoadWheelHeights();

	// FloatV2 physics: replaces everything below, but *after* the road-position
	// tracking above has run — Tick treats road section / distance / road-x as
	// inputs. Toggle with V; see Physics_FloatV2.h.
	if (scr::gUseFloatV2Physics)
		{
		scr::PhysicsInput in;
		in.Left       = (left_right_value < 0);
		in.Right      = (left_right_value > 0);
		in.Accelerate = (accelerate != 0);
		in.Brake      = (brake != 0);
		in.Boost      = (boost_activated != 0);

		// Snapshot the legacy values before the step overwrites them, so the
		// dump below compares like with like.
		long dbg_y = player_y, dbg_ys = player_world_y_speed;
		long dbg_rfl = front_left_road_height, dbg_rfr = front_right_road_height;
		long dbg_rr = rear_road_height;
		CalculateActualWheelHeights();
		long dbg_afl = front_left_actual_height, dbg_afr = front_right_actual_height;
		long dbg_ar = rear_actual_height;

		// The crane sits inside CarCollisionDetection on the Amiga, which FloatV2
		// replaces, so it has to be driven from here instead. It feeds its lift
		// through car_collision_y_acceleration, which the step takes as an input.
		LiftCarOntoTrackFloatV2(scr::gFloatV2Dt);

		scr::FloatV2_RunStep(in, scr::gFloatV2Dt);

		// FloatV2 replaces CarCollisionDetection, so the landing thump that
		// hangs off the end of it has to be driven from here. The step's
		// grounded_count / damage_value have already been copied back out.
		PlayGroundedSound();

		// K arms a dump for as long as the car is on a curved piece, so a
		// corner can be captured without having to time the N key.
		if (scr::gFloatV2DumpOnCurves && (Track[player_current_piece].type & 0x80))
			scr::gFloatV2DebugSteps = 1;

		// At 60Hz the dump is six times longer and the event of interest is a
		// single step, so flag the step where amount-below-road jumps. That is
		// the signature of ProcessWheel's (1.078125 / dtRatio) predictive term
		// amplifying a one-step discontinuity in the road-height inputs.
		{
		static double prev_below = 0.0;
		double now_below = scr::gDbgBelowFL;
		if (now_below < scr::gDbgBelowFR) now_below = scr::gDbgBelowFR;
		if (now_below < scr::gDbgBelowR)  now_below = scr::gDbgBelowR;
		if ((scr::gFloatV2DebugSteps > 0) && ((now_below - prev_below) > 1500.0))
			printf("*** SPIKE: max below %.0f -> %.0f in one step ***\n",
				   prev_below, now_below);
		prev_below = now_below;
		}

		if (scr::gFloatV2DebugSteps > 0)
			{
			--scr::gFloatV2DebugSteps;
			printf("FV2 dt=%.4f sec=%ld  y %ld->%ld  yspd %ld->%ld  [unmirror=%s]\n",
				   scr::gFloatV2Dt, player_current_piece,
				   dbg_y, player_y, dbg_ys, player_world_y_speed,
				   scr::gFloatV2UnreverseCurveDist ? "ON" : "OFF");
			printf("    road  legacy FL/FR/R %ld %ld %ld | fv2 %.0f %.0f %.0f\n",
				   dbg_rfl, dbg_rfr, dbg_rr,
				   scr::gDbgRoadFL, scr::gDbgRoadFR, scr::gDbgRoadR);
			printf("    act   legacy FL/FR/R %ld %ld %ld | fv2 %.0f %.0f %.0f\n",
				   dbg_afl, dbg_afr, dbg_ar,
				   scr::gDbgActFL, scr::gDbgActFR, scr::gDbgActR);
			printf("    below fv2 FL/FR/R %.0f %.0f %.0f  touching=%d\n",
				   scr::gDbgBelowFL, scr::gDbgBelowFR, scr::gDbgBelowR,
				   (int)touching_road);
			printf("    Z: zspd %.0f  engine %.0f->%.0f  grip %.0f  collZ %.0f "
				   "gravZ %.0f  totalZ %.0f  worldZspd %.0f  in(a=%d b=%d)\n",
				   scr::gDbgZSpeed, scr::gDbgEngineIn, scr::gDbgEngineOut,
				   scr::gDbgGrip, scr::gDbgCollZ, scr::gDbgGravZ,
				   scr::gDbgTotalZ, scr::gDbgWorldZSpeed,
				   (int)in.Accelerate, (int)in.Brake);
			printf("    yaw: yAng %.0f  secY %.0f  err %.0f  yRotSpd %.1f "
				   "yRotAcc %.0f  lr=%d align=%d atSide=%d\n",
				   scr::gDbgYAngle, scr::gDbgSectionYAngle, scr::gDbgHeadingErr,
				   scr::gDbgYRotSpeed, scr::gDbgYRotAccel,
				   scr::gDbgLeftRight, scr::gDbgAlignFired, scr::gDbgAtSideByte);
			printf("    lookup: rawFL %.0f -> storedFL %.0f  surfZ %.0f "
				   "posZspd %.0f  blend=%d  distIntoSec %ld\n",
				   scr::gDbgRawRoadFL, scr::gDbgRoadFL, scr::gDbgSurfZ,
				   scr::gDbgPosZSpeed, scr::gDbgBlendUsed,
				   players_distance_into_section);
			printf("    surfFL: legacy sec/seg/zF/xF %ld/%ld/%ld/%ld | fv2 %d/%d/%d/%d"
				   "  (dz %ld  dx %ld)\n",
				   gDbgLegacyPiece, gDbgLegacySeg, gDbgLegacyZFrac, gDbgLegacyXFrac,
				   scr::gDbgFV2Section, scr::gDbgFV2Seg,
				   scr::gDbgFV2ZFrac, scr::gDbgFV2XFrac,
				   (long)((scr::gDbgFV2Seg*256 + scr::gDbgFV2ZFrac)
						  - (gDbgLegacySeg*256 + gDbgLegacyZFrac)),
				   (long)(scr::gDbgFV2XFrac - gDbgLegacyXFrac));
			printf("    tilt: xAng %.0f (spd %.1f)  zAng %.0f (spd %.1f)\n",
				   scr::gDbgXAngle, scr::gDbgXRotSpeed,
				   scr::gDbgZAngle, scr::gDbgZRotSpeed);
			// Corner diagnostics. roadX is the across-the-road input FloatV2
			// adds each wheel's offset to; on curves the legacy code computes
			// it by a completely different route (abs(radius - dist), so never
			// negative) than on straights (signed, can go off either edge).
			// pieceType: 0 = straight, 0x40 = diagonal, 0x80|.. = curve.
			printf("    road-x: roadX %ld (fv2 %ld)  pieceType 0x%02lx  curveLeft %d  oppDir %d  segment %ld/%ld\n",
				   players_road_x_position,
				   scr::FV2_PlayersRoadXPosition(players_road_x_position),
				   (long)(Track[player_current_piece].type & 0xFF),
				   (int)Track[player_current_piece].curveToLeft,
				   (int)Track[player_current_piece].oppositeDirection,
				   player_current_segment,
				   (long)Track[player_current_piece].numSegments);
			fflush(stdout);
			}

		// The off-map bookkeeping is not part of the physics step, but it has
		// to run every frame in both paths: it drives the off-road sound and
		// the crane-back-onto-the-track reset in CarBehaviour.
		UpdateOffMapStatus();
		return;
		}

	// Legacy path is authoritative this step, so FloatV2 must re-seed from
	// these globals if the toggle is flipped on later.
	scr::gFloatV2NeedsSeed = true;

	CalculateActualWheelHeights();

	CalculateXZSpeeds();

	SetWheelRotationSpeed();
	CalculateGravityAcceleration();
	CarCollisionDetection();

	//if (B.1bb72 != 0)		// always set
		{
		CalculateTotalAcceleration();

		CalculateSteering();

		CalculateWorldAcceleration();
		ReduceWorldAcceleration();

		CalculateXZRotationAcceleration();
		UpdatePlayersRotationSpeed();
		CalculateFinalRotationSpeed();
		}

	UpdatePlayersWorldSpeed();
	UpdatePlayersPosition();

#ifdef PLAY_AMIGA_RECORDING
	if (StartOfAmigaRecording)
	{
	GetRecordedAmigaLong(&player_x); player_x *= (PC_FACTOR * 4);
	GetRecordedAmigaLong(&player_y);
	GetRecordedAmigaLong(&player_z); player_z *= (PC_FACTOR * 4);
	GetRecordedAmigaWord(&player_x_angle);
	GetRecordedAmigaWord(&player_y_angle);
	GetRecordedAmigaWord(&player_z_angle);

	++AmigaRecordingFrame;
	if (AmigaRecordingFrame > 0)
		StartOfAmigaRecording = FALSE;
	}
	else
	{
	// throw values away
	long temp;
	GetRecordedAmigaLong(&temp);
	GetRecordedAmigaLong(&temp);
	GetRecordedAmigaLong(&temp);
	GetRecordedAmigaWord(&temp);
	GetRecordedAmigaWord(&temp);
	GetRecordedAmigaWord(&temp);
	}
#endif

	// 23/08/1998 - extra bit to set flags
	UpdateOffMapStatus();


	//VALUE1 = player_z_speed;
	//VALUE1 = player_current_piece;

	//VALUE1 = Track[player_current_piece].coords[0].y;
	//long temp = Track[player_current_piece].numSegments;
	//VALUE2 = Track[player_current_piece].coords[(temp*4)].y;

//	fprintf(out, "------------------------------------------------------------\n");
	return;
	}


/*	======================================================================================= */
/*	Function:		UpdateOffMapStatus														*/
/*																							*/
/*	Description:	Flag whether the car has left the track entirely, and count how long	*/
/*					it has been down there so CarBehaviour can crane it back on.			*/
/*	======================================================================================= */

static void UpdateOffMapStatus (void)
	{
	if (player_distance_off_road >= (256-ROAD_WIDTH/2))
		{
		if (off_map_status == 0)
			{
			// Remember which side it went off, so the crane picks it up from there
			// (set.road.centre.values, StuntCarRacer.s:13478, records this the frame
			// the car first leaves the map).
			long x_offset = players_road_x_position - (ROAD_WIDTH/2);
			if (Track[player_current_piece].oppositeDirection)
				x_offset = -x_offset;

			swing_from_left = (x_offset < 0 ? TRUE : FALSE);
			}

		off_map_status = 0x80;
		}
	else
		{
		off_map_status = 0;
		off_track_count = 0;
		smaller_limit_required = FALSE;
		}

	// Not while the crane has the car.  The drop start leaves it sitting on the ground
	// beside an elevated track - off the map by this test, and touching the ground -
	// so without this the off-track timer runs out mid-hoist and craning it back on
	// interrupts the drop start.  The Amiga skips the whole off-map countdown while
	// car.on.chains.countdown is set (race.loop, StuntCarRacer.s:10375).
	if ((! ON_CHAINS) && (off_map_status != 0) && (touching_road) && (player_y < 0x1000000))
		{
		off_track_count++;
		smaller_limit_required = TRUE;
		}
	}


/*	======================================================================================= */
/*	Function:		GetPieceUsingMap														*/
/*																							*/
/*	Description:	Use Track_Map to get piece number that world x/z point is within		*/
/*	======================================================================================= */

static long GetPieceUsingMap (long x, long z, long *piece_out)
	{
	long map_x, map_z, piece;

	// locate the map square that the point is in
	map_x = x >> LOG_CUBE_SIZE;
	map_z = z >> LOG_CUBE_SIZE;

	if (((map_x < 0) || (map_x >= NUM_TRACK_CUBES)) ||
		((map_z < 0) || (map_z >= NUM_TRACK_CUBES)))
		{
		// off the map
		//fprintf(out, "GetPieceUsingMap - world point is off map\n");
		return(FALSE);
		}

	// lookup piece number within map
	piece = Track_Map[map_x][map_z];
	if (piece == -1)
		{
		// no piece at this place on the map
		//fprintf(out, "GetPieceUsingMap - no piece at map position\n");
		return(FALSE);
		}

	*piece_out = piece;
	return(TRUE);
	}


/*	======================================================================================= */
/*	Function:		CalcXZRelativeToPiece													*/
/*																							*/
/*	Description:	Calculate position of world x/z point, relative to required piece		*/
/*	======================================================================================= */

static void CalcXZRelativeToPiece (long x, long z, long piece, long *rx_out, long *rz_out)
	{
	long piece_x, piece_z;

	// calculate x/z position of piece's front left corner, within world
	piece_x = Track[piece].x << LOG_CUBE_SIZE;
	piece_z = Track[piece].z << LOG_CUBE_SIZE;

	// calculate point's x/z position relative to the piece (and in same range)
	*rx_out = (x - piece_x) >> LOG_PRECISION;
	*rz_out = (z - piece_z) >> LOG_PRECISION;
	}


/*	======================================================================================= */
/*	Function:		CalculateWheelXZOffsets													*/
/*																							*/
/*	Description:	Calculate offsets from player's position (car's centre point)			*/
/*	======================================================================================= */

static void CalculateWheelXZOffsets (void)
	{
	short *trig_coeffs = TrigCoefficients();

	// rear wheel is just (0, 0, -CAR_LENGTH/2) split into components
	rear_wheel_x_offset = (static_cast<long>(trig_coeffs[Z_X_COMP]) * (-CAR_LENGTH/2) * PC_FACTOR);
	rear_wheel_z_offset = (static_cast<long>(trig_coeffs[Z_Z_COMP]) * (-CAR_LENGTH/2) * PC_FACTOR);

	// front left wheel is just (-CAR_WIDTH/2, 0, CAR_LENGTH/2) split into components
	front_left_wheel_x_offset = (static_cast<long>(trig_coeffs[X_X_COMP]) * (-CAR_WIDTH/2) * PC_FACTOR);
	front_left_wheel_x_offset += (static_cast<long>(trig_coeffs[Z_X_COMP]) * (CAR_LENGTH/2) * PC_FACTOR);
	front_left_wheel_z_offset = (static_cast<long>(trig_coeffs[X_Z_COMP]) * (-CAR_WIDTH/2) * PC_FACTOR);
	front_left_wheel_z_offset += (static_cast<long>(trig_coeffs[Z_Z_COMP]) * (CAR_LENGTH/2) * PC_FACTOR);

	// front right wheel is just (CAR_WIDTH/2, 0, CAR_LENGTH/2) split into components
	front_right_wheel_x_offset = (static_cast<long>(trig_coeffs[X_X_COMP]) * (CAR_WIDTH/2) * PC_FACTOR);
	front_right_wheel_x_offset += (static_cast<long>(trig_coeffs[Z_X_COMP]) * (CAR_LENGTH/2) * PC_FACTOR);
	front_right_wheel_z_offset = (static_cast<long>(trig_coeffs[X_Z_COMP]) * (CAR_WIDTH/2) * PC_FACTOR);
	front_right_wheel_z_offset += (static_cast<long>(trig_coeffs[Z_Z_COMP]) * (CAR_LENGTH/2) * PC_FACTOR);

	// could also possibly work out the wheel y offsets here
	// rather than doing it in calculate.actual.wheel.heights
	// but calculate.actual.wheel.heights doesn't use components
	return;
	}

#ifdef NOT_USED
void TempCentrePoint (long *x, long *y, long *z)
	{
	*x = player_x;
	*y = player_y;
	*z = player_z;
	}

void TempRearPoint (long *x, long *y, long *z)
	{
	*x = player_x + rear_wheel_x_offset;
	//*y = player_y;

	*y = (((-rear_actual_height / 32) << LOG_PRECISION) * PC_FACTOR);
	*z = player_z + rear_wheel_z_offset;
	}

void TempFrontLeftPoint (long *x, long *y, long *z)
	{
	*x = player_x + front_left_wheel_x_offset;
	//*y = player_y;

	*y = (((-front_left_actual_height / 32) << LOG_PRECISION) * PC_FACTOR);
	*z = player_z + front_left_wheel_z_offset;
	}

void TempFrontRightPoint (long *x, long *y, long *z)
	{
	*x = player_x + front_right_wheel_x_offset;
	//*y = player_y;

	*y = (((-front_right_actual_height / 32) << LOG_PRECISION) * PC_FACTOR);
	*z = player_z + front_right_wheel_z_offset;
	}


void StuntCarRearWheelXZ (long *x, long *z)
	{
	CalcYXZTrigCoefficients(player_x_angle,
							player_y_angle,
							player_z_angle);

	CalculateWheelXZOffsets();

	*x = rear_wheel_x_offset + player_x;
	*z = rear_wheel_z_offset + player_z;
	}

void StuntCarFrontLeftWheelXZ (long *x, long *z)
	{
	// must have previously called StuntCarRearWheelXZ

	*x = front_left_wheel_x_offset + player_x;
	*z = front_left_wheel_z_offset + player_z;
	}

void StuntCarFrontRightWheelXZ (long *x, long *z)
	{
	// must have previously called StuntCarRearWheelXZ

	*x = front_right_wheel_x_offset + player_x;
	*z = front_right_wheel_z_offset + player_z;
	}


void StuntCarWheelXZ (long piece, long segment, long scrx, long scrz, long *x, long *z)
	{
	long x1, x2, x3, x4;
	long z1, z2, z3, z4;
	long sxa, sxb, sx;
	long sza, szb, sz;

	x1 = Track[piece].coords[(segment*4)].x;
	z1 = Track[piece].coords[(segment*4)].z;

	x2 = Track[piece].coords[(segment*4)+1].x;
	z2 = Track[piece].coords[(segment*4)+1].z;

	segment++;
	x3 = Track[piece].coords[(segment*4)].x;
	z3 = Track[piece].coords[(segment*4)].z;

	x4 = Track[piece].coords[(segment*4)+1].x;
	z4 = Track[piece].coords[(segment*4)+1].z;

	// first do x interpolation
	//fprintf(out, "x1 %d, x2 %d, x3 %d, x4 %d\n", x1, x2, x3, x4);
	//fprintf(out, "scrx %d, scrz %d\n", scrx, scrz);

	sxa = (x1<<LOG_SURFACE_SIZE) + (scrx * (x2-x1));
	sxb = (x3<<LOG_SURFACE_SIZE) + (scrx * (x4-x3));

	sza = (z1<<LOG_SURFACE_SIZE) + (scrx * (z2-z1));
	szb = (z3<<LOG_SURFACE_SIZE) + (scrx * (z4-z3));

	// now do z interpolation
	sx = (sxa<<(LOG_PRECISION-LOG_SURFACE_SIZE)) + ((scrz * (sxb-sxa))>>((LOG_SURFACE_SIZE*2)-LOG_PRECISION));
	//fprintf(out, "sx %d\n", sx);

	sz = (sza<<(LOG_PRECISION-LOG_SURFACE_SIZE)) + ((scrz * (szb-sza))>>((LOG_SURFACE_SIZE*2)-LOG_PRECISION));

	// add x/z position of piece's front left corner, within world
	*x = sx + (Track[piece].x << LOG_CUBE_SIZE);
	*z = sz + (Track[piece].z << LOG_CUBE_SIZE);
	}
#endif

/*	======================================================================================= */
/*	Function:		CalculateRoadWheelHeights												*/
/*																							*/
/*	Description:	Calculate the road height (y value) directly below each car wheel		*/
/*					NOTE: following method is not taken from Amiga StuntCarRacer			*/
/*	======================================================================================= */

typedef enum
	{
	FRONT_LEFT = 0,
	FRONT_RIGHT,
	REAR,
	NUM_WHEEL_POSITIONS,
	CENTRE	// NOTE: This isn't a wheel position, but is used by CalculatePlayersRoadPosition
	} WheelPositionType;


// VALUES FROM FOLLOWING ARE SLIGHTLY DIFFERENT TO AMIGA STUNT CAR RACER, BUT WORK OK

static void CalculateRoadWheelHeights (void)
	{
	long i, height;
	COORD_3D wheel_pos[NUM_WHEEL_POSITIONS];

	at_side_byte = 0;

	// Amiga StuntCarRacer cleared which.side.byte at the end of draw.world, which on that
	// machine was also once per physics step.  Here the two run at different rates, so it
	// has to be cleared by whoever recomputes it - otherwise every render frame between
	// two physics steps sees 0 and the sparks blink out.  (The FloatV2 path does the same
	// thing for itself, Physics_FloatV2.cpp:378.)
	which_side_byte = 0;

	// create array of wheel world x/z positions
	wheel_pos[FRONT_LEFT].x = front_left_wheel_x_offset + player_x;
	wheel_pos[FRONT_LEFT].z = front_left_wheel_z_offset + player_z;

	wheel_pos[FRONT_RIGHT].x = front_right_wheel_x_offset + player_x;
	wheel_pos[FRONT_RIGHT].z = front_right_wheel_z_offset + player_z;

	wheel_pos[REAR].x = rear_wheel_x_offset + player_x;
	wheel_pos[REAR].z = rear_wheel_z_offset + player_z;

	// initialise heights to previous values (from globals)
	wheel_pos[FRONT_LEFT].y = front_left_road_height;
	wheel_pos[FRONT_RIGHT].y = front_right_road_height;
	wheel_pos[REAR].y = rear_road_height;

	// calculate world road height at wheel positions
	for (i = 0; i < NUM_WHEEL_POSITIONS; i++)
		{
		CalculateWorldRoadHeight(i, wheel_pos[i].x, wheel_pos[i].z, &height);

		// convert the result to PC StuntCarRacer magnitude
		height = ((height / PC_FACTOR) >> (LOG_PRECISION-3));

		CalculateRoadWheelHeight(height, &wheel_pos[i].y);

		// 23/08/1998 - also store player_distance_off_road
		if (i == REAR)
			player_distance_off_road = abs(distance_off_road);
		}

	// store heights in global variables
	front_left_road_height = wheel_pos[FRONT_LEFT].y;
	front_right_road_height = wheel_pos[FRONT_RIGHT].y;
	rear_road_height = wheel_pos[REAR].y;
	return;
	}


static void CalculateRoadWheelHeight (long height, long *height_out)
	{
	if (wheel_off_road)
		CalculateIfCarOffRoad(&height);

	wheel_off_road = FALSE;

	// get angle in Amiga StuntCarRacer format (i.e. correct sign)
	long angle = (player_x_angle < (_180_DEGREES) ? (player_x_angle) :
													(player_x_angle - _360_DEGREES));

	if ((abs(player_z_speed) >= 0xA00) || (abs(angle) >= 0x600))
		{
		// use height as is
		*height_out = height;
		}
	else
		{
		// save the average of calculated (new) height and the previous value
		// this is possibly for when the car is being lowered onto the road
		*height_out = ((height + *height_out) / 2);
		}

	return;
	}


static void CalculateIfCarOffRoad (long *height)
	{
	// calculate how far the current wheel is off the left or right of the road
	long x = abs(distance_off_road);

	if (x > ((3*CAR_WIDTH)/4))
		{
		// signal whole car is off road
		*height = OFF_ROAD_HEIGHT;
		at_side_byte = (at_side_byte >> 1) | 0x80;
		}
	else
		{
		// use the amount the wheel is off the road to drop the height of the wheel,
		// to make the car fall off the edge gradually (i.e. invisible sloping sides)
		*height -= ((x * 16) + 0x100);

		if (*height < OFF_ROAD_HEIGHT)
			{
			// signal whole car is off road
			*height = OFF_ROAD_HEIGHT;
			at_side_byte = (at_side_byte >> 1) | 0x80;
			}
		else
			{
			// store which side the car is falling off

			// logic here is different to Amiga StuntCarRacer, due to distance_off_road being different
			// from wheel.road.x.position and also plus.180.degrees not being used
			long w = distance_off_road >> 8;

			if (w & 0x80)
				{
				if (off_left)
					which_side_byte = 0x80;	// left
				else if (off_right)
					which_side_byte = 0x40;	// right
				}
			}
		}

	return;
	}


// current surface co-ords
static long sx1, sy1, sz1, sx2, sy2, sz2, sx3, sy3, sz3, sx4, sy4, sz4;


static void CalculateWorldRoadHeight (long wheel, long x, long z, long *y_out)
	{
	// starts with the piece/surface that was used last time
	// this avoids locating the wrong map square,
	// e.g. for diagonal pieces that run into adjacent squares

	static long piece = -1, segment = -1;
	static long first_time = TRUE, prevTrackID = NO_TRACK;

	//fprintf(out, "CalculateWorldRoadHeight\n");

	// Reset variables when the track changes
	if (TrackID != prevTrackID)
	{
		piece = -1;
		segment = -1;
		first_time = TRUE;
		prevTrackID = TrackID;
	}

//****************


	// 14/05/1998 - first section has been re-written to allow the function to handle
	// the case when the point has been off the road and then returned to an entirely
	// different area of the road (e.g. car placed back onto track in different place)
	long this_piece;
	if (! GetPieceUsingMap(x, z, &this_piece))
		{
#if defined(DEBUG) || defined(_DEBUG)
		if (debug_position) fprintf(out, "CalculateWorldRoadHeight error\n");
#endif
		if (first_time)
			{
			// get the four (x,y,z) points for the first surface of the default piece
			piece = 0;
			segment = 0;
			GetSurfaceCoords(piece, segment);
			}
		}
	else
		{
#if defined(DEBUG) || defined(_DEBUG)
		if (debug_position) fprintf(out, "piece %d, this_piece %d\n", piece, this_piece);
#endif
		if ((first_time) ||
			(abs(this_piece - piece) > 1))	// moved by more than one piece
			{
			// check the move is not from the last to first piece, or vice versa
			if ((!((this_piece == (NumTrackPieces - 1)) && (piece == 0))) &&
				(!((this_piece == 0) && (piece == (NumTrackPieces - 1)))))
				{
				// get the four (x,y,z) points for the current surface of the piece
				piece = this_piece;
				segment = 0;
				GetSurfaceCoords(piece, segment);
				}
			}
		}

	first_time = FALSE;		// ensure flag is cleared


//****************


	// find the surface that the point is located within
	// first check point is not before or after surface (z direction)
	long xs, xp, zs, zp, rx = 0, rz = 0;
	long before_surface = TRUE, after_surface = TRUE, num_piece_changes;


	// 'before surface' loop
	num_piece_changes = 0;
	while (before_surface)
		{
		// really only need to do following when piece changes, but do it always at the moment
		CalcXZRelativeToPiece(x, z, piece, &rx, &rz);

#if defined(DEBUG) || defined(_DEBUG)
		if (debug_position)
		{
		fprintf(out, "before_surface sx2, sy2, sz2: 0x%x, 0x%x, 0x%x\n", sx2, sy2, sz2);
		fprintf(out, "before_surface sx3, sy3, sz3: 0x%x, 0x%x, 0x%x\n", sx3, sy3, sz3);
		fprintf(out, "before_surface sx1, sy1, sz1: 0x%x, 0x%x, 0x%x\n", sx1, sy1, sz1);
		fprintf(out, "before_surface sx4, sy4, sz4: 0x%x, 0x%x, 0x%x\n", sx4, sy4, sz4);
		}
#endif
		// calculate top dot product => before_surface
		xs = sx1 - sx4; zs = sz1 - sz4;		// current segment vector
		xp = rx - sx4; zp = rz - sz4;		// current point vector
		before_surface = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		if (before_surface)
			{
			// future improvement: try a move in one direction,
			// if this is worse then move in other direction

			// DIRECTION DEPENDANT - WHOLE SECTION
			if (segment < (Track[piece].numSegments - 1))
				{
				segment++;
				}
			else
				{
				// go to next piece if already at last surface
				piece++; if (piece > (NumTrackPieces - 1)) piece = 0;
				segment = 0;

				num_piece_changes++;
				}

			// get the four (x,y,z) points for the new surface
			GetSurfaceCoords(piece, segment);
			}

		// prevent an infinite loop
		if (num_piece_changes >= NumTrackPieces)
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "CalculateWorldRoadHeight - infinite loop trapped (1)\n");
#endif
			break;
			}
		}

#if defined(DEBUG) || defined(_DEBUG)
	// warn if surface search was inefficient
	if (num_piece_changes > 2)		// arbitrary number
		fprintf(out, "CalculateWorldRoadHeight - %d changes (1)\n", num_piece_changes);
#endif

	// 'after surface' loop
	num_piece_changes = 0;
	while (after_surface)
		{
		// really only need to do following when piece changes, but do it always at the moment
		CalcXZRelativeToPiece(x, z, piece, &rx, &rz);

#if defined(DEBUG) || defined(_DEBUG)
		if (debug_position)
		{
		fprintf(out, "after_surface sx2, sy2, sz2: 0x%x, 0x%x, 0x%x\n", sx2, sy2, sz2);
		fprintf(out, "after_surface sx3, sy3, sz3: 0x%x, 0x%x, 0x%x\n", sx3, sy3, sz3);
		fprintf(out, "after_surface sx1, sy1, sz1: 0x%x, 0x%x, 0x%x\n", sx1, sy1, sz1);
		fprintf(out, "after_surface sx4, sy4, sz4: 0x%x, 0x%x, 0x%x\n", sx4, sy4, sz4);
		}
#endif
		// calculate bottom dot product => after_surface
		xs = sx3 - sx2; zs = sz3 - sz2;		// current segment vector
		xp = rx - sx2; zp = rz - sz2;		// current point vector
		after_surface = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		if (after_surface)
			{
			// future improvement: try a move in one direction,
			// if this is worse then move in other direction

			// DIRECTION DEPENDANT - WHOLE SECTION
			if (segment > 0)
				{
				segment--;
				}
			else
				{
				// go to previous piece if already at first surface
				piece--; if (piece < 0) piece = (NumTrackPieces - 1);
				segment = (Track[piece].numSegments - 1);

				num_piece_changes++;
				}

			// get the four (x,y,z) points for the new surface
			GetSurfaceCoords(piece, segment);
			}

		// prevent an infinite loop
		if (num_piece_changes >= NumTrackPieces)
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "CalculateWorldRoadHeight - infinite loop trapped (2)\n");
#endif
			break;
			}
		}

#if defined(DEBUG) || defined(_DEBUG)
	// warn if surface search was inefficient
	if (num_piece_changes > 2)		// arbitrary number
		fprintf(out, "CalculateWorldRoadHeight - %d changes (2)\n", num_piece_changes);
#endif

//****************


	// now know that point is between start edge and end edge of surface
	// find out if point is off left or right of surface

	if (wheel != CENTRE)	// don't do this for CENTRE position (to allow road_x to include being off the road)
	{
		// calculate left dot product => off_left
		xs = sx2 - sx1; zs = sz2 - sz1;		// current segment vector
		xp = rx - sx1; zp = rz - sz1;		// current point vector
		off_left = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		// calculate right dot product => off_right
		xs = sx4 - sx3; zs = sz4 - sz3;		// current segment vector
		xp = rx - sx3; zp = rz - sz3;		// current point vector
		off_right = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		wheel_off_road = FALSE; distance_off_road = 0;
		if (off_left || off_right)
		{
			long d = 0, ex = 0, ez = 0;

			// wheel is off road
			wheel_off_road = TRUE;

			// need to make sure that road height at edge of surface is calculated
			// (i.e. point has to be within bounds of piece), therefore the local
			// point (rx/rz) is modified by the following, to be at the relevant edge

			if (off_left)
			{
				// get distance from and position at left edge
				d = CalcDistanceOffRoad(rx, rz, sx2, sz2, sx1, sz1, sx3, sz3, &ex, &ez);
			}
			else if (off_right)
			{
				// get distance from and position at right edge
				d = CalcDistanceOffRoad(rx, rz, sx3, sz3, sx4, sz4, sx2, sz2, &ex, &ez);
			}

			// note: following value is -'ve
			distance_off_road = d;

			rx = ex;
			rz = ez;
		}
	}

//****************


	// calculate height of surface at x,z position using linear interpolation

	long sx, sz, calculated_segment;
	long sya, syb, y;
	// get distance from left edge / top edge, i.e. sx / sz
	calculated_segment = segment;

	if (wheel != CENTRE)
	{
		CalcSurfacePosition(piece, rx, rz, sx2, sz2, sx1, sz1, sx3, sz3, &sx, &sz, NULL, &calculated_segment);

		if (wheel == REAR)
		{
		// Reduce sx to (0 - 255)
		rear_wheel_surface_x_position = sx >> (LOG_SURFACE_SIZE-8);
		}
	}
	else
	{
		// Called by CalculatePlayersRoadPosition
		// Set player_current_piece, player_current_segment, players_distance_into_section and players_road_x_position
		long road_x;
		CalcSurfacePosition(piece, rx, rz, sx2, sz2, sx1, sz1, sx3, sz3, &sx, &sz, &road_x, &calculated_segment);

		player_current_piece = piece;
		player_current_segment = calculated_segment;

		players_distance_into_section = (calculated_segment * 256) + (sz >> (LOG_SURFACE_SIZE-8));
		//VALUE1 = players_distance_into_section;
		if (calculated_segment >= Track[piece].numSegments)
		{
			MessageBox(NULL, L"calculated_segment out of range", L"Error", MB_OK);
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "piece %d, calculated_segment %d, numSegments %d\n", piece, calculated_segment, Track[piece].numSegments);
#endif
		}

		players_road_x_position = road_x;
		//VALUE2 = players_road_x_position;
	}


	// 22/10/1998 - if the curve calculation output a different segment to the one that was identified
	//				earlier then the co-ordinates must be retrieved for the calculated segment.
	if (calculated_segment != segment)
		{
		segment = calculated_segment;
		// get the four (x,y,z) points for the new surface
		GetSurfaceCoords(piece, segment);
		}

	/*
	CCPIECE = piece;
	CCSEGMENT = segment;
	CCSURFACEX = sx;
	CCSURFACEZ = sz;
	*/

	// i.e. calculate y at offset (sx, sz)
	//		given (sx1, sy1, sz1)
	//			  (sx2, sy2, sz2)
	//			  (sx3, sy3, sz3)
	//			  (sx4, sy4, sz4)
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position)
	{
	fprintf(out, "interpolate sx, sz: %d, %d\n", sx, sz);
	fprintf(out, "interpolate sx2, sy2, sz2: 0x%x, 0x%x, 0x%x\n", sx2, sy2, sz2);
	fprintf(out, "interpolate sx3, sy3, sz3: 0x%x, 0x%x, 0x%x\n", sx3, sy3, sz3);
	fprintf(out, "interpolate sx1, sy1, sz1: 0x%x, 0x%x, 0x%x\n", sx1, sy1, sz1);
	fprintf(out, "interpolate sx4, sy4, sz4: 0x%x, 0x%x, 0x%x\n", sx4, sy4, sz4);
	}
#endif

	// Diagnostic for the residual straight-line road-height difference against
	// FloatV2. The corner Y data and the two interpolators have both been shown
	// to agree exactly, so the only place a difference can come from is the
	// surface position itself — which legacy derives geometrically here and
	// FloatV2 reconstructs from players_distance_into_section plus a scaled
	// wheel offset. Reported in FloatV2's 0-255 fractions for direct comparison.
	if (wheel == FRONT_LEFT)
		{
		gDbgLegacyPiece = piece;
		gDbgLegacySeg   = calculated_segment;
		gDbgLegacyZFrac = sz >> (LOG_SURFACE_SIZE-8);
		gDbgLegacyXFrac = sx >> (LOG_SURFACE_SIZE-8);
		}

	// first do x interpolation
	sya = sy1 + ((sx * (sy4-sy1)) >> LOG_SURFACE_SIZE);
	syb = sy2 + ((sx * (sy3-sy2)) >> LOG_SURFACE_SIZE);

	// now do z interpolation
	y = (syb << LOG_SURFACE_SIZE) + (sz * (sya-syb));

	// 02/08/1998 - maybe should not do the following - just leave value as is
	//			  - don't need to make the value any bigger
	*y_out = (y << (LOG_PRECISION-LOG_SURFACE_SIZE));
	return;
	}


static void GetSurfaceCoords (long piece, long segment)
	{
	if ((segment < 0) || (segment >= Track[piece].numSegments))
	{
		MessageBox(NULL, L"GetSurfaceCoords segment out of range", L"Error", MB_OK);
#if defined(DEBUG) || defined(_DEBUG)
		fprintf(out, "GetSurfaceCoords piece %d, segment %d, numSegments %d\n", piece, segment, Track[piece].numSegments);
#endif
	}
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position) fprintf(out, "GetSurfaceCoords piece %d, segment %d, numSegments %d\n", piece, segment, Track[piece].numSegments);
#endif

	sx2 = Track[piece].coords[(segment*4)].x;
	sy2 = Track[piece].coords[(segment*4)].y;
	sz2 = Track[piece].coords[(segment*4)].z;
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position) fprintf(out, "GetSurfaceCoords sx2, sy2, sz2: 0x%x, 0x%x, 0x%x\n", sx2, sy2, sz2);
#endif

	sx3 = Track[piece].coords[(segment*4)+1].x;
	sy3 = Track[piece].coords[(segment*4)+1].y;
	sz3 = Track[piece].coords[(segment*4)+1].z;
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position) fprintf(out, "GetSurfaceCoords sx3, sy3, sz3: 0x%x, 0x%x, 0x%x\n", sx3, sy3, sz3);
#endif

	segment++;
	sx1 = Track[piece].coords[(segment*4)].x;
	sy1 = Track[piece].coords[(segment*4)].y;
	sz1 = Track[piece].coords[(segment*4)].z;
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position) fprintf(out, "GetSurfaceCoords sx1, sy1, sz1: 0x%x, 0x%x, 0x%x\n", sx1, sy1, sz1);
#endif

	sx4 = Track[piece].coords[(segment*4)+1].x;
	sy4 = Track[piece].coords[(segment*4)+1].y;
	sz4 = Track[piece].coords[(segment*4)+1].z;
#if defined(DEBUG) || defined(_DEBUG)
	if (debug_position) fprintf(out, "GetSurfaceCoords sx4, sy4, sz4: 0x%x, 0x%x, 0x%x\n", sx4, sy4, sz4);
#endif
	return;
	}


static long CalcDistanceOffRoad (long x, long z,
								 long ox, long oz,
								 long ux, long uz,
								 long vx, long vz,
								 long *ex, long *ez)
	{
	// ox, oz - origin point

	// z vector
	ux -= ox;
	uz -= oz;

	// x vector
	vx -= ox;
	vz -= oz;

	// calculate (perpendicular ?) distance from left or right edge
	// method is similar to that used when texture mapping
	long v, denominator, distance;

	v = (((x-ox) * uz) + ((oz-z) * ux));	// needs to be divided by denominator
	denominator = ((uz * vx) - (ux * vz));
	// do divide afterwards to avoid need for floating point calculation
	if (denominator == 0)
		distance = 0;	// 27/07/2007 prevent division by zero
	else
		distance = (v * ROAD_WIDTH) / denominator;

	// calculate position where perpendicular meets edge of road
	if (denominator == 0)
	{
		// 27/07/2007 prevent division by zero
		*ex = x;
		*ez = z;
	}
	else
	{
		*ex = x - ((v * vx) / denominator);
		*ez = z - ((v * vz) / denominator);
	}

	return(distance);
	}


static void CalcSurfacePosition (long piece,
								 long x, long z,
								 long ox, long oz,
								 long ux, long uz,
								 long vx, long vz,
								 long *sx, long *sz,
								 long *road_x,
								 long *segment_out)		// only updated by the calculation for curves
	{
	if (Track[piece].type & 0x80)	// curve
		{
		long piece_y_angle, radius;
		long surface_x, numSegments, piece_z;
		double distance_from_centre, d;

		// 22/10/1998 - NOTE: This method is now used for curved pieces because it treats them as true circular arcs
		//					  (based upon calculate.players.road.position) which removes the 'jitter' problem

		// must be a curve (type will be -'ve)
		CalcCurveMeasurements(piece, x, z, &piece_y_angle, &radius, &distance_from_centre);

		// adjust for normal direction of travel
		if (Track[piece].oppositeDirection)
			piece_y_angle = (MAX_ANGLE/8) - piece_y_angle;

		// limit piece_y_angle to valid range
		if (piece_y_angle < 0) piece_y_angle = 0;
		if (piece_y_angle >= (MAX_ANGLE/8)) piece_y_angle = (MAX_ANGLE/8)-1;


		// calculate surface x position
		if (distance_from_centre < static_cast<double>(radius))
			d = static_cast<double>(radius) - distance_from_centre;
		else
			d = distance_from_centre - static_cast<double>(radius);

		surface_x = (static_cast<long>((d * SURFACE_SIZE) / (ROAD_WIDTH * PC_FACTOR)));
		// Also calculate road_x if required (it has a different range to surface_x)
		if (road_x)
			*road_x = (static_cast<long>(d / PC_FACTOR));

		if (surface_x >= SURFACE_SIZE) surface_x = SURFACE_SIZE-1;
		*sx = surface_x;


		// calculate surface z position and output calculated segment
		numSegments = Track[piece].numSegments;
		piece_z = (((piece_y_angle << LOG_SURFACE_SIZE) * numSegments) / (MAX_ANGLE/8));

		*sz = piece_z & (SURFACE_SIZE-1);
		*segment_out = piece_z >> LOG_SURFACE_SIZE;
		return;
		}
	else
		{
		// straight or diagonal straight

		// 22/10/1998 - NOTE: This method is no longer used for curved pieces because it is only
		//					  accurate for rectangular segments and also because curved pieces are
		//					  only approximate circular arcs (which produced the 'jitter' problem)

		// ox, oz - origin point

		// z vector
		ux -= ox;
		uz -= oz;

		// x vector
		vx -= ox;
		vz -= oz;

		// calculate (perpendicular ?) distance from left and top edge
		// method is similar to that used when texture mapping
		long u, v, denominator;

		// left edge - calculate surface x position
		v = (((x-ox) * uz) + ((oz-z) * ux));	// needs to be divided by denominator
		denominator = ((uz * vx) - (ux * vz));
		// do divide afterwards to avoid need for floating point calculation
		if (denominator == 0)
		{
			*sx = 0;	// 28/06/2007 prevent division by zero
			if (road_x)
				*road_x = 0;
		}
		else
		{
			*sx = (v * SURFACE_SIZE) / denominator;
			// Also calculate road_x if required (it has a different range to surface_x)
			if (road_x)
				*road_x = (v * ROAD_WIDTH) / denominator;
		}

		// 07/01/1999
		if (*sx >= SURFACE_SIZE)
			{
			//fprintf(out, "sx overflow trapped\n");
			*sx = SURFACE_SIZE-1;
			}
		if (*sx < 0)
			{
			//fprintf(out, "sx underflow trapped\n");
			*sx = 0;
			}

		// top edge - calculate surface z position
		u = (((x-ox) * vz) + ((oz-z) * vx));	// needs to be divided by denominator
		denominator = ((ux * vz) - (uz * vx));
		// do divide afterwards to avoid need for floating point calculation
		if (denominator == 0)
			*sz = 0;	// 28/06/2007 prevent division by zero
		else
			*sz = (u * SURFACE_SIZE) / denominator;

		// 07/01/1999
		if (*sz >= SURFACE_SIZE)
			{
			//fprintf(out, "sz overflow trapped\n");
			*sz = SURFACE_SIZE-1;
			}
		if (*sz < 0)
			{
			//fprintf(out, "sz underflow trapped\n");
			*sz = 0;
			}
		return;
		}
	}


/*	======================================================================================= */
/*	Function:		CalculateActualWheelHeights												*/
/*																							*/
/*	Description:	Calculate the height (y value) of each car wheel						*/
/*	======================================================================================= */

static void CalculateActualWheelHeights (void)
	{
	short sin_x, cos_x;
	short sin_z, cos_z;

	// see note at bottom of CalculateWheelXZOffsets regarding
	// a possible different method of calculating these heights

	GetSinCos(player_x_angle, &sin_x, &cos_x);	// cosine not used
	GetSinCos(player_z_angle, &sin_z, &cos_z);	// cosine not used


	rear_actual_height = player_y;
	// 29/06/1998 - sign changed on next line
	rear_actual_height -= (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
	rear_actual_height >>= 8;

	front_right_actual_height = player_y;
	// 29/06/1998 - sign changed on next line
	front_right_actual_height += (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
	// 29/06/1998 - sign changed on next line
	front_right_actual_height -= (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
	front_right_actual_height >>= 8;

	front_left_actual_height = player_y;
	// 29/06/1998 - sign changed on next line
	front_left_actual_height += (static_cast<long>(sin_x) << (4+15-LOG_PRECISION));
	// 29/06/1998 - sign changed on next line
	front_left_actual_height += (static_cast<long>(sin_z) << (3+15-LOG_PRECISION));
	front_left_actual_height >>= 8;
	return;
	}


/*	======================================================================================= */
/*	Function:		CalculateXZSpeeds														*/
/*																							*/
/*	Description:	Calculates player's actual X/Z speeds by rotating world speed values	*/
/*	======================================================================================= */

static void CalculateXZSpeeds (void)
{
	short *trig_coeffs = TrigCoefficients();

	// this function basically does the same as RotateCoordinate,
	// then removes the precision from the resulting values

	player_x_speed =  ((player_world_x_speed * static_cast<long>(trig_coeffs[X_X_COMP])) >> LOG_PRECISION);
	player_x_speed += ((player_world_y_speed * static_cast<long>(trig_coeffs[X_Y_COMP])) >> LOG_PRECISION);
	player_x_speed += ((player_world_z_speed * static_cast<long>(trig_coeffs[X_Z_COMP])) >> LOG_PRECISION);

	player_y_speed = 0;	// zero for current implementation

// player's Y speed not used but would be calculated as :-
//
//	player_y_speed =  ((player_world_x_speed * static_cast<long>(trig_coeffs[Y_X_COMP])) >> LOG_PRECISION);
//	player_y_speed += ((player_world_y_speed * static_cast<long>(trig_coeffs[Y_Y_COMP])) >> LOG_PRECISION);
//	player_y_speed += ((player_world_z_speed * static_cast<long>(trig_coeffs[Y_Z_COMP])) >> LOG_PRECISION);

	player_z_speed =  ((player_world_x_speed * static_cast<long>(trig_coeffs[Z_X_COMP])) >> LOG_PRECISION);
	player_z_speed += ((player_world_y_speed * static_cast<long>(trig_coeffs[Z_Y_COMP])) >> LOG_PRECISION);
	player_z_speed += ((player_world_z_speed * static_cast<long>(trig_coeffs[Z_Z_COMP])) >> LOG_PRECISION);
	return;
}


/*	======================================================================================= */
/*	Function:		SetWheelRotationSpeed													*/
/*																							*/
/*	Description:	(not needed yet)			*/
/*	======================================================================================= */

/*
set.wheel.rotation.speed :-

// pos.players.z.speed not stored by this function - use abs(players.z.speed) instead

	if (touching.road == 0)
		{
		// Not touching road, so reduce wheel speed by one quarter
		reduction = wheel.rotation.speed / 4;
		wheel.rotation.speed - reduction;
		return;
		}

	// touching road
	if (abs(players.z.speed) < WHEEL_SPEED_LOW_THRESHOLD)
		{
		// multiply by 8 and use as wheel speed
		wheel.rotation.speed = abs(players.z.speed) * 8;
		}
	else
		{
		// double it, add $3000 and use as wheel speed
		wheel.rotation.speed = (abs(players.z.speed) * 2) + WHEEL_SPEED_HIGH_OFFSET;
		if (wheel.rotation.speed > WHEEL_SPEED_MAX)
			wheel.rotation.speed = WHEEL_SPEED_MAX_CLAMPED;		// set to maximum value
		}
	return;
*/
static void SetOneWheelRotationSpeed(long wheel_touching_road, long wheel_z_speed, long *wheel_rotation_speed)
{
	if(wheel_touching_road == 0) 
	{
		// Not touching road, so reduce wheel speed by one quarter
		long reduction = (*wheel_rotation_speed) / 4;
		*wheel_rotation_speed -= reduction;
		return;
	}
	if(abs(wheel_z_speed) < WHEEL_SPEED_LOW_THRESHOLD)
		{
		// multiply by 8 and use as wheel speed
		*wheel_rotation_speed = abs(wheel_z_speed) * 8;
		}
	else
		{
		// double it, add $3000 and use as wheel speed
		*wheel_rotation_speed = (abs(wheel_z_speed) * 2) + WHEEL_SPEED_HIGH_OFFSET;
		if (*wheel_rotation_speed > WHEEL_SPEED_MAX)
			*wheel_rotation_speed = WHEEL_SPEED_MAX_CLAMPED;		// set to maximum value
		}
}

static void SetWheelRotationSpeed()
{
	SetOneWheelRotationSpeed(front_left_amount_below_road, player_z_speed, &front_left_wheel_speed);
	SetOneWheelRotationSpeed(front_right_amount_below_road, player_z_speed, &front_right_wheel_speed);
}

/*	======================================================================================= */
/*	Function:		CalculateGravityAcceleration											*/
/*																							*/
/*	Description:	Calculate car acceleration due to gravity								*/
/*	======================================================================================= */

static void CalculateGravityAcceleration (void)
	{
	short *trig_coeffs = TrigCoefficients();

	// Gravity acts on the Y axis only.  Therefore only Y components are used
	// 17/05/1998 - CAR_WEIGHT renamed to GRAVITY_ACCELERATION

	// Acceleration along car's X axis
	gravity_x_acceleration = ((-GRAVITY_ACCELERATION *
											static_cast<long>(trig_coeffs[X_Y_COMP])) >> LOG_PRECISION);

	// Acceleration along car's Y axis
	gravity_y_acceleration = ((-GRAVITY_ACCELERATION *
											static_cast<long>(trig_coeffs[Y_Y_COMP])) >> LOG_PRECISION);

	// Acceleration along car's Z axis
	gravity_z_acceleration = ((-GRAVITY_ACCELERATION *
											static_cast<long>(trig_coeffs[Z_Y_COMP])) >> LOG_PRECISION);

#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing accelerations by four
	gravity_x_acceleration++;
	gravity_y_acceleration++;
	gravity_z_acceleration++;
	gravity_x_acceleration >>= 1;
	gravity_y_acceleration >>= 1;
	gravity_z_acceleration >>= 1;
#endif
	return;
	}


/*	======================================================================================= */
/*	Function:		CarCollisionDetection													*/
/*																							*/
/*	Description:	Calculate car acceleration caused by collision with other objects		*/
/*	======================================================================================= */

long damaged_limit = 10;	// Actually track/league dependant (could add to track data)

	// NOTE: road_cushion_value is 0 for standard league and 1 for super league
	//		 fourteen_frames_elapsed has value of 0 or -1 (set)
long road_cushion_value = 0, fourteen_frames_elapsed = 0;


// following are only global due to use by two functions - could be passed in instead.
// The front pair are also read by DrawCockpit(), which needs the signed height
// difference rather than amount.below.road: the latter is zeroed the moment a wheel
// leaves the road, so it cannot say how far a wheel has drooped.
long front_left_height_difference,
	 front_right_height_difference;
static long rear_height_difference;

static long front_difference_below_road,
			overall_difference_below_road;


static void CarCollisionDetection (void)
	{
	// local variables
	long difference;

	long average_front_amount_below_road,
		 average_amount_below_road;


	grounded_count = 0;
	damage_value = 0;
	damaged = 0;

	// Front left wheel collision
	CalculateWheelCollision(front_left_road_height,
							front_left_actual_height,
							&front_left_height_difference,
							&old_front_left_difference,
							&front_left_amount_below_road,
							&front_left_damage);

	// Front right wheel collision
	CalculateWheelCollision(front_right_road_height,
							front_right_actual_height,
							&front_right_height_difference,
							&old_front_right_difference,
							&front_right_amount_below_road,
							&front_right_damage);

	// Rear wheel collision
	CalculateWheelCollision(rear_road_height,
							rear_actual_height,
							&rear_height_difference,
							&old_rear_difference,
							&rear_amount_below_road,
							&rear_damage);


//****************************************

	average_front_amount_below_road = (front_left_amount_below_road + front_right_amount_below_road) >> 1;
	average_amount_below_road = (average_front_amount_below_road + rear_amount_below_road) >> 1;


	CalculateCarCollisionAcceleration(average_amount_below_road);

	difference = (front_left_amount_below_road - front_right_amount_below_road) * 3;
	// limit to maximum
	if (difference > 0x1000) difference = 0x1000;
	if (difference < -0x1000) difference = -0x1000;
	front_difference_below_road = difference;

//****************************************

	difference = average_front_amount_below_road - rear_amount_below_road;
	overall_difference_below_road = difference;


//****************************************

	touching_road = (average_amount_below_road != 0 ? TRUE : FALSE);

	if ((! touching_road) && (! ON_CHAINS))
		{
		// get angle in Amiga StuntCarRacer format (i.e. correct sign)
		long angle = (player_x_angle < (_180_DEGREES) ? (player_x_angle) :
														(player_x_angle - _360_DEGREES));

		if (((angle < 0) && ((TrackID == ROLLER_COASTER) || (TrackID == SKI_JUMP)))
			||
			(angle >= 0))
			{
			difference = -128;

			// check roller coaster - don't need to do anything
			// check ski jump
			if ((angle < 0) && (TrackID == SKI_JUMP))
				difference = -8;

			if (angle >= 0x1000)
				difference = -256;

			difference -= overall_difference_below_road;
			if ((difference < 0) && (player_x_rotation_speed >= -256))
				overall_difference_below_road = difference;
			}
		}


	// The legacy path is one physics step per Amiga frame.
	LiftCarOntoTrack();

	car_to_road_collision_z_acceleration = car_collision_z_acceleration;

	// With the FloatV2 opponent running, the collision response belongs to the
	// opponent step, which applies it once per Amiga frame (PhysicsFloatV2.cs:137)
	// rather than once per player physics step.
	if ((opponentsID != NO_OPPONENT) &&
		!(scr::gUseFloatV2Physics && scr::gUseFloatV2Opponent))
		CarToCarCollision();

//****************************************

//******** Play grounded sound if necessary ********

	PlayGroundedSound();

	return;
	}


/*	======================================================================================= */
/*	Function:		PlayGroundedSound														*/
/*																							*/
/*	Description:	Landing thump, played once per physics step in which a wheel newly		*/
/*					touched down. Called from the tail of the legacy CarCollisionDetection	*/
/*					and, since FloatV2 replaces that function wholesale, directly from the	*/
/*					FloatV2 step as well.													*/
/*	======================================================================================= */

static void PlayGroundedSound (void)
	{
	if (grounded_delay > 0) --grounded_delay;

	if (grounded_count == 0)
		return;

	long amiga_volume = (damage_value >> 8) * 4;
	// minimum volume = 28, maximum volume = 64
	if (amiga_volume < 28) amiga_volume = 28;
	if (amiga_volume > 64) amiga_volume = 64;

	GroundedSoundBuffer->SetVolume(AmigaVolumeToDirectX(amiga_volume));

	if (grounded_delay == 0)
		{
		//GroundedSoundBuffer->SetCurrentPosition(0);
		GroundedSoundBuffer->Play(NULL,NULL,NULL);	// not looping

		// The Amiga's 5-frame retrigger guard is half a second at its 10Hz
		// step rate. grounded_delay counts steps, so scale it when FloatV2 is
		// running faster, otherwise the thump machine-guns on a bumpy landing.
		long reload = 5;
		if (scr::gUseFloatV2Physics && scr::gFloatV2Dt > 0.0)
			{
			reload = lround(5.0 * (0.1 / scr::gFloatV2Dt));
			if (reload < 1) reload = 1;
			}
		grounded_delay = reload;
		}
	}


static void CalculateWheelCollision (long road_height,
									 long actual_height,
									 long *height_difference_out,
									 long *old_difference_in_out,
									 long *amount_below_road_in_out,
									 long *damage_in_out)
	{
	long new_difference;
	long amount_below_road, old_amount_below_road;
	long damage;


	*height_difference_out = road_height - actual_height - wreck_wheel_height_reduction;

	new_difference = *height_difference_out;
	if (new_difference > 0x1400)
		new_difference = 0x1400;
	else if (new_difference < -0x300)
		new_difference = -0x300;

	amount_below_road = new_difference - *old_difference_in_out;
	// 21/05/1998 - '/ 256' changed to '>> 8', to match Amiga StuntCarRacer exactly
	amount_below_road = ((amount_below_road * INCREASE) >> 8) + new_difference;

	if (amount_below_road >= 0)
		{
		old_amount_below_road = *amount_below_road_in_out;
		*amount_below_road_in_out = amount_below_road;

		if ((amount_below_road >= 0x400) && (old_amount_below_road < 0x200))
			grounded_count++;	// wheel grounded - update grounded wheel count

		damage = *amount_below_road_in_out - (road_cushion_value * 256);
		if (ON_CHAINS)
			damaged_count = 0;			// the crane's touchdown must not damage the car
		else if (damage >= 0x700)
			{
			if (damage > damage_value)
				damage_value = damage;

			damage -= 0x600;
			if (fourteen_frames_elapsed == 0)
				{
				damaged_count++;
				if (damaged_count < damaged_limit)
					{
					damage /= 256;
					// NOTE next line may be unnecessary
					damage &= 0xff;
					damage += (damage / 2);
					damage += *damage_in_out;
					if (damage > 0xff) damage = 0xff;
					*damage_in_out = damage;
					damaged = 0x80;
					}
				}
			if (*amount_below_road_in_out >= 0x1200)
				*amount_below_road_in_out = 0x11ff;
			}
		else
			damaged_count = 0;
		}
	else
		{
		*amount_below_road_in_out = 0;
		damaged_count = 0;
		}

	*old_difference_in_out = new_difference;
	}


static void CalculateCarCollisionAcceleration (long average_amount_below_road)
	{
	// 21/05/1998 - changed to use shifts rather than divides, to match Amiga StuntCarRacer exactly

	// average_amount_below_road is the force exerted by the road on the car.
	//
	// Force is directed through the Y axis of the road surface.  Therefore only
	// Y components are used.
	//
	// X acceleration = force * -cosx.sinz
	//
	// Y acceleration = force * cosx.cosz
	//
	// Z acceleration = force * sinx

	long x_inclination_to_road;
	long y_inclination_to_road = 0;
	long z_inclination_to_road;
	long log_car_length_factor = 4, log_car_width_factor = 3;	// Length is twice the width
	long front_height_difference, surface_value;
	long surface_sinx, surface_cosx;
	long surface_sinz, surface_cosz;
	long surface_cosx_cosz, surface_cosx_sinz;

	// y_inclination_to_road is zero because road exists in X and Z planes only

	// Calculate x_inclination_to_road
	front_height_difference = (front_left_height_difference +
							   front_right_height_difference) >> 1;
	x_inclination_to_road = (front_height_difference -
							 rear_height_difference) >> log_car_length_factor;

	// Calculate sin and cos of X angle between car and road surface
	CalculateInclinationSinCos(x_inclination_to_road,
							   &surface_sinx,
							   &surface_cosx);

	// Calculate z_inclination_to_road
	z_inclination_to_road = (front_left_height_difference -
							 front_right_height_difference) >> log_car_width_factor;

	// Calculate sin and cos of Z angle between car and road surface
	CalculateInclinationSinCos(z_inclination_to_road,
							   &surface_sinz,
							   &surface_cosz);

	surface_cosx_cosz = (surface_cosx * surface_cosz) >> 8;
	surface_cosx_sinz = (surface_cosx * surface_sinz) >> 8;

	//******** Calculate car collision X acceleration ********

	if (z_inclination_to_road < 0)
		surface_value = -surface_cosx_sinz;
	else
		surface_value = surface_cosx_sinz;

	car_collision_x_acceleration = (average_amount_below_road * surface_value) >> 8;

	//******** Calculate car collision Y acceleration ********

	if (y_inclination_to_road < 0)		// never the case at the moment
		surface_value = -surface_cosx_cosz;
	else
		surface_value = surface_cosx_cosz;

	car_collision_y_acceleration = (average_amount_below_road * surface_value) >> 8;

	//******** Calculate car collision Z acceleration ********

	if (x_inclination_to_road < 0)
		surface_value = surface_sinx;
	else
		surface_value = -surface_sinx;

	car_collision_z_acceleration = (average_amount_below_road * surface_value) >> 8;


#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing accelerations by four
	car_collision_x_acceleration++;
	car_collision_y_acceleration++;
	car_collision_z_acceleration++;
	car_collision_x_acceleration >>= 1;
	car_collision_y_acceleration >>= 1;
	car_collision_z_acceleration >>= 1;
#endif
	return;
	}


static long Cosine_Conversion_Table[] =

// Used to convert a sin value from (0*256 - 1*256) into a cosine value.
//
// There are 128 values in this table representing sin values increasing in
// increments of 1/128.
//
// Each value is calculated by getting the inverse sin of the sin value, to
// give the actual angle, then taking the cosine of this angle.  The result
// is then multiplied by 256.
//
// First 8 values should ideally be 256.

// NOTE: They can be changed to 256 at some point, to see what happens, because they are now longs
{
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xfe,
	0xfe,0xfe,0xfd,0xfd,0xfd,0xfd,0xfc,0xfc,
	0xfb,0xfb,0xfb,0xfa,0xfa,0xf9,0xf9,0xf8,
	0xf8,0xf7,0xf7,0xf6,0xf6,0xf5,0xf4,0xf4,
	0xf3,0xf3,0xf2,0xf1,0xf0,0xf0,0xef,0xee,
	0xed,0xec,0xec,0xeb,0xea,0xe9,0xe8,0xe7,
	0xe6,0xe5,0xe4,0xe3,0xe2,0xe1,0xe0,0xdf,
	0xde,0xdd,0xdb,0xda,0xd9,0xd8,0xd6,0xd5,
	0xd4,0xd2,0xd1,0xcf,0xce,0xcc,0xcb,0xc9,
	0xc8,0xc6,0xc5,0xc3,0xc1,0xbf,0xbe,0xbc,
	0xba,0xb8,0xb6,0xb4,0xb2,0xb0,0xae,0xac,
	0xa9,0xa7,0xa5,0xa2,0xa0,0x9d,0x9b,0x98,
	0x95,0x92,0x8f,0x8c,0x89,0x86,0x83,0x7f,
	0x7c,0x78,0x74,0x70,0x6c,0x68,0x63,0x5e,
	0x59,0x53,0x4d,0x47,0x3f,0x37,0x2d,0x20
};


static void CalculateInclinationSinCos (long inclination_in,
										long *inclination_sin_out,
										long *inclination_cos_out)
	{
	// inclination_in is effectively the sin of the inclination angle

	// but it currently has the sign removed and is limited to 255 to enable indexing
	// into the Cosine_Conversion_Table - the sign is re-introduced by the above routine,
	// just before each of the accelerations is calculated

	// future change would be to remove the table and use trig functions instead
	// this would remove the need for a sign check in the above function and may
	// also remove the need to limit the value below to 255

	inclination_in = abs(inclination_in);

	if (inclination_in < 256)
		*inclination_sin_out = inclination_in;
	else
		*inclination_sin_out = 255;

	// note only 128 values in table
	*inclination_cos_out = Cosine_Conversion_Table[(*inclination_sin_out)/2];
	return;
	}


/*	======================================================================================= */
/*	Function:		LiftCarOntoTrack														*/
/*																							*/
/*	Description:	The crane.  Transcribed from Amiga StuntCarRacer's						*/
/*					lift.car.onto.track (Reference only/StuntCarRacer.s:7869),				*/
/*					raise.car.off.ground (:7989) and swing.car (:8012).						*/
/*																							*/
/*					The car does not oscillate on the chains.  It hangs rolled to one		*/
/*					side, the roll decays, and then it hangs there for a random number		*/
/*					of frames before being let go - which is why no two drop starts are		*/
/*					timed alike.															*/
/*	======================================================================================= */

	// players.smaller.y (StuntCarRacer.s:13391).  player_y is the Amiga's players.world.y
	// unchanged - both take the wheel heights as world.y >> 8 - so this is the Amiga's
	// shift as well.  It puts the height into the same units as required_raise_height,
	// which is (road height >> 2).
static long PlayersSmallerY (void)
	{
	return (player_y >> 11);
	}


	// A height servo pulling the car up to required_raise_height + (amount << 8).
	// The return value is the Amiga's signed byte result: only stage 1 looks at it,
	// to decide the crane has taken hold.
static long RaiseCarOffGround (long amount)
	{
	long d3 = static_cast<int16_t>(PlayersSmallerY() - required_raise_height - (amount << 8));

	long d0 = (d3 >> 3) - 256;
	if (d0 < -512) d0 = -512;

	car_collision_y_acceleration -= d0;

	// lsr.w #8 and then a byte add, so this is the unsigned top byte plus 2
	return static_cast<signed char>(((static_cast<unsigned long>(d3) & 0xffff) >> 8) + 2);
	}


#define	SWING_REDUCTION		238		// REDUCTION, StuntCarRacer.s:29

	// Decays the roll towards +/-16 (swing_magnitude's high byte) when adjust is -1,
	// and writes the resulting roll angle.  Returns TRUE once the roll has settled.
static long SwingCar (long adjust)
	{
	long target = 16;					// d4

	if (swing_from_left)
		{
		adjust = -adjust;
		target = -16;
		}

	long step = ((adjust << 8) * SWING_REDUCTION) >> 8;

	// players.x.offset.from.road.centre, from set.road.centre.values (StuntCarRacer.s:13452)
	long x_offset = players_road_x_position - (ROAD_WIDTH/2);
	if (Track[player_current_piece].oppositeDirection)
		x_offset = -x_offset;

	if (static_cast<signed char>(swing_magnitude >> 8) != target)
		swing_magnitude = static_cast<int16_t>(swing_magnitude + step);

	player_z_angle = ((swing_magnitude - (x_offset << 5)) & (MAX_ANGLE - 1));

	overall_difference_below_road = 0;

	return (static_cast<signed char>(swing_magnitude >> 8) == target ? TRUE : FALSE);
	}


	// One Amiga frame of the crane.  Called once per frame in both physics paths -
	// the lift it writes into car_collision_y_acceleration is a whole frame's impulse
	// (FloatV2 divides those by dtRatio precisely so that one step delivers the lot),
	// so running it per physics step would multiply the lift by the step count.

static void LiftCarOntoTrack (void)
	{
	long d1 = car_on_chains_countdown;

	if (d1 == 0)
		return;								// car.not.on.chains

	// SCR_CRANE_TRACE=1 dumps one line per crane frame.
	{
	static long trace = -1;
	if (trace < 0)
		{
		const char *env = getenv("SCR_CRANE_TRACE");
		trace = ((env != NULL) && (atol(env) != 0)) ? 1 : 0;
		}
	if (trace)
		printf("crane rrh=%7ld rah=%7ld cd=%3ld y=%08lx smaller=%6ld req=%6ld target=%6ld yspeed=%8ld touch=%ld "
			   "| roadx=%5ld xoff=%5ld zang=%6ld swing=%6ld xspd=%7ld x=%09lx\n",
			   rear_road_height, rear_actual_height,
			   d1, player_y, PlayersSmallerY(), required_raise_height,
			   required_raise_height + ((d1 == 229) ? (3 << 8) : (d1 == 228 ? (4 << 8) : (2 << 8))),
			   player_world_y_speed, touching_road,
			   players_road_x_position,
			   players_road_x_position - (ROAD_WIDTH/2),
			   player_z_angle, swing_magnitude,
			   player_world_x_speed, player_x);
	}

	if (d1 >= 230)
		{
		// Being picked up: hold the full swing, to whichever side the car left the road.
		// (The Amiga also syncs the side with the other machine here, coll1.sub2.sub3,
		// which has no equivalent in this port - there is no link-up.)
		swing_magnitude = (swing_from_left ? -(44 << 8) : (44 << 8));

		--car_on_chains_countdown;
		return;
		}

	if (d1 == 229)
		{
		// lift.car.stage1
		SwingCar(0);

		if (RaiseCarOffGround(3) >= 0)
			--car_on_chains_countdown;
		return;
		}

	if (d1 == 228)
		{
		// lift.car.stage2 - stays here, no countdown, while the roll decays
		RaiseCarOffGround(4);

		if (! SwingCar(-1))
			return;

		// The roll has decayed, so start the hang - as the Amiga does.  The swing
		// itself is still moving at this point; stage 3 decides where in that swing
		// it is safe to let go.
		chain_release_hold = 0;
		chain_last_road_x  = -1;

		// Touchdown, see chain_touchdown_frames.  Drop start only - a re-lift after
		// going off the track starts from just above the road and has nowhere to
		// descend from.
		chain_touchdown_frames = (drop_start_done ? 0 : CHAIN_TOUCHDOWN_FRAMES);

		// The hang before the drop is random: 160..191, released once the
		// byte reads positive again, so 33..64 Amiga frames.  (The Amiga used a fixed
		// 0x8c in practice mode; this port has no practice mode.)
		car_on_chains_countdown = 160 + (SCR_Rand() & 0x1f);
		return;
		}

	// lift.car.stage3
	SwingCar(0);

	if (chain_touchdown_frames > 0)
		{
		// Touchdown.  raise.car.off.ground(0) would target required_raise_height
		// exactly, which with the >> 3 hold shift is road level in players_smaller_y
		// units - but a servo aimed at the surface only lowers the car until it
		// touches, so the springs never load and the car settles without compressing.
		// Aiming one unit under the road (256 players_smaller_y, 2048 in the road
		// height units the suspension works in) means the crane is still pulling down
		// at road level: the car arrives with the speed of a three-unit drop from the
		// hang height, compresses, rebounds, and then sits with weight on the springs.
		// The crane can only ever win by the difference between its lift at that depth
		// (256 - d3/8 == 224) and CAR.WEIGHT (317), so the squat stays shallow.
		//
		// The hang countdown is frozen meanwhile, so this lengthens the sequence
		// rather than eating into the hang.  Fixed count, not "until touching_road":
		// a piece the servo cannot plant the car on must not hang the crane.
		RaiseCarOffGround(CHAIN_TOUCHDOWN_AMOUNT);
		--chain_touchdown_frames;
		return;
		}

	RaiseCarOffGround(2);

	chain_frame_fraction += 238;
	long fourteen = ((chain_frame_fraction <= 255) ? -1 : 0);
	chain_frame_fraction &= 0xff;

	if (fourteen == 0)
		{
		// not allowed to reach zero here - that would read as "off the chains"
		if (--car_on_chains_countdown == 0)
			++car_on_chains_countdown;
		}

	if (! drop_start_done)
		{
		// The drop start itself is on the random timer.
		if (static_cast<signed char>(car_on_chains_countdown) < 0)
			return;
		}
	else
		{
		// Every later lift is held until fire is pressed.
		if (! chain_fire_pressed)
			return;
		}

	/*
	 * The timer says let go.  On the Amiga it would, at whatever phase of the swing
	 * the car happens to be in.
	 *
	 * swing.car writes players.z.angle = swing.magnitude - (x.offset << 5), so once
	 * the magnitude stops decaying the roll is a position servo: the car hangs
	 * tilted towards the road, the crane's lift (which acts along the car's own y
	 * axis, not the world's) drags it sideways, and it comes to rest where the tilt
	 * reaches zero - x.offset = +/-4096/32 = +/-128, about two thirds of the way
	 * from the road centre to the edge.  That is where the Amiga's drop start puts
	 * you.
	 *
	 * player.to.side.of.road starts the car at x.offset 320, well beyond the road
	 * edge, so the servo has half a road width to travel and arrives with momentum.
	 * The resulting oscillation is only lightly damped (the chain drag is $6000 >> 3)
	 * and still has roughly +/-40 road units of amplitude when the timer expires.
	 * Caught on the way out it lets go at x.offset ~180 with the car still drifting
	 * outwards; the outer wheel sits 32 road units further out again and the edge is
	 * at ROAD_WIDTH/2 == 192, so the car lands on the lip of the track and falls off.
	 *
	 * So rather than wait for the swing to die - which costs seconds the Amiga does
	 * not spend - hold the release for the part of the swing where the drop is safe:
	 * inside the band, and not still travelling outwards.  Bounded, because a piece
	 * where the servo cannot reach its own null (a steep bank, say) must not leave
	 * the car hanging for ever.
	 */
	{
	#define	CHAIN_SAFE_X_OFFSET		144		// outer wheel then lands 16 short of the edge
	#define	CHAIN_MAX_RELEASE_HOLD	40		// two swing periods or so

	long x_offset = players_road_x_position - (ROAD_WIDTH/2);
	long drift    = ((chain_last_road_x < 0) ? 0 : (players_road_x_position - chain_last_road_x));

	chain_last_road_x = players_road_x_position;

	// Measure both towards the side the car hangs on, so the test is one-sided.
	if (x_offset < 0)
		{
		x_offset = -x_offset;
		drift    = -drift;
		}

	if (((x_offset > CHAIN_SAFE_X_OFFSET) || (drift > 2))
		&& (++chain_release_hold < CHAIN_MAX_RELEASE_HOLD))
		return;

	chain_release_hold = 0;
	chain_last_road_x  = -1;
	}

	// car.off.chains
	car_on_chains_countdown = 0;
	off_map_status = 0;

	// The car only wants a drop start at the start of the race.
	drop_start_done = TRUE;
	}


	// Drives the crane from the FloatV2 step, which runs at some other rate.

static void LiftCarOntoTrackFloatV2 (double dt)
	{
	if (! ON_CHAINS)
		return;

	const double BaseDt = 0.1;			// one Amiga frame, as in Physics_FloatV2.cpp

	chain_frame_phase += (dt / BaseDt);
	if (chain_frame_phase >= 0.999999999)
		{
		chain_frame_phase -= 1.0;
		LiftCarOntoTrack();
		}
	}


/*	======================================================================================= */
/*	Function:		CopyLegacyToFloatV2 / CopyFloatV2ToLegacy								*/
/*																							*/
/*	Description:	Boundary between the legacy fixed-point globals and the FloatV2 port.	*/
/*					See Physics_FloatV2.h for the unit mapping.								*/
/*	======================================================================================= */

#define	FV2_XZ_SCALE	(PC_FACTOR * 4)		// player_x/z are 8x the Amiga world units

namespace scr {

// Angle form conversion across the boundary. Legacy globals hold 0..65535
// (MAX_ANGLE == 65536 == 360 degrees); FloatV2 holds the signed equivalent,
// because its pitch/roll clamps and WrapAngle are both written around zero.
static long FV2_ToSignedAngle (long a)
	{
	a &= (MAX_ANGLE - 1);
	return (a >= _180_DEGREES) ? (a - MAX_ANGLE) : a;
	}

static long FV2_ToUnsignedAngle (double a)
	{
	return static_cast<long>(a) & (MAX_ANGLE - 1);
	}

// The C# keeps EnginePower as the Amiga stored it: a byte-reversed word, which
// ComputeEngineAcceleration swaps back before use. Our engine_power is a plain
// 240/320, so pre-reverse it here or the swap-back yields 0xF000 == -4096 and
// the car drives backwards under full power. The swap is its own inverse.
static int16_t FV2_SwapEnginePower (long p)
	{
	return static_cast<int16_t>(((p & 0xFF) << 8) | ((p >> 8) & 0xFF));
	}

// DistanceIntoSection vs NormalDistanceIntoSection.
//
// These are NOT the same value in the reference, and the adapter was feeding
// the same legacy long to both. Physics.cs:3672 builds them as:
//
//     NormalDistanceIntoSection = <raw, piece's own coordinate order>
//     DistanceIntoSection       = DetailNearRoad(Normal, numberOfSegments, plus180)
//
// and DetailNearRoad (Physics.cs:4055) is exactly the plus180 mirror:
//
//     if (plus180) return (numberOfSegments << 8) - normalDistance;
//
// FloatV2's road-height lookup uses *NormalDistanceIntoSection* (the raw one)
// and applies plus180 itself at lookup time, via `reversed` in FV2_GetRoadHeight
// and `plus180` in ProcessOneWheel. But Track.cpp:1422 already builds
// Track[].coords in travel order for plus180 sections, so the legacy
// players_distance_into_section that reaches us is the *mirrored* form — i.e.
// it corresponds to DistanceIntoSection, not to NormalDistanceIntoSection.
//
// So Normal has to be un-mirrored back out of it. This applies to every piece
// type, not just curves: Track.cpp reverses the coords for any section with
// bit 0x10 set. (An earlier version of this experiment gated on curves only,
// which is why toggling it changed nothing either way.)
static long FV2_NormalDistanceIntoSection (long dist)
	{
	if (!scr::gFloatV2UnreverseCurveDist)
		return dist;

	const long piece = player_current_piece;
	if (!Track[piece].oppositeDirection)		// plus180 clear: the two agree
		return dist;

	// 8.8 fixed point; numSegments segments per piece.
	return (Track[piece].numSegments * 256) - dist;
	}

// SectionYAngle for FloatV2.
//
// The same plus180 story as the Z and X mirrors above: FloatV2 wants the
// piece's own frame and applies the travel-direction reversal itself.
//
// CalcSectionYAngle's curve path adds a half-turn for oppositeDirection
// (Car_Behaviour.cpp:3823). The reference's equivalent — Physics.cs:3988,
//
//     num3 = num + 16384 - curveToLeftWord    // curveToLeftWord == -32768 for left
//
// — has no plus180 term at all: CalculatePlayersRoadPositionCurve returns
// plus180 separately (outPlus180) for the lookup to apply. So the half-turn has
// to come back off again here, on curves, whichever way they bend.
//
// The curveToLeftWord term is *not* a second half-turn on top of a correct
// angle: it is the ±90 that turns the radius vector into the tangent, i.e. the
// curve-direction term, and legacy already carries it in the other algebraic
// form — CalcCurveMeasurements returns a magnitude and line 3817 negates it for
// right curves. Treating it as an extra 180 (the previous version of this
// function, XOR-ing oppositeDirection with curveToLeft) happens to give the
// right answer on oppDir/left pieces, because there the spurious 180 cancels
// legacy's, but fabricates one on every plain left curve — which is what threw
// the car into the air on left corners.
//
// This matters because SectionYAngle sets the frame the wheel XZ offsets are
// built in (MakeRotationMatrix's sectionRelAngle = YAngle - SectionYAngle).
// Half a turn there negates sinSec/cosSec, which swaps front/rear and
// left/right wheels, so all three sample the road in the wrong place.
//
// Diagnosed originally from a K dump crossing section 25 (curveLeft 0, oppDir 0
// — fine, heading error ~0) into section 26 (curveLeft 1, oppDir 1 — heading
// error 32641, i.e. exactly 180 degrees, road heights diverging from legacy by
// 3000+ and amount-below-road saturating at 4607 on all three wheels).
static double FV2_SectionYAngle (void)
	{
	const long piece = player_current_piece;

	long rx = 0, rz = 0;
	CalcXZRelativeToPiece(player_x, player_z, piece, &rx, &rz);
	long angle = CalcSectionYAngle(piece, rx, rz);

	// Curves only: straights and diagonals return before CalcSectionYAngle's
	// oppositeDirection adjustment, so there is nothing to undo on those.
	if ((Track[piece].type & 0x80) && Track[piece].oppositeDirection)
		angle -= _180_DEGREES;

	// Legacy holds unsigned 0..65535; FloatV2 wants the signed form (and the
	// 22/05/1998 reversal, so the sense matches the car's own YAngle).
	angle = (-angle & (MAX_ANGLE - 1));
	if (angle >= _180_DEGREES) angle -= MAX_ANGLE;
	return static_cast<double>(angle);
	}

// PlayersRoadXPosition — the across-road counterpart of the Z un-mirror above.
//
// Reversing the direction of travel flips both axes, and Track.cpp:1422
// reverses the whole coordinate list for plus180 sections, so the legacy
// players_road_x_position that reaches us is mirrored across the road as well
// as along it. FloatV2 expects the piece's own frame and applies its own flip
// at lookup time, by swapping the left/right Y-coord blocks in the `reversed`
// branch of FV2_GetRoadHeight — so without this the X axis is flipped once too
// often, exactly as Z was.
//
// Diagnosed from matched 10Hz/60Hz K dumps with the Z un-mirror already on:
// the along-road index then tracked legacy's segment correctly, sections 25 and
// 27 (oppDir 0) matched legacy within ~30, and only section 26 (oppDir 1)
// diverged — by an amount that grew as roadX moved across the road, which is
// the signature of a mirrored across-road coordinate on a banked piece.
//
// ROAD_WIDTH (0x180 == 384) is also FloatV2's off-road bound, so the mirror is
// symmetric: a negative road_x maps above 384 and still reads as off-road.
static long FV2_PlayersRoadXPosition (long roadX)
	{
	if (!scr::gFloatV2UnreverseCurveDist)
		return roadX;
	if (!Track[player_current_piece].oppositeDirection)
		return roadX;

	return ROAD_WIDTH - roadX;
	}

// Boost reserve: legacy keeps a plain count, FloatV2 keeps the Amiga's packed
// BCD (it decrements with BcdSubtract1). Track data supplies the league maxima.
// StandardBoost / SuperBoost are declared at file scope above (Track.cpp defines
// them outside namespace scr, so they cannot be declared extern from in here).

static uint8_t FV2_ToBcd (long value)
	{
	if (value < 0)  value = 0;
	if (value > 99) value = 99;
	return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
	}

static long FV2_FromBcd (uint8_t bcd)
	{
	return static_cast<long>(((bcd >> 4) & 0xF) * 10 + (bcd & 0xF));
	}

// Per-step: only the values the legacy code still owns. Everything else is
// FloatV2's own state and must NOT be round-tripped through the legacy longs
// each step — the fractional parts (BoostUnit, damage remainders, sub-unit
// positions) are exactly what makes the physics rate-independent, and a trip
// through `long` would truncate them away every tick.
void CopyLegacyRoadStateToFloatV2 (PhysicsStateF& s)
	{
	// Road-position tracking: inputs to Tick, not outputs of it. The .NET
	// build maintains these outside the physics assembly; here the legacy
	// CalculateRoadWheelHeights() has just recomputed them for us.
	s.RoadSection               = static_cast<uint8_t>(player_current_piece);
	s.DistanceIntoSection       = static_cast<double>(players_distance_into_section);
	s.NormalDistanceIntoSection = static_cast<double>(FV2_NormalDistanceIntoSection(players_distance_into_section));
	s.PlayersRoadXPosition      = static_cast<double>(FV2_PlayersRoadXPosition(players_road_x_position));

	s.SectionYAngle = FV2_SectionYAngle();

	// Car-to-car impulses are produced by the (still legacy) collision code.
	s.CarToCarXAcceleration = static_cast<int16_t>(car_collision_x_acceleration);
	s.CarToCarYAcceleration = static_cast<int16_t>(car_collision_y_acceleration);
	s.CarToCarZAcceleration = static_cast<int16_t>(car_collision_z_acceleration);

	// League / track state that can change between steps.
	s.RoadCushionValue      = static_cast<uint8_t>(road_cushion_value);
	s.FourteenFramesElapsed = static_cast<uint8_t>(fourteen_frames_elapsed);
	s.OffMapStatus          = static_cast<uint8_t>(off_map_status);
	s.CarOnChainsCountdown  = static_cast<uint8_t>(car_on_chains_countdown);

	// While the crane has the car, swing.car owns the roll angle outright - it is
	// written, not integrated - so it has to be pushed in rather than left to the
	// step's own rotation.
	if (car_on_chains_countdown != 0)
		s.ZAngle = static_cast<double>(FV2_ToSignedAngle(player_z_angle));

	s.RoadID                = static_cast<uint8_t>(TrackID);
	s.EnginePower           = FV2_SwapEnginePower(engine_power);
	s.BoostUnitValue        = static_cast<uint8_t>(boost_unit_value);
	}

// Full seed: on enabling the toggle, or after ResetPlayer/PositionCarAbovePiece
// has moved the car outside the physics.
void CopyLegacyToFloatV2 (PhysicsStateF& s)
	{
	CopyLegacyRoadStateToFloatV2(s);

	s.RoadSection             = static_cast<uint8_t>(player_current_piece);
	s.DistanceIntoSection     = static_cast<double>(players_distance_into_section);
	s.NormalDistanceIntoSection = static_cast<double>(FV2_NormalDistanceIntoSection(players_distance_into_section));
	s.PlayersRoadXPosition    = static_cast<double>(FV2_PlayersRoadXPosition(players_road_x_position));

	// SectionYAngle: the legacy code computes this inside its own
	// CalculateSteering, which we are replacing. See FV2_SectionYAngle.
	s.SectionYAngle = FV2_SectionYAngle();

	// --- Position / orientation --------------------------------------------
	s.WorldX = static_cast<double>(player_x) / FV2_XZ_SCALE;
	s.WorldY = static_cast<double>(player_y);
	s.WorldZ = static_cast<double>(player_z) / FV2_XZ_SCALE;

	// Angles: the legacy globals are unsigned 0..65535, FloatV2 works in the
	// signed -32768..32767 form (see WrapAngle, and the +-11264 pitch/roll
	// clamps in UpdatePosition). Convert, or a car sitting at a hair's-breadth
	// nose-down 65200 seeds as +65200, hits the clamp, and is launched.
	s.XAngle = static_cast<double>(FV2_ToSignedAngle(player_x_angle));
	s.YAngle = static_cast<double>(FV2_ToSignedAngle(player_y_angle));
	s.ZAngle = static_cast<double>(FV2_ToSignedAngle(player_z_angle));

	s.WorldXSpeed = static_cast<double>(player_world_x_speed);
	s.WorldYSpeed = static_cast<double>(player_world_y_speed);
	s.WorldZSpeed = static_cast<double>(player_world_z_speed);

	s.XRotationSpeed = static_cast<double>(player_x_rotation_speed);
	s.YRotationSpeed = static_cast<double>(player_y_rotation_speed);
	s.ZRotationSpeed = static_cast<double>(player_z_rotation_speed);

	// --- Car / league state -------------------------------------------------
	s.EnginePower     = FV2_SwapEnginePower(engine_power);
	s.BoostUnitValue  = static_cast<uint8_t>(boost_unit_value);
	// Boost reserve is another Amiga representation the C# preserves: FloatV2
	// decrements it with BcdSubtract1, so it is packed BCD, while our legacy
	// global is a plain count (--boostReserve, and the HUD prints it %02d).
	s.BoostReserve    = FV2_ToBcd(boostReserve);
	s.BoostUnit       = static_cast<double>(boostUnit);
	// BoostMaxUnits is the post-decrement clamp. Leaving it 0 (the value a
	// zero-initialised PhysicsStateF has) makes `if (next >= max) next = max`
	// fire on the very first decrement and zero the reserve -- boost dies about
	// a second after the lights. It must be the league maximum, not the reserve
	// at seed time: seeding it from the current reserve would clamp the count
	// permanently at whatever it happened to be when the toggle flipped.
	s.BoostMaxUnits   = FV2_ToBcd(bSuperLeague ? SuperBoost : StandardBoost);
	s.BoostActivated  = static_cast<uint8_t>(boost_activated);
	s.Accelerating    = static_cast<uint8_t>(accelerating ? 128 : 0);
	s.WreckWheelHeightReduction = static_cast<int32_t>(wreck_wheel_height_reduction);
	s.SmashedCountdown = static_cast<uint8_t>(smashed_countdown);
	s.TouchingRoad     = static_cast<uint8_t>(touching_road ? 1 : 0);
	s.RoadCushionValue = static_cast<uint8_t>(road_cushion_value);
	s.OffMapStatus     = static_cast<uint8_t>(off_map_status);
	s.DamagedCount     = static_cast<uint8_t>(damaged_count);
	s.DamagedLimit     = static_cast<uint8_t>(damaged_limit);
	s.Damaged          = static_cast<uint8_t>(damaged);
	s.GroundedCount    = static_cast<uint8_t>(grounded_count);
	s.FourteenFramesElapsed = static_cast<uint8_t>(fourteen_frames_elapsed);
	s.CarOnChainsCountdown  = static_cast<uint8_t>(car_on_chains_countdown);
	s.RoadID           = static_cast<uint8_t>(TrackID);
	s.IsSuperLeague    = bSuperLeague;

	// Legacy has no equivalent of B1bb72 ("always set", per CarMovement), so
	// forces are always applied.
	s.CarOnTrack = 1;

	s.FrontLeftDamage  = static_cast<uint8_t>(front_left_damage);
	s.FrontRightDamage = static_cast<uint8_t>(front_right_damage);
	s.RearDamage       = static_cast<uint8_t>(rear_damage);

	s.FrontLeftRoadHeight  = static_cast<double>(front_left_road_height);
	s.FrontRightRoadHeight = static_cast<double>(front_right_road_height);
	s.RearRoadHeight       = static_cast<double>(rear_road_height);

	s.CarToCarXAcceleration = static_cast<int16_t>(car_collision_x_acceleration);
	s.CarToCarYAcceleration = static_cast<int16_t>(car_collision_y_acceleration);
	s.CarToCarZAcceleration = static_cast<int16_t>(car_collision_z_acceleration);

	s.AtSideByte    = static_cast<uint8_t>(at_side_byte);
	s.WhichSideByte = static_cast<uint8_t>(which_side_byte);
	}

void CopyFloatV2ToLegacy (const PhysicsStateF& s)
	{
	player_x = static_cast<long>(s.WorldX * FV2_XZ_SCALE);
	player_y = static_cast<long>(s.WorldY);
	player_z = static_cast<long>(s.WorldZ * FV2_XZ_SCALE);

	// Back to the legacy unsigned 0..65535 form (see CopyLegacyToFloatV2).
	player_x_angle = FV2_ToUnsignedAngle(s.XAngle);
	player_y_angle = FV2_ToUnsignedAngle(s.YAngle);
	// ...except the roll while the crane has the car.  swing.car writes the roll
	// outright, but it only runs on Amiga frames, and at 60Hz that is one step in
	// six.  Letting the step's own integration own the angle for the other five and
	// then snapping it back on the sixth is a sawtooth - a jerk during the hoist,
	// and a violent one once the wheels touch and suspension roll starts feeding
	// ZRotationSpeed.  Pinning it here keeps the pushed-in value (see
	// CopyLegacyRoadStateToFloatV2) in force for every step of the frame.  At
	// dt == 0.1 every step is a crane frame, so this changes nothing.
	if (! ON_CHAINS)
		player_z_angle = FV2_ToUnsignedAngle(s.ZAngle);

	player_world_x_speed = static_cast<long>(s.WorldXSpeed);
	player_world_y_speed = static_cast<long>(s.WorldYSpeed);
	player_world_z_speed = static_cast<long>(s.WorldZSpeed);

	player_x_rotation_speed = static_cast<long>(s.XRotationSpeed);
	player_y_rotation_speed = static_cast<long>(s.YRotationSpeed);
	player_z_rotation_speed = static_cast<long>(s.ZRotationSpeed);

	player_z_speed = static_cast<long>(s.PlayersZSpeed);
	player_x_speed = static_cast<long>(s.PlayersXSpeed);

	// Wheel/collision state the renderer and HUD read.
	touching_road = (s.TouchingRoad != 0) ? TRUE : FALSE;
	front_left_amount_below_road  = static_cast<long>(s.FrontLeftAmountBelowRoad);
	front_right_amount_below_road = static_cast<long>(s.FrontRightAmountBelowRoad);
	rear_amount_below_road        = static_cast<long>(s.RearAmountBelowRoad);

	front_left_height_difference  = static_cast<long>(s.FrontLeftHeightDifference);
	front_right_height_difference = static_cast<long>(s.FrontRightHeightDifference);
	rear_height_difference        = static_cast<long>(s.RearHeightDifference);

	front_left_road_height  = static_cast<long>(s.FrontLeftRoadHeight);
	front_right_road_height = static_cast<long>(s.FrontRightRoadHeight);
	rear_road_height        = static_cast<long>(s.RearRoadHeight);

	front_left_damage  = static_cast<long>(s.FrontLeftDamage);
	front_right_damage = static_cast<long>(s.FrontRightDamage);
	rear_damage        = static_cast<long>(s.RearDamage);
	damaged            = static_cast<long>(s.Damaged);
	damaged_count      = static_cast<long>(s.DamagedCount);
	damage_value       = static_cast<long>(s.DamageValue);
	grounded_count     = static_cast<long>(s.GroundedCount);

	boostReserve    = FV2_FromBcd(s.BoostReserve);	// BCD -> plain count for the HUD
	boostUnit       = static_cast<long>(s.BoostUnit);
	boost_activated = static_cast<long>(s.BoostActivated);
	accelerating    = (static_cast<int8_t>(s.Accelerating) < 0) ? TRUE : FALSE;

	at_side_byte    = static_cast<long>(s.AtSideByte);
	which_side_byte = static_cast<long>(s.WhichSideByte);
	rear_wheel_surface_x_position = static_cast<long>(s.RearWheelSurfaceXPosition);

	// Car-to-car impulses were consumed by the tick.
	car_collision_x_acceleration = 0;
	car_collision_y_acceleration = 0;
	car_collision_z_acceleration = 0;

	// Wheel spin animation: legacy tracks the two front wheels separately,
	// FloatV2 keeps a single value.
	front_left_wheel_speed  = static_cast<long>(s.WheelRotationSpeed);
	front_right_wheel_speed = static_cast<long>(s.WheelRotationSpeed);
	}

} // namespace scr


/*	======================================================================================= */
/*	Function:		CalculateTotalAcceleration												*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void CalculateTotalAcceleration (void)
	{
	long reduction, twice_y;

	player_y_acceleration = gravity_y_acceleration +
							car_collision_y_acceleration;

#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing accelerations by four
//	engine_z_acceleration++;		// 10/12/1998
//	engine_z_acceleration >>= 1;	// 10/12/1998
#endif

	// reduce engine_z_acceleration if car is accelerating and not travelling backwards
	// this probably simulates the effect of wind resistance and the
	// car having reduced ability to accelerate as speed increases
	//21/05/1998 - old method - if ((engine_z_acceleration > 0) && (player_z_speed >= 0))
	reduction = ((engine_z_acceleration / 256) | (player_z_speed / 256)) & 0xff;
	if (!(reduction & 0x80))	// i.e. not negative
		{
		if (engine_z_acceleration&0xff)
			{
			engine_z_acceleration -= reduction;
			}
		}

	// limit engine_z_acceleration to (2 * car_collision_y_acceleration) ?
	// this possibly prevents the car from accelerating
	// if it is not touching the road sufficiently (not enough grip)
	twice_y = GetTwiceCollisionYAcceleration();		// should always be +'ve
	if (abs(engine_z_acceleration) >= twice_y)
		{
		if (engine_z_acceleration < 0)
			twice_y = -twice_y;		// correct sign

		engine_z_acceleration = twice_y;
		}

	player_z_acceleration = engine_z_acceleration +
							gravity_z_acceleration +
							car_collision_z_acceleration;

	CalculateXAcceleration();
	return;
	}


static long GetTwiceCollisionYAcceleration (void)
	{
	if (! touching_road)
		return(0);

	return(car_collision_y_acceleration * 2);
	}


static void CalculateXAcceleration (void)
	{
	long twice_y, acceleration, speed_diff;

	acceleration = gravity_x_acceleration + car_collision_x_acceleration;
	speed_diff = acceleration - player_x_speed;	// speed increase minus current speed

	twice_y = GetTwiceCollisionYAcceleration();		// should always be +'ve
	if (abs(speed_diff) >= twice_y)
		{
		if (player_x_speed < 0)
			twice_y = -twice_y;		// correct sign

		acceleration -= twice_y;
		player_x_acceleration = acceleration;

		// FOLLOWING VALUE NOT USED AT PRESENT
		////collision_in_air = TRUE;	// not sure if it really signifies collision in air
									// don't think it is used anyway
		}
	else
		{
		// why isn't gravity_x_acceleration added here ?
		player_x_acceleration = car_collision_x_acceleration - player_x_speed;

		// FOLLOWING VALUE NOT USED AT PRESENT
		////collision_in_air = FALSE;
		}

	return;
	}


/*	======================================================================================= */
/*	Function:		CalculateSteering														*/
/*																							*/
/*	Description:	Allow the car to steer													*/
/*	======================================================================================= */

static long y_angle_difference, difference_angle, pos_difference_angle;


static void CalculateSteering (void)
	{
	// basically affects player_y_angle
	//			     and player_y_rotation_acceleration (which affects player_y_angle)
	//
	// reason why player_y_angle sometimes needs direct adjustment:-
	//	   to give a one-off adjustment - adjusting the acceleration has a continuing effect

	static long piece = 0;
	long rx, rz;
	long section_y_angle, scaled_pos_difference_angle;
	long left_hand_bend, steering_amount, section_steering_amount;
	long backwards = FALSE;

	// find the piece that the car is currently on
	IdentifyPiece(player_x, player_z, &piece);
	player_current_piece = piece;

	// get section steering amount
	section_steering_amount = Track[piece].steeringAmount;


	// calculate car x/z position relative to the piece (and in same range)
	CalcXZRelativeToPiece(player_x, player_z, piece, &rx, &rz);

	// calculate y angle of piece at the point where the centre of the car lies
	section_y_angle = CalcSectionYAngle(piece, rx, rz);

	// 22/05/1998 - temporarily reverse section_y_angle
	section_y_angle = (-section_y_angle & (MAX_ANGLE - 1));

	// calculate the difference between the section and player's y angle
	// this value should go increasingly -'ve when turning to the right
	// and should go increasingly +'ve when turning to the left
	y_angle_difference = section_y_angle - player_y_angle;

	// extra adjustment due to PC StuntCarRacer angles not taking full words
	// should make y_angle_difference range from -180 to 180 degrees
	if (y_angle_difference > (_180_DEGREES)) y_angle_difference -= (_360_DEGREES);
	if (y_angle_difference < -(_180_DEGREES)) y_angle_difference += (_360_DEGREES);
	// this value should go increasingly -'ve when turning to the right
	// and should go increasingly +'ve when turning to the left

	// extra logic to allow car to drive round track in either direction
	// Amiga StuntCarRacer didn't allow for this
	// 01/07/1998 - NOTE: backwards flag only applies to curves
	if (y_angle_difference > (_90_DEGREES))
		{
		y_angle_difference -= (_180_DEGREES);
		backwards = TRUE;
		}
	if (y_angle_difference < -(_90_DEGREES))
		{
		y_angle_difference += (_180_DEGREES);
		backwards = TRUE;
		}

	//SECTION_Y_ANGLE = section_y_angle;


	// If player is on a curved section then adjust the difference angle
	left_hand_bend = FALSE;
	if ((Track[piece].type == 0x80) || (Track[piece].type == 0xc0))
		{
		// curve
		// 01/07/1998 - correctly identify left/right hand bend when driving round backwards
		if ((Track[piece].type == 0x80) ^ Track[piece].oppositeDirection ^ backwards)
			{
			// right hand bend
			y_angle_difference += 217;
			}
		else
			{
			// left hand bend
			y_angle_difference -= 217;
			left_hand_bend = TRUE;
			}
		}


	difference_angle = y_angle_difference;
	pos_difference_angle = abs(y_angle_difference);

	// Save a scaled positive difference angle ranging from 0 to $7fff
	if (pos_difference_angle < WHEEL_SPEED_LOW_THRESHOLD)
		scaled_pos_difference_angle = pos_difference_angle << 4;
	else
		scaled_pos_difference_angle = 0x7fff;	// set to maximum


	// If on last segment of road section then get data for next section
		// (perhaps because last co-ords aren't used by StuntCarRacer ?)
		// - not done at present


	//fprintf(out, "left_right_value %d\n", left_right_value);
	if (left_right_value != 0)
		{
		// player.is.steering

		// work out if pos_difference_angle is going to increase
		// i.e. car is trying to keep in line with the track or not
		long increasing = ((difference_angle < 0) ^ (left_right_value < 0));

		if ((Track[piece].type == 0x80) || (Track[piece].type == 0xc0))
			{
			// curve
			//fprintf(out, "curve\n");
			if ((left_right_value >= 0) ^ left_hand_bend)
				{
				// steering into the bend
				steering_amount = section_steering_amount + 45;
				}
			else
				{
				// steering away from bend
				steering_amount = section_steering_amount - 35;

				// NOTE: left_right_value below just used as +'ve/-'ve flag
				if (left_hand_bend)
					left_right_value = -1;
				else
					left_right_value = 1;

				// ensure steering assistance is not done
				increasing = TRUE;
				}
			}
		else
			{
			// straight
			//fprintf(out, "straight\n");
			steering_amount = section_steering_amount;
			}

		if (! increasing)
			{
			// Add current difference (between player and road) onto steering amount
			// to assist steering when car is trying to keep in line with track
			steering_amount += (scaled_pos_difference_angle >> 8);
			}

		CalculateSteeringAcceleration(steering_amount);
		// end of function
		}
	else
		{
		// player.not.steering
		y_angle_difference = 0;		// zero steering acceleration

		if ((Track[piece].type == 0x00) || (Track[piece].type == 0x40))
			{
			// straight
			AlignCarWithRoad();
			AdjustSteeringAcceleration();
			}
		else
			{
			// curve

			// NOTE: left_right_value below just used as +'ve/-'ve flag
			if (left_hand_bend)
				left_right_value = -1;
			else
				left_right_value = 1;

			steering_amount = section_steering_amount;
			// give effect of centrifugal force ?
			CalculateSteeringAcceleration(steering_amount);
			}
		}

	return;
	}


static void CalculateSteeringAcceleration (long steering_amount)
	{
	// Steering acceleration increases as player's speed increases

	long steering_acceleration;
	// get y_angle_difference, pos_difference_angle from calling function


	// following value calculated in slightly odd way, to match Amiga StuntCarRacer
	steering_acceleration = (player_z_speed * steering_amount) >> 8;

	if (left_right_value < 0)
		{
		// steering left
		steering_acceleration = -steering_acceleration;
		}

	steering_acceleration = steering_acceleration >> 3;


	// store steering acceleration
	y_angle_difference = steering_acceleration;

	if (pos_difference_angle >= (30*256))
		{
		AlignCarWithRoad();
		}
	AdjustSteeringAcceleration();
	return;
	}


static void AlignCarWithRoad (void)
	{
	// Following code used to gradually bring the car back in
	// line with the road - this helps steering considerably

	long adjust, speed;

	// eventually get difference_angle, pos_difference_angle from calling function

	adjust = pos_difference_angle;

	if (adjust >= 256)
		{
		adjust -= (30*256);
		if (adjust >= 0)
			{
			// this section makes a large adjustment, e.g. 60 degrees,
			// for when the car is very out of line (e.g. sideways with respect to road)

			// just use remainder to adjust player's y angle
			// needs to correct signs because PC StuntCarRacer rotation is in opposite direction
			if (difference_angle >= 0)
				player_y_angle += adjust;
			else
				player_y_angle -= adjust;

			return;
			}

		// set adjustment amount to maximum
		adjust = 255;
		}

	// Adjustment of player's Y angle increases as player's speed increases

	speed = abs(player_z_speed) + 0xa00;
	if (speed > 0x7f00)
		speed = 0x7f00;		// set speed amount to maximum



	adjust = ((adjust * speed) >> 15);

#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing adjustment by four
	adjust++;
	adjust >>= 1;
#endif

	if (adjust == 0) adjust = 1;		// atleast do some adjusting


	// needs to correct signs because PC StuntCarRacer rotation is in opposite direction
	if (difference_angle >= 0)
		player_y_angle += adjust;
	else
		player_y_angle -= adjust;

	return;
	}


static void AdjustSteeringAcceleration (void)
	{
	// eventually get y_angle_difference from calling function

	long acceleration = y_angle_difference - player_y_rotation_speed;

	// store steering acceleration
	// needs to correct signs because PC StuntCarRacer rotation is in opposite direction
	if (touching_road)
		player_y_rotation_acceleration = acceleration;
	else
		player_y_rotation_acceleration = 0;	// steering disabled

#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing accelerations by four
	player_y_rotation_acceleration++;
	player_y_rotation_acceleration >>= 1;
#endif

	return;
	}


// current piece x/z co-ords (i.e. four corners of piece)
static long px1, pz1, px2, pz2, px3, pz3, px4, pz4;


static void IdentifyPiece (long x, long z, long *piece_in_out)
	{
	// find the piece that the point is located within
	long piece = *piece_in_out;

	// defaults to the input piece if no piece could be found using the map
	GetPieceUsingMap(x, z, &piece);

	// get the four (x,y,z) corner points of the piece
	GetPieceCoords(piece);


//****************


	// check point is not before or after piece (z direction)
	long xs, xp, zs, zp, rx, rz;
	long before_piece = TRUE, after_piece = TRUE, num_piece_changes;


	// 'before piece' loop
	num_piece_changes = 0;
	while (before_piece)
		{
		CalcXZRelativeToPiece(x, z, piece, &rx, &rz);

		// calculate top dot product => before_piece
		xs = px1 - px4; zs = pz1 - pz4;		// current segment vector
		xp = rx - px4; zp = rz - pz4;		// current point vector
		before_piece = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		if (before_piece)
			{
			// future improvement: try a move in one direction,
			// if this is worse then move in other direction

			// DIRECTION DEPENDANT - WHOLE SECTION
			// go to next piece
			piece++; if (piece > (NumTrackPieces - 1)) piece = 0;
			num_piece_changes++;

			// get the four (x,y,z) corner points of the new piece
			GetPieceCoords(piece);
			}

		// prevent an infinite loop
		if (num_piece_changes >= NumTrackPieces)
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "IdentifyPiece - infinite loop trapped (1)\n");
#endif
			break;
			}
		}

#if defined(DEBUG) || defined(_DEBUG)
	// warn if piece search was inefficient
	if (num_piece_changes > 2)		// arbitrary number
		fprintf(out, "IdentifyPiece - %d changes (1)\n", num_piece_changes);
#endif


	// 'after piece' loop
	num_piece_changes = 0;
	while (after_piece)
		{
		CalcXZRelativeToPiece(x, z, piece, &rx, &rz);

		// calculate bottom dot product => after_piece
		xs = px3 - px2; zs = pz3 - pz2;		// current segment vector
		xp = rx - px2; zp = rz - pz2;		// current point vector
		after_piece = (((xs * zp) - (xp * zs)) < 0 ? TRUE : FALSE);

		if (after_piece)
			{
			// future improvement: try a move in one direction,
			// if this is worse then move in other direction

			// DIRECTION DEPENDANT - WHOLE SECTION
			// go to previous piece
			piece--; if (piece < 0) piece = (NumTrackPieces - 1);
			num_piece_changes++;

			// get the four (x,y,z) corner points of the new piece
			GetPieceCoords(piece);
			}

		// prevent an infinite loop
		if (num_piece_changes >= NumTrackPieces)
			{
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "IdentifyPiece - infinite loop trapped (2)\n");
#endif
			break;
			}
		}

#if defined(DEBUG) || defined(_DEBUG)
	// warn if piece search was inefficient
	if (num_piece_changes > 2)		// arbitrary number
		fprintf(out, "IdentifyPiece - %d changes (2)\n", num_piece_changes);
#endif

	*piece_in_out = piece;
	return;
	}


static void GetPieceCoords (long piece)
	{
	long numSegments = Track[piece].numSegments;

	px2 = Track[piece].coords[0].x;
	pz2 = Track[piece].coords[0].z;

	px3 = Track[piece].coords[1].x;
	pz3 = Track[piece].coords[1].z;

	px1 = Track[piece].coords[(numSegments*4)].x;
	pz1 = Track[piece].coords[(numSegments*4)].z;

	px4 = Track[piece].coords[(numSegments*4)+1].x;
	pz4 = Track[piece].coords[(numSegments*4)+1].z;

	return;
	}


/*	======================================================================================= */
/*	Function:		CalculateWorldAcceleration												*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void CalculateWorldAcceleration (void)
	{
	short *trig_coeffs = TrigCoefficients();

	// Adds components of player's (i.e. rotated) X, Y and Z accelerations
	// to give world acceleration values.

	// this function basically does the same as WorldOffset,
	// then removes the precision from the resulting values

	total_world_x_acceleration =  ((player_x_acceleration * static_cast<long>(trig_coeffs[X_X_COMP])) >> LOG_PRECISION);
	total_world_x_acceleration += ((player_y_acceleration * static_cast<long>(trig_coeffs[Y_X_COMP])) >> LOG_PRECISION);
	total_world_x_acceleration += ((player_z_acceleration * static_cast<long>(trig_coeffs[Z_X_COMP])) >> LOG_PRECISION);

	total_world_y_acceleration =  ((player_x_acceleration * static_cast<long>(trig_coeffs[X_Y_COMP])) >> LOG_PRECISION);
	total_world_y_acceleration += ((player_y_acceleration * static_cast<long>(trig_coeffs[Y_Y_COMP])) >> LOG_PRECISION);
	total_world_y_acceleration += ((player_z_acceleration * static_cast<long>(trig_coeffs[Z_Y_COMP])) >> LOG_PRECISION);

	total_world_z_acceleration =  ((player_x_acceleration * static_cast<long>(trig_coeffs[X_Z_COMP])) >> LOG_PRECISION);
	total_world_z_acceleration += ((player_y_acceleration * static_cast<long>(trig_coeffs[Y_Z_COMP])) >> LOG_PRECISION);
	total_world_z_acceleration += ((player_z_acceleration * static_cast<long>(trig_coeffs[Z_Z_COMP])) >> LOG_PRECISION);
	return;
	}


/*	======================================================================================= */
/*	Function:		ReduceWorldAcceleration													*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

extern bool player_close_to_opponent;
extern bool opponent_behind_player;


static void ReduceWorldAcceleration (void)
	{
	long	amount = 0, factor, normal_situation = TRUE;
	long	y_speed, z_speed, reduction;

	factor = 1;		// set maximum reduction factor

	if ((touching_road) || (ON_CHAINS))
		{
		amount = abs(car_to_road_collision_z_acceleration >> 8);

		if ((amount >= 3) || (off_map_status != 0) || (WRECKED) || (ON_CHAINS))
			{
			// collision_z_acceleration large, off map, wrecked or on chains
			if ((WRECKED) || (ON_CHAINS))
				factor = 3;		// set medium reduction factor

			amount = 0x6000;
			normal_situation = FALSE;
			}
		}

	// Normal case - car not on chains, little Z collision with road
	if (normal_situation)
		{
		// reduce accelerations, depending upon car speed

		// get greatest of player's x, y and z speeds
		amount = abs(player_x_speed);

		y_speed = abs(player_y_speed);		// zero for current implementation
		if (y_speed > amount)
			amount = y_speed;

		z_speed = abs(player_z_speed);
		if (z_speed > amount)
			amount = z_speed;

		factor = 5;		// set minimum reduction factor

		// Check slipstream
		//
		// If player and opponent are in line left to right and the opponent is
		// infront of the player then the player is in the slipstream of the
		// opponent, so there is less drag on the player's car.
		if ((player_close_to_opponent) && (!opponent_behind_player))
			{
			// Make reduction smaller
			amount -= (20*128);
			if (amount < 0) amount = 0;
			}
		}

	// Reduce acceleration values using current speed values.
	// amount = reduction amount, factor = overall reduction factor.
	reduction = (((player_world_x_speed * amount) >> 16) >> factor);
	total_world_x_acceleration -= reduction;

	reduction = (((player_world_y_speed * amount) >> 16) >> factor);
	total_world_y_acceleration -= reduction;

	reduction = (((player_world_z_speed * amount) >> 16) >> factor);
	total_world_z_acceleration -= reduction;
	return;
	}


/*	======================================================================================= */
/*	Function:		CalculateXZRotationAcceleration											*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void CalculateXZRotationAcceleration (void)
	{
	// Calculate values using current car rotation speeds and inclination values
	// between the car and the road, in order to damp the car X and Z angles
	// and keep the car level with the road, on its X and Z axes.  Also give
	// effect of acceleration.

	// Remember that players_y_rotation_acceleration is set by CalculateSteering()

	// overall.difference.below.road is effectively car X inclination.
	//
	// front.difference.below.road is effectively car Z inclination.

	// question: why aren't the x/z inclinations calculated by
	//			 calculate_car_collision_acceleration used here ?
	//
	// - perhaps values used are really accelerations rather than inclinations

	player_x_rotation_acceleration = overall_difference_below_road -
										(player_x_rotation_speed >> 4);
	if (touching_road)
		{
		// This part lifts the car up at the front during forwards acceleration
		// and, vice versa, dips the front of the car during backwards acceleration.
		player_x_rotation_acceleration += (player_z_acceleration >> 2);
		}

	player_z_rotation_acceleration = front_difference_below_road -
										(player_z_rotation_speed >> 4);

#ifdef	HIGHER_FRAME_RATE
	// 08/11/1998 - allow four times the frame rate by dividing accelerations by four
	player_x_rotation_acceleration++;
	player_z_rotation_acceleration++;
	player_x_rotation_acceleration >>= 1;
	player_z_rotation_acceleration >>= 1;
#endif
	return;
	}


/*	======================================================================================= */
/*	Function:		UpdatePlayersRotationSpeed												*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void UpdatePlayersRotationSpeed (void)
	{
	long acceleration;

	acceleration = ((player_x_rotation_acceleration * REDUCTION) >> 8);
	player_x_rotation_speed += acceleration;

	acceleration = ((player_y_rotation_acceleration * REDUCTION) >> 8);
	player_y_rotation_speed += acceleration;

	acceleration = ((player_z_rotation_acceleration * REDUCTION) >> 8);
	player_z_rotation_speed += acceleration;
	return;
	}


/*	======================================================================================= */
/*	Function:		CalculateFinalRotationSpeed												*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void CalculateFinalRotationSpeed (void)
	{
	short sin_x, cos_x;
	short sin_z, cos_z;

	GetSinCos(player_x_angle, &sin_x, &cos_x);	// cosine not used
	GetSinCos(player_z_angle, &sin_z, &cos_z);

	// shouldn't need changing because Amiga StuntCarRacer Z rotation appears to be same as
	// PC StuntCarRacer Z rotation (i.e. RotX = Xcosz - Ysinz, RotY = Xsinz + Ycosz)
	// and so does X rotation

	player_final_x_rotation_speed =  ((player_x_rotation_speed * static_cast<long>(cos_z)) >> LOG_PRECISION);
	player_final_x_rotation_speed += ((player_y_rotation_speed * static_cast<long>(-sin_z)) >> LOG_PRECISION);

	player_final_y_rotation_speed =  ((player_x_rotation_speed * static_cast<long>(sin_z)) >> LOG_PRECISION);
	player_final_y_rotation_speed += ((player_y_rotation_speed * static_cast<long>(cos_z)) >> LOG_PRECISION);

	// Calculate final Z rotation speed by rotating Y rotation speed about
	// the X axis and adding it onto the Z rotation speed.
	player_final_z_rotation_speed =  player_z_rotation_speed;
	player_final_z_rotation_speed += ((player_final_y_rotation_speed * static_cast<long>(sin_x)) >> LOG_PRECISION);
	return;
	}


/*	======================================================================================= */
/*	Function:		UpdatePlayersWorldSpeed													*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void UpdatePlayersWorldSpeed (void)
	{
	long acceleration;

	acceleration = ((total_world_x_acceleration * REDUCTION) >> 8);
	player_world_x_speed += acceleration;

	acceleration = ((total_world_y_acceleration * REDUCTION) >> 8);
	player_world_y_speed += acceleration;

	acceleration = ((total_world_z_acceleration * REDUCTION) >> 8);
	player_world_z_speed += acceleration;
	return;
	}

/*	======================================================================================= */
/*	Function:		UpdatePlayersPosition													*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static void UpdatePlayersPosition (void)
	{
	long speed, angle, limit;

//******** Set player's new position ********

#ifdef ORIGINAL
	// following speeds could be worked out differently, using just one shift, then an AND
	// not sure yet why the speeds need the bottom bits clear anyway
	speed = ((player_world_x_speed * REDUCTION) >> 8);
	speed <<= 6;
	// convert to PC StuntCarRacer magnitude
	speed *= (PC_FACTOR * 4);
	player_x += speed;

	speed = ((player_world_y_speed * REDUCTION) >> 8);
	speed <<= 7;	// not sure why this is different
	player_y += speed;

	speed = ((player_world_z_speed * REDUCTION) >> 8);
	speed <<= 6;
	// convert to PC StuntCarRacer magnitude
	speed *= (PC_FACTOR * 4);
	player_z += speed;
#else
	// 22/10/1998 - simplified the above
	speed = ((player_world_x_speed * REDUCTION) * PC_FACTOR);
	player_x += speed;

	speed = ((player_world_y_speed * REDUCTION) >> 1);
	player_y += speed;

	speed = ((player_world_z_speed * REDUCTION) * PC_FACTOR);
	player_z += speed;
#endif

	if (player_y >= 0x10000000)
		player_y = 0x10000000;

//******** Set player's new angles ********

	speed = ((player_final_x_rotation_speed * REDUCTION) >> 8);
	player_x_angle += speed;

	speed = ((player_final_y_rotation_speed * REDUCTION) >> 8);
	player_y_angle += speed;

	speed = ((player_final_z_rotation_speed * REDUCTION) >> 8);
	player_z_angle += speed;

	// 19/05/1998 - limit to valid range as no longer stored as words
	player_x_angle &= (MAX_ANGLE - 1);
	player_y_angle &= (MAX_ANGLE - 1);
	player_z_angle &= (MAX_ANGLE - 1);

//******** Check player's X angle ********

	if ((at_side_byte == 0xe0) && (smaller_limit_required))
		{
		// all wheels off road and car on ground
		limit = 11*256;
		}
	else // not off side of track
		{
		limit = 45*256;
		}

	// get angle in Amiga StuntCarRacer format (i.e. correct sign)
	angle = (player_x_angle < (_180_DEGREES) ? (player_x_angle) :
											   (player_x_angle - _360_DEGREES));

	if (abs(angle) > limit)
		{
		if (angle >= 0)
			angle = limit;
		else
			angle = -limit;

		// get players_x_angle in PC StuntCarRacer format (i.e. correct sign)
		player_x_angle = (angle > 0 ? (angle) : (angle + _360_DEGREES));

		if (((player_x_rotation_speed >= 0) && (angle < 0)) ||
			((player_x_rotation_speed < 0) && (angle >= 0)))
			{
			// values have different signs
			player_x_rotation_speed = 0;
			}
		}

//******** Check player's Z angle ********

	// get angle in Amiga StuntCarRacer format (i.e. correct sign)
	angle = (player_z_angle < (_180_DEGREES) ? (player_z_angle) :
											   (player_z_angle - _360_DEGREES));

	if (abs(angle) > limit)
		{
		if (angle >= 0)
			angle = limit;
		else
			angle = -limit;

		// get players_z_angle in PC StuntCarRacer format (i.e. correct sign)
		player_z_angle = (angle > 0 ? (angle) : (angle + _360_DEGREES));

		if (((player_z_rotation_speed >= 0) && (angle < 0)) ||
			((player_z_rotation_speed < 0) && (angle >= 0)))
			{
			// values have different signs
			player_z_rotation_speed = 0;
			}
		}

//****************************************

	// rest of Amiga StuntCarRacer code not needed
	return;
	}


/*	======================================================================================= */
/*	Function:		CalcSectionYAngle														*/
/*																							*/
/*	Description:	Calculates the piece y angle at the x/z point (e.g. centre of car).		*/
/*																							*/
/*					(Assumes the provided x/z point is within and relative to the piece.)	*/
/*	======================================================================================= */

static long CalcSectionYAngle (long piece,
							   long x,
							   long z)
	{
	long section_y_angle;
	long radius;					// not used
	double distance_from_centre;	// not used

	// check for and handle straight
	if (Track[piece].type == 0x00)
		{
		section_y_angle = -Track[piece].roughPieceAngle;
		section_y_angle &= (MAX_ANGLE - 1);
		return(section_y_angle);
		}
	// check for and handle diagonal straight
	else if (Track[piece].type == 0x40)
		{
		// Amiga StuntCarRacer always adds 0x2000 on for these pieces (i.e. 45 degrees)
		section_y_angle = -(Track[piece].roughPieceAngle + (MAX_ANGLE/8));
		section_y_angle &= (MAX_ANGLE - 1);
		return(section_y_angle);
		}

	// must be a curve (type will be -'ve)
	CalcCurveMeasurements(piece, x, z, &section_y_angle, &radius, &distance_from_centre);

	// change sign if right hand curve (i.e. default calculation is for left hand curve)
	if (! Track[piece].curveToLeft)
		section_y_angle = -section_y_angle;

	// add on rough piece angle to get final section y angle
	section_y_angle -= Track[piece].roughPieceAngle;

	// adjust for normal direction of travel
	if (Track[piece].oppositeDirection)
		section_y_angle += (MAX_ANGLE/2);	// plus 180 degrees

	// limit to valid range
	section_y_angle &= (MAX_ANGLE - 1);

	return(section_y_angle);
	}


/*	======================================================================================= */
/*	Function:		CalcCurveMeasurements													*/
/*																							*/
/*	Description:	Calculates 1) y angle within the piece at the x/z point.				*/
/*							   2) inner or outer edge radius of the piece.					*/
/*							   3) distance from the piece's circle centre to the x/z point.	*/
/*																							*/
/*					(Assumes the provided x/z point is within and relative to the piece.)	*/
/*	======================================================================================= */

static void CalcCurveMeasurements (long piece,
								   long x,
								   long z,
								   long *y_angle_out,
								   long *radius_out,
								   double *distance_from_centre_out)
	{
	long xf, zf, xl, zl, xo, zo, xc, zc;
	long numSegments, radius;
	double o, a, radians, angle;

	// NOTE: Assumes x/z are relative to (and in same range as) piece co-ordinates

	// start of code - initialise outputs to zero
	*y_angle_out = 0;
	*radius_out = 0;
	*distance_from_centre_out = 0;

	// calculate radius of circle that piece is taken from
	// calculates inner or outer edge radius, depending upon co-ordinate layout
	numSegments = Track[piece].numSegments;

	// get first and last co-ordinates from inner or outer edge
	long first = 0, last = numSegments*4;

	xf = Track[piece].coords[first].x;
	zf = Track[piece].coords[first].z;
	xl = Track[piece].coords[last].x;
	zl = Track[piece].coords[last].z;

	// assumes all curved pieces are 45 degree circular arcs
	radius = abs(xl - xf) + abs(zl - zf);

	// 14/05/1998 - need to use horizontal/vertical edge when calculating circle centre
	// check first edge is horizontal/vertical, if not then use last edge
	xo = Track[piece].coords[first+1].x;
	zo = Track[piece].coords[first+1].z;
	if ((xo != xf) && (zo != zf))
		{
		// use last edge
		// note that variable names are now misleading (i.e. opposite meaning)
		xf = Track[piece].coords[last].x;
		zf = Track[piece].coords[last].z;
		xl = Track[piece].coords[first].x;
		zl = Track[piece].coords[first].z;

		xo = Track[piece].coords[last+1].x;
		zo = Track[piece].coords[last+1].z;
		}

	// check resulting edge is horizontal/vertical
	if ((xo != xf) && (zo != zf))
		{
#if defined(DEBUG) || defined(_DEBUG)
		fprintf(out, "Piece %d has no horizontal or vertical edge\n", piece);
#endif
		return;
		}

	// calculate co-ordinate of circle centre
	// uses first co-ordinate from other edge, for comparison
	if (xo != xf)
		{
		// piece edge is horizontal
		if (xf < xl)
			xc = xf + radius;
		else
			xc = xf - radius;

		zc = zf;

		o = static_cast<double>(z - zc);
		a = static_cast<double>(x - xc);
		}
	else if (zo != zf)
		{
		// piece edge is vertical
		xc = xf;

		if (zf < zl)
			zc = zf + radius;
		else
			zc = zf - radius;

		o = static_cast<double>(x - xc);
		a = static_cast<double>(z - zc);
		}
	else
		{
#if defined(DEBUG) || defined(_DEBUG)
		fprintf(out, "Piece %d edge is invalid (both ends are same)\n", piece);
#endif
		return;
		}

	// use inverse tan to calculate basic angle in radians
	if (a == 0)		// prevent division by zero
		radians = static_cast<double>(PI) / static_cast<double>(2);	// 90 degrees
	else
		radians = atan(o/a);	// inverse tan

	// convert radians to internal angle (also round up)
	angle = ((radians * static_cast<double>(MAX_ANGLE)) / (static_cast<double>(2) * static_cast<double>(PI)));
	// convert to absolute and round up as follows (because abs() isn't for doubles)
	if (angle > 0)
		*y_angle_out = static_cast<long>(angle + static_cast<double>(0.5));
	else
		*y_angle_out = static_cast<long>(static_cast<double>(0.5) - angle);


	// output radius
	*radius_out = radius;


	// calculate distance from circle centre to (x,z) point
	*distance_from_centre_out = sqrt((o*o) + (a*a));
	return;
	}


/*	======================================================================================= */
/*	Function:		AmigaVolumeToDirectX													*/
/*																							*/
/*	Description:	Convert an Amiga volume level to a value for use with DirectX			*/
/*	======================================================================================= */

long AmigaVolumeToDirectX (long amiga_volume)
	{
	static long first_time = TRUE;
	static long directx_volume[MAX_AMIGA_VOLUME+1];		// range 0 to MAX
	long i;
	double db;

	if ( first_time )
	    {
		// populate the lookup table
		// NOTE: volume 0 cannot be calculated
		directx_volume[0] = -100 * DIRECTX_VOLUME_FACTOR;	// -100 dB, essentially silent

		for ( i = 1; i < (MAX_AMIGA_VOLUME+1); i++ )
			{
			db = static_cast<double>(20) * log10(static_cast<double>(i)/static_cast<double>(MAX_AMIGA_VOLUME));
			directx_volume[i] = static_cast<long>(db * static_cast<double>(DIRECTX_VOLUME_FACTOR));
			}

		first_time = FALSE;
		}

	// validate Amiga volume
	if ((amiga_volume < 0) || (amiga_volume > MAX_AMIGA_VOLUME))
		{
#if defined(DEBUG) || defined(_DEBUG)
		fprintf(out, "Invalid Amiga Volume %d - defaulting to maximum\n", amiga_volume);
#endif
		amiga_volume = MAX_AMIGA_VOLUME;
		}

	// return DirectX volume
	return(directx_volume[amiga_volume]);
	}


/*	======================================================================================= */
/*	Function:		PositionCarAbovePiece													*/
/*																							*/
/*	Description:	Position car above middle of piece, facing correct direction			*/
/*	======================================================================================= */

/*
extracts from :-

set.players.restart.position
	find suitable road section for car to start on
	position car above middle of section's square, facing correct direction
	calculate player's x offset from centre of road (flag if off road)
	put player to one side of the road (player.to.side.of.road)
	return;
*/

extern unsigned char sections_car_can_be_put_on[];


static void PositionCarAbovePiece (long piece)
{
	long piece_x, piece_z, height;

	//******** Find section to lower car onto ********
	for (;;)
	{
		long t = GetPieceAngleAndTemplate(piece);
		t &= 0xf;	// templateNum
		if (sections_car_can_be_put_on[t] & 0x80)
		{
			// go to previous piece if already at first surface
			piece--; if (piece < 0) piece = (NumTrackPieces - 1);
		}
		else
			break;
	}

	// Should also reject Track specific pieces (DAT.1c8e8)


	// calculate x/z position of piece's front left corner, within world
	piece_x = Track[piece].x << LOG_CUBE_SIZE;
	piece_z = Track[piece].z << LOG_CUBE_SIZE;

	// set car x/z position to middle of piece
	player_x = piece_x + CUBE_SIZE/2;// + CUBE_SIZE/16;
	player_z = piece_z + CUBE_SIZE/2;// + CUBE_SIZE/16;

	// set car y position
//	debug_position = TRUE;
	CalculateWorldRoadHeight(0, player_x, player_z, &height);
#if defined(DEBUG) || defined(_DEBUG)
	debug_position = FALSE;
	fprintf(out, "PositionCarAbovePiece x,z: 0x%x,0x%x.  CalculateWorldRoadHeight: 0x%x\n", player_x, player_z, height);
#endif
	// convert the result to PC StuntCarRacer magnitude
	height = ((height / PC_FACTOR) >> (LOG_PRECISION-3));

	/*
	 * Then the tail of set.road.position.values (StuntCarRacer.s:13760).
	 *
	 * The car goes back on the crane's chains, and where it starts from depends on
	 * which kind of lift this is.  The drop start at the beginning of a race starts
	 * from a fixed height right up in the air; a re-lift after going off the track
	 * starts from just above the road.  required_raise_height is the height the
	 * crane holds it at either way, and is in players_smaller_y units.
	 */
	/*
	 * How high the crane holds the car.
	 *
	 * The Amiga writes required.raise.height = rear.road.height >> 2, and places the
	 * car above the piece at (rear.road.height << 9) + $180000.  Both are a factor of
	 * two larger than the road itself, which sits at world.y = road height << 8:
	 * calculate.actual.wheel.heights (StuntCarRacer.s:15873) takes the wheel heights as
	 * world.y >> 8, and the collision compares those against the road heights directly.
	 *
	 * The two are consistent with each other, so the crane does hold the car steady -
	 * but at 2 * road height + 0x1800 instead of 0x1800 above the road, so the higher
	 * the piece, the further above it the car hangs.  (players.smaller.y is world.y >> 11,
	 * so one of its units is eight of road height's, and the servo settles 488 of them
	 * below its target - lift is 256 - d3/8 clamped at 512, gravity is CAR.WEIGHT 317.)
	 * On track 1's start piece, road height 10240 with the off-road floor at 0x1000,
	 * the car is released 10432 above the road - two thirds of the whole drop from the
	 * road down to the floor.
	 *
	 * Reading the shift as >> 3 instead holds the car a fixed 0x1800 above the road,
	 * the constant the Amiga itself pairs it with, and the height the pre-crane PC port
	 * dropped the car from.  Compared side by side against the Amiga, that is the one
	 * that looks right - the transcribed >> 2 visibly hoists the car too far up - so it
	 * is the default, and SCR_CRANE_HOLD=amiga selects the literal shift for comparison.
	 */
	static long hold_shift = 0;
	if (hold_shift == 0)
		{
		const char *env = getenv("SCR_CRANE_HOLD");
		hold_shift = ((env != NULL) && (strcmp(env, "amiga") == 0)) ? 2 : 3;
		}

	if (! drop_start_done)
		{
		player_y = 0x100000;					// players.world.y = 16 << 16
		car_on_chains_countdown = 240;
		}
	else
		{
		// (rear.road.height << 9) + $180000.  Placed so that stage 1's
		// raise.car.off.ground(3) returns zero on the first frame, which is how the
		// crane knows it has taken hold - so it follows the shift above.
		player_y = ((height << (11 - hold_shift)) + 0x180000);
		car_on_chains_countdown = 230;
		}

	required_raise_height = (height >> hold_shift);

	if (getenv("SCR_CRANE_TRACE") != NULL)
		printf("PositionCarAbovePiece piece=%ld drop_start_done=%ld road height=%ld (0x%lx) "
			   "ground player_y would be 0x%lx, set player_y=0x%lx, required=%ld\n",
			   piece, drop_start_done, height, height, (height << 8), player_y, required_raise_height);

	swing_magnitude = 0;
	chain_frame_phase = 0.0;
#if defined(DEBUG) || defined(_DEBUG)
	fprintf(out, "PositionCarAbovePiece player_y 0x%x\n", player_y);
#endif

	// clear car x/z angle
	player_x_angle = 0;
	player_z_angle = 0;

	// set car y angle
	player_y_angle = Track[piece].roughPieceAngle;

	if (Track[piece].oppositeDirection)
		{
		player_y_angle += (MAX_ANGLE/2);	// plus 180 degrees
		}

	// check for and handle diagonal straight
	if (Track[piece].type == 0x40)
		{
		// Amiga StuntCarRacer always adds 0x2000 on for these pieces (i.e. 45 degrees)
		player_y_angle += (MAX_ANGLE/8);
		}

	player_y_angle &= (MAX_ANGLE - 1);

	/*
	 * Then player.to.side.of.road
	 *
	 * Shift player in x direction by 160.
	 *
	 * This is actually x = 160, z = 0 being rotated about the y axis and then added to the player x and z.
	 *
	 * The side is whichever one the car left the road on (swing_from_left), so the
	 * crane picks it up where it went off rather than always from the right.
	 */
	long side = (swing_from_left ? -160 : 160);

	// The Amiga's arithmetic (ptsor1, StuntCarRacer.s:8070) is
	// (160 << 7) * trig * 2 >> 16 << 6, i.e. 160 * trig / 4 world units.  Its
	// sin/cos table is full scale 32768 (get.sin, :20112, ends on lsr.w #1 of an
	// unsigned 0..$ffff entry), so that is the 160 * 8192 the routine's own comment
	// quotes: 20/128 of a map cube.  That is far enough that the car hangs beyond the
	// edge of the road - road half width is 384 in piece coords, this is 640 - and so
	// ends up on the ground beside the track rather than on the road surface.
	//
	// Our Sin_Cos[] is PRECISION (16384) full scale, half the Amiga's, and player_x/z
	// are 8x the Amiga's world units, so the same shift is (160 * 4) * cos here.
	//
	// SCR_SIDE_OFFSET overrides the 4 while the distance is being matched against the
	// Amiga by eye; unset (or 0) keeps the transcribed value.
	static long scale = 0;
	if (scale == 0)
		{
		const char *env = getenv("SCR_SIDE_OFFSET");
		scale = ((env != NULL) && (atol(env) > 0)) ? atol(env) : 4;
		}

	short sin_y, cos_y;
	GetSinCos(player_y_angle, &sin_y, &cos_y);
	player_x += (side * scale * static_cast<long>(cos_y));
	player_z -= (side * scale * static_cast<long>(sin_y));
}


/*	======================================================================================= */
/*	Function:		CalculateDisplaySpeed													*/
/*																							*/
/*	Description:	Calculate speed value for display, using player_z_speed					*/
/*	======================================================================================= */

long CalculateDisplaySpeed (void)
	{
	long speed;

	speed = player_z_speed;
	if (speed < 0x1100) speed = 0; // first few values are not displayed

	speed = ((speed * 183) >> 15);

	return(speed*200)>>7;	// on screen full 240 speed is 128 lenght, but first 40 are not displayed!
	}

/*	======================================================================================= */
/*	Function:		UpdateEngineRevs														*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static long engineRevs = 0;
static long engineRevsChange = 0;
static long engineFluctuation = 0;

// Tested against Amiga
static void UpdateEngineRevs (void)
{
int c;

#ifdef	TEST_AMIGA_UER
	long temp;
	if (GetRecordedAmigaWord(&temp))
	{
		++VALUE2;
		touching_road = temp ? TRUE : FALSE;
	}

	if (GetRecordedAmigaWord(&temp))	//players.input
	{
		accelerate = temp & 0x01 ? TRUE : FALSE;
		brake = temp & 0x02 ? TRUE : FALSE;

	GetRecordedAmigaWord(&player_z_speed);
	GetRecordedAmigaWord(&engineRevs);
	}
#endif

	if (!touching_road)
	{
		// Not touching road, so test if joystick is held forwards or backwards
		c = 0;
		if (accelerate || brake)
			c = 0x9000;
	}
	else
	{
		// Touching road
		c = player_z_speed & (~0xf);	// zero low four bits
		if (c < 0) c = -c;
	}

	c += 0x580;
	c = c >> 3;
	if (engineRevs < 192)
	{
		// If engine revs. are low then increase them slowly (e.g. at race start)
		c = 2;
	}
	else
	{
		// Otherwise calculate revs. change depending on current engine revs.
		c -= engineRevs;
		c = c >> 3;
	}

	engineRevsChange = c;


	/*
	 * Now adjust revs. change
	 */
	if (engineRevsChange >= 0x100)
	{
		// If revs. change is $100 or greater then set to $100
		engineRevsChange = 0x100;
	}
	else if (engineRevsChange < 0)
	{
		if (touching_road)
		{
			// Touching road, so set revs. change to $ff00 minimum
			if (engineRevsChange < -0x100)
				engineRevsChange = -0x100;
		}
		else
		{
			// Not touching road, so set revs. change to $ffe0 minimum
			if (engineRevsChange < -0x20)
				engineRevsChange = -0x20;
		}
	}

	engineFluctuation = rand() & 0xf;

#ifdef	TEST_AMIGA_UER
	CompareRecordedAmigaWord("engine.revs.change", &engineRevsChange);
#endif
}


/*	======================================================================================= */
/*	Function:		FramesWheelsEngine														*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

extern long engineSoundPlaying;

int enginePeriod = 198;
int engineSoundIndex = -1;

//#define	JUST_USE_ONE_SOUND

// Ideas to try:
// All sounds playing but mute the ones that aren't required - WORSE
// Start the new sound playing at the same percentage through as the previous sound - SLIGHTLY BETTER

void FramesWheelsEngine (IDirectSoundBuffer8 *engineSoundBuffers[])
{
/* SECTION BELOW HASN'T BEEN CONVERTED
	clr.w	d1
	clr.w	d2
	tst.b	frame.count
	beq	fwe1
	subq.b	#1,frame.count

fwe1	tst.b	fade.frame.count
	beq	fwe2
	subq.b	#1,fade.frame.count

fwe2	tst.b	B.5d724
	bpl	fwe3

	tst.b	no.wheel.update
	bne	fwe3
	jsr	update.wheel.rotation

fwe3	move.w	sprite.DMA.value,dmacon+custom
*/
// wheel update

leftwheel_angle = (leftwheel_angle+front_left_wheel_speed)&WHEEL_ANGLE_MASK;
rightwheel_angle = (leftwheel_angle+front_right_wheel_speed)&WHEEL_ANGLE_MASK;

int period, index;
DWORD freq;
int r = engineRevs + engineRevsChange;
static int lastEngineSoundIndex = -1;
DWORD currentPlayCursor;

	if (r < 0)
	{
		//if (turnEngineOff)
		//	{
		// stop engine sound
		// return;
		//	}

		r = 0;
	}

	engineRevs = r;

	r += 378;
	period = 4800000 / r;

	index = 6;

#ifndef JUST_USE_ONE_SOUND
	if (period >= 0x3fff) period = 0x3ffe;

	period = period | engineFluctuation;
	if (period < 124) period = 124;	// lowest possible period

	// Calculate sound index that will give period < 256
	while (period >= 256)
	{
		period >>= 1;
		--index;

		if (index < 0) index = 0;
	}
	freq = AMIGA_PAL_HZ / period;	// Rearranging formula: period = clock constant (AMIGA_PAL_HZ) / frequency (samples per second)

#else

	// Calculate r for sample after tick over sound (i.e. index 1)
	// (Tick over sound cannot be played at high enough frequency)
	// (Even index 1 requires frequencies > 100000Hz sometimes, so may not work on all systems)
	while (index > 1)
	{
		period >>= 1;
		--index;

		r <<= 1;
	}
	freq = ((AMIGA_PAL_HZ/1000) * r) / (4800000/1000);
#endif

	// temp store new engine sound index and period
	enginePeriod = period;
	engineSoundIndex = index;

//	fprintf(out, "period %d, freq %d\n", period, freq);
	if (!engineSoundPlaying)
	{
		// Reset last index so that logic below will restart the engine (e.g. after game was paused)
		lastEngineSoundIndex = -1;
	}

	if (engineSoundIndex != lastEngineSoundIndex)
	{
		// Stop the old engine sound
		if (lastEngineSoundIndex >= 0)
		{
			engineSoundBuffers[lastEngineSoundIndex]->GetCurrentPosition(&currentPlayCursor, NULL);
			engineSoundBuffers[lastEngineSoundIndex]->Stop();
		}
		else
			currentPlayCursor = 0;

		// Start the new engine sound

		// Attempt to start at same position through as previous sound
		if (engineSoundIndex > lastEngineSoundIndex)
			currentPlayCursor = currentPlayCursor / 2;
		else
			currentPlayCursor = currentPlayCursor * 2;

		engineSoundBuffers[engineSoundIndex]->SetCurrentPosition(currentPlayCursor);
		engineSoundBuffers[engineSoundIndex]->Play(NULL,NULL,DSBPLAY_LOOPING);

	lastEngineSoundIndex = engineSoundIndex;
	engineSoundPlaying = TRUE;
	}

	// Set the frequency of the current engine sound
	engineSoundBuffers[engineSoundIndex]->SetFrequency(freq);
}

#ifdef TESTENGINE
extern IDirectSoundBuffer8 *EngineSoundBuffers[];

void EngineSoundStopped (void)
{
	/*
DWORD freq = 3546895 / enginePeriod;

	fprintf(out, "Engine sound stopped\n");

	// Start the new engine sound
	EngineSoundBuffers[engineSoundIndex]->SetCurrentPosition(0);
	EngineSoundBuffers[engineSoundIndex]->Play(NULL,NULL,NULL);

	// Set the frequency of the current engine sound
	EngineSoundBuffers[engineSoundIndex]->SetFrequency(freq);

	engineSoundPlaying = TRUE;
	*/

int period, index;
DWORD freq;
int r;

//	touching_road = TRUE;
//	player_z_speed += 0x100;
//	UpdateEngineRevs();

	engineRevsChange = 2;

	r = engineRevs + engineRevsChange;

	engineRevs = r;

	r += 378;
	period = 4800000 / r;

	index = 6;

	if (period >= 0x3fff) period = 0x3ffe;

//	period = period | engineFluctuation;
	if (period < 124) period = 124;	// lowest possible period

	// Calculate sound index that will give period < 256
	while (period >= 256)
	{
		period >>= 1;
		--index;

		if (index < 0) index = 0;
	}
	freq = AMIGA_PAL_HZ / period;

	enginePeriod = period;
	engineSoundIndex = index;

//	if (index > 0)
//		return;

	// Start the new engine sound
	EngineSoundBuffers[engineSoundIndex]->SetCurrentPosition(0);
	EngineSoundBuffers[engineSoundIndex]->Play(NULL,NULL,NULL);

	// Set the frequency of the current engine sound
	EngineSoundBuffers[engineSoundIndex]->SetFrequency(freq);
}
#endif


/*	======================================================================================= */
/*	Function:		CalculatePlayersRoadPosition											*/
/*																							*/
/*	Description:	Calculate player position values required for Opponent Behaviour		*/
/*	======================================================================================= */

void CalculatePlayersRoadPosition (void)
{
long height;	// Not used

	// Calculate the position of the car's centre
	CalculateWorldRoadHeight(CENTRE, player_x, player_z, &height);
}


/*	======================================================================================= */
/*	Function:		DrawSceneParticles														*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

/*	The sparks and dust are plotted into the Amiga's scene bitmap, so the cockpit is drawn
	over the top of them.  Hence this runs before DrawCockpit, rather than at the end of
	the frame with the rest of the overlays.									*/
void DrawSceneParticles( void )
{
#ifdef SCR_PORTABLE
	// DrawFilledRectangle plots straight into GL in screen space, so it needs the flat,
	// untextured, unculled state the 2D overlays run in.  Drawing after DrawCockpit used
	// to leave that set up for us; here we are still in the middle of the world pass, with
	// the track's texturing and back-face culling live, and the rectangles vanish.
	const GLboolean had_texture = glIsEnabled(GL_TEXTURE_2D);
	const GLboolean had_cull    = glIsEnabled(GL_CULL_FACE);
	const GLboolean had_blend   = glIsEnabled(GL_BLEND);
	const GLboolean had_depth   = glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
#endif

	// Draw other graphics that are done as part of 'draw.world'
	bool emit_dust = false;
	if ((!ON_CHAINS) && (off_map_status != 0))
		emit_dust = DrawDustClouds();

	const bool emit_sparks = DrawSparks();

	// One update for the whole table, whatever the two above decided: particles already
	// in flight finish their arc even once nothing is throwing new ones.
	UpdateSparks(emit_sparks, emit_dust);

	// Consume the world tick (the dust puffs change shape on it)
	if (bWorldStepDue)
	{
		++spark_step_count;
		bWorldStepDue = FALSE;
	}

#ifdef SCR_PORTABLE
	if (had_texture) glEnable(GL_TEXTURE_2D);
	if (had_cull)    glEnable(GL_CULL_FACE);
	if (had_blend)   glEnable(GL_BLEND);
	if (had_depth)   glEnable(GL_DEPTH_TEST);
#endif
}

/*	======================================================================================= */
/*	Sparks and dust clouds																	*/
/*																							*/
/*	Description:	Port of the Amiga particle effect ("Reference only/StuntCarRacer.s",		*/
/*					draw.sparks :14283, draw.dust.clouds :14248 and the shared				*/
/*					sparks.or.clouds :14340).  Both effects drive one table: yellow sparks	*/
/*					when a wheel is scraping the road edge, grey-brown dust puffs when the	*/
/*					car is off the map.  draw.sparks bails out when off.map.status says so,	*/
/*					so only ever one of the two is live.									*/
/*																							*/
/*					The particles live in the Amiga's 256x128 playfield, plotted straight	*/
/*					into the scene bitmap, so they map onto our cockpit window rather than	*/
/*					the whole screen (see SCR_WINDOW_* in 3D_Engine.h).						*/
/*	======================================================================================= */

// draw.sparks uses d1 = 31, draw.dust.clouds d1 = 15 (its "machine" branch is dead code -
// machine is only ever written as 0, StuntCarRacer.s:4113 and :9943).
#define	MAX_SPARKS		32
#define	NUM_DUST_CLOUDS	16

// TAB.1c380 / TAB.1c3c0 (position) and TAB.1c400 / TAB.1c440 (per-step delta).
//
// The Amiga integrated these as integers once per race.loop pass, because that was also its
// draw rate.  We draw at the display's refresh instead, so they are kept in floating point
// and integrated against a fraction of a world step: same parabola, same flight time, just
// sampled finely enough to read as a trajectory rather than as eight jumps.  Velocities are
// still in the original per-world-step units.
static double spark_x[MAX_SPARKS], spark_y[MAX_SPARKS];
static double spark_dx[MAX_SPARKS], spark_dy[MAX_SPARKS];

// Which of the two effects each slot was thrown as.  The Amiga could switch the whole table
// from sparks to clouds between one frame and the next, because it wiped it on the way in
// and out of the two states; we let particles outlive the thing that threw them, so a slot
// has to remember what it is.
static bool spark_is_dust[MAX_SPARKS];

// ferocity.of.sparks.or.clouds - the clamped speed, which sets how fast particles are thrown.
static long spark_ferocity = 0;

// A y of 128 or more means "not in flight"; initialise.sparks.table parks them all at 212.
#define	SPARK_DEAD_Y	212

static void InitialiseSparksTable (void)
{
	for (long i = 0; i < MAX_SPARKS; i++)
		spark_y[i] = SPARK_DEAD_Y;
}

/*	Convert a rectangle of Amiga playfield pixels to device coordinates and fill it.
	The cockpit window subtends exactly the playfield's field of view, so playfield
	(0,0)..(256,128) covers the window opening.									*/
static void FillPlayfieldRect (long px, long py, long w, long h, long colour_index)
{
long screen_width, screen_height;

	// The Amiga blitted into a 256x128 bitmap, so anything hanging over an edge was simply
	// cut off.  Clip the same way, or a puff thrown near the edge spills over the cockpit.
	if (px < 0) { w += px; px = 0; }
	if (py < 0) { h += py; py = 0; }
	if (px + w > AMIGA_PLAYFIELD_WIDTH)  w = AMIGA_PLAYFIELD_WIDTH  - px;
	if (py + h > AMIGA_PLAYFIELD_HEIGHT) h = AMIGA_PLAYFIELD_HEIGHT - py;
	if ((w <= 0) || (h <= 0)) return;

	GetScreenDimensions(&screen_width, &screen_height);

	const float base_width  = wideScreen ? static_cast<float>(BASE_WIDTH_WIDESCREEN)
										 : static_cast<float>(BASE_WIDTH_STANDARD);
	const float scaleX = static_cast<float>(screen_width) / base_width;
	const float scaleY = static_cast<float>(screen_height) / static_cast<float>(BASE_HEIGHT);

	// The whole cockpit panel shifts right in widescreen (Car.cpp DrawCockpit)
	const float left = SCR_WINDOW_LEFT + (wideScreen ? COCKPIT_WIDESCREEN_OFFSET * 2.0f : 0.0f);

	const float sx = SCR_WINDOW_WIDTH  / static_cast<float>(AMIGA_PLAYFIELD_WIDTH);
	const float sy = SCR_WINDOW_HEIGHT / static_cast<float>(AMIGA_PLAYFIELD_HEIGHT);

	const float x1 = (left + px * sx) * scaleX;
	const float x2 = (left + (px + w) * sx) * scaleX;
	const float y1 = (SCR_WINDOW_TOP + py * sy) * scaleY;
	const float y2 = (SCR_WINDOW_TOP + (py + h) * sy) * scaleY;

	DrawFilledRectangle(static_cast<long>(x1), static_cast<long>(y1),
						static_cast<long>(x2), static_cast<long>(y2),
						SCRGB(SCR_BASE_COLOUR + colour_index));
}

/*	One dust puff (draw.spark.sub :14490 -> draw.spark.sub2 :26964).  The Amiga blitted one
	of eight cloud bitmaps from its graphics bank; we have no copy of that art, so the puff
	is drawn as a blob of the same size (graphic.info entries 29..36 give 48-80 pixels wide
	by 28-38 lines) in the ground's own colours.										*/
static void DrawDustPuff (long index, long x, long y)
{
	// TAB.60fac - which of the eight cloud shapes this particle shows this frame
	static const unsigned char shape_order[16] = {3,6,7,2,1,5,0,4,0,5,1,2,7,6,2,7};
	// TAB.60f9c - the x offset the blit applies per shape
	static const long shape_x_offset[8] = {0x20,0x20,0x20,0x28,0x18,0x20,0x20,0x20};
	// graphic.info, entries 29..36: (words wide - 1, lines high - 1)
	static const long shape_width[8]  = {64,64,64,80,48,64,64,64};
	static const long shape_height[8] = {34,31,38,36,28,34,34,36};

	const long t = shape_order[(index + spark_step_count) & 15];

	// draw.spark.sub adds 32/16, draw.spark.sub2 then takes 2 words / 16 lines back off
	const long left = x - shape_x_offset[t];
	const long top  = y;
	const long w    = shape_width[t];
	const long h    = shape_height[t];

	// The original art is a billowing white cloud - several overlapping lobes, shaded grey
	// underneath, with black flecks of grit thrown up with it.  We have no copy of the
	// bitmaps, so the silhouette is built from five ellipses (in units of the shape's own
	// width and height) and filled row by row.  The lobe layout is jittered per shape so
	// the eight frames do not all read as the same blob.
	struct Lobe { float cx, cy, rx, ry; };
	static const Lobe lobes[8][5] =
		{
		{{0.30f,0.55f,0.30f,0.42f},{0.52f,0.34f,0.26f,0.32f},{0.72f,0.52f,0.28f,0.42f},{0.42f,0.72f,0.26f,0.28f},{0.62f,0.74f,0.24f,0.26f}},
		{{0.26f,0.58f,0.26f,0.40f},{0.46f,0.32f,0.28f,0.30f},{0.70f,0.50f,0.30f,0.44f},{0.36f,0.76f,0.24f,0.24f},{0.60f,0.70f,0.26f,0.30f}},
		{{0.32f,0.48f,0.32f,0.44f},{0.56f,0.30f,0.24f,0.28f},{0.74f,0.56f,0.26f,0.40f},{0.46f,0.74f,0.28f,0.26f},{0.66f,0.76f,0.22f,0.24f}},
		{{0.24f,0.60f,0.24f,0.36f},{0.44f,0.36f,0.26f,0.34f},{0.66f,0.46f,0.28f,0.42f},{0.82f,0.66f,0.18f,0.30f},{0.50f,0.76f,0.30f,0.24f}},
		{{0.34f,0.52f,0.34f,0.44f},{0.58f,0.36f,0.28f,0.34f},{0.70f,0.62f,0.28f,0.36f},{0.44f,0.76f,0.26f,0.24f},{0.28f,0.34f,0.22f,0.26f}},
		{{0.28f,0.54f,0.28f,0.42f},{0.50f,0.30f,0.30f,0.30f},{0.74f,0.54f,0.26f,0.40f},{0.40f,0.74f,0.28f,0.26f},{0.64f,0.76f,0.24f,0.24f}},
		{{0.30f,0.44f,0.28f,0.40f},{0.54f,0.60f,0.30f,0.38f},{0.74f,0.40f,0.24f,0.36f},{0.42f,0.78f,0.24f,0.22f},{0.68f,0.72f,0.26f,0.26f}},
		{{0.26f,0.50f,0.26f,0.44f},{0.48f,0.36f,0.28f,0.32f},{0.68f,0.56f,0.30f,0.40f},{0.36f,0.74f,0.26f,0.26f},{0.58f,0.74f,0.28f,0.26f}},
		};

	// Flecks of grit, in the same normalised space (draw.spark.sub2's clouds are speckled)
	static const float fleck[8][2] =
		{
		{0.34f,0.36f},{0.58f,0.30f},{0.46f,0.52f},{0.70f,0.46f},
		{0.30f,0.62f},{0.62f,0.66f},{0.50f,0.78f},{0.78f,0.60f},
		};

	// Two playfield lines at a time: at this size that is still smooth, and it keeps the
	// fill count down when the whole table of sixteen puffs is up.
	for (long row = 0; row < h; row += 2)
		{
		const float v = (row + 1.0f) / static_cast<float>(h);

		float lo = 1.0f, hi = 0.0f;
		for (long l = 0; l < 5; l++)
			{
			const Lobe &b = lobes[t][l];
			const float dy = (v - b.cy) / b.ry;
			if ((dy <= -1.0f) || (dy >= 1.0f)) continue;

			const float half = b.rx * sqrtf(1.0f - dy*dy);
			if (b.cx - half < lo) lo = b.cx - half;
			if (b.cx + half > hi) hi = b.cx + half;
			}
		if (hi <= lo) continue;

		const long rx1 = left + static_cast<long>(lo * w);
		const long rx2 = left + static_cast<long>(hi * w);
		const long rh  = (row + 2 <= h) ? 2 : (h - row);

		// White body, grey along the shaded underside
		FillPlayfieldRect(rx1, top + row, rx2 - rx1, rh, (v > 0.72f) ? 14 : 15);
		}

	for (long f = 0; f < 8; f++)
		{
		if (((index + spark_step_count + f) & 3) != 0) continue;	// only some show

		const long fx = left + static_cast<long>(fleck[f][0] * w);
		const long fy = top  + static_cast<long>(fleck[f][1] * h);
		FillPlayfieldRect(fx, fy, 2, 1, 0);
		}
}

/*	draw.spark :14417.  Returns true if the particle was in flight (and drawn), false if it
	has expired - in which case it is marked dead ready for the respawn pass.			*/
static bool DrawSpark (long index, bool dust)
{
const long x = static_cast<long>(spark_x[index]);
const long y = static_cast<long>(spark_y[index]);

	// The Amiga compares unsigned, so anything that has run off an edge counts as expired
	if ((y < 1) || (y >= AMIGA_PLAYFIELD_HEIGHT) || (x < 0) || (x >= AMIGA_PLAYFIELD_WIDTH))
		{
		spark_y[index] = SPARK_DEAD_Y;
		return false;
		}

	if (dust)
		{
		DrawDustPuff(index, x, y);
		return true;
		}

	if (x >= 254)	// the spark is two pixels wide
		{
		spark_y[index] = SPARK_DEAD_Y;
		return false;
		}

	// Two by two pixels: colour 3 (yellow) except for a colour 15 (white) top right
	FillPlayfieldRect(x, y-1, 2, 2, 3);
	FillPlayfieldRect(x+1, y-1, 1, 1, 15);

	return true;
}

/*	sparks.or.clouds :14340 - advance and draw the whole table, and throw new particles from
	the slots that have expired.

	The Amiga only ever ran this while the effect was active, and called
	initialise.sparks.table (parking every slot as dead) the moment it stopped - so the
	shower vanished the instant a wheel left the edge.  It could afford that: at 8.3Hz a
	spark only existed for a handful of frames anyway.  Here the arc is drawn properly, so
	cutting it off mid-flight is very visible.  Emission stops with the scrape; whatever is
	already in the air finishes its trajectory and falls off the bottom of the view.	*/
static void UpdateSparks (bool emit_sparks, bool emit_dust)
{
long i;

	const bool emit  = emit_sparks || emit_dust;
	const bool dust  = emit_dust;
	const long count = dust ? NUM_DUST_CLOUDS : MAX_SPARKS;

	// How much of a world step has passed since we last drew.  Clamped so that a stall,
	// or coming back from the menu, does not teleport every particle off the playfield.
	static double last_time = 0.0;
	const double now = DXUTGetTime();
	double dt = (last_time > 0.0) ? ((now - last_time) / gWorldStepSeconds) : 0.0;
	last_time = now;
	if (dt < 0.0) dt = 0.0;
	if (dt > 1.0) dt = 1.0;

	// Draw at the current position, then step: gravity pulls each particle back down.
	// Always the whole table, not just the first `count` slots - a dust cloud only uses
	// half of them, and sparks thrown before the car went off the map are still flying.
	for (i = MAX_SPARKS - 1; i >= 0; --i)
		{
		if (!DrawSpark(i, spark_is_dust[i]))
			continue;

		spark_dy[i] += 2.0 * dt;
		spark_y[i] += spark_dy[i] * dt;
		spark_x[i] += spark_dx[i] * dt;
		}

	if (!emit)
		return;

	// Throw a new particle from every slot that has expired
	for (i = count - 1; i >= 0; --i)
		{
		if ((spark_y[i] >= 0.0) && (spark_y[i] < AMIGA_PLAYFIELD_HEIGHT))
			continue;	// still in flight

		long v = spark_ferocity >> 1;
		if (!dust) v >>= 1;		// sparks are thrown half as hard as dust is
		v += (rand() & 7);
		spark_dy[i] = ~v;		// upwards, i.e. -(v+1)

		long start_x;
		if (dust)
			{
			// draw.spark2 :14477 - anywhere across the playfield, from just off the bottom
			start_x = rand() & 0xff;
			spark_x[i] = start_x;
			spark_y[i] = (rand() & 7) + 118;
			}
		else
			{
			// ddc10 - from the middle half of the playfield, where the wheels are
			start_x = (rand() & 0x7f) + 64;
			spark_x[i] = start_x;
			spark_y[i] = 119 + (rand() & 7);
			}

		// Fan out from the centre: the further from the middle, the more sideways speed
		spark_dx[i] = (start_x - 128) >> 3;
		spark_is_dust[i] = dust;

		DrawSpark(i, dust);
		}
}

/*	======================================================================================= */
/*	Function:		DrawDustClouds															*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

// Returns true if the car should be throwing up dust this frame.
static bool DrawDustClouds (void)
{
	long p = abs(player_z_speed) >> 8;
	if (p > 16) p = 16;			// set to maximum
	spark_ferocity = p;

	p = rand();
	p &= 0x1c;
	p += 450;

	OffRoadSoundBuffer->SetFrequency(AMIGA_PAL_HZ / p);

	if (!touching_road)
		return false;			// airborne, so nothing to kick up

//	OffRoadSoundBuffer->Stop();
//	OffRoadSoundBuffer->SetCurrentPosition(0);
	OffRoadSoundBuffer->Play(NULL,NULL,NULL);	// not looping

	return true;
}

/*	======================================================================================= */
/*	Function:		DrawSparks																*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

// Returns true if a wheel is scraping hard enough to be throwing sparks this frame.
static bool DrawSparks (void)
{
int p;

	//VALUE1 = distance_off_road;
	//VALUE2 = which_side_byte;
	if (which_side_byte) goto on_an_edge;

	if (NOT_WRECKED) return false;	// if car is not scraping on road

on_an_edge:
	if (off_map_status != 0) return false;	// dust clouds will be drawn instead

	p = abs(player_z_speed) >> 8;
	if (p < 1) return false;		// if speed is not large enough

	if (p > 50) p = 50;		// set to maximum
	spark_ferocity = p;

	p >>= 1;
	if (p > 31) p = 31;

	p ^= 0x31;
	p &= 0xff;
	p <<= 2;
	p += 170;

	WreckSoundBuffer->SetFrequency(AMIGA_PAL_HZ / p);

	if (!touching_road)
		return false;			// airborne, so nothing is scraping

//	WreckSoundBuffer->SetCurrentPosition(0);
	WreckSoundBuffer->Play(NULL,NULL,NULL);	// not looping

	return true;
}

/*	======================================================================================= */
/*	Function:		UpdateDamage															*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

void UpdateDamage (void)
{
	if (damaged)
	{
		long d = (front_left_damage + front_right_damage) / 2;	// average front damage
		new_damage = (d + rear_damage) / 2;					// total average damage
		// value new_damage must be used to draw damage line
	}

	if (smashed_countdown)
	{
		--smashed_countdown;
		if (smashed_countdown == 69)
		{
			// change smash to hole, by copying 'damage hole' graphic to damage.hole.position
			nholes++;
			goto PlayCreakSound;
		}

		if (damaged) goto PlayCreakSound;

		return;
	}

	if (!damaged) return;

	if (damage_value < 0x1400) goto PlayCreakSound;

	// if (damage.hole.position == 0) goto PlayCreakSound;
	//--damage.hole.position
	// copy 'damage hole smashed' graphic to damage.hole.position
	nholes++;

	smashed_countdown = 69;

	// Play smash sound effect
	//SmashSoundBuffer->SetCurrentPosition(0);
	SmashSoundBuffer->Play(NULL,NULL,NULL);	// not looping
	return;

PlayCreakSound:
	long amiga_volume = (damage_value >> 8) * 4;
	// minimum volume = 28, maximum volume = 64
	if (amiga_volume < 28) amiga_volume = 28;
	if (amiga_volume > 64) amiga_volume = 64;

	CreakSoundBuffer->SetVolume(AmigaVolumeToDirectX(amiga_volume));
	//CreakSoundBuffer->SetCurrentPosition(0);
	CreakSoundBuffer->Play(NULL,NULL,NULL);	// not looping
	return;
}

/*	======================================================================================= */
/*	Function:		UpdateLapData															*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

#define	LAP_THAT_FINISHES_RACE (4)

extern long opponents_current_piece;	// use as opponents_road_section

bool raceFinished, raceWon;
long lapNumber[NUM_CARS];
static bool carOnFirstHalfOfLap[NUM_CARS] = {false, false};

// Lap stopwatch.  The Amiga keeps three BCD bytes per slot - minutes, seconds and
// hundredths (add.to.lap.time in "Reference only/StuntCarRacer.s") - and ticks them on
// by a fixed 19 hundredths per race.loop iteration, which only reads as real time if
// that loop runs at about 5.3Hz.  We run the world at a different (and variable) rate,
// so the clock accumulates wall-clock seconds instead: the digits then mean what they
// say, and lap times are comparable with a real stopwatch.
//
// The minute digit clamps at 9 exactly as the original does (cmpi.b #10 / bge).
#define LAP_TIME_MAX_SECONDS	(10.0 * 60.0 - 0.01)

// After crossing the line the readout freezes on the completed lap for a moment before
// reverting to the running clock; the Amiga holds it for 27 race.loop iterations
// (B.1bbcc, set in start.of.new.lap).
#define LAP_TIME_HOLD_SECONDS	(3.2)

double currentLapTime = 0.0;
double lastLapTime = 0.0;
double bestLapTime = 0.0;
bool   bBestLapTimeSet = false;
double lapTimeHoldRemaining = 0.0;

// The opponent's clock.  The Amiga never displays this, but the league awards a point for
// the fastest lap of a race ("Winner 2pts     Best Lap 1pt"), so the two have to be
// comparable.  Timed exactly as the player's is, just without the readout or the hold.
double oppCurrentLapTime = 0.0;
double oppBestLapTime = 0.0;
bool   bOppBestLapTimeSet = false;

void ResetLapData (long car)
{
	raceFinished = raceWon = FALSE;
	lapNumber[car] = 0;
	carOnFirstHalfOfLap[car] = false;

	currentLapTime = lastLapTime = bestLapTime = 0.0;
	bBestLapTimeSet = false;
	lapTimeHoldRemaining = 0.0;

	oppCurrentLapTime = oppBestLapTime = 0.0;
	bOppBestLapTimeSet = false;
}

void UpdateLapData (double elapsedSeconds)
{
	long car, current_piece, start_finish_piece = (StartLinePiece + 1 < NumTrackPieces) ? (StartLinePiece + 1) : 0;

	// add.to.lap.time: the clock runs whether or not the player has started a lap yet,
	// and is cleared on each crossing of the line (clear.three.bytes).
	if (!raceFinished)
	{
		currentLapTime += elapsedSeconds;
		if (currentLapTime > LAP_TIME_MAX_SECONDS)
			currentLapTime = LAP_TIME_MAX_SECONDS;
	}

	if (lapTimeHoldRemaining > 0.0)
		lapTimeHoldRemaining -= elapsedSeconds;

	if (!raceFinished)
	{
		oppCurrentLapTime += elapsedSeconds;
		if (oppCurrentLapTime > LAP_TIME_MAX_SECONDS)
			oppCurrentLapTime = LAP_TIME_MAX_SECONDS;
	}

	for (car = OPPONENT; car < NUM_CARS; car++)
	{
		current_piece = car == PLAYER ? player_current_piece : opponents_current_piece;

		if (carOnFirstHalfOfLap[car])
		{
			if (current_piece == HalfALapPiece)
				carOnFirstHalfOfLap[car] = false;
		}
		else if (current_piece == start_finish_piece)
		{
			carOnFirstHalfOfLap[car] = true;
			++lapNumber[car];

			// start.of.new.lap: the first crossing only starts the clock (the original
			// skips show/copy when players.lap == 1); later ones complete a lap.
			if (car == PLAYER)
			{
				if (lapNumber[PLAYER] > 1)
				{
					lastLapTime = currentLapTime;

					// new.lap.sub3: keep it only if it beats the stored best
					if (!bBestLapTimeSet || (lastLapTime < bestLapTime))
					{
						bestLapTime = lastLapTime;
						bBestLapTimeSet = true;
					}

					lapTimeHoldRemaining = LAP_TIME_HOLD_SECONDS;
				}

				currentLapTime = 0.0;
			}
			else
			{
				if (lapNumber[OPPONENT] > 1)
				{
					if (!bOppBestLapTimeSet || (oppCurrentLapTime < oppBestLapTime))
					{
						oppBestLapTime = oppCurrentLapTime;
						bOppBestLapTimeSet = true;
					}
				}

				oppCurrentLapTime = 0.0;
			}
		}
	}

//	VALUE2 = lapNumber[OPPONENT];
//	VALUE3 = carOnFirstHalfOfLap[PLAYER] ? 1 : 0;

	for (car = OPPONENT; car < NUM_CARS; car++)
	{
		if (!raceFinished)
		{
			if (lapNumber[car] == LAP_THAT_FINISHES_RACE)
			{
				raceFinished = true;

				// frames to show message for = 44; about 5.64 seconds

				if (CalculateIfWinning(start_finish_piece) < 0)
					raceWon = true;
				else
					raceWon = false;
			}
		}
	}
}

#ifdef NOT_USED
/*	======================================================================================= */
/*	Function:		RewindRecording,														*/
/*					Record,																	*/
/*					PlayBack,																*/
/*					BeginActionReplay														*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

#define	RECORDING_SIZE	(4096)

typedef struct
	{
	BYTE input;		// this is sufficient to hold required input
	} RECORDING;

static size_t RecordingIndex, EndOfRecording;			// indexes into following buffer
static RECORDING RecordingBuffer[RECORDING_SIZE];


static void RewindRecording (void)
	{
	RecordingIndex = 0;
	}


static void Record (DWORD input)
	{
	if (RecordingIndex < RECORDING_SIZE)
		{
		RecordingBuffer[RecordingIndex].input = (BYTE)input;
		++RecordingIndex;
		}

	//VALUE2 = RecordingIndex;
	}


static void PlayBack (DWORD *input)
	{
	if (RecordingIndex < EndOfRecording)
		{
		*input = static_cast<DWORD>(RecordingBuffer[RecordingIndex].input);
		++RecordingIndex;
		}
	else
		{
		*input = 0;
		RewindRecording();
		Replay = FALSE;
		ReplayFinished = TRUE;
		}

	//VALUE2 = RecordingIndex;
	}


static void WriteRecordingToFile (void)
	{
	char filename[80];
	FILE *f;
	size_t i, size;

	sprintf(filename, "Track%dRecording.bin", TrackID);

	if ((f = fopen(filename, "wb")) == NULL )		// write, binary
		{
		fprintf(out, "Can't open %s\n", filename);
		return;
		}

	size = EndOfRecording;
	if ((i = fwrite(RecordingBuffer, sizeof(RECORDING), size, f)) != size)
		{
		fprintf(out, "Can't write %s correctly (%d)\n", filename, i);
		fclose(f);
		return;
		}

	fclose(f);
	return;
	}


static void ReadRecordingFromFile (void)
	{
	char filename[80];
	errno_t err;
	FILE *in_file;
	size_t i;

	memset(RecordingBuffer, 0, sizeof(RecordingBuffer));

	sprintf_s(filename, sizeof(filename), "Track%dRecording.bin", TrackID);

	if ((err = fopen_s(&in_file, filename, "rb")) != 0)		// read, binary
		{
		fprintf(out, "Can't open %s\n", filename);
		return;
		}

	i = fread(RecordingBuffer, sizeof(char), RECORDING_SIZE, in_file);
	if (i == 0)
		{
		fprintf(out, "Can't read %s correctly (%d)\n", filename, i);
		fclose(in_file);
		return;
		}
	EndOfRecording = i;

	fclose(in_file);
	return;
	}


// request replay of last game
void RequestGameReplay (void)
	{
	EndOfRecording = RecordingIndex;
//	note - think this function has a bug WriteRecordingToFile();
	RewindRecording();
	ReplayRequested = TRUE;

	ReplayLooping = FALSE;
	}


// request replay of pre-recorded game
void RequestStoredReplay (void)
	{
	ReadRecordingFromFile();
	RewindRecording();
	ReplayRequested = TRUE;

	ReplayLooping = TRUE;
	}
#endif

#ifdef USE_AMIGA_RECORDING
/*	======================================================================================= */
/*	Function:		GetRecordedAmigaWord,													*/
/*					GetRecordedAmigaLong,													*/
/*																							*/
/*	Description:				*/
/*	======================================================================================= */

static FILE *AmigaFile = NULL;


static bool OpenAmigaRecording( void )
{
errno_t err;

	if (AmigaFile) return(TRUE);	// Already open

	if ((err = fopen_s(&AmigaFile, "SCRecording.bin", "rb")) != 0)		// read, binary
		{
		fprintf(out, "Can't open SCRecording.bin\n");
		return(FALSE);
		}

	StartOfAmigaRecording = TRUE;
	AmigaRecordingFrame = 0;
	return(TRUE);
}

bool GetRecordedAmigaWord( long *value_out )
{
char b[2];
short s;
size_t i;

	if (!ReplayAmigaRecording) return(FALSE);

	if (!OpenAmigaRecording()) return(FALSE);

	i = fread(b, sizeof(char), 2, AmigaFile);
    if (i == 0)
		{
		int e = ferror(AmigaFile);
		if (e) fprintf(out, "Can't read Amiga word correctly (%d)\n", e);

		return(FALSE);
		}

	s = ((b[0] & 0xff) << 8) | (b[1] & 0xff);

	*value_out = static_cast<long>(s);
	return(TRUE);
}

bool GetRecordedAmigaLong( long *value_out )
{
char b[4];
long l;
size_t i;

	if (!ReplayAmigaRecording) return(FALSE);

	if (!OpenAmigaRecording()) return(FALSE);

	i = fread(b, sizeof(char), 4, AmigaFile);
    if (i == 0)
		{
		int e = ferror(AmigaFile);
		if (e) fprintf(out, "Can't read Amiga long correctly (%d)\n", e);

		return(FALSE);
		}

	l = ((b[0] & 0xff) << 24) | ((b[1] & 0xff) << 16) | ((b[2] & 0xff) << 8) | (b[3] & 0xff);

	*value_out = l;
	return(TRUE);
}

void CompareAmigaWord( char *name, long amiga_value, long *value )
{
	if (*value != amiga_value)
//	if (abs(*value - amiga_value) > 1)
	{
		++VALUE1;	// Count differences
		fprintf(out, "%s different %d %d (VALUE2 %d)\n", name, amiga_value, *value, VALUE2);
//		*value = amiga_value;	// Use Amiga value when different
	}
}

void CompareRecordedAmigaWord( char *name, long *value )
{
long amiga_value;

	if (!GetRecordedAmigaWord(&amiga_value)) return;

	CompareAmigaWord( name, amiga_value, value );
}

void CloseAmigaRecording( void )
{
	if (AmigaFile)
	{
		fclose(AmigaFile);
		AmigaFile = NULL;
	}
}
#endif
