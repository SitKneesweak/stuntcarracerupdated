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
#include "Opponent_Behaviour.h"

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

/*	R.58e7a passes d0=11 to R.58e30 before a race, so 'RACE  n of m' lands on row 11.		*/
#define RACE_ROW			11

/*	The portrait grid in heads.png.  Columns are divisions (leftmost is Division I) and		*/
/*	rows are the three drivers in that division, so driver ID d is at (d/3, d%3) - which		*/
/*	is exactly how the opening ladder is ordered.											*/
/*																							*/
/*	These are the artwork's own cell rectangles, taken off the file a pixel at a time: each	*/
/*	cell is bounded by a one-pixel white frame, columns starting at x 2/82/162/242 and rows	*/
/*	at y 12/67/122, 74x54 including that frame.  Getting this wrong shows up on the fixture	*/
/*	screen, where a cell that is too wide drags in the grey gutter on one side and clips		*/
/*	the neighbour's frame off on the other.													*/
static const int kHeadCellX[NUM_DIVISIONS] = { 2, 82, 162, 242 };
static const int kHeadCellY[DRIVERS_PER_DIVISION] = { 12, 67, 122 };
#define HEAD_CELL_W		74
#define HEAD_CELL_H		54

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

static MenuScreenType gScreen     = MS_MAIN;
static bool			  gActive     = true;
static int			  gSelection  = 0;
static bool			  gRaceIsLeague = false;
static int			  gRaceTrack    = 0;	// the track the running race is on

static char gNameBuffer[16] = "";
static int  gNameLength     = 0;

static char gPromoted[16]  = "";
static char gRelegated[16] = "";

/*	True when the Hall of Fame was picked off the menu, so ESC/fire returns to the menu		*/
/*	rather than continuing the end-of-season run of screens.								*/
static bool gHallFromMenu = false;

/*	The times from the race just finished, for the RESULT screen.							*/
static double gLastLapTime  = 0.0;
static double gLastRaceTime = 0.0;

/*	What the race just finished did to the records, for the 'New track records' screen		*/
/*	that sits between the race picture and the RESULT.  gRecordScreenReturn is where fire	*/
/*	goes from there: a league race carries on to the RESULT, a practise run drops back to	*/
/*	the menu.																				*/
static bool			 gNewRecordRace   = false;
static bool			 gNewRecordLap    = false;
static int			 gNewRecordTrack  = 0;
static MenuScreenType gRecordScreenReturn = MS_SELECT;

/*	Track records, kept for the Hall of Fame.  Zero means "not set yet", which the original	*/
/*	shows as a row of dashes ('------------' in TEXT.5ec48).								*/
static double gRecordLap[8]  = { 0 };
static double gRecordRace[8] = { 0 };
static int    gRecordLapDriver[8];
static int    gRecordRaceDriver[8];

static void MenuScreensDraw( void );

bool MenuScreensActive( void ) { return gActive; }
void MenuScreensDeactivate( void ) { gActive = false; }
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
		"name-entry", "opponents", "main", "select", "league-choice", "practise-track", "division", "fixture",
		"race-win", "race-lost", "track-record", "result", "table", "championship", "changes", "super-league",
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
	gHallFromMenu  = false;

	/*	The Amiga puts the game-type menu up first and only then asks for a name.		*/
	MenuScreensGoto(MS_MAIN);
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

/*	The 'Race Time:' / 'Best Lap :' strip along the bottom of the fixture screen, on its		*/
/*	own green band (TEXT.5ec92's two strings, drawn by R.5f074).  Zero prints as the			*/
/*	original's row of dashes, which is what a race that has not been run yet shows.			*/
/*	The band is embossed rather than flat: lit along its top and left edges and shaded		*/
/*	along the bottom and right, which is what gives it the raised look on the Amiga.			*/
static const AmigaPen kTimesPanelPaper  = {  85, 170,  68 };
static const AmigaPen kTimesPanelLight  = { 145, 215, 120 };
static const AmigaPen kTimesPanelShadow = {  40, 105,  35 };

#define TIMES_PANEL_Y	171
#define TIMES_PANEL_H	24
#define TIMES_ROW_1		22
#define TIMES_ROW_2		23

