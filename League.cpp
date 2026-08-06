/*	======================================================================================= */
/*	File:			League.cpp																*/
/*																							*/
/*	Description:	Implementation of the single-player league season - see League.h for	*/
/*					where each table comes from in the 68k disassembly.						*/
/*	======================================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "League.h"

/*	======================================================================================= */
/*	Static data																				*/
/*	======================================================================================= */

/*	opponents.names.source, with the original's leading space and padding stripped.  Slot	*/
/*	11 is the blank the player's own name goes into.										*/
static const char *kDriverNames[NUM_LEAGUE_DRIVERS] =
	{
	"Hot Rod",
	"Whizz Kid",
	"Bad Guy",
	"The Dodger",
	"Big Ed",
	"Max Boost",
	"Dare Devil",
	"High Flyer",
	"Bully Boy",
	"Jumping Jack",
	"Road Hog",
	""					// the player
	};

/*	TAB.648c2, first eight bytes: the track pair for each division, bottom division first.	*/
static const int kDivisionTracks[NUM_DIVISIONS][TRACKS_PER_DIVISION] =
	{
	{ 0, 2 },		// Division IV - Little Ramp, Hump Back
	{ 1, 3 },		// Division III - Stepping Stones, Big Ramp
	{ 6, 7 },		// Division II - High Jump, Roller Coaster
	{ 4, 5 }		// Division I - Ski Jump, Draw Bridge
	};

/*	The opening ladder, top first.  The player starts at the bottom of Division IV, which	*/
/*	puts the three Division I drivers from heads.png (Hot Rod, Whizz Kid, Bad Guy) at the	*/
/*	top, and Jumping Jack and Road Hog above the player at the bottom.						*/
static const int kOpeningLadder[NUM_LEAGUE_DRIVERS] =
	{
	0,  1,  2,			// Division I    - Hot Rod, Whizz Kid, Bad Guy
	3,  4,  5,			// Division II   - The Dodger, Big Ed, Max Boost
	6,  7,  8,			// Division III  - Dare Devil, High Flyer, Bully Boy
	9, 10, PLAYER_DRIVER	// Division IV - Jumping Jack, Road Hog, you
	};

/*	======================================================================================= */
/*	Season state																			*/
/*	======================================================================================= */

int  gLeagueLadder[NUM_LEAGUE_DRIVERS];
LeagueDriver gLeagueTable[NUM_LEAGUE_DRIVERS];
LeagueFixture gLeagueFixtures[RACES_PER_SEASON];

int  gLeagueRace       = 0;
int  gLeagueSeason     = 0;
bool gLeagueSuperLeague = false;
char gPlayerName[16]   = "";

/*	======================================================================================= */
/*	Queries																					*/
/*	======================================================================================= */

const char *LeagueDriverName( int driver )
	{
	if ((driver < 0) || (driver >= NUM_LEAGUE_DRIVERS))
		return "";
	if (driver == PLAYER_DRIVER)
		return gPlayerName[0] ? gPlayerName : "You";
	return kDriverNames[driver];
	}

int LeagueDriverPosition( int driver )
	{
	for (int i = 0; i < NUM_LEAGUE_DRIVERS; i++)
		if (gLeagueLadder[i] == driver)
			return i;
	return NUM_LEAGUE_DRIVERS - 1;
	}

/*	The ladder is stored top first but divisions are indexed bottom first, so the two run	*/
/*	in opposite directions: ladder positions 9..11 are division index 0 (Division IV).		*/
int LeagueDivisionOfPosition( int position )
	{
	return (NUM_DIVISIONS - 1) - (position / DRIVERS_PER_DIVISION);
	}

int LeagueDivisionNumber( int divisionIndex )
	{
	return NUM_DIVISIONS - divisionIndex;
	}

int LeaguePlayerDivision( void )
	{
	return LeagueDivisionOfPosition(LeagueDriverPosition(PLAYER_DRIVER));
	}

int LeagueDivisionTrack( int divisionIndex, int which )
	{
	if ((divisionIndex < 0) || (divisionIndex >= NUM_DIVISIONS))
		divisionIndex = 0;
	return kDivisionTracks[divisionIndex][which & 1];
	}

/*	======================================================================================= */
/*	Season																					*/
/*	======================================================================================= */

static void ClearTable( void )
	{
	memset(gLeagueTable, 0, sizeof(gLeagueTable));
	}

void LeagueNewCareer( const char *name )
	{
	snprintf(gPlayerName, sizeof(gPlayerName), "%s", name ? name : "");

	memcpy(gLeagueLadder, kOpeningLadder, sizeof(gLeagueLadder));
	ClearTable();

	gLeagueRace        = 0;
	gLeagueSeason      = 0;
	gLeagueSuperLeague = false;

	LeagueStartSeason();
	}

void LeagueStartSeason( void )
	{
	ClearTable();
	gLeagueRace = 0;

	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	/*	The two other drivers in the player's division.									*/
	int opponents[DRIVERS_PER_DIVISION - 1];
	int n = 0;
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = gLeagueLadder[first + i];
		if ((driver != PLAYER_DRIVER) && (n < DRIVERS_PER_DIVISION - 1))
			opponents[n++] = driver;
		}
	while (n < DRIVERS_PER_DIVISION - 1)		// defensive: never expected
		opponents[n++] = 0;

	/*	Each opponent once on each of the division's two tracks.  The original's exact	*/
	/*	fixture ordering isn't recoverable from the disassembly, so the player takes on	*/
	/*	one opponent over both tracks before moving on to the next.						*/
	for (int race = 0; race < RACES_PER_SEASON; race++)
		{
		gLeagueFixtures[race].opponent = opponents[race / 2];
		gLeagueFixtures[race].trackID  = LeagueDivisionTrack(division, race & 1);
		gLeagueFixtures[race].raced    = false;
		gLeagueFixtures[race].won      = false;
		gLeagueFixtures[race].bestLap  = false;
		}
	}

