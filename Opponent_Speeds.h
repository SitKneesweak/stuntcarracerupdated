#ifndef OPPONENT_SPEEDS_H
#define OPPONENT_SPEEDS_H

/*	=========================================================================================	*/
/*	The opponent's required speed for every piece of a track, built once per race exactly as	*/
/*	the Amiga builds it in srd1e..srd116.													*/
/*																							*/
/*	Kept here, free of any game or platform dependency, so tests/opponent_speed_test can run	*/
/*	the same code the game runs and check it against captured Amiga output.					*/
/*	=========================================================================================	*/

#define OPP_SPEED_NUM_TRACKS		8
#define OPP_SPEED_MAX_OVERRIDES		16

/*	The per-piece speed overrides the Amiga reads out of the track data stream as DAT.1c8a8	*/
/*	(piece) and DAT.1c8c8 (speed).  The port's track .bin files were converted to a fixed	*/
/*	layout that stops after SuperBoost and so do not carry them; they are recovered here		*/
/*	from the stuntcarracer.net build's raw track data, which is the original Amiga stream.	*/
/*																							*/
/*	This is the authored data that brakes the opponent into every ramp.  Without it the		*/
/*	opponent arrives at a jump carrying its cruising speed and sails past the landing.		*/
/*																							*/
/*	Bit 7 of a speed means two things: the value ignores opponents_max_speed (masked off		*/
/*	with 0x7f where it is used), and it arms the three-piece run-up in the generator.		*/
/*	The pairs come from the road, not from the league tables, so they are the same in both	*/
/*	Standard and Super league - only the base speed differs.									*/

struct OPP_SPEED_OVERRIDE { unsigned char piece, speed; };

static const OPP_SPEED_OVERRIDE gOppSpeedOverrides[OPP_SPEED_NUM_TRACKS][OPP_SPEED_MAX_OVERRIDES] =
	{
	/* Little Ramp     */ {{33,0x46},{3,0x58}},
	/* Stepping Stones */ {{3,0xd4},{8,0x3f},{15,0xbe},{17,0xbd},{19,0xbb},{21,0xba},{44,0xf3},{30,0x42}},
	/* Hump Back       */ {{0,0x52},{1,0x4d},{27,0x4c},{37,0x4f},{40,0x4d},{52,0x5c}},
	/* Big Ramp        */ {{32,0xd6},{14,0x4e},{15,0x4b},{19,0x4b},{20,0x46}},
	/* Ski Jump        */ {{3,0xd8},{21,0x54},{24,0x36},{32,0xc2},{0,0x42},{39,0xc9}},
	/* Draw Bridge     */ {{3,0x62},{6,0x55},{7,0x50},{20,0x43},{61,0xe4},{65,0xd8},{45,0x5a},{46,0x50},{47,0xc6}},
	/* High Jump       */ {{39,0xd3},{40,0xce},{2,0xd3},{23,0x55},{22,0x52},{21,0x52}},
	/* Roller Coaster  */ {{6,0x2a},{7,0x29},{14,0x36},{26,0x54},{27,0x4a},{77,0x52},{76,0x5a}}
	};

static const long gOppSpeedOverrideCount[OPP_SPEED_NUM_TRACKS] = { 2, 8, 6, 5, 6, 9, 6, 7 };


/*	=========================================================================================	*/
/*	Function:		BuildOpponentSpeedValues												*/
/*																							*/
/*	Description:	Fills speeds_out[0..num_pieces-1] for one track.  Three mechanisms:		*/
/*																							*/
/*					 - the override table wins outright on the pieces it names;				*/
/*					 - "value" is a running accumulator climbing 10 a piece, so the			*/
/*					   opponent winds up along a straight instead of picking an				*/
/*					   unrelated speed for each piece;										*/
/*					 - an override with bit 7 set arms a three-piece countdown that marks	*/
/*					   the pieces before it, which is the run-up to a jump.					*/
/*																							*/
/*					The loop runs backwards, as the Amiga's does, and exactly once.  The		*/
/*					Amiga's copy.prompt.groups starts at 2 and looks like two passes, but	*/
/*					srd1d enters through "bne srd116", which decrements it to 1 before the	*/
/*					first pass runs; the second test at srd116 then falls through to rts.	*/
/*					So the accumulator starts fresh at 0x7c on the last piece of the track.	*/
/*																							*/
/*					piece_angle is the track's Piece_Angle_And_Template, can_be_put_on the	*/
/*					16-byte sections_car_can_be_put_on flag table.							*/
/*	=========================================================================================	*/

/*	overrides/n_ovr are passed in rather than looked up by track ID so that a track built	*/
/*	by tools/trackc.py can supply its own braking points.  A custom track with none simply	*/
/*	passes 0, and the generic path below still winds the opponent up along the straights		*/
/*	and slows it for the curves - it just will not brake for that track's jumps.				*/

static inline void BuildOpponentSpeedValues( const OPP_SPEED_OVERRIDE *overrides,
											 long n_ovr,
											 long num_pieces,
											 const char *piece_angle,
											 const unsigned char *can_be_put_on,
											 long base_speed,
											 unsigned char *speeds_out )
	{
	long countdown = 0;
	long value = 0x7c;						// Amiga: move.b #$7c,d0 / move.b d0,value

	for (long i = num_pieces - 1; i >= 0; i--)
		{
		long d0 = -1;

		// srd1f: is this piece named in the override table?
		for (long j = n_ovr - 1; j >= 0; j--)
			if (overrides[j].piece == i)
				{
				d0 = overrides[j].speed;
				if (d0 & 0x80)
					countdown = 3;
				value = d0 & 0x7f;
				break;
				}

		if (d0 < 0)
			{
			// srd111a: the generic path
			if (can_be_put_on[piece_angle[i] & 0x0f] & 0x80)
				{
				value = (base_speed - 10) & 0xff;
				d0 = base_speed & 0xff;
				}
			else
				{
				long t = (value + 10) & 0xff;
				if ((t & 0x80) == 0)
					value = t;
				d0 = value;
				}

			// srd113a: the three-piece run-up
			if (countdown > 0)
				{
				countdown--;
				d0 |= 0x80;
				}
			}

		speeds_out[i] = static_cast<unsigned char>(d0);
		}
	}

#endif	// OPPONENT_SPEEDS_H