static void DrawTimesPanel( double raceTime, double lapTime )
	{
	const int x = AMIGA_PANEL_X;
	const int y = TIMES_PANEL_Y;
	const int w = AMIGA_PANEL_W;
	const int h = TIMES_PANEL_H;

	AmigaMenuFillRect(x, y, w, h, kTimesPanelPaper);

	AmigaMenuFillRect(x, y,         w, 1, kTimesPanelLight);	// top
	AmigaMenuFillRect(x, y,         1, h, kTimesPanelLight);	// left
	AmigaMenuFillRect(x, y + h - 1, w, 1, kTimesPanelShadow);	// bottom
	AmigaMenuFillRect(x + w - 1, y, 1, h, kTimesPanelShadow);	// right

	char buffer[32];
	AmigaMenuSetInk(AMIGA_INK_BLACK);
	FormatTime(buffer, sizeof(buffer), raceTime);
	AmigaMenuPrintF(6, TIMES_ROW_1, "Race Time: %s", buffer);
	FormatTime(buffer, sizeof(buffer), lapTime);
	AmigaMenuPrintF(6, TIMES_ROW_2, "Best Lap : %s", buffer);
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

/*	Each portrait cell is a face with a name plate across the bottom, inside the cell's		*/
/*	white frame: nine rows starting 44 down the cell, spanning the 72 pixels between the		*/
/*	frame's two edges.  Same rectangle every driver's plate occupies in the artwork.			*/
#define HEAD_NAME_H		9
#define HEAD_NAME_Y		44
#define HEAD_NAME_X		1
#define HEAD_NAME_W		(HEAD_CELL_W - 2)

/*	Room for ten characters at the font's seven-pixel advance.  The baked-in names go up		*/
/*	to twelve ("Jumpin' Jack") because the artist drew them in a six-wide hand-lettered		*/
/*	face; printing with the game font, ten is what fits.									*/
#define HEAD_NAME_MAX_CHARS		(HEAD_NAME_W / AMIGA_CHAR_WIDTH)

/*	The name plate colours, sampled from the artwork: near-black paper, near-white ink -		*/
/*	the same two the artist's own plates use, so the player's cell reads as one of the set.	*/
static const AmigaPen kNamePlateInk   = { 242, 242, 242 };
static const AmigaPen kNamePlatePaper = {  10,  10,  10 };

/*	Centre a string over a portrait cell rather than on the character grid - the cells are	*/
/*	74 pixels wide against a 7 pixel advance, so nothing lines up with a column.				*/
static void PrintCentredOn( int x, int width, int y, const char *text )
	{
	const int textX = x + (width - (int)strlen(text) * AMIGA_CHAR_WIDTH) / 2;
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintPixel(textX, y, text);
	}

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
		AmigaMenuFillRect(x + HEAD_NAME_X, y + HEAD_NAME_Y, HEAD_NAME_W, HEAD_NAME_H,
						  kNamePlatePaper);

		/*	Centred on the plate, and clipped to what the plate will hold rather than	*/
		/*	allowed to run out over the frame and into the cell next door.				*/
		char name[HEAD_NAME_MAX_CHARS + 1];
		snprintf(name, sizeof(name), "%s", LeagueDriverName(PLAYER_DRIVER));

		const int len   = (int)strlen(name);
		const int textX = x + HEAD_NAME_X + (HEAD_NAME_W - len * AMIGA_CHAR_WIDTH) / 2;

		AmigaMenuSetInk(kNamePlateInk);
		AmigaMenuPrintPixel(textX, y + HEAD_NAME_Y + 1, name);
		AmigaMenuSetInk(AMIGA_INK_TEXT);
		}
	}

