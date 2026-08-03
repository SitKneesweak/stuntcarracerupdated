/*	======================================================================================= */
/*	File:			MenuScreens.cpp															*/
/*																							*/
/*	Description:	The Amiga menu flow.  Every string here is the original's, taken from	*/
/*					the text tables in the 68k disassembly:									*/
/*																							*/
/*						main.game.selection.text	'Single Player League', 'Multiplayer',	*/
/*													'Enter another driver', 'Continue',		*/
/*													'Tracks in DIVISION ', 'Track:  The ',	*/
/*													'DRIVERS CHAMPIONSHIP', 'Track record'	*/
/*						TEXT.5a69a					'Practise ', 'Start the Racing Season',	*/
/*													'Load/Save/Replay', 'SUPER DIVISION ',	*/
/*													'to the SUPER LEAGUE', 'Hall of Fame'	*/
/*						league.text					'DIVISION ', 'RACE  ', 'The ', ' V ',	*/
/*													'RESULT', 'Race Winner: ',				*/
/*													'Fastest Lap: ', 'RESULTS TABLE',		*/
/*													'DRIVER     RACED WIN LAP  PTS',		*/
/*													'Promotion for ', 'Relegation for ',	*/
/*													' CHANGES', 'NAME?'						*/
/*						TEXT.5ec92					'HALL of FAME', 'Race Time: ',			*/
/*													'Best Lap : ', 'TRACK BONUS POINTS'		*/
/*																							*/
/*					Menu entries sit on rows 13, 16, 19 and 22 with the selection bar		*/
/*					drawn behind them, and the 'SELECT' heading at column 17 row 11 - all	*/
/*					straight out of menu.bar.positions and the tables above.					*/
/*	======================================================================================= */

#include "dx_linux.h"
#include "StuntCarRacer.h"
#include "AmigaMenu.h"
#include "MenuScreens.h"
#include "League.h"
#include "Track.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*	======================================================================================= */
/*	Layout constants, from the disassembly													*/
/*	======================================================================================= */

/*	menu.bar.positions: the rows successive menu entries are drawn on.						*/
static const int kMenuRows[4] = { 13, 16, 19, 22 };
#define MAX_MENU_ENTRIES	4

/*	fill.bar prints the entry starting at column 5, as "N. Text".							*/
#define MENU_ENTRY_COLUMN	5

/*	31,17,11,'SELECT'																		*/
#define SELECT_COLUMN		17

/*	The logo overhangs the top of the panel by a dozen pixels, so row 7 - which the			*/
/*	original could print a heading on - is no longer clear of it.  Headings that sat on		*/
/*	row 7 go on row 9 instead, the first row fully below the logo.							*/
#define HEADING_ROW			9
#define SELECT_ROW			11

/*	The portrait grid in heads.png.  Columns are divisions (leftmost is Division I) and		*/
/*	rows are the three drivers in that division, so driver ID d is at (d/3, d%3) - which		*/
/*	is exactly how the opening ladder is ordered.  Measured off the artwork's own borders.	*/
static const int kHeadCellX[NUM_DIVISIONS] = { 0, 77, 157, 237 };
static const int kHeadCellY[DRIVERS_PER_DIVISION] = { 13, 68, 123 };
#define HEAD_CELL_W		79
#define HEAD_CELL_H		55

/*	The Hall of Fame heading - 'TRACK  DRIVER   LAP-TIME    DRIVER  RACE-TIME' at column	*/
/*	0 - allows the track only six characters before the first DRIVER column, so that		*/
/*	screen needs short forms rather than the full names below.								*/
static const char *kTrackShortNames[8] =
	{
	"LITTLE",
	"STEPS",
	"HUMP",
	"BIG",
	"SKI",
	"BRIDGE",
	"HIGH",
	"ROLLER"
	};

/*	Track names as the original prints them, after "The ".									*/
static const char *kTrackNames[8] =
	{
	"LITTLE RAMP",
	"STEPPING STONES",
	"HUMP BACK",
	"BIG RAMP",
	"SKI JUMP",
	"DRAW BRIDGE",
	"HIGH JUMP",
	"ROLLER COASTER"
	};

/*	======================================================================================= */
/*	State																					*/
/*	======================================================================================= */

static MenuScreenType gScreen     = MS_NAME_ENTRY;
static bool			  gActive     = true;
static int			  gSelection  = 0;
static bool			  gRaceIsLeague = false;
static int			  gRaceTrack    = 0;	// the track the running race is on

