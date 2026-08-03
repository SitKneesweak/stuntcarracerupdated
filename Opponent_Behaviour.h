
#ifndef	_OPPONENT_BEHAVIOUR
#define	_OPPONENT_BEHAVIOUR

/*	========= */
/*	Constants */
/*	========= */
#define	NO_OPPONENT	(-1)

/*	===================== */
/*	Structure definitions */
/*	===================== */

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

#endif	/* _OPPONENT_BEHAVIOUR */