/*	R.645c6 - "clear.menu, then print the player's division at 31,15,9" - runs at the top	*/
/*	of every screen the league puts up, which is why 'DIVISION 4' sits above the SELECT		*/
/*	menu as well as above a fixture.  The super league form is 31,12,9,'SUPER DIVISION '.	*/
static void DrawDivisionHeading( void )
	{
	const int division = LeaguePlayerDivision();

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	if (gLeagueSuperLeague)
		AmigaMenuPrintF(12, HEADING_ROW, "SUPER DIVISION %d", LeagueDivisionNumber(division));
	else
		AmigaMenuPrintF(15, HEADING_ROW, "DIVISION %d", LeagueDivisionNumber(division));
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
	/*	get.players.name (~line 8220): 'NAME?', then fill.bar with B.1bb16=1 - entry 1 of	*/
	/*	menu.bar.positions, which is row 16 - and finally underline.text in pen 10 from	*/
	/*	X 106 to X 190 at Y 133.  There is no 'press any key' prompt on this screen: the	*/
	/*	name field is the whole of it.													*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(18, 14, "NAME?");				// 31,18,14,'NAME?'

	#define NAME_ROW	16
	AmigaMenuBar(NAME_ROW, false);

	/*	input.name sets the print column to 14 and prints '>', then the name runs on		*/
	/*	from there.																		*/
	AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
	AmigaMenuPrintF(14, NAME_ROW, ">%s", gNameBuffer);
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	/*	The underline is 14 pixels down the bar on the Amiga (bar at row*8-9, line at Y	*/
	/*	133), so hang it off the bar rather than off Y=133 directly - AmigaMenuBar sits	*/
	/*	four pixels lower than the address arithmetic implies, and the line has to move	*/
	/*	with it.  X 106..190 is twelve characters, the original's max.name.length.		*/
	AmigaMenuFillRect(106, AmigaMenuBarY(NAME_ROW) + 14, 190 - 106 + 1, 1, AMIGA_INK_GREEN);
	}

static void DrawMainMenu( void )
	{
	/*	main.menu.selection passes d1=16, d2=2 to get.main.menu.selection, which picks		*/
	/*	entries 16..18 of TAB.5bcd0 - offsets $0a/$1f/$71 into main.game.selection.text.		*/
	/*	'Enter another driver' and 'Continue' are the multiplayer sub-menu, not this one.	*/
	static const char *entries[3] =
		{
		"Single Player League",
		"Multiplayer",
		"Computer Link"
		};
	DrawMenu(entries, 3, gSelection);
	}

static void DrawSelectMenu( void )
	{
	/*	R.5baea's menu, entries 0..2 of TAB.5bcd0 - offsets $ec/$0a/$14 into TEXT.5a69a.	*/
	/*	Load/Save/Replay ($2c) is the next entry in the table; the disassembly's count		*/
	/*	stops short of it, but the port has the screen so it stays on the menu.				*/
	static const char *entries[4] =
		{
		"Hall of Fame",
		"Practise",
		"Start the Racing Season",
		"Load/Save/Replay"
		};
	DrawDivisionHeading();
	DrawMenu(entries, 4, gSelection);
	}

/*	Not in the original: there, the Super League is something you are given after winning		*/
/*	Division I (league.offset, see LeagueEndSeason) and never something you ask for.  This	*/
/*	screen sits between 'Start the Racing Season' and the first fixture and lets the season	*/
/*	be run either way - the flag it sets is the same one, so everything downstream (engine	*/
/*	power and boost in Car_Behaviour, opponent speeds in Opponent_Behaviour, the cockpit		*/
/*	artwork in Car.cpp, the SUPER DIVISION heading) follows on its own.						*/
static void DrawLeagueChoice( void )
	{
	static const char *entries[2] =
		{
		"League",
		"Super League"
		};
	DrawDivisionHeading();
	DrawMenu(entries, 2, gSelection);
	}