static char gNameBuffer[16] = "";
static int  gNameLength     = 0;

static char gPromoted[16]  = "";
static char gRelegated[16] = "";

/*	The times from the race just finished, for the RESULT screen.							*/
static double gLastLapTime  = 0.0;
static double gLastRaceTime = 0.0;

/*	Track records, kept for the Hall of Fame.  Zero means "not set yet", which the original	*/
/*	shows as a row of dashes ('------------' in TEXT.5ec48).								*/
static double gRecordLap[8]  = { 0 };
static double gRecordRace[8] = { 0 };
static int    gRecordLapDriver[8];
static int    gRecordRaceDriver[8];

static void MenuScreensDraw( void );

bool MenuScreensActive( void ) { return gActive; }
bool MenuScreensRaceIsLeague( void ) { return gRaceIsLeague; }

void MenuScreensGoto( MenuScreenType screen )
	{
	gScreen    = screen;
	gSelection = 0;
	gActive    = true;
	}

/*	======================================================================================= */
/*	Function:		MenuScreensDumpAll														*/
/*																							*/
/*	Description:	Render every screen to <prefix>-NN-<name>.ppm against a made-up			*/
/*					part-finished season.  Set SCR_MENU_DUMPALL to a path prefix to run		*/
/*					it at start-up.  Layout here is fixed to a character grid, so being		*/
/*					able to eyeball all thirteen screens at once is the quickest way to		*/
/*					catch a heading or column that has drifted off the panel.				*/
/*	======================================================================================= */

static void MenuScreensDumpAll( const char *prefix )
	{
	static const char *names[] =
		{
		"name-entry", "main", "select", "practise-track", "division", "fixture",
		"result", "table", "championship", "changes", "super-league",
		"hall-of-fame", "load-save", "link"
		};

	/*	A season with two races run, so the tables have something in them.				*/
	LeagueNewCareer("ANDREW");
	LeagueRecordResult(true,  true);
	LeagueRecordResult(false, true);

	snprintf(gPromoted,  sizeof(gPromoted),  "%s", LeagueDriverName(PLAYER_DRIVER));
	snprintf(gRelegated, sizeof(gRelegated), "%s", LeagueDriverName(10));
	gLastLapTime  = 27.31;
	gLastRaceTime = 112.64;
	MenuScreensRecordTimes(0, PLAYER_DRIVER, 27.31, 112.64);
	MenuScreensRecordTimes(5, 3, 31.09, 128.44);

	snprintf(gNameBuffer, sizeof(gNameBuffer), "ANDREW");
	gNameLength = 6;
	gSelection  = 1;

	for (int i = 0; i <= (int)MS_LINK; i++)
		{
		char path[256];
		gScreen = (MenuScreenType)i;
		MenuScreensDraw();

		snprintf(path, sizeof(path), "%s-%02d-%s.ppm", prefix, i, names[i]);
		AmigaMenuWritePPM(path);
		printf("AmigaMenu: wrote %s\n", path);
		}
	}

void MenuScreensInit( void )
	{
	for (int i = 0; i < 8; i++)
		{
		gRecordLapDriver[i]  = -1;
		gRecordRaceDriver[i] = -1;
		}
	gNameBuffer[0] = '\0';
	gNameLength    = 0;

	const char *dumpPrefix = getenv("SCR_MENU_DUMPALL");
	if (dumpPrefix)
		MenuScreensDumpAll(dumpPrefix);

	gNameBuffer[0] = '\0';
	gNameLength    = 0;
	MenuScreensGoto(MS_NAME_ENTRY);
	}

/*	======================================================================================= */
/*	Helpers																					*/
/*	======================================================================================= */

/*	Draw a menu: the 'SELECT' heading, then numbered entries on rows 13/16/19/22, each on		*/
/*	its own bar - grey, or amber for the highlighted one.									*/
static void DrawMenu( const char *const *entries, int count, int selected )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(SELECT_COLUMN, SELECT_ROW, "SELECT");

	for (int i = 0; i < count; i++)
		{
		AmigaMenuBar(kMenuRows[i], i == selected);

		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, kMenuRows[i], "%d. %s", i + 1, entries[i]);
		}
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

