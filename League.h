/*	======================================================================================= */
/*	File:			League.h																*/
/*																							*/
/*	Description:	The original single-player league season.								*/
/*																							*/
/*					Everything here is taken from the 68k disassembly rather than guessed	*/
/*					at; the tables it is built on are:										*/
/*																							*/
/*					opponents.names.source	12 driver name slots, 16 bytes each.  Eleven	*/
/*											CPU drivers and one blank - the blank is the	*/
/*											player, which is why the shipped heads.png		*/
/*											has the player's portrait cell defaced.			*/
/*																							*/
/*					TAB.648c2				$00,$02,$01,$03,$06,$07,$04,$05 - the two		*/
/*											track IDs belonging to each division, bottom	*/
/*											division first.  Cross-checking against the		*/
/*											track name list and the division/track legend	*/
/*											along the bottom of heads.png confirms it:		*/
/*											Div IV = Little Ramp + Hump Back, Div III =		*/
/*											Stepping Stones + Big Ramp, Div II = High Jump	*/
/*											+ Roller Coaster, Div I = Ski Jump + Drawbridge.	*/
/*																							*/
/*					DAT.1c9c2				the 12-entry ladder.  Promotion and relegation	*/
/*											swap adjacent entries across a division			*/
/*											boundary (see the .label6 loop at ~line 19880).	*/
/*																							*/
/*					league.offset			zero normally, 11 once the player has won		*/
/*											Division I - that is the SUPER LEAGUE flag.		*/
/*																							*/
/*					The division shown on screen is "4 minus the division index", so		*/
/*					division index 0 is Division IV at the bottom of the ladder.			*/
/*	======================================================================================= */

#ifndef __LEAGUE_H_
#define __LEAGUE_H_

#define NUM_LEAGUE_DRIVERS		12
#define DRIVERS_PER_DIVISION	3
#define NUM_DIVISIONS			4
#define TRACKS_PER_DIVISION		2

/*	Two opponents in your division, raced twice each - once on each of the division's		*/
/*	two tracks.																				*/
#define RACES_PER_SEASON		4

/*	The player is always driver 11: the blank slot in opponents.names.source.				*/
#define PLAYER_DRIVER			11

/*	Per-driver season record - the columns of the RESULTS TABLE screen, which the			*/
/*	disassembly labels "DRIVER     RACED WIN LAP  PTS".										*/
struct LeagueDriver
	{
	int		raced;			// races completed this season
	int		wins;			// races won            (2 points each)
	int		bestLaps;		// fastest laps set     (1 point each)
	int		points;
	};

/*	One fixture in the player's season.														*/
struct LeagueFixture
	{
	int		opponent;		// driver ID
	int		trackID;		// 0..7, indexes the track tables
	bool	raced;
	bool	won;
	bool	bestLap;
	};

/*	======================================================================================= */
/*	Season state																			*/
/*	======================================================================================= */

/*	The ladder, top first: gLeagueLadder[0..2] are Division I, [9..11] Division IV.			*/
extern int  gLeagueLadder[NUM_LEAGUE_DRIVERS];
extern LeagueDriver gLeagueTable[NUM_LEAGUE_DRIVERS];
extern LeagueFixture gLeagueFixtures[RACES_PER_SEASON];

extern int  gLeagueRace;			// 0..RACES_PER_SEASON-1, the next race to run
extern int  gLeagueSeason;			// seasons completed
extern bool gLeagueSuperLeague;		// league.offset != 0
extern char gPlayerName[16];

/*	--- Queries -------------------------------------------------------------------------	*/

/*	Driver name, as printed.  Trimmed of the leading space the original stores.				*/
const char *LeagueDriverName( int driver );

/*	Ladder position of a driver, and the division index (0 = Division IV, 3 = Division I)	*/
/*	that position falls in.  LeagueDivisionNumber turns an index into the number shown		*/
/*	on screen, which counts the other way.													*/
int  LeagueDriverPosition( int driver );
int  LeagueDivisionOfPosition( int position );
int  LeagueDivisionNumber( int divisionIndex );
int  LeaguePlayerDivision( void );

/*	The two track IDs belonging to a division, straight out of TAB.648c2.					*/
int  LeagueDivisionTrack( int divisionIndex, int which );

/*	--- Season ---------------------------------------------------------------------------	*/

/*	Start a fresh career: reset the ladder to its opening order, clear records.				*/
void LeagueNewCareer( const char *name );

/*	Build this season's four fixtures from the player's current division.					*/
void LeagueStartSeason( void );

/*	The fixture about to be raced, or NULL when the season is over.							*/
const LeagueFixture *LeagueCurrentFixture( void );

/*	Record the outcome of the player's race and simulate the corresponding CPU-vs-CPU		*/
/*	result, then advance to the next fixture.												*/
void LeagueRecordResult( bool playerWon, bool playerBestLap );

/*	True once all four fixtures have been raced.											*/
bool LeagueSeasonComplete( void );

/*	Apply promotion and relegation and roll the ladder into the next season.  Fills the		*/
/*	supplied buffers with the "Promotion for" / "Relegation for" lines to show, or leaves	*/
/*	them empty if nothing changed.															*/
void LeagueEndSeason( char *promoted, char *relegated, int bufferSize );

#endif //__LEAGUE_H_