/*	R.58888, "display opponents": the twelve drivers as one full-screen picture, four		*/
/*	divisions across with each division's two tracks under it.  The 68k unpacked the		*/
/*	people bitmap over the whole screen and then printed the twelve names from the current	*/
/*	ladder into it, so a driver who has been promoted past you shows up in his new			*/
/*	division.  Bitmap/heads.png already carries the artwork's own names, so only the cells	*/
/*	whose driver has moved need repainting - plus the player's, which is blank in the		*/
/*	original and scribbled over in this artwork.											*/
static void DrawOpponents( void )
	{
	AmigaMenuClear(AMIGA_INK_BLACK);
	AmigaMenuBlit("heads.png", 0, 0);

	for (int position = 0; position < NUM_LEAGUE_DRIVERS; position++)
		{
		const int driver = gLeagueLadder[position];
		if ((driver == position) && (driver != PLAYER_DRIVER))
			continue;						// the baked-in name is still the right one

		const int x = kHeadCellX[position / DRIVERS_PER_DIVISION];
		const int y = kHeadCellY[position % DRIVERS_PER_DIVISION];

		AmigaMenuFillRect(x, y + HEAD_NAME_Y, HEAD_CELL_W, HEAD_NAME_H, kNamePlatePaper);

		const char *name  = LeagueDriverName(driver);
		const int   textX = x + (HEAD_CELL_W - (int)strlen(name) * AMIGA_CHAR_WIDTH) / 2;

		AmigaMenuSetInk(kNamePlateInk);
		AmigaMenuPrintPixel(textX, y + HEAD_NAME_Y + 1, name);
		}

	/*	No prompt: the bottom of the picture is the divisions' track lists, and the		*/
	/*	original just sat on wait.for.fire here.										*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
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

	DrawDivisionHeading();

	/*	The three drivers, with their portraits.  Three 74-wide cells fill all but two	*/
	/*	pixels of the 224-wide panel, so they sit edge to edge and their own white		*/
	/*	frames do the separating.														*/
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = gLeagueLadder[first + i];
		DrawPortrait(driver, AMIGA_PANEL_X + 1 + i * HEAD_CELL_W, AMIGA_PANEL_Y + 26);
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

	/*	R.58e7a's pre-race branch: the division heading, then R.58e30 with d0=11 for		*/
	/*	'RACE  n of m' on row 11, the two heads, 'Track:  The ...' on row 20 (R.5eae0	*/
	/*	with d2=20) and the empty race-time panel underneath.							*/
	DrawDivisionHeading();
	AmigaMenuPrintF(14, RACE_ROW, "RACE  %d of %d", gLeagueRace + 1, RACES_PER_SEASON);

	/*	The two drivers head to head, with ' V ' in the gutter between them.				*/
	/*	R.58914 draws B.1ca27 first and B.1ca28 second - the opponent on the left and	*/
	/*	the player on the right, which is the way round the Amiga screen shots show.		*/
	/*	Sixteen pixels in from each end of the panel, which leaves the ' V ' gutter		*/
	/*	between them - the spacing the Amiga screen shots show.							*/
	#define FIXTURE_HEAD_INSET	16
	const int portraitY = AMIGA_PANEL_Y + 46;
	const int leftX     = AMIGA_PANEL_X + FIXTURE_HEAD_INSET;
	const int rightX    = AMIGA_PANEL_X + AMIGA_PANEL_W - FIXTURE_HEAD_INSET - HEAD_CELL_W;

	DrawPortrait(fixture->opponent, leftX,  portraitY);
	DrawPortrait(PLAYER_DRIVER,     rightX, portraitY);

	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintPixel((leftX + HEAD_CELL_W + rightX) / 2 - (AMIGA_CHAR_WIDTH / 2),
						portraitY + (HEAD_CELL_H / 2) - (AMIGA_CHAR_HEIGHT / 2), "V");
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	AmigaMenuPrintF(6, 20, "Track:  The %s", kTrackNames[fixture->trackID]);

	/*	The times the race is about to fill in, dashed out until it has.					*/
	DrawTimesPanel(0.0, 0.0);
	}

/*	The two race-end pictures, shown between the chequered flag and the RESULT screen: the	*/
/*	crowd cheering the winner in, or watching you trail past.  Like the opponents ladder		*/
/*	they are full-screen artwork rather than something inside the menu panel, so they paint	*/
/*	their own background and print nothing over the top.									*/
static void DrawRacePicture( bool won )
	{
	AmigaMenuClear(AMIGA_INK_BLACK);
	AmigaMenuBlit(won ? "racewin.png" : "racelost.png", 0, 0);
	}

/*	'New track records', shown straight after the race picture when the run just beaten a	*/
/*	track record - the track it was set on, then whichever of the two records fell, each		*/
/*	with the driver who now holds it.  The two lines share one bar: the bar is 17 pixels		*/
/*	tall against an 8 pixel row, so bars on two adjacent rows run together into the single	*/
/*	amber slab the original shows.															*/
#define RECORD_ROW_1	18
#define RECORD_ROW_2	19

