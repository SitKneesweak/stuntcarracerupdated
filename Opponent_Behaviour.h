
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

#endif	/* _OPPONENT_BEHAVIOUR */
