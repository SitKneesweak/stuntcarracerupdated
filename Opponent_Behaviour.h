
#ifndef	_OPPONENT_BEHAVIOUR
#define	_OPPONENT_BEHAVIOUR

/*	========= */
/*	Constants */
/*	========= */
#define	NO_OPPONENT	(-1)

/*	===================== */
/*	Structure definitions */
/*	===================== */

/*	Ask for an opponent to be drawn at random - the port's own dev track menu, which has no	*/
/*	league behind it to name one.															*/
#define	RANDOM_OPPONENT	(-2)

/*	========================== */
/*	External data declarations */
/*	========================== */

/*	The driver being raced, 0..10, or NO_OPPONENT for a practise run.  ResetOpponent used	*/
/*	to draw this at random every race; the Amiga never does.  mgs9 (~line 10039 of the		*/
/*	disassembly) picks the league fixture's opponent - whichever of the pair is not the		*/
/*	player - and stores it in opponents.ID, so a league race is always against the driver	*/
/*	the fixture screen just named.															*/
extern long opponentsID;

/*	============================== */
/*	External function declarations */
/*	============================== */

/*	Choose who the next race is against: a driver ID, NO_OPPONENT for a practise run, or	*/
/*	RANDOM_OPPONENT.  Practise is solo on the Amiga - race.mode is only negative for a		*/
/*	league race, and every opponent call in the race loop hangs off it (the no.opponent4		*/
/*	and no.opponent5 branches of draw.world, ~line 20280).									*/
extern void SetRaceOpponent( long driver );

/*	============================== */
/*	External function declarations */
/*	============================== */
extern void OpponentBehaviour (long *x,
							   long *y,
							   long *z,
							   float *x_angle,
							   float *y_angle,
							   float *z_angle,
							   bool bOpponentPaused);

extern void CarToCarCollision( void );

extern long CalculateIfWinning( long start_finish_piece );

extern long CalculateOpponentsDistance (void);

extern void GetOpponentWheelCompression( long *rear_left, long *rear_right, long *front );

extern long GetOpponentZSpeed( void );

/*	=============================================================================== */
/*	Opponent speed tuning - a testing bench, not a player-facing difficulty setting	*/
/*	=============================================================================== */
/*																						*/
/*	Every opponent speed in the game is built from opp_track_speed_values, a table		*/
/*	straight out of the Amiga's DAT.1fe2c: a random mask and a base, per track, once		*/
/*	for the league and again for the Super League.  These let the two bases be nudged	*/
/*	from the menus so a track that drives too fast or too slow can be found by feel		*/
/*	rather than by rebuilding, without touching the table the disassembly authorises.	*/
/*																						*/
/*	The offsets are per track AND per league - Little Ramp is raced in both, and it is	*/
/*	the Super League run of it that is worth tuning.  They are saved, in their own file	*/
/*	beside the profile, because finding a number takes more sittings than one; while		*/
/*	any of them is set the Hall of Fame stops accepting times, so no record can be set	*/
/*	against altered opposition - and that holds across launches too, until R resets		*/
/*	the table and takes the file with it.												*/
#define OPPONENT_TUNING_MIN		(-64)
#define OPPONENT_TUNING_MAX		(64)

extern long OpponentTuningGet( long trackID, bool superLeague );

/*	Setting or clearing writes the file straight away: the bench is used by racing, and	*/
/*	a race is left by whatever route the tester likes, up to closing the window.			*/
extern void OpponentTuningSet( long trackID, bool superLeague, long offset );
extern void OpponentTuningClear( void );

/*	Read the offsets back at start-up.  Silent about a missing or unreadable file - in	*/
/*	either case the speeds are the table's own, which is where a fresh install starts.	*/
extern void OpponentTuningLoad( void );

/*	True if any track in either league has been moved off its original value.			*/
extern bool OpponentTuningActive( void );

/*	The tuned base for one of the two speed groups.  `group` is 8 for the opponent's		*/
/*	maximum speed or 24 for the per-piece target, matching the table's own layout.		*/
extern long OpponentTuningBase( long trackID, long group, bool superLeague );

#endif	/* _OPPONENT_BEHAVIOUR */