/*	The original prints times as m:ss.hh (print.dec.digit2 / R.5edee around line 19470).		*/
static void FormatTime( char *buffer, int size, double seconds )
	{
	if (seconds <= 0.0)
		{
		snprintf(buffer, size, "------------");
		return;
		}
	const int total = (int)(seconds * 100.0 + 0.5);
	snprintf(buffer, size, "%d:%02d.%02d", (total / 6000), (total / 100) % 60, total % 100);
	}

/*	Each portrait cell is a face with a name plate across the bottom ten pixels.				*/
#define HEAD_NAME_H		10
#define HEAD_NAME_Y		(HEAD_CELL_H - HEAD_NAME_H)

/*	The name plate colours, sampled from the artwork.										*/
static const AmigaPen kNamePlateInk   = { 255, 255, 255 };
static const AmigaPen kNamePlatePaper = {  20,  20,  90 };

static void DrawPortrait( int driver, int x, int y )
	{
	if ((driver < 0) || (driver >= NUM_LEAGUE_DRIVERS))
		return;

	AmigaMenuBlitRect("heads.png",
					  kHeadCellX[driver / DRIVERS_PER_DIVISION],
					  kHeadCellY[driver % DRIVERS_PER_DIVISION],
					  HEAD_CELL_W, HEAD_CELL_H, x, y);

	/*	Driver 11's cell is the player's.  It carries no name in the original (the		*/
	/*	name slot in opponents.names.source is blank), and in the artwork shipped with	*/
	/*	this port the plate has been scribbled over by whoever cracked the disk - so		*/
	/*	repaint it and print whatever name was entered.									*/
	if (driver == PLAYER_DRIVER)
		{
		AmigaMenuFillRect(x, y + HEAD_NAME_Y, HEAD_CELL_W, HEAD_NAME_H, kNamePlatePaper);

		const char *name = LeagueDriverName(PLAYER_DRIVER);
		const int   len  = (int)strlen(name);
		const int   textX = x + (HEAD_CELL_W - len * AMIGA_CHAR_WIDTH) / 2;

		AmigaMenuSetInk(kNamePlateInk);
		AmigaMenuPrintPixel(textX, y + HEAD_NAME_Y + 1, name);
		AmigaMenuSetInk(AMIGA_INK_TEXT);
		}
	}

/*	Row 23 is the last one that fits inside the panel: its glyphs occupy y 184..192 and		*/
/*	the panel ends at 195.  Row 24 would hang over the frame.								*/
#define PROMPT_ROW	23

