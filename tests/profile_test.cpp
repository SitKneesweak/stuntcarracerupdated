/*	=======================================================================================	*/
/*	File:			profile_test.cpp														*/
/*																							*/
/*	Description:	Round-trips a career through Profile.cpp: play part of a season, save,	*/
/*					wipe everything in memory, load, and check that what comes back is		*/
/*					what went in.  The point is the promise the feature makes - quit and		*/
/*					come back and you are where you left off - so the assertions are on		*/
/*					the ladder, the season position, the table and the Hall of Fame			*/
/*					rather than on the file's text.											*/
/*																							*/
/*					Writes into a temporary HOME, so it never touches the real profile.		*/
/*					Build and run:  make -C tests && ./tests/profile_test					*/
/*	=======================================================================================	*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../League.h"
#include "../Profile.h"

#define MENU_RECORD_TRACKS	8
#define MENU_RECORD_LEAGUES	2

/*	The Hall of Fame table, which in the game lives in MenuScreens.cpp.  Standing it up		*/
/*	here is what keeps this test clear of SDL and the renderer.  One table per league,		*/
/*	the same shape as the real one.															*/
static double gLap[MENU_RECORD_LEAGUES][MENU_RECORD_TRACKS];
static double gRace[MENU_RECORD_LEAGUES][MENU_RECORD_TRACKS];
static int    gLapDriver[MENU_RECORD_LEAGUES][MENU_RECORD_TRACKS];
static int    gRaceDriver[MENU_RECORD_LEAGUES][MENU_RECORD_TRACKS];

static bool RecordIndexOK( int t, int l )
	{
	return (t >= 0) && (t < MENU_RECORD_TRACKS) && (l >= 0) && (l < MENU_RECORD_LEAGUES);
	}

void MenuScreensGetRecord( int t, int l, double *lap, int *lapDrv, double *race, int *raceDrv )
	{
	if (!RecordIndexOK(t, l)) return;
	if (lap)     *lap     = gLap[l][t];
	if (lapDrv)  *lapDrv  = gLapDriver[l][t];
	if (race)    *race    = gRace[l][t];
	if (raceDrv) *raceDrv = gRaceDriver[l][t];
	}

void MenuScreensSetRecord( int t, int l, double lap, int lapDrv, double race, int raceDrv )
	{
	if (!RecordIndexOK(t, l)) return;
	gLap[l][t]  = lap;  gLapDriver[l][t]  = lapDrv;
	gRace[l][t] = race; gRaceDriver[l][t] = raceDrv;
	}

void MenuScreensClearRecords( void )
	{
	for (int l = 0; l < MENU_RECORD_LEAGUES; l++)
		for (int i = 0; i < MENU_RECORD_TRACKS; i++)
			{
			gLap[l][i] = gRace[l][i] = 0.0;
			gLapDriver[l][i] = gRaceDriver[l][i] = -1;
			}
	}

/*	=======================================================================================	*/

static int gFailures = 0;

static void Check( bool ok, const char *what )
	{
	printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		gFailures++;
	}