static void DrawTrackRecord( void )
	{
	DrawDivisionHeading();

	char heading[64];
	snprintf(heading, sizeof(heading), "Track:   The %s", kTrackNames[gNewRecordTrack]);
	AmigaMenuPrintCentred(11, heading);

	AmigaMenuPrintCentred(15, "New track records");

	/*	One slab across both rows.  Two AmigaMenuBar calls would very nearly do it, but	*/
	/*	each bar carries its own white top rule and black bottom rule, so the second		*/
	/*	one's rules would be drawn straight through the middle of the block.  Fill it	*/
	/*	by hand instead: the same bevel, top and bottom of the pair rather than of each.	*/
	const int barTop    = AmigaMenuBarY(RECORD_ROW_1);
	const int barBottom = AmigaMenuBarY(RECORD_ROW_2) + MENU_BAR_HEIGHT - 1;

	AmigaMenuFillRect(AMIGA_PANEL_X, barTop, AMIGA_PANEL_W, barBottom - barTop + 1,
					  AMIGA_BAR_SELECTED);
	AmigaMenuFillRect(AMIGA_PANEL_X, barTop,    AMIGA_PANEL_W, 1, AMIGA_INK_WHITE);
	AmigaMenuFillRect(AMIGA_PANEL_X, barBottom, AMIGA_PANEL_W, 1, AMIGA_INK_BLACK);

	char time[24];
	AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);

	/*	Both lines are always printed - the screen is about the track, not just the one	*/
	/*	line that moved - but only a record this race actually took shows a driver, so	*/
	/*	an unbeaten record still reads as the row of dashes it does everywhere else.		*/
	FormatTime(time, sizeof(time), gRecordRace[gNewRecordTrack]);
	AmigaMenuPrintF(5, RECORD_ROW_1, "Race Time: %-10.10s %s",
					(gRecordRaceDriver[gNewRecordTrack] >= 0)
						? LeagueDriverName(gRecordRaceDriver[gNewRecordTrack]) : "",
					time);

	FormatTime(time, sizeof(time), gRecordLap[gNewRecordTrack]);
	AmigaMenuPrintF(5, RECORD_ROW_2, "Best Lap : %-10.10s %s",
					(gRecordLapDriver[gNewRecordTrack] >= 0)
						? LeagueDriverName(gRecordLapDriver[gNewRecordTrack]) : "",
					time);

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

/*	The RESULT screen: the fixture that was just run on its bar, then the two portraits		*/
/*	the points went to - the race winner under 'Winner 2pts' and the fastest lap under		*/
/*	'Best Lap 1pt', which is where the scoring in LeagueRecordResult comes from.				*/
/*																							*/
/*	The rows are worked back from the portraits: a 54-pixel cell sitting on the bottom of	*/
/*	the panel (which ends at y 195) starts at y 138, so the last row of text clear of it		*/
/*	is row 16 (y 128..136) and everything above steps up from there.							*/
#define RESULT_RACE_ROW			10
#define RESULT_FIXTURE_ROW		12
#define RESULT_HEADING_ROW		14
#define RESULT_LABEL_Y			128
#define RESULT_PORTRAIT_Y		138