const LeagueFixture *LeagueCurrentFixture( void )
	{
	if (gLeagueRace >= RACES_PER_SEASON)
		return NULL;
	return &gLeagueFixtures[gLeagueRace];
	}

/*	Score a race into the table: 2 points for the win, 1 for the fastest lap.				*/
static void Score( int driver, bool won, bool bestLap )
	{
	gLeagueTable[driver].raced++;
	if (won)
		{
		gLeagueTable[driver].wins++;
		gLeagueTable[driver].points += 2;
		}
	if (bestLap)
		{
		gLeagueTable[driver].bestLaps++;
		gLeagueTable[driver].points += 1;
		}
	}

void LeagueRecordResult( bool playerWon, bool playerBestLap )
	{
	if (gLeagueRace >= RACES_PER_SEASON)
		return;

	LeagueFixture *fixture = &gLeagueFixtures[gLeagueRace];
	fixture->raced   = true;
	fixture->won     = playerWon;
	fixture->bestLap = playerBestLap;

	Score(PLAYER_DRIVER, playerWon, playerBestLap);
	Score(fixture->opponent, !playerWon, !playerBestLap);

	/*	The two CPU drivers in the division also race each other while you're racing one	*/
	/*	of them, so the table stays consistent.  Higher up the ladder wins more often.	*/
	/*	They meet twice a season, same as every other pair, so only pair them off on		*/
	/*	the first race of each track - otherwise they'd finish the season on six races	*/
	/*	to your four.																	*/
	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	int cpu[DRIVERS_PER_DIVISION - 1];
	int n = 0;
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = gLeagueLadder[first + i];
		if ((driver != PLAYER_DRIVER) && (n < DRIVERS_PER_DIVISION - 1))
			cpu[n++] = driver;
		}

	const int cpuMeeting = gLeagueRace / 2;			// one per opponent block

	if ((n == DRIVERS_PER_DIVISION - 1) && ((gLeagueRace & 1) == 0)
		&& (cpuMeeting < DRIVERS_PER_DIVISION - 1))
		{
		/*	cpu[0] sits above cpu[1] on the ladder, so give it the edge, but let the	*/
		/*	runner-up take the fastest lap off it in the other meeting.				*/
		Score(cpu[0], cpuMeeting == 0, cpuMeeting == 0);
		Score(cpu[1], cpuMeeting != 0, cpuMeeting != 0);
		}

	gLeagueRace++;
	}

bool LeagueSeasonComplete( void )
	{
	return gLeagueRace >= RACES_PER_SEASON;
	}

/*	======================================================================================= */
/*	Function:		LeagueEndSeason															*/
/*																							*/
/*	Description:	Promotion and relegation.  The 68k swaps adjacent ladder entries			*/
/*					across a division boundary, so the division winner trades places			*/
/*					with the driver directly above them and the division's bottom			*/
/*					driver trades with the one directly below.  Winning Division I			*/
/*					sets league.offset, which is the SUPER LEAGUE flag.						*/
/*	======================================================================================= */

void LeagueEndSeason( char *promoted, char *relegated, int bufferSize )
	{
	if (promoted)  promoted[0]  = '\0';
	if (relegated) relegated[0] = '\0';

	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	/*	Order this division's drivers by points to find the winner and the bottom.		*/
	int order[DRIVERS_PER_DIVISION];
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		order[i] = gLeagueLadder[first + i];

	for (int i = 0; i < DRIVERS_PER_DIVISION - 1; i++)
		for (int j = i + 1; j < DRIVERS_PER_DIVISION; j++)
			if (gLeagueTable[order[j]].points > gLeagueTable[order[i]].points)
				{
				const int t = order[i]; order[i] = order[j]; order[j] = t;
				}

	const int winner = order[0];
	const int bottom = order[DRIVERS_PER_DIVISION - 1];

	/*	Rewrite the division in finishing order, then move the winner up and the bottom	*/
	/*	driver down across the division boundaries.										*/
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		gLeagueLadder[first + i] = order[i];

	if (first > 0)					// not already in Division I - promote the winner
		{
		const int above = gLeagueLadder[first - 1];
		gLeagueLadder[first - 1] = winner;
		gLeagueLadder[first]     = above;
		if (promoted)
			snprintf(promoted, bufferSize, "%s", LeagueDriverName(winner));
		}
	else if (winner == PLAYER_DRIVER)
		{
		/*	Won Division I - league.offset in the original.  Super League from here.		*/
		gLeagueSuperLeague = true;
		}

	if (first + DRIVERS_PER_DIVISION < NUM_LEAGUE_DRIVERS)	// not Division IV - relegate
		{
		const int last  = first + DRIVERS_PER_DIVISION - 1;
		const int below = gLeagueLadder[last + 1];
		gLeagueLadder[last + 1] = bottom;
		gLeagueLadder[last]     = below;
		if (relegated)
			snprintf(relegated, bufferSize, "%s", LeagueDriverName(bottom));
		}

	gLeagueSeason++;
	LeagueStartSeason();
	}