static void PressAnyKeyPrompt( void )
	{
	AmigaMenuSetInk(AMIGA_INK_BLACK);		// dark red would vanish into the grey panel
	AmigaMenuPrintCentred(PROMPT_ROW, "Press RETURN to continue");
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

/*	======================================================================================= */
/*	Screen drawing																			*/
/*	======================================================================================= */

static void DrawNameEntry( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(18, 14, "NAME?");				// 31,18,14,'NAME?'
	AmigaMenuPrintF(12, 17, "> %s_", gNameBuffer);
	PressAnyKeyPrompt();
	}

static void DrawMainMenu( void )
	{
	static const char *entries[4] =
		{
		"Single Player League",
		"Multiplayer",
		"Enter another driver",
		"Continue"
		};
	DrawMenu(entries, 4, gSelection);
	}

static void DrawSelectMenu( void )
	{
	static const char *entries[3] =
		{
		"Practise",
		"Start the Racing Season",
		"Load/Save/Replay"
		};
	DrawMenu(entries, 3, gSelection);
	}

static void DrawPractiseTracks( void )
	{
	/*	No 'SELECT' heading here: eight entries two rows apart already fill the panel	*/
	/*	from row 9 - the first row clear of the logo - down to row 23.					*/

	/*	Two rows per entry: the selection bar is 17 pixels tall, so entries any closer	*/
	/*	than two rows would be swallowed by their neighbour's bar.						*/
	for (int i = 0; i < 8; i++)
		{
		const int row = 9 + i * 2;
		AmigaMenuBar(row, i == gSelection);
		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%d. The %s", i + 1, kTrackNames[i]);
		}
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

static void DrawDivision( void )
	{
	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	if (gLeagueSuperLeague)
		AmigaMenuPrintF(12, HEADING_ROW, "SUPER DIVISION %d", LeagueDivisionNumber(division));
	else
		AmigaMenuPrintF(15, HEADING_ROW, "DIVISION %d", LeagueDivisionNumber(division));

	/*	The three drivers, with their portraits.  Three 79-wide cells across the			*/
	/*	224-wide panel leaves a small gutter between them.								*/
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = gLeagueLadder[first + i];
		DrawPortrait(driver, AMIGA_PANEL_X + 3 + i * 73, AMIGA_PANEL_Y + 26);
		}

	AmigaMenuPrintF(6, 19, "Tracks in DIVISION %d", LeagueDivisionNumber(division));
	for (int t = 0; t < TRACKS_PER_DIVISION; t++)
		AmigaMenuPrintF(8, 20 + t, "The %s", kTrackNames[LeagueDivisionTrack(division, t)]);

	PressAnyKeyPrompt();
	}

static void DrawFixture( void )
	{
	const LeagueFixture *fixture = LeagueCurrentFixture();
	if (fixture == NULL)
		return;

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintF(16, HEADING_ROW, "RACE  %d", gLeagueRace + 1);		// 31,14,13,'RACE  '

	/*	The two drivers head to head, with ' V ' in the gutter between them.				*/
	const int portraitY = AMIGA_PANEL_Y + 26;
	DrawPortrait(PLAYER_DRIVER,     AMIGA_PANEL_X + 16,  portraitY);
	DrawPortrait(fixture->opponent, AMIGA_PANEL_X + 129, portraitY);

	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintPixel(AMIGA_PANEL_X + 105, portraitY + 22, "V");
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	AmigaMenuPrintF(6, 20, "Track:  The %s", kTrackNames[fixture->trackID]);

	PressAnyKeyPrompt();
	}

static void DrawResult( void )
	{
	/*	gLeagueRace has already advanced past the race just run.						*/
	const int index = (gLeagueRace > 0) ? (gLeagueRace - 1) : 0;
	const LeagueFixture *fixture = &gLeagueFixtures[index];

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(17, 11, "RESULT");						// 31,17,15,'RESULT'

	const int winner  = fixture->won     ? PLAYER_DRIVER : fixture->opponent;
	const int fastest = fixture->bestLap ? PLAYER_DRIVER : fixture->opponent;

	AmigaMenuPrintF(7, 14, "Race Winner: %s", LeagueDriverName(winner));
	AmigaMenuPrintF(7, 16, "Fastest Lap: %s", LeagueDriverName(fastest));

	/*	'Race Time: ' and 'Best Lap : ' from TEXT.5ec92 - the player's own times.		*/
	char buffer[32];
	FormatTime(buffer, sizeof(buffer), gLastRaceTime);
	AmigaMenuPrintF(7, 19, "Race Time: %s", buffer);
	FormatTime(buffer, sizeof(buffer), gLastLapTime);
	AmigaMenuPrintF(7, 20, "Best Lap : %s", buffer);

	PressAnyKeyPrompt();
	}

static void DrawTable( void )
	{
	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(14, 11, "RESULTS TABLE");					// 31,14,11,'RESULTS TABLE'
	AmigaMenuPrintAt(6, 14, "DRIVER     RACED WIN LAP  PTS");	// 31,6,14,...

	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = gLeagueLadder[first + i];
		const LeagueDriver *rec = &gLeagueTable[driver];

		if (driver == PLAYER_DRIVER)
			AmigaMenuBar(16 + i * 2);

		AmigaMenuSetInk(driver == PLAYER_DRIVER ? AMIGA_INK_BAR_TEXT : AMIGA_INK_TEXT);
		AmigaMenuPrintF(6, 16 + i * 2, "%-11.11s %3d  %3d %3d  %3d",
						LeagueDriverName(driver),
						rec->raced, rec->wins, rec->bestLaps, rec->points);
		}
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	PressAnyKeyPrompt();
	}