static void DrawResult( void )
	{
	/*	gLeagueRace has already advanced past the race just run.						*/
	const int index = (gLeagueRace > 0) ? (gLeagueRace - 1) : 0;
	const LeagueFixture *fixture = &gLeagueFixtures[index];

	DrawDivisionHeading();
	AmigaMenuPrintF(14, RESULT_RACE_ROW, "RACE  %d of %d", index + 1, RACES_PER_SEASON);

	/*	'The X V The Y' from league.text, on a bar of its own.							*/
	AmigaMenuBar(RESULT_FIXTURE_ROW, false);
	char line[64];
	snprintf(line, sizeof(line), "%s V %s",
			 LeagueDriverName(fixture->opponent), LeagueDriverName(PLAYER_DRIVER));
	AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
	AmigaMenuPrintCentred(RESULT_FIXTURE_ROW, line);
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	AmigaMenuPrintCentred(RESULT_HEADING_ROW, "RESULT");

	const int winner  = fixture->won     ? PLAYER_DRIVER : fixture->opponent;
	const int fastest = fixture->bestLap ? PLAYER_DRIVER : fixture->opponent;

	/*	The same two positions the fixture screen puts its heads in, so the result reads	*/
	/*	as the fixture screen filled in rather than as a new layout.						*/
	const int leftX  = AMIGA_PANEL_X + FIXTURE_HEAD_INSET;
	const int rightX = AMIGA_PANEL_X + AMIGA_PANEL_W - FIXTURE_HEAD_INSET - HEAD_CELL_W;

	PrintCentredOn(leftX,  HEAD_CELL_W, RESULT_LABEL_Y, "Winner 2pts");
	PrintCentredOn(rightX, HEAD_CELL_W, RESULT_LABEL_Y, "Best Lap 1pt");

	DrawPortrait(winner,  leftX,  RESULT_PORTRAIT_Y);
	DrawPortrait(fastest, rightX, RESULT_PORTRAIT_Y);
	}

/*	The division table: the three drivers in finishing order as portraits, with their		*/
/*	figures in a column under each.  Three 74-wide cells fill all but two pixels of the		*/
/*	224-wide panel, the same edge-to-edge run the DIVISION screen uses.						*/
#define TABLE_PORTRAIT_Y	98
#define TABLE_FIGURES_Y		(TABLE_PORTRAIT_Y + HEAD_CELL_H + 4)
#define TABLE_FIGURE_STEP	8
/*	The figures are laid out label-left / value-right inside each 74-wide column.  The		*/
/*	value is pulled further in than the label so that a column's value and the next			*/
/*	column's label do not end up touching - the columns sit edge to edge, so without that	*/
/*	gutter the row reads as one run of text.												*/
#define TABLE_LABEL_INSET	3
#define TABLE_VALUE_INSET	9

