/*	======================================================================================= */
/*	File:			Profile.cpp																*/
/*																							*/
/*	Description:	Reading and writing the driver profile - see Profile.h.					*/
/*																							*/
/*					The format is plain text, one record per line, with a version on the		*/
/*					first line.  Binary would be smaller and would also mean a saved			*/
/*					career could not survive a change to any of the structs it holds; a		*/
/*					career is worth more than the bytes, so text it is.  Unknown keys are	*/
/*					skipped rather than rejected, so a profile written by a later build		*/
/*					with more in it still loads what this one understands.					*/
/*	======================================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p)	_mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MKDIR(p)	mkdir(p, 0755)
#endif

#include "League.h"
#include "Profile.h"

/*	The Hall of Fame table lives in MenuScreens.cpp, but MenuScreens.h pulls in				*/
/*	dx_linux.h and with it SDL, which a file that only reads and writes numbers has no		*/
/*	business needing.  Declared here instead - it is three functions, and it keeps the		*/
/*	profile linkable on its own (tests/profile_test.cpp does exactly that).					*/
#define MENU_RECORD_TRACKS	8
#define MENU_RECORD_LEAGUES	2
void MenuScreensGetRecord( int trackID, int league, double *lapTime, int *lapDriver,
													double *raceTime, int *raceDriver );
void MenuScreensSetRecord( int trackID, int league, double lapTime, int lapDriver,
													double raceTime, int raceDriver );
void MenuScreensClearRecords( void );

#define SCR_PROFILE_VERSION		1

static bool gProfileExists = false;

/*	======================================================================================= */
/*	Where the file lives																	*/
/*	======================================================================================= */

/*	Make one directory, ignoring "already there".  Anything else is left to fail later		*/
/*	when the file itself cannot be opened, which is where the caller gives up quietly.		*/
static void MakeDir( const char *path )
	{
	MKDIR(path);
	}

/*	The directory everything this port saves lives in, made on demand.  Worked out once:		*/
/*	the answer cannot change while the game is running, and the mkdirs should not be			*/
/*	repeated on every save.																	*/
static const char *ProfileDir( void )
	{
	static char dir[1024];
	static bool tried = false;

	if (tried)
		return dir[0] ? dir : NULL;
	tried  = true;
	dir[0] = '\0';

#if defined(_WIN32)
	const char *appData = getenv("APPDATA");
	if (appData && appData[0])
		snprintf(dir, sizeof(dir), "%s\\StuntCarRacer", appData);
#elif defined(__APPLE__)
	const char *home = getenv("HOME");
	if (home && home[0])
		{
		/*	Library and Application Support both exist on any real account, but		*/
		/*	make them anyway rather than assume - a sandboxed or freshly made HOME	*/
		/*	has neither, and mkdir does not create parents.							*/
		char support[1024];
		snprintf(support, sizeof(support), "%s/Library", home);
		MakeDir(support);
		snprintf(support, sizeof(support), "%s/Library/Application Support", home);
		MakeDir(support);
		snprintf(dir, sizeof(dir), "%s/StuntCarRacer", support);
		}
#else
	const char *xdg = getenv("XDG_DATA_HOME");
	if (xdg && xdg[0])
		snprintf(dir, sizeof(dir), "%s/StuntCarRacer", xdg);
	else
		{
		const char *home = getenv("HOME");
		if (home && home[0])
			{
			char share[1024];
			snprintf(share, sizeof(share), "%s/.local", home);
			MakeDir(share);
			snprintf(share, sizeof(share), "%s/.local/share", home);
			MakeDir(share);
			snprintf(dir, sizeof(dir), "%s/.local/share/StuntCarRacer", home);
			}
		}
#endif

	if (!dir[0])
		return NULL;			// no home directory to speak of - saving is off

	MakeDir(dir);
	return dir;
	}

const char *ProfileDataPath( const char *filename )
	{
	static char path[1024];

	const char *dir = ProfileDir();
	if (dir == NULL)
		return NULL;			// no home directory to speak of - saving is off

#ifdef _WIN32
	snprintf(path, sizeof(path), "%s\\%s", dir, filename);
#else
	snprintf(path, sizeof(path), "%s/%s", dir, filename);
#endif
	return path;
	}