static void DrawChampionship( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(10, 9, "DRIVERS CHAMPIONSHIP");			// 31,10,9,'DRIVERS CHAMPIONSHIP'

	for (int position = 0; position < NUM_LEAGUE_DRIVERS; position++)
		{
		const int driver = gLeagueLadder[position];
		const int row    = 11 + position;
		if (row > AMIGA_PANEL_ROW1)
			break;

		if (driver == PLAYER_DRIVER)
			{
			AmigaMenuBar(row);
			AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
			}
		else
			AmigaMenuSetInk(AMIGA_INK_TEXT);

		AmigaMenuPrintF(6, row, "%2d. %-13.13s DIVISION %d",
						position + 1, LeagueDriverName(driver),
						LeagueDivisionNumber(LeagueDivisionOfPosition(position)));
		}
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

static void DrawChanges( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintF(13, 11, "DIVISION %d CHANGES",
					LeagueDivisionNumber(LeaguePlayerDivision()));

	int row = 15;
	if (gPromoted[0])
		AmigaMenuPrintF(8, row++, "Promotion for  %s", gPromoted);
	if (gRelegated[0])
		AmigaMenuPrintF(8, row++, "Relegation for %s", gRelegated);
	if (!gPromoted[0] && !gRelegated[0])
		AmigaMenuPrintCentred(row, "No changes");

	PressAnyKeyPrompt();
	}

static void DrawSuperLeague( void )
	{
	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintCentred(11, "EXCELLENT DRIVING - WELL DONE");
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(15, "Promotion for");
	AmigaMenuPrintCentred(16, LeagueDriverName(PLAYER_DRIVER));
	AmigaMenuPrintCentred(18, "to the SUPER LEAGUE");
	PressAnyKeyPrompt();
	}

/*	The Hall of Fame is the one screen the original draws outside the menu frame: its		*/
/*	column headings sit at 31,0,7 and 31,16,1, i.e. column 0 and row 1, well outside the		*/
/*	panel.  At 45 characters the heading only fits because it spans the full 320 pixels.		*/
static void DrawHallOfFame( void )
	{
	AmigaMenuClear(AMIGA_INK_BLACK);
	AmigaMenuSetInk(AMIGA_INK_WHITE);

	AmigaMenuPrintAt(16, 1, "HALL of FAME");					// 31,16,1,'HALL of FAME'
	if (gLeagueSuperLeague)
		AmigaMenuPrintAt(16, 3, "SUPER LEAGUE");				// 31,16,5,'SUPER LEAGUE'

	AmigaMenuSetInk(AMIGA_INK_RED);
	/*	31,0,7,'TRACK  DRIVER   LAP-TIME    DRIVER  RACE-TIME'						*/
	AmigaMenuPrintAt(0, 7, "TRACK  DRIVER   LAP-TIME    DRIVER  RACE-TIME");
	AmigaMenuSetInk(AMIGA_INK_WHITE);

	for (int track = 0; track < 8; track++)
		{
		char lap[24], race[24];
		FormatTime(lap,  sizeof(lap),  gRecordLap[track]);
		FormatTime(race, sizeof(race), gRecordRace[track]);

		AmigaMenuPrintF(0, 9 + track * 2, "%-6.6s %-8.8s %-11.11s %-7.7s %-9.9s",
						kTrackShortNames[track],
						(gRecordLapDriver[track]  >= 0) ? LeagueDriverName(gRecordLapDriver[track])  : "",
						lap,
						(gRecordRaceDriver[track] >= 0) ? LeagueDriverName(gRecordRaceDriver[track]) : "",
						race);
		}

	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintAt(6, 24, "Press RETURN to continue");
	}

/*	The original's Load/Save/Replay submenu.  The entries are the real ones (TEXT.5a69a),	*/
/*	but this port has no career persistence or replay recorder behind them, so the screen	*/
/*	says so rather than pretending.															*/
static void DrawLoadSave( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(12, 10, "Load/Save/Replay");
	AmigaMenuPrintCentred(14, "Load    Save    Replay");
	AmigaMenuPrintCentred(17, "Saving a career and replays");
	AmigaMenuPrintCentred(18, "are not available in this port.");
	AmigaMenuPrintCentred(21, "Cancel");
	PressAnyKeyPrompt();
	}

static void DrawLink( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(13, 11, "Computer Link");
	AmigaMenuPrintCentred(15, "The two-player link needs two");
	AmigaMenuPrintCentred(16, "Amigas and a null-modem cable,");
	AmigaMenuPrintCentred(17, "so it is not available here.");
	PressAnyKeyPrompt();
	}

/*	======================================================================================= */
/*	Function:		MenuScreensRender														*/
/*	======================================================================================= */

/*	Build the current screen into the surface, without presenting it.						*/
static void MenuScreensDraw( void )
	{
	AmigaMenuFrame();

	switch (gScreen)
		{
		case MS_NAME_ENTRY:		DrawNameEntry();		break;
		case MS_MAIN:			DrawMainMenu();			break;
		case MS_SELECT:			DrawSelectMenu();		break;
		case MS_PRACTISE_TRACK:	DrawPractiseTracks();	break;
		case MS_DIVISION:		DrawDivision();			break;
		case MS_FIXTURE:		DrawFixture();			break;
		case MS_RESULT:			DrawResult();			break;
		case MS_TABLE:			DrawTable();			break;
		case MS_CHAMPIONSHIP:	DrawChampionship();		break;
		case MS_CHANGES:		DrawChanges();			break;
		case MS_SUPER_LEAGUE:	DrawSuperLeague();		break;
		case MS_HALL_OF_FAME:	DrawHallOfFame();		break;
		case MS_LOADSAVE:		DrawLoadSave();			break;
		case MS_LINK:			DrawLink();				break;
		}
	}

void MenuScreensRender( IDirect3DDevice9 *pd3dDevice )
	{
	MenuScreensDraw();
	AmigaMenuPresent(pd3dDevice);
	}

/*	======================================================================================= */
/*	Input																					*/
/*	======================================================================================= */

#ifdef linux
#define KEY_UP		SDLK_UP
#define KEY_DOWN	SDLK_DOWN
#define KEY_ENTER	SDLK_RETURN
#define KEY_BACK	SDLK_BACKSPACE
#define KEY_ESCAPE	SDLK_ESCAPE
#else
#define KEY_UP		VK_UP
#define KEY_DOWN	VK_DOWN
#define KEY_ENTER	VK_RETURN
#define KEY_BACK	VK_BACK
#define KEY_ESCAPE	VK_ESCAPE
#endif

/*	How many entries the current screen offers, for the up/down keys.						*/
static int EntryCount( void )
	{
	switch (gScreen)
		{
		case MS_MAIN:			return 4;
		case MS_SELECT:			return 3;
		case MS_PRACTISE_TRACK:	return 8;
		default:				return 0;
		}
	}

/*	Begin the fixture the season is currently pointing at.									*/
static void StartLeagueRace( void )
	{
	const LeagueFixture *fixture = LeagueCurrentFixture();
	if (fixture == NULL)
		return;

	gRaceIsLeague = true;
	gRaceTrack    = fixture->trackID;
	if (MenuStartTrack(fixture->trackID))
		gActive = false;
	}

static void HandleNameEntry( int key )
	{
	if ((key == KEY_ENTER) && (gNameLength > 0))
		{
		LeagueNewCareer(gNameBuffer);
		MenuScreensGoto(MS_MAIN);
		return;
		}

	if ((key == KEY_BACK) && (gNameLength > 0))
		{
		gNameBuffer[--gNameLength] = '\0';
		return;
		}

	/*	The original's name entry accepts the printable set its font can draw.			*/
	if ((key >= ' ') && (key < 127) && (gNameLength < (int)sizeof(gNameBuffer) - 1))
		{
		char c = (char)key;
		if ((c >= 'a') && (c <= 'z'))		// the Amiga stores names upper case
			c = (char)(c - 'a' + 'A');
		gNameBuffer[gNameLength++] = c;
		gNameBuffer[gNameLength]   = '\0';
		}
	}

static void ActivateMain( void )
	{
	switch (gSelection)
		{
		case 0:	MenuScreensGoto(MS_SELECT);			break;	// Single Player League
		case 1:	MenuScreensGoto(MS_LINK);			break;	// Multiplayer
		case 2:												// Enter another driver
			gNameBuffer[0] = '\0';
			gNameLength    = 0;
			MenuScreensGoto(MS_NAME_ENTRY);
			break;
		case 3:	MenuScreensGoto(MS_SELECT);			break;	// Continue
		}
	}

static void ActivateSelect( void )
	{
	switch (gSelection)
		{
		case 0:	MenuScreensGoto(MS_PRACTISE_TRACK);	break;	// Practise
		case 1:												// Start the Racing Season
			LeagueStartSeason();
			MenuScreensGoto(MS_DIVISION);
			break;
		case 2:	MenuScreensGoto(MS_LOADSAVE);		break;	// Load/Save/Replay
		}
	}

void MenuScreensKey( int key )
	{
	if (key == 0)
		return;

	if (gScreen == MS_NAME_ENTRY)
		{
		HandleNameEntry(key);
		return;
		}

	/*	Up and down move the selection bar (next.menu.bar.up / next.menu.bar.down).		*/
	const int entries = EntryCount();
	if (entries > 0)
		{
		if (key == KEY_UP)
			{
			gSelection = (gSelection + entries - 1) % entries;
			return;
			}
		if (key == KEY_DOWN)
			{
			gSelection = (gSelection + 1) % entries;
			return;
			}
		if ((key >= '1') && (key < '1' + entries))
			{
			gSelection = key - '1';
			return;
			}
		}

	if ((key != KEY_ENTER) && (key != ' '))
		return;

	switch (gScreen)
		{
		case MS_MAIN:			ActivateMain();		break;
		case MS_SELECT:			ActivateSelect();	break;

		case MS_PRACTISE_TRACK:
			gRaceIsLeague = false;
			gRaceTrack    = gSelection;
			if (MenuStartTrack(gSelection))
				gActive = false;
			break;

		case MS_DIVISION:		MenuScreensGoto(MS_FIXTURE);	break;
		case MS_FIXTURE:		StartLeagueRace();				break;
		case MS_RESULT:			MenuScreensGoto(MS_TABLE);		break;

		case MS_TABLE:
			if (LeagueSeasonComplete())
				MenuScreensGoto(MS_CHAMPIONSHIP);
			else
				MenuScreensGoto(MS_FIXTURE);
			break;

		case MS_CHAMPIONSHIP:
			{
			const bool wasTop = (LeaguePlayerDivision() == NUM_DIVISIONS - 1);
			LeagueEndSeason(gPromoted, gRelegated, sizeof(gPromoted));
			if (wasTop && gLeagueSuperLeague)
				MenuScreensGoto(MS_SUPER_LEAGUE);
			else
				MenuScreensGoto(MS_CHANGES);
			}
			break;

		case MS_SUPER_LEAGUE:	MenuScreensGoto(MS_CHANGES);		break;
		case MS_CHANGES:		MenuScreensGoto(MS_HALL_OF_FAME);	break;
		case MS_HALL_OF_FAME:	MenuScreensGoto(MS_DIVISION);		break;
		case MS_LOADSAVE:		MenuScreensGoto(MS_SELECT);			break;
		case MS_LINK:			MenuScreensGoto(MS_MAIN);			break;

		default:
			break;
		}
	}

/*	======================================================================================= */
/*	Function:		MenuScreensRaceFinished													*/
/*																							*/
/*	Description:	Score the race that just ended and pick the screen to come back to.		*/
/*	======================================================================================= */

void MenuScreensAbandonRace( void )
	{
	gActive = true;
	if (gRaceIsLeague)
		{
		gRaceIsLeague = false;
		MenuScreensGoto(MS_FIXTURE);		// the fixture stands, run it again
		}
	else
		MenuScreensGoto(MS_SELECT);
	}

void MenuScreensRaceFinished( bool playerWon, bool playerBestLap,
							  double playerLapTime, double playerRaceTime )
	{
	gActive       = true;
	gLastLapTime  = playerLapTime;
	gLastRaceTime = playerRaceTime;

	if (!gRaceIsLeague)
		{
		/*	A practise run still counts for the Hall of Fame.							*/
		MenuScreensRecordTimes(gRaceTrack, PLAYER_DRIVER, playerLapTime, playerRaceTime);
		MenuScreensGoto(MS_SELECT);
		return;
		}

	MenuScreensRecordTimes(gRaceTrack, PLAYER_DRIVER, playerLapTime, playerRaceTime);

	LeagueRecordResult(playerWon, playerBestLap);
	gRaceIsLeague = false;
	MenuScreensGoto(MS_RESULT);
	}

/*	Record a lap or race time against a track, for the Hall of Fame.						*/
void MenuScreensRecordTimes( int trackID, int driver, double lapTime, double raceTime )
	{
	if ((trackID < 0) || (trackID >= 8))
		return;

	if ((lapTime > 0.0) && ((gRecordLap[trackID] == 0.0) || (lapTime < gRecordLap[trackID])))
		{
		gRecordLap[trackID]       = lapTime;
		gRecordLapDriver[trackID] = driver;
		}

	if ((raceTime > 0.0) && ((gRecordRace[trackID] == 0.0) || (raceTime < gRecordRace[trackID])))
		{
		gRecordRace[trackID]       = raceTime;
		gRecordRaceDriver[trackID] = driver;
		}
	}