static void DrawTable( void )
	{
	static const char *kPlaces[DRIVERS_PER_DIVISION] = { "First", "Second", "Third" };

	const int division = LeaguePlayerDivision();
	const int first    = ((NUM_DIVISIONS - 1) - division) * DRIVERS_PER_DIVISION;

	/*	Order the division by points, then wins, then fastest laps - the ladder order is	*/
	/*	last season's, and this screen is about this season's standings.					*/
	int order[DRIVERS_PER_DIVISION];
	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		order[i] = gLeagueLadder[first + i];

	for (int i = 0; i < DRIVERS_PER_DIVISION - 1; i++)
		for (int j = i + 1; j < DRIVERS_PER_DIVISION; j++)
			{
			const LeagueDriver *a = &gLeagueTable[order[i]];
			const LeagueDriver *b = &gLeagueTable[order[j]];
			if ((b->points > a->points) ||
				((b->points == a->points) && (b->wins > a->wins)) ||
				((b->points == a->points) && (b->wins == a->wins) && (b->bestLaps > a->bestLaps)))
				{
				const int swap = order[i];
				order[i] = order[j];
				order[j] = swap;
				}
			}

	DrawDivisionHeading();

	for (int i = 0; i < DRIVERS_PER_DIVISION; i++)
		{
		const int driver = order[i];
		const int x      = AMIGA_PANEL_X + 1 + i * HEAD_CELL_W;
		const LeagueDriver *rec = &gLeagueTable[driver];

		PrintCentredOn(x, HEAD_CELL_W, 11 * AMIGA_CHAR_HEIGHT, kPlaces[i]);
		DrawPortrait(driver, x, TABLE_PORTRAIT_Y);

		static const char *kLabels[4] = { "Raced", "Wins", "Laps", "Points" };
		const int values[4] = { rec->raced, rec->wins, rec->bestLaps, rec->points };

		for (int line = 0; line < 4; line++)
			{
			const int y = TABLE_FIGURES_Y + line * TABLE_FIGURE_STEP;
			char value[8];
			snprintf(value, sizeof(value), "%d", values[line]);

			AmigaMenuPrintPixel(x + TABLE_LABEL_INSET, y, kLabels[line]);
			AmigaMenuPrintPixel(x + HEAD_CELL_W - TABLE_VALUE_INSET
									- (int)strlen(value) * AMIGA_CHAR_WIDTH,
								y, value);
			}
		}

	AmigaMenuSetInk(AMIGA_INK_TEXT);
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
	DrawDivisionHeading();
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
	/*	Some screens are drawn over the whole display rather than inside the menu frame:	*/
	/*	the opponents picture, the two race-end pictures and the Hall of Fame.  They all	*/
	/*	paint their own background.														*/
	if (gScreen == MS_OPPONENTS)
		{
		DrawOpponents();
		return;
		}

	if ((gScreen == MS_RACE_WIN) || (gScreen == MS_RACE_LOST))
		{
		DrawRacePicture(gScreen == MS_RACE_WIN);
		return;
		}

	AmigaMenuFrame();

	switch (gScreen)
		{
		case MS_NAME_ENTRY:		DrawNameEntry();		break;
		case MS_OPPONENTS:								break;	// handled above
		case MS_RACE_WIN:								break;	// handled above
		case MS_RACE_LOST:								break;	// handled above
		case MS_MAIN:			DrawMainMenu();			break;
		case MS_SELECT:			DrawSelectMenu();		break;
		case MS_LEAGUE_CHOICE:	DrawLeagueChoice();		break;
		case MS_PRACTISE_TRACK:	DrawPractiseTracks();	break;
		case MS_DIVISION:		DrawDivision();			break;
		case MS_FIXTURE:		DrawFixture();			break;
		case MS_TRACK_RECORD:	DrawTrackRecord();		break;
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
		case MS_MAIN:			return 3;
		case MS_SELECT:			return 4;
		case MS_LEAGUE_CHOICE:	return 2;
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

	/*	mgs9 stores the fixture's opponent in opponents.ID before previewing the road,	*/
	/*	so the driver you race is the one the fixture screen just put up against you -	*/
	/*	with that driver's own attributes, not a stranger's.							*/
	SetRaceOpponent(fixture->opponent);

	if (MenuStartTrack(fixture->trackID))
		gActive = false;
	}

static void HandleNameEntry( int key )
	{
	if ((key == KEY_ENTER) && (gNameLength > 0))
		{
		LeagueNewCareer(gNameBuffer);

		/*	main.game.selection: get.players.name, jsr R.648b2 'display opponents',		*/
		/*	then jsr R.5baea, the SELECT menu.  Nothing else in between.					*/
		MenuScreensGoto(MS_OPPONENTS);
		return;
		}

	if ((key == KEY_BACK) && (gNameLength > 0))
		{
		gNameBuffer[--gNameLength] = '\0';
		return;
		}

	/*	The original's name entry accepts the printable set its font can draw.  It		*/
	/*	stopped at twelve characters; stop at ten instead, which is what the name plate	*/
	/*	under the portrait holds, so a name is never taken and then shown truncated.		*/
	if ((key >= ' ') && (key < 127) && (gNameLength < HEAD_NAME_MAX_CHARS))
		{
		/*	input.name only forces upper case when do.key.validation is set, and on		*/
		/*	this screen it isn't - the name goes in as typed.							*/
		gNameBuffer[gNameLength++] = (char)key;
		gNameBuffer[gNameLength]   = '\0';
		}
	}

static void ActivateMain( void )
	{
	switch (gSelection)
		{
		case 0:												// Single Player League
			gNameBuffer[0] = '\0';
			gNameLength    = 0;
			MenuScreensGoto(MS_NAME_ENTRY);
			break;

		/*	Neither of these is offered by this port, so both land on the apology.		*/
		case 1:	MenuScreensGoto(MS_LINK);			break;	// Multiplayer
		case 2:	MenuScreensGoto(MS_LINK);			break;	// Computer Link
		}
	}

static void ActivateSelect( void )
	{
	switch (gSelection)
		{
		case 0:												// Hall of Fame
			gHallFromMenu = true;
			MenuScreensGoto(MS_HALL_OF_FAME);
			break;
		case 1:	MenuScreensGoto(MS_PRACTISE_TRACK);	break;	// Practise
		case 2:												// Start the Racing Season
			/*	mgs9 goes straight to the fixture screen (R.64664) and from there	*/
			/*	into set.and.preview.road - there is no division screen in between.	*/
			/*	The port asks which league first; the fixture screen follows from	*/
			/*	there.  Start on whichever the season is already set to, so a player	*/
			/*	who earned the Super League keeps it by just pressing RETURN.		*/
			MenuScreensGoto(MS_LEAGUE_CHOICE);
			gSelection = gLeagueSuperLeague ? 1 : 0;
			break;
		case 3:	MenuScreensGoto(MS_LOADSAVE);		break;	// Load/Save/Replay
		}
	}

static void ActivateLeagueChoice( void )
	{
	/*	Same flag the original sets on winning Division I, so the season runs exactly as	*/
	/*	a promoted one would - only the choosing is new.  It is set before				*/
	/*	LeagueStartSeason so the division heading and fixtures are drawn in the right	*/
	/*	league from the first screen on.												*/
	gLeagueSuperLeague = (gSelection == 1);

	LeagueStartSeason();
	MenuScreensGoto(MS_FIXTURE);
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
		case MS_LEAGUE_CHOICE:	ActivateLeagueChoice();	break;

		/*	Shown after the name has been entered, and again once a season is over.	*/
		case MS_OPPONENTS:		MenuScreensGoto(MS_SELECT);		break;

		case MS_PRACTISE_TRACK:
			gRaceIsLeague = false;
			gRaceTrack    = gSelection;
			SetRaceOpponent(NO_OPPONENT);		// practise is solo
			if (MenuStartTrack(gSelection))
				gActive = false;
			break;

		case MS_DIVISION:		MenuScreensGoto(MS_SELECT);		break;

		case MS_FIXTURE:		StartLeagueRace();				break;

		/*	The race-end picture is a pause on the way to the result, not a screen	*/
		/*	with anything to choose on it.											*/
		case MS_RACE_WIN:
		case MS_RACE_LOST:
			/*	The record screen only appears when the race actually beat one.	*/
			MenuScreensGoto((gNewRecordRace || gNewRecordLap) ? MS_TRACK_RECORD
															  : MS_RESULT);
			break;

		case MS_TRACK_RECORD:
			gNewRecordRace = false;
			gNewRecordLap  = false;
			MenuScreensGoto(gRecordScreenReturn);
			break;

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
		case MS_HALL_OF_FAME:
			if (gHallFromMenu)
				{
				gHallFromMenu = false;
				MenuScreensGoto(MS_SELECT);
				}
			else
				/*	mgsd: R.646d6 (changes), R.5933a, then R.648b2 - the ladder	*/
				/*	picture again, with the new order - and back to the menu.	*/
				MenuScreensGoto(MS_OPPONENTS);
			break;

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

	/*	Note what this run took before handing the times to the record table, so the		*/
	/*	'New track records' screen knows whether it has anything to say.					*/
	gNewRecordTrack  = gRaceTrack;
	gNewRecordLap    = (playerLapTime  > 0.0) && ((gRecordLap[gRaceTrack]  == 0.0) ||
												  (playerLapTime  < gRecordLap[gRaceTrack]));
	gNewRecordRace   = (playerRaceTime > 0.0) && ((gRecordRace[gRaceTrack] == 0.0) ||
												  (playerRaceTime < gRecordRace[gRaceTrack]));

	MenuScreensRecordTimes(gRaceTrack, PLAYER_DRIVER, playerLapTime, playerRaceTime);

	if (!gRaceIsLeague)
		{
		/*	A practise run still counts for the Hall of Fame, and still gets told		*/
		/*	when it has set a record - there is just no result to score behind it.		*/
		gRecordScreenReturn = MS_SELECT;
		MenuScreensGoto((gNewRecordRace || gNewRecordLap) ? MS_TRACK_RECORD : MS_SELECT);
		return;
		}

	gRecordScreenReturn = MS_RESULT;

	LeagueRecordResult(playerWon, playerBestLap);
	gRaceIsLeague = false;

	/*	The picture comes first and the RESULT screen behind it, so you see how the race	*/
	/*	went before you are told what it was worth.										*/
	MenuScreensGoto(playerWon ? MS_RACE_WIN : MS_RACE_LOST);
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