const char *ProfilePath( void )
	{
	static char path[1024];

	const char *p = ProfileDataPath("profile.txt");
	if (p == NULL)
		return NULL;

	/*	Its own copy: callers hold on to this one across other ProfileDataPath calls,	*/
	/*	and the tests read it back after everything else has run.						*/
	snprintf(path, sizeof(path), "%s", p);
	return path;
	}

bool ProfileExists( void )
	{
	return gProfileExists;
	}

/*	======================================================================================= */
/*	Saving																					*/
/*	======================================================================================= */

void ProfileSave( void )
	{
	const char *path = ProfilePath();
	if (path == NULL)
		return;

	FILE *f = fopen(path, "w");
	if (f == NULL)
		return;

	fprintf(f, "StuntCarRacerProfile %d\n", SCR_PROFILE_VERSION);

	/*	The name is the rest of the line: the original's entry accepts spaces.			*/
	fprintf(f, "name %s\n",   gPlayerName);
	fprintf(f, "season %d\n", gLeagueSeason);
	fprintf(f, "race %d\n",   gLeagueRace);
	fprintf(f, "super %d\n",  gLeagueSuperLeague ? 1 : 0);

	fprintf(f, "ladder");
	for (int i = 0; i < NUM_LEAGUE_DRIVERS; i++)
		fprintf(f, " %d", gLeagueLadder[i]);
	fprintf(f, "\n");

	for (int i = 0; i < NUM_LEAGUE_DRIVERS; i++)
		fprintf(f, "table %d %d %d %d %d\n", i,
				gLeagueTable[i].raced, gLeagueTable[i].wins,
				gLeagueTable[i].bestLaps, gLeagueTable[i].points);

	for (int i = 0; i < RACES_PER_SEASON; i++)
		fprintf(f, "fixture %d %d %d %d %d %d\n", i,
				gLeagueFixtures[i].opponent, gLeagueFixtures[i].trackID,
				gLeagueFixtures[i].raced ? 1 : 0,
				gLeagueFixtures[i].won   ? 1 : 0,
				gLeagueFixtures[i].bestLap ? 1 : 0);

	/*	Times go out at millisecond resolution, which is finer than the Hall of Fame		*/
	/*	prints them and enough to keep the ordering of two close records.				*/
	/*																					*/
	/*	The league's own records keep the original 'record' key and the Super League's	*/
	/*	go out under a new one, rather than both moving to a key that carries the		*/
	/*	league.  That way a profile written here still loads its league records into a	*/
	/*	build from before the split - which skips keys it does not know - and one		*/
	/*	written by that build still loads everything it had into this one.				*/
	for (int i = 0; i < MENU_RECORD_TRACKS; i++)
		{
		double lap = 0.0, race = 0.0;
		int    lapDriver = -1, raceDriver = -1;
		MenuScreensGetRecord(i, 0, &lap, &lapDriver, &race, &raceDriver);
		fprintf(f, "record %d %.3f %d %.3f %d\n", i, lap, lapDriver, race, raceDriver);

		lap = race = 0.0;
		lapDriver  = raceDriver = -1;
		MenuScreensGetRecord(i, 1, &lap, &lapDriver, &race, &raceDriver);
		fprintf(f, "superrecord %d %.3f %d %.3f %d\n", i, lap, lapDriver, race, raceDriver);
		}

	fclose(f);
	gProfileExists = true;
	}

/*	======================================================================================= */
/*	Loading																					*/
/*	======================================================================================= */

/*	Strip the trailing newline (and any \r, for a profile carried over from Windows).		*/
static void TrimEOL( char *line )
	{
	size_t n = strlen(line);
	while ((n > 0) && ((line[n - 1] == '\n') || (line[n - 1] == '\r')))
		line[--n] = '\0';
	}