int main( void )
	{
	/*	Somewhere disposable to save into.  ProfilePath caches its answer on the first	*/
	/*	call, so this has to be set before anything else touches the profile.			*/
	char tmpHome[] = "/tmp/scrprofileXXXXXX";
	if (mkdtemp(tmpHome) == NULL)
		{
		printf("FAIL  could not make a temporary HOME\n");
		return 1;
		}
	setenv("HOME", tmpHome, 1);
	setenv("XDG_DATA_HOME", tmpHome, 1);
	setenv("APPDATA", tmpHome, 1);

	Check(!ProfileExists(), "no profile before anything is saved");
	Check(!ProfileLoad(),   "loading with no profile on disk fails cleanly");

	/*	--- A career part-way through its first season ---------------------------------	*/
	LeagueNewCareer("HENRY");
	MenuScreensClearRecords();

	LeagueRecordResult(true,  true);		// race 1: won it, and set the fastest lap
	LeagueRecordResult(false, false);		// race 2: lost

	/*	Damage carried out of race 2 and into race 3 - the season is driven in one car.	*/
	gLeagueDamageHoles = 3;

	const int  savedTrack   = gLeagueFixtures[0].trackID;
	const int  savedOpponent = gLeagueFixtures[2].opponent;
	const int  savedPoints  = gLeagueTable[PLAYER_DRIVER].points;
	const int  savedPosition = LeagueDriverPosition(PLAYER_DRIVER);

	MenuScreensSetRecord(savedTrack, 0, 27.312, PLAYER_DRIVER, 112.640, PLAYER_DRIVER);

	/*	The same track in the Super League, which is a separate record and must not		*/
	/*	come back as the league's.														*/
	MenuScreensSetRecord(savedTrack, 1, 24.875, 3, 101.220, 3);

	ProfileSave();
	Check(ProfileExists(), "a profile exists after saving");

	/*	--- Wipe it all, as quitting and restarting would ------------------------------	*/
	LeagueNewCareer("");
	MenuScreensClearRecords();
	Check(gLeagueRace == 0, "the in-memory career really was wiped");
	Check(gLeagueDamageHoles == 0, "a new driver starts with an undamaged car");

	/*	--- And back ------------------------------------------------------------------	*/
	Check(ProfileLoad(), "the profile loads");

	Check(strcmp(gPlayerName, "HENRY") == 0,          "the driver's name comes back");
	Check(gLeagueRace == 2,                            "the season resumes at race 3");
	Check(gLeagueSeason == 0,                          "the season number comes back");
	Check(!gLeagueSuperLeague,                         "the Super League flag comes back");
	Check(gLeagueDamageHoles == 3,                     "the carried damage comes back");
	Check(LeagueDriverPosition(PLAYER_DRIVER) == savedPosition, "the ladder comes back");
	Check(gLeagueTable[PLAYER_DRIVER].points == savedPoints,    "the points come back");
	Check(gLeagueTable[PLAYER_DRIVER].wins == 1,       "the win comes back");
	Check(gLeagueTable[PLAYER_DRIVER].bestLaps == 1,   "the fastest lap comes back");
	Check(gLeagueFixtures[0].raced && gLeagueFixtures[0].won,   "fixture 1 is still won");
	Check(gLeagueFixtures[1].raced && !gLeagueFixtures[1].won,  "fixture 2 is still lost");
	Check(gLeagueFixtures[2].opponent == savedOpponent,         "the fixtures come back");

	double lap = 0.0, race = 0.0;
	int    lapDrv = -1, raceDrv = -1;
	MenuScreensGetRecord(savedTrack, 0, &lap, &lapDrv, &race, &raceDrv);
	Check((lap > 27.311) && (lap < 27.313),   "the lap record comes back");
	Check((race > 112.63) && (race < 112.65), "the race record comes back");
	Check((lapDrv == PLAYER_DRIVER) && (raceDrv == PLAYER_DRIVER),
		  "the record holders come back");

	MenuScreensGetRecord(savedTrack, 1, &lap, &lapDrv, &race, &raceDrv);
	Check((lap > 24.874) && (lap < 24.876),   "the Super League lap record comes back");
	Check((race > 101.21) && (race < 101.23), "the Super League race record comes back");
	Check((lapDrv == 3) && (raceDrv == 3),    "the Super League record holders come back");

	MenuScreensGetRecord((savedTrack + 1) & 7, 0, &lap, &lapDrv, &race, &raceDrv);
	Check((lap == 0.0) && (lapDrv == -1), "an unset record stays unset");

	MenuScreensGetRecord((savedTrack + 1) & 7, 1, &lap, &lapDrv, &race, &raceDrv);
	Check((lap == 0.0) && (lapDrv == -1), "an unset Super League record stays unset");

	/*	--- A name with a space in it, which the entry screen allows -------------------	*/
	LeagueNewCareer("BIG ED");
	ProfileSave();
	LeagueNewCareer("");
	Check(ProfileLoad() && (strcmp(gPlayerName, "BIG ED") == 0), "a name with a space survives");

	/*	--- Seasons roll over ---------------------------------------------------------	*/
	for (int i = 0; i < RACES_PER_SEASON; i++)
		LeagueRecordResult(true, true);
	char promoted[16], relegated[16];
	LeagueEndSeason(promoted, relegated, sizeof(promoted));
	const int division = LeaguePlayerDivision();
	ProfileSave();

	LeagueNewCareer("");
	Check(ProfileLoad(), "the profile loads after a season ends");
	Check(gLeagueSeason == 1,                "the season count came back");
	Check(LeaguePlayerDivision() == division, "the promotion came back");
	Check(gLeagueRace == 0,                  "the new season starts at race 1");

	/*	--- A profile from before the leagues were split -------------------------------	*/
	/*	It has 'record' lines and no 'superrecord' ones, and must still load: those		*/
	/*	times are the driver's, whichever league they were set in.						*/
	{
	MenuScreensClearRecords();
	MenuScreensSetRecord(0, 0, 30.500, PLAYER_DRIVER, 120.000, PLAYER_DRIVER);
	MenuScreensSetRecord(0, 1, 25.000, PLAYER_DRIVER, 100.000, PLAYER_DRIVER);
	ProfileSave();

	/*	Rewrite the file with every 'superrecord' line dropped.						*/
	char old[65536];
	size_t used = 0;
	FILE *in = fopen(ProfilePath(), "r");
	char  line[512];
	while (in && (fgets(line, sizeof(line), in) != NULL))
		if (strncmp(line, "superrecord ", 12) != 0)
			used += snprintf(old + used, sizeof(old) - used, "%s", line);
	if (in) fclose(in);

	FILE *out = fopen(ProfilePath(), "w");
	if (out) { fwrite(old, 1, used, out); fclose(out); }

	MenuScreensClearRecords();
	Check(ProfileLoad(), "a profile with no Super League records still loads");

	MenuScreensGetRecord(0, 0, &lap, &lapDrv, &race, &raceDrv);
	Check((lap > 30.499) && (lap < 30.501), "its records land in the league table");
	MenuScreensGetRecord(0, 1, &lap, &lapDrv, &race, &raceDrv);
	Check(lap == 0.0, "and the Super League table starts empty");
	}

	/*	--- Deleting ------------------------------------------------------------------	*/
	ProfileDelete();
	Check(!ProfileExists(), "deleting clears the profile");
	Check(!ProfileLoad(),   "and there is nothing left to load");

	/*	--- A corrupt file is refused rather than half-loaded --------------------------	*/
	FILE *f = fopen(ProfilePath(), "w");
	if (f)
		{
		fprintf(f, "not a profile at all\n");
		fclose(f);
		}
	Check(!ProfileLoad(), "a file that is not a profile is refused");

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "PASSED",
		   gFailures, (gFailures == 1) ? "" : "s");
	return gFailures ? 1 : 0;
	}