bool ProfileLoad( void )
	{
	const char *path = ProfilePath();
	if (path == NULL)
		return false;

	FILE *f = fopen(path, "r");
	if (f == NULL)
		return false;

	char line[512];
	int  version = 0;

	if ((fgets(line, sizeof(line), f) == NULL) ||
		(sscanf(line, "StuntCarRacerProfile %d", &version) != 1) ||
		(version < 1) || (version > SCR_PROFILE_VERSION))
		{
		fclose(f);
		return false;
		}

	/*	Start from a known career so a short or partial file cannot leave the ladder		*/
	/*	half-written.  Everything the file does carry then overwrites it.				*/
	LeagueNewCareer("");
	MenuScreensClearRecords();

	while (fgets(line, sizeof(line), f) != NULL)
		{
		TrimEOL(line);

		if (strncmp(line, "name ", 5) == 0)
			{
			snprintf(gPlayerName, sizeof(gPlayerName), "%s", line + 5);
			continue;
			}

		int value = 0;
		if (sscanf(line, "season %d", &value) == 1) { gLeagueSeason = value; continue; }
		if (sscanf(line, "race %d",   &value) == 1) { gLeagueRace   = value; continue; }
		if (sscanf(line, "super %d",  &value) == 1) { gLeagueSuperLeague = (value != 0); continue; }

		if (strncmp(line, "ladder", 6) == 0)
			{
			const char *s = line + 6;
			for (int i = 0; i < NUM_LEAGUE_DRIVERS; i++)
				{
				int driver = 0, used = 0;
				if (sscanf(s, " %d%n", &driver, &used) != 1)
					break;
				if ((driver >= 0) && (driver < NUM_LEAGUE_DRIVERS))
					gLeagueLadder[i] = driver;
				s += used;
				}
			continue;
			}

		int index = 0, a = 0, b = 0, c = 0, d = 0, e = 0;

		if (sscanf(line, "table %d %d %d %d %d", &index, &a, &b, &c, &d) == 5)
			{
			if ((index >= 0) && (index < NUM_LEAGUE_DRIVERS))
				{
				gLeagueTable[index].raced    = a;
				gLeagueTable[index].wins     = b;
				gLeagueTable[index].bestLaps = c;
				gLeagueTable[index].points   = d;
				}
			continue;
			}

		if (sscanf(line, "fixture %d %d %d %d %d %d", &index, &a, &b, &c, &d, &e) == 6)
			{
			if ((index >= 0) && (index < RACES_PER_SEASON))
				{
				gLeagueFixtures[index].opponent = a;
				gLeagueFixtures[index].trackID  = b;
				gLeagueFixtures[index].raced    = (c != 0);
				gLeagueFixtures[index].won      = (d != 0);
				gLeagueFixtures[index].bestLap  = (e != 0);
				}
			continue;
			}

		double lap = 0.0, race = 0.0;
		int    lapDriver = -1, raceDriver = -1;
		if (sscanf(line, "record %d %lf %d %lf %d",
				   &index, &lap, &lapDriver, &race, &raceDriver) == 5)
			{
			MenuScreensSetRecord(index, 0, lap, lapDriver, race, raceDriver);
			continue;
			}

		/*	A profile from before the leagues were split has no 'superrecord' lines,	*/
		/*	so its Super League times stay where they were saved - in the league		*/
		/*	table.  Guessing which of them belonged to which league is not possible	*/
		/*	from the file, and dropping them would lose records that were earned.	*/
		if (sscanf(line, "superrecord %d %lf %d %lf %d",
				   &index, &lap, &lapDriver, &race, &raceDriver) == 5)
			{
			MenuScreensSetRecord(index, 1, lap, lapDriver, race, raceDriver);
			continue;
			}

		/*	Anything else is from a build that knows more than this one - skip it.	*/
		}

	fclose(f);

	/*	A ladder that did not contain the player would leave the season pointing at a	*/
	/*	division the player is not in, so fall back rather than run with it.				*/
	bool hasPlayer = false;
	for (int i = 0; i < NUM_LEAGUE_DRIVERS; i++)
		if (gLeagueLadder[i] == PLAYER_DRIVER)
			hasPlayer = true;

	if (!hasPlayer || (gLeagueRace < 0) || (gLeagueRace > RACES_PER_SEASON))
		{
		LeagueNewCareer("");
		MenuScreensClearRecords();
		return false;
		}

	gProfileExists = true;
	return true;
	}

/*	======================================================================================= */
/*	Deleting																				*/
/*	======================================================================================= */

void ProfileDelete( void )
	{
	const char *path = ProfilePath();
	if (path)
		remove(path);
	gProfileExists = false;
	}
