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
#include "Profile.h"
#include "Track.h"
#include "Opponent_Behaviour.h"
#include "Det_Rand.h"
/*	Net_Game.h includes only <cstdint> - no socket header ever reaches dx_linux.h.		*/
#include "Net_Game.h"

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

/*	The league the race that is starting runs under.  A season race and a time trial take	*/
/*	the career's own; the Single Race screen sets it to whichever it was asked for, which	*/
/*	is why this is a variable and not just gLeagueSuperLeague read again at the far end.		*/
static bool			  gRaceSuperLeague = false;

/*	The Single Race screen's three choices, kept between visits so a track being tuned		*/
/*	can be raced again without setting it up each time.										*/
static int	gSingleTrack    = 0;
static long	gSingleOpponent = RANDOM_OPPONENT;
static bool	gSingleSuper    = false;
static int	gSingleField    = 0;		// which of the three rows left/right is changing

/*	gSingleTrack when the Track row is on 'Random': the track is drawn when the race			*/
/*	starts, so the same setting gives a different one each time.								*/
#define RANDOM_TRACK	(-1)

/*	The league a time trial runs under.  The career's own until the screen is asked for		*/
/*	the other one - the Super League car is a different car to learn a track in, and		*/
/*	there was no way to drive it without starting a season in it.							*/
static bool	gPractiseSuper  = false;

/*	True when the race that just started came off the Single Race screen, so its result		*/
/*	comes back here rather than to the SELECT menu.											*/
static bool	gRaceIsSingle   = false;

/*	Where the tuning bench returns to - it is reachable from more than one screen now.		*/
static MenuScreenType gTuningReturn = MS_SELECT;

static char gNameBuffer[16] = "";
static int  gNameLength     = 0;

/*	The host address typed on the join screen, and whatever went wrong last time it was	*/
/*	tried.  Long enough for a bracketed IPv6 literal or a hostname.						*/
static char gAddressBuffer[64] = "";
static int  gAddressLength     = 0;
static char gAddressError[64]  = "";

static char gPromoted[16]  = "";
static char gRelegated[16] = "";

/*	True when the Hall of Fame was picked off the menu, so ESC/fire returns to the menu		*/
/*	rather than continuing the end-of-season run of screens.								*/
static bool gHallFromMenu = false;

/*	True when the ladder picture is being shown on the way into a season, so fire carries	*/
/*	on to the first fixture rather than dropping back to the menu.  The picture belongs		*/
/*	where it tells you something: after the league has been chosen, showing who is in which	*/
/*	division for the season about to be raced.												*/
static bool gOppToFixture = false;

/*	Which league's records the Hall of Fame is showing.  The two leagues are two different	*/
/*	races over the same eight tracks - the Super League car is faster and the road is the	*/
/*	same length - so a single table would have the Super League quietly erase every league	*/
/*	record the moment the career was promoted.  Two tables, one page each.					*/
static bool gHallSuper = false;

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
static bool			 gNewRecordSuper  = false;
static MenuScreenType gRecordScreenReturn = MS_SELECT;

/*	Track records, kept for the Hall of Fame.  Zero means "not set yet", which the original	*/
/*	shows as a row of dashes ('------------' in TEXT.5ec48).  The first index is the			*/
/*	league the time was set in: [0] is the league proper, [1] the Super League.				*/
static double gRecordLap[2][8]  = {{ 0 }};
static double gRecordRace[2][8] = {{ 0 }};
static int    gRecordLapDriver[2][8];
static int    gRecordRaceDriver[2][8];

static void MenuScreensDraw( void );

bool MenuScreensActive( void ) { return gActive; }
void MenuScreensDeactivate( void ) { gActive = false; }
bool MenuScreensRaceIsLeague( void ) { return gRaceIsLeague; }
bool MenuScreensRaceSuperLeague( void ) { return gRaceSuperLeague; }

void MenuScreensGoto( MenuScreenType screen )
	{
	/*	The Hall of Fame opens on the league the career is in, which is the one whose		*/
	/*	records the player just changed; left/right goes to the other.					*/
	if (screen == MS_HALL_OF_FAME)
		gHallSuper = gLeagueSuperLeague;

	gScreen    = screen;
	gSelection = 0;
	gActive    = true;
	}

/*	Write the career out, but only once there is a driver to write: before the NAME?		*/
/*	screen has been through, the league tables hold nobody's season and saving them would	*/
/*	conjure a profile out of a menu the player only looked at.								*/
static void SaveCareer( void )
	{
	if (ProfileExists())
		ProfileSave();
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
		"name-entry", "opponents", "main", "select", "league-choice", "practise-track",
		"single-race", "division", "fixture",
		"race-win", "race-lost", "track-record", "result", "table", "championship", "changes", "super-league",
		"hall-of-fame", "load-save", "mp-menu", "mp-track", "mp-join", "mp-wait", "tuning"
		};

	/*	A season with two races run, so the tables have something in them.				*/
	LeagueNewCareer("ANDREW");
	LeagueRecordResult(true,  true);
	LeagueRecordResult(false, true);

	snprintf(gPromoted,  sizeof(gPromoted),  "%s", LeagueDriverName(PLAYER_DRIVER));
	snprintf(gRelegated, sizeof(gRelegated), "%s", LeagueDriverName(10));
	gLastLapTime  = 27.31;
	gLastRaceTime = 112.64;
	MenuScreensRecordTimes(0, false, PLAYER_DRIVER, 27.31, 112.64);
	MenuScreensRecordTimes(5, false, 3, 31.09, 128.44);
	MenuScreensRecordTimes(0, true,  PLAYER_DRIVER, 24.87, 101.22);

	snprintf(gNameBuffer, sizeof(gNameBuffer), "ANDREW");
	gNameLength = 6;
	gSelection  = 1;

	/*	The join screen needs something in its field, and the waiting screen something	*/
	/*	in its status line, or two of the dumps come out blank.							*/
	snprintf(gAddressBuffer, sizeof(gAddressBuffer), "192.168.1.42");
	gAddressLength = (int)strlen(gAddressBuffer);

	for (int i = 0; i <= (int)MS_TUNING; i++)
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
	MenuScreensClearRecords();
	gNameBuffer[0] = '\0';
	gNameLength    = 0;

	const char *dumpPrefix = getenv("SCR_MENU_DUMPALL");
	if (dumpPrefix)
		MenuScreensDumpAll(dumpPrefix);

	gNameBuffer[0] = '\0';
	gNameLength    = 0;
	gHallFromMenu  = false;
	gOppToFixture  = false;

	/*	Pick up the saved career, if there is one.  The dump-all run above leaves a		*/
	/*	made-up season behind, so load after it rather than before: whichever runs		*/
	/*	last is what the menus show, and that should be the player's own career.			*/
	if (!ProfileLoad())
		{
		LeagueNewCareer("");
		MenuScreensClearRecords();
		}

	/*	The opponent tuning bench keeps its own file, outside the career: a number		*/
	/*	being hunted should still be there tomorrow, and should not go when a new		*/
	/*	driver wipes the profile.														*/
	OpponentTuningLoad();

	/*	Until a race picks otherwise, the league is the career's own.					*/
	gRaceSuperLeague = gLeagueSuperLeague;
	gSingleSuper     = gLeagueSuperLeague;
	gPractiseSuper   = gLeagueSuperLeague;

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
		/*	menu.bar.positions only has room for the four entries the original's		*/
		/*	menus had.  A fifth needs the entries closer together: two rows apart,	*/
		/*	which is what the track lists already use and is the tightest the		*/
		/*	17-pixel selection bar allows.											*/
		const int row = (count <= MAX_MENU_ENTRIES) ? kMenuRows[i] : (13 + i * 2);

		AmigaMenuBar(row, i == selected);

		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%d. %s", i + 1, entries[i]);
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

/*	Whose career this is.  Not in the original - there was no profile to be uncertain		*/
/*	about, since the name was typed afresh every time the machine was switched on.  Now		*/
/*	that it persists, the menus have to say who you are.  It goes on the 'SELECT' row at		*/
/*	the left edge of the panel: the heading sits at column 17 and the name is capped at		*/
/*	twelve characters by the entry screen, so the two never meet.							*/
static void DrawDriverName( void )
	{
	if (!gPlayerName[0])
		return;

	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintF(MENU_ENTRY_COLUMN, SELECT_ROW, "%.12s", gPlayerName);
	AmigaMenuSetInk(AMIGA_INK_TEXT);
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
	/*	Only two entries now.  The Amiga's third was 'Computer Link', its null-modem		*/
	/*	serial link-up; this port replaces it with a network game reached from			*/
	/*	'Multiplayer', so an entry that could only ever apologise has been dropped.		*/
	static const char *entries[2] =
		{
		"Single Player",
		"Multiplayer"
		};
	DrawMenu(entries, 2, gSelection);
	DrawDriverName();
	}

static void DrawSelectMenu( void )
	{
	/*	R.5baea's menu, entries 0..2 of TAB.5bcd0 - offsets $ec/$0a/$14 into TEXT.5a69a.	*/
	/*	Load/Save/Replay ($2c) is the next entry in the table; the disassembly's count		*/
	/*	stops short of it, but the port has the screen so it stays on the menu.				*/
	/*	'Practise' has become two entries.  The original's practise was a solo run round	*/
	/*	a track, which is a time trial in everything but name, so it keeps the behaviour	*/
	/*	and gets the name; 'Single Race' is the one that was missing - a one-off race		*/
	/*	against a driver you choose, in whichever league you choose, outside the season.	*/
	static const char *entries[5] =
		{
		"Time Trial",
		"Single Race",
		"Start the Racing Season",
		"Hall of Fame",
		"Load/Save/Replay"
		};
	DrawDivisionHeading();
	DrawMenu(entries, 5, gSelection);
	DrawDriverName();
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
	DrawDriverName();
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

/*	The name to print for a single race's opponent choice.									*/
static const char *SingleOpponentName( long opponent )
	{
	if (opponent == NO_OPPONENT)		return "Nobody (solo)";
	if (opponent == RANDOM_OPPONENT)	return "Random";
	return LeagueDriverName((int)opponent);
	}

/*	Not the original's: a one-off race, set up in full - track, driver and league - and		*/
/*	run outside the season, so a track can be raced against the opposition you want to		*/
/*	see rather than whoever the next fixture happens to name.  It is also what makes the		*/
/*	tuning bench usable: tune a number, drop back here, race that same track and league		*/
/*	again.  Three rows, up/down to pick one and left/right to change it.						*/
static void DrawSingleRace( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(15, HEADING_ROW, "SINGLE RACE");

	static const char *labels[3] = { "Track", "Against", "League" };
	char values[3][32];
	if (gSingleTrack == RANDOM_TRACK)
		snprintf(values[0], sizeof(values[0]), "Random");
	else
		snprintf(values[0], sizeof(values[0]), "The %s", kTrackNames[gSingleTrack]);
	snprintf(values[1], sizeof(values[1]), "%s", SingleOpponentName(gSingleOpponent));
	snprintf(values[2], sizeof(values[2]), "%s", gSingleSuper ? "SUPER LEAGUE" : "League");

	for (int i = 0; i < 3; i++)
		{
		const int row = 12 + i * 3;
		AmigaMenuBar(row, i == gSingleField);

		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		/*	An arrow on the row the left/right keys are on, so which line they		*/
		/*	change is not something to work out from the bar colour alone.  '>' and	*/
		/*	not '<': the Amiga font has no '<' glyph.								*/
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%-8.8s %c %s",
						labels[i],
						(i == gSingleField) ? '>' : ' ',
						values[i]);
		}

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(21, "Left/Right changes the row");
	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintCentred(23, "RETURN races.  T tunes it.");
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	}

static void DrawPractiseTracks( void )
	{
	/*	No 'TIME TRIAL' heading, though the screen could do with one: eight entries two	*/
	/*	rows apart already run from row 9 - the first row clear of the logo - down to	*/
	/*	row 23, and a bar any lower would hang over the bottom of the panel.  The menu	*/
	/*	entry that leads here is what names it.											*/

	/*	Two rows per entry: the selection bar is 17 pixels tall, so entries any closer	*/
	/*	than two rows would be swallowed by their neighbour's bar.						*/
	for (int i = 0; i < 8; i++)
		{
		const int row = 9 + i * 2;
		AmigaMenuBar(row, i == gSelection);
		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%d. The %s", i + 1, kTrackNames[i]);

		/*	Which league the trial runs in, on the selected row and nowhere else:	*/
		/*	the eight entries already fill every row the panel has, so there is no	*/
		/*	line to put it on of its own.  Right-aligned against the last column		*/
		/*	that fits inside the panel (column 35), which the longest track name -	*/
		/*	'2. The STEPPING STONES', ending at column 26 - stays clear of.  The		*/
		/*	'>' is the Single Race screen's arrow, marking the row left/right change.*/
		if (i == gSelection)
			{
			const char *league = gPractiseSuper ? "SUPER" : "LEAGUE";
			AmigaMenuPrintF(36 - 2 - (int)strlen(league), row, "> %s", league);
			}
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
	const int league = gNewRecordSuper ? 1 : 0;

	FormatTime(time, sizeof(time), gRecordRace[league][gNewRecordTrack]);
	AmigaMenuPrintF(5, RECORD_ROW_1, "Race Time: %-10.10s %s",
					(gRecordRaceDriver[league][gNewRecordTrack] >= 0)
						? LeagueDriverName(gRecordRaceDriver[league][gNewRecordTrack]) : "",
					time);

	FormatTime(time, sizeof(time), gRecordLap[league][gNewRecordTrack]);
	AmigaMenuPrintF(5, RECORD_ROW_2, "Best Lap : %-10.10s %s",
					(gRecordLapDriver[league][gNewRecordTrack] >= 0)
						? LeagueDriverName(gRecordLapDriver[league][gNewRecordTrack]) : "",
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
	/*	The panel grey the menus sit on, so the two full-screen tables belong to the		*/
	/*	same game as the screens either side of them rather than being white text on		*/
	/*	black.  There is no frame art out here - the frame's hole is panel-sized - so		*/
	/*	the paper covers the screen and the rows are the menus' own bars.				*/
	AmigaMenuClear(AMIGA_PAPER);

	/*	Amber for the heading: the menus' one bright colour, and the only thing on		*/
	/*	this screen that is not either a bar or a record.								*/
	AmigaMenuSetInk(AMIGA_BAR_SELECTED);
	AmigaMenuPrintAt(16, 1, "HALL of FAME");					// 31,16,1,'HALL of FAME'

	/*	Which of the two tables is up.  The original prints 'SUPER LEAGUE' here when		*/
	/*	the career is in it; this page can be either, so it always says which.			*/
	/*																					*/
	/*	One row up from the original's, along with everything else above the table:		*/
	/*	the headings now sit on a bar, and a bar starting on row 6 reaches up into row	*/
	/*	5's glyphs.  What that buys is a clear row 24 at the bottom for the prompt,		*/
	/*	which the old spacing ran into.													*/
	/*	Which league, and whose records these are, on one line - they are one fact		*/
	/*	between them, and putting them on separate rows crowded the title.  Centred by	*/
	/*	hand: AmigaMenuPrintCentred centres within the menu panel, and this screen is	*/
	/*	outside it.  No apostrophe in 'driver's' - the Amiga font has no glyph for one	*/
	/*	and draws a block.																*/
	char subtitle[64];
	if (gPlayerName[0])
		snprintf(subtitle, sizeof(subtitle), "%s records for %.12s",
				 gHallSuper ? "SUPER LEAGUE" : "LEAGUE", gPlayerName);
	else
		snprintf(subtitle, sizeof(subtitle), "%s records",
				 gHallSuper ? "SUPER LEAGUE" : "LEAGUE");

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt((45 - (int)strlen(subtitle)) / 2, 3, subtitle);

	/*	Say so rather than let a player wonder why a good lap never landed.  Amber is	*/
	/*	the palette's one bright ink, which is what a line like this wants.				*/
	if (OpponentTuningActive())
		{
		AmigaMenuSetInk(AMIGA_BAR_SELECTED);
		AmigaMenuPrintAt(0, 4, "Opponent tuning active - times not recorded");
		}

	/*	A bar under the headings, which tiles onto the top of the table below - and		*/
	/*	gives the red something light to sit on, which the panel grey does not.			*/
	AmigaMenuTableRow(6, AMIGA_BAR);

	AmigaMenuSetInk(AMIGA_INK_RED);
	/*	31,0,7,'TRACK  DRIVER   LAP-TIME    DRIVER  RACE-TIME'						*/
	AmigaMenuPrintAt(0, 6, "TRACK  DRIVER   LAP-TIME    DRIVER  RACE-TIME");

	const int league = gHallSuper ? 1 : 0;

	for (int track = 0; track < 8; track++)
		{
		char lap[24], race[24];
		FormatTime(lap,  sizeof(lap),  gRecordLap[league][track]);
		FormatTime(race, sizeof(race), gRecordRace[league][track]);

		const int lapDriver  = gRecordLapDriver[league][track];
		const int raceDriver = gRecordRaceDriver[league][track];

		/*	Amber where the player holds one of the two records, light grey where	*/
		/*	neither is theirs.  The menus use amber for the entry that matters, and	*/
		/*	on this screen what matters is which of these times you set yourself -	*/
		/*	which is otherwise buried in a column of driver names.					*/
		const bool mine = (lapDriver  == PLAYER_DRIVER) ||
						  (raceDriver == PLAYER_DRIVER);

		const int row = 8 + track * 2;
		AmigaMenuTableRow(row, mine ? AMIGA_BAR_SELECTED : AMIGA_BAR);

		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(0, row, "%-6.6s %-8.8s %-11.11s %-7.7s %-9.9s",
						kTrackShortNames[track],
						(lapDriver  >= 0) ? LeagueDriverName(lapDriver)  : "",
						lap,
						(raceDriver >= 0) ? LeagueDriverName(raceDriver) : "",
						race);
		}

	/*	Both keys on one line under the table.  That the other league is one keypress	*/
	/*	away is worth saying - nothing else on this screen suggests a second page - and	*/
	/*	saying it down here leaves the top of the screen to the records.				*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(0, 24, "RETURN continues   Left/Right swaps league");
	}

/*	======================================================================================= */
/*	Opponent speed tuning																	*/
/*																							*/
/*	Not one of the original's screens and not meant for players: it is reached by pressing	*/
/*	T on the SELECT menu and appears nowhere in the flow, because what it edits is the		*/
/*	table the game's difficulty comes out of (see Opponent_Behaviour.h).  It exists so		*/
/*	that a track which drives wrong - the Super League Little Ramp being the case it was		*/
/*	written for - can be found by racing it, changing a number and racing it again.			*/
/*																							*/
/*	Drawn full screen like the Hall of Fame: two leagues side by side needs more than the	*/
/*	28 characters the menu panel allows.													*/
/*	======================================================================================= */

/*	Which league column the left/right keys are editing.  Not gLeagueSuperLeague: this		*/
/*	screen edits both leagues whichever one the career is currently in.						*/
static bool gTuningColumnSuper = true;

/*	What the next race off the screen the bench was opened from will be: the league it		*/
/*	will run under and the track it will be on.  This is the pair the tuning table has to	*/
/*	be read against, and telling the tester which they are is most of what makes the		*/
/*	screen usable - editing the LEAGUE column and then racing the Super League is the one	*/
/*	mistake it invites.																		*/
static void NextRaceIntent( bool *super, int *track )
	{
	switch (gTuningReturn)
		{
		case MS_SINGLE_RACE:
			*super = gSingleSuper;
			/*	A random track is not known until RETURN, so the bench opens on	*/
			/*	the one that was last raced instead.								*/
			*track = (gSingleTrack == RANDOM_TRACK) ? gRaceTrack : gSingleTrack;
			break;

		case MS_FIXTURE:
			{
			const LeagueFixture *fixture = LeagueCurrentFixture();
			*super = gLeagueSuperLeague;
			*track = (fixture != NULL) ? fixture->trackID : 0;
			}
			break;

		default:
			{
			/*	Off the SELECT menu there is no track chosen yet, so the season's	*/
			/*	next fixture is the best answer available - it is where RETURN on	*/
			/*	that menu leads.													*/
			const LeagueFixture *fixture = LeagueCurrentFixture();
			*super = gLeagueSuperLeague;
			*track = (fixture != NULL) ? fixture->trackID : 0;
			}
			break;
		}
	}

/*	The grid this screen is laid out on.  Full screen, so 45 characters across and 25 rows	*/
/*	down, the same as the Hall of Fame - which is what the spacing below is copied from.		*/
/*																							*/
/*		 0         1         2         3         4											*/
/*		 0123456789012345678901234567890123456789012345										*/
/*	 6	 TRACK            LEAGUE        SUPER												*/
/*	 8	>LITTLE RAMP      +00  66/61   >-24  4e/49											*/
/*																							*/
/*	The marker column in front of each field is what carries the arrows, so a cell only		*/
/*	ever moves between ' ' and '>' - nothing shifts sideways as the cursor is walked			*/
/*	around, which is what made the old layout jump.											*/
#define TUNE_COL_TRACK		0		// marker, then 15 characters of track name
#define TUNE_COL_LEAGUE		17		// marker, then '+00  66/61'
#define TUNE_COL_SUPER		31

#define TUNE_ROW_FIRST		8		// first track
#define TUNE_ROW_STEP		2		// every other row, as the Hall of Fame does

static void DrawTuningCell( int column, int row, int track, bool super, bool live )
	{
	const long offset = OpponentTuningGet(track, super);

	/*	An arrow for the cell the keys are on.  The row it is on is already an amber		*/
	/*	bar, so the arrow is what says which of the row's two cells is being edited.		*/
	/*	'>' rather than brackets: the Amiga font has no '[' or ']' - the slots those		*/
	/*	characters fall in hold something else entirely, and printing them draws			*/
	/*	rubbish.																		*/
	const bool cursor = (gTuningColumnSuper == super) && (track == gSelection);

	AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
	AmigaMenuPrintF(column, row, "%c", cursor ? '>' : ' ');

	/*	Green for the column the next race actually reads and black for the other, so	*/
	/*	a glance down the table says which numbers are the ones in play.  Both are dark	*/
	/*	on the light bar, which is the menus' own arrangement - the bar carries the		*/
	/*	emphasis and the ink stays readable.											*/
	AmigaMenuSetInk(live ? AMIGA_INK_GREEN : AMIGA_INK_BAR_TEXT);

	/*	The two numbers behind the offset are what is actually being tuned - the base	*/
	/*	the opponent's top speed is built from and the base of its per-piece target -	*/
	/*	so show them rather than make the tester hold the table in their head.			*/
	AmigaMenuPrintF(column + 1, row, "%+03d  %02x/%02x",
					(int)offset,
					(unsigned)OpponentTuningBase(track, 8,  super),
					(unsigned)OpponentTuningBase(track, 24, super));
	}

static void DrawTuning( void )
	{
	/*	The menus' panel grey and the menus' bars, as the Hall of Fame now uses - this	*/
	/*	is the other full-screen table and the two should not look like different games.	*/
	AmigaMenuClear(AMIGA_PAPER);

	/*	The inks, which the Hall of Fame now follows too: amber on the paper for the		*/
	/*	title and for anything the reader has to notice, white on the paper for			*/
	/*	ordinary text, red for column headings - which sit on a bar, because red on		*/
	/*	the panel grey is too dark to read a line of - black on a bar for the data,		*/
	/*	and green on a bar for what the next race will read.							*/
	AmigaMenuSetInk(AMIGA_BAR_SELECTED);
	AmigaMenuPrintAt(15, 1, "OPPONENT TUNING");			// centred on 45 columns
	AmigaMenuPrintAt(3, 2, "Testing only - kept - times not recorded");

	/*	The trap this screen sets is editing one league and racing the other, which		*/
	/*	looks exactly like tuning that does nothing.  So say outright what the next		*/
	/*	race will read - the track it will be on and the league it will run under - and	*/
	/*	mark that column and that row in the table below.								*/
	bool raceSuper = false;
	int  raceTrack = 0;
	NextRaceIntent(&raceSuper, &raceTrack);

	/*	No comma: the Amiga font's comma slot holds one of the junk glyphs and prints	*/
	/*	as a stray block, so the dash does the separating on this screen.				*/
	/*																					*/
	/*	White rather than the green used inside the table: the panel grey out here is	*/
	/*	dark, and the menu green is dark too - it only reads against a bar.				*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintF(0, 4, "Next race: %s - %s",
					kTrackNames[raceTrack], raceSuper ? "SUPER LEAGUE" : "LEAGUE");

	/*	Row 5 stays blank, so the table below reads as its own block.					*/

	/*	The headings get a bar of their own, which tiles onto the top of the table and	*/
	/*	gives the dark menu green something light to sit on - on the panel grey it is	*/
	/*	nearly invisible.																*/
	AmigaMenuTableRow(6, AMIGA_BAR);

	/*	Headings sit one character in from each field, over the numbers rather than		*/
	/*	over the marker column, so the live column's own arrow has somewhere to go.		*/
	/*	'LIVE' fits between the two columns and says in a word what the arrow and the	*/
	/*	colour say twice over - this is the one the next race will read.				*/
	AmigaMenuSetInk(AMIGA_INK_RED);
	AmigaMenuPrintAt(TUNE_COL_TRACK  + 1, 6, "TRACK");
	AmigaMenuPrintAt(TUNE_COL_LEAGUE + 1, 6, "LEAGUE");
	AmigaMenuPrintAt(TUNE_COL_SUPER  + 1, 6, "SUPER");

	AmigaMenuSetInk(AMIGA_INK_GREEN);
	const int liveColumn = raceSuper ? TUNE_COL_SUPER : TUNE_COL_LEAGUE;
	AmigaMenuPrintAt(liveColumn, 6, ">");
	AmigaMenuPrintAt(liveColumn + 8, 6, "LIVE");

	for (int track = 0; track < 8; track++)
		{
		const int row = TUNE_ROW_FIRST + track * TUNE_ROW_STEP;

		/*	Amber for the row the keys are on, exactly as a menu marks its selected	*/
		/*	entry, and light grey for the rest.										*/
		AmigaMenuTableRow(row, (track == gSelection) ? AMIGA_BAR_SELECTED : AMIGA_BAR);

		/*	An arrow against the track the next race is on.  Full names, not the		*/
		/*	Hall of Fame's short forms: two columns of numbers leave room for them,	*/
		/*	and 'LITTLE RAMP' saves working out what 'LITTLE' was short for.			*/
		AmigaMenuSetInk((track == raceTrack) ? AMIGA_INK_GREEN : AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(TUNE_COL_TRACK, row, "%c%-15.15s",
						(track == raceTrack) ? '>' : ' ', kTrackNames[track]);

		DrawTuningCell(TUNE_COL_LEAGUE, row, track, false, !raceSuper);
		DrawTuningCell(TUNE_COL_SUPER,  row, track, true,   raceSuper);
		}

	/*	Row 23 is left blank: the table wants a gap under it before the key list, and	*/
	/*	which column is being edited is already on the screen twice over - the cursor	*/
	/*	arrow sits in it, and the green arrow on row 6 says which one the race reads.	*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(0, 24, "LR +-1   -/= +-8   TAB column   R reset");
	}

/*	The original's Load/Save/Replay submenu.  Loading and saving are no longer something	*/
/*	to ask for - the career is written out after every race and read back at start-up -		*/
/*	so what is left for this screen is the one decision that is still the player's: keep	*/
/*	this driver, or start again as someone else.  Replays are still not in the port.		*/
static void DrawLoadSave( void )
	{
	static const char *entries[2] =
		{
		"Continue this career",
		"New driver"
		};

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(12, 9, "Load/Save/Replay");

	char driver[32];
	snprintf(driver, sizeof(driver), "Driver: %.12s", gPlayerName);
	AmigaMenuPrintCentred(11, driver);
	AmigaMenuPrintCentred(12, "This career saves itself.");

	for (int i = 0; i < 2; i++)
		{
		const int row = 14 + i * 3;
		AmigaMenuBar(row, i == gSelection);
		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%d. %s", i + 1, entries[i]);
		}

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(21, "New driver erases this one.");
	}

/*	======================================================================================= */
/*	Multiplayer																				*/
/*																							*/
/*	The Amiga's own two-player mode was 'Computer Link', a null-modem cable between two		*/
/*	Amigas at 9600 baud (establish.computer.link, StuntCarRacer.s:4037).  That is gone and	*/
/*	is not coming back, so the entry is gone with it and 'Multiplayer' leads here instead:	*/
/*	the same one-on-one race over a network, with one machine hosting and the other typing	*/
/*	in its address.																			*/
/*	======================================================================================= */

static void DrawMultiplayerMenu( void )
	{
	static const char *entries[2] =
		{
		"Host a Race",
		"Join a Race"
		};
	DrawMenu(entries, 2, gSelection);

	/*	The Amiga font's punctuation slots below '.' hold leftover graphics rather than	*/
	/*	glyphs - an apostrophe or a comma prints as a block of noise (see AmigaFont.h,	*/
	/*	only the decimal point, minus and underscore are patched in).  All the text on	*/
	/*	these screens sticks to letters, digits, dots and dashes for that reason.		*/
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(23, "One hosts - the other joins.");
	}

static void DrawMultiplayerTrack( void )
	{
	/*	Same eight-entry layout as the practise track list - the host is choosing the	*/
	/*	track for both players, so this is the last thing it does before opening a port.	*/
	for (int i = 0; i < 8; i++)
		{
		const int row = 9 + i * 2;
		AmigaMenuBar(row, i == gSelection);
		AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
		AmigaMenuPrintF(MENU_ENTRY_COLUMN, row, "%d. The %s", i + 1, kTrackNames[i]);
		}
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(24, "You are hosting - pick the track.");
	}

static void DrawMultiplayerJoin( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(14, 11, "Join a Race");
	AmigaMenuPrintCentred(13, "Type the address to connect to.");

	#define ADDRESS_ROW	16
	AmigaMenuBar(ADDRESS_ROW, false);
	AmigaMenuSetInk(AMIGA_INK_BAR_TEXT);
	AmigaMenuPrintF(AMIGA_PANEL_COL0 + 1, ADDRESS_ROW, ">%s", gAddressBuffer);
	AmigaMenuSetInk(AMIGA_INK_TEXT);

	AmigaMenuPrintCentred(20, "RETURN connects. ESC goes back.");

	if (gAddressError[0] != '\0')
		{
		AmigaMenuSetInk(AMIGA_INK_RED);
		AmigaMenuPrintCentred(22, gAddressError);
		AmigaMenuSetInk(AMIGA_INK_TEXT);
		}
	}

/*	Hosting or connecting.  There is nothing to choose here - the screen exists so the		*/
/*	player can see what the session is doing, and back out of it with ESC.					*/
static void DrawMultiplayerWait( void )
	{
	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintAt(14, 11, "Multiplayer");

	/*	The status line can run longer than the panel is wide, so wrap it by hand onto	*/
	/*	up to three rows rather than letting it disappear off the edge.					*/
	const char *s = scr::NetGameStatusLine();
	const int   width = 34;
	int row = 15;
	if (scr::NetGameFailed())
		AmigaMenuSetInk(AMIGA_INK_RED);

	while ((*s != '\0') && (row < 21))
		{
		char line[64];
		int take = (int)strlen(s);
		if (take > width)
			{
			/*	Break on the last space that fits, so words stay whole.			*/
			take = width;
			while ((take > 0) && (s[take] != ' '))
				--take;
			if (take == 0)
				take = width;
			}
		snprintf(line, sizeof(line), "%.*s", take, s);
		AmigaMenuPrintCentred(row++, line);

		s += take;
		while (*s == ' ')
			++s;
		}

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	AmigaMenuPrintCentred(23, "ESC to cancel.");
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

	/*	Full screen, for the same reason the Hall of Fame is - its table is wider than	*/
	/*	the menu panel.																	*/
	if (gScreen == MS_TUNING)
		{
		DrawTuning();
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
		case MS_SINGLE_RACE:	DrawSingleRace();		break;
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
		case MS_MP_MENU:		DrawMultiplayerMenu();	break;
		case MS_MP_TRACK:		DrawMultiplayerTrack();	break;
		case MS_MP_JOIN:		DrawMultiplayerJoin();	break;
		case MS_MP_WAIT:		DrawMultiplayerWait();	break;
		case MS_TUNING:									break;	// handled above
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

#ifdef SCR_PORTABLE
#define KEY_UP		SDLK_UP
#define KEY_DOWN	SDLK_DOWN
#define KEY_ENTER	SDLK_RETURN
#define KEY_BACK	SDLK_BACKSPACE
#define KEY_ESCAPE	SDLK_ESCAPE
#define KEY_LEFT	SDLK_LEFT
#define KEY_RIGHT	SDLK_RIGHT
#define KEY_TAB		SDLK_TAB
#else
#define KEY_UP		VK_UP
#define KEY_DOWN	VK_DOWN
#define KEY_ENTER	VK_RETURN
#define KEY_BACK	VK_BACK
#define KEY_ESCAPE	VK_ESCAPE
#define KEY_LEFT	VK_LEFT
#define KEY_RIGHT	VK_RIGHT
#define KEY_TAB		VK_TAB
#endif

/*	How many entries the current screen offers, for the up/down keys.						*/
static int EntryCount( void )
	{
	switch (gScreen)
		{
		/*	'Computer Link' is gone - see DrawMultiplayerMenu.						*/
		case MS_MAIN:			return 2;
		case MS_SELECT:			return 5;
		case MS_LEAGUE_CHOICE:	return 2;
		case MS_PRACTISE_TRACK:	return 8;
		case MS_TUNING:			return 8;	// up/down walk the tracks; see HandleTuning
		case MS_LOADSAVE:		return 2;
		case MS_MP_MENU:		return 2;
		case MS_MP_TRACK:		return 8;
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
	gRaceIsSingle = false;
	gRaceTrack    = fixture->trackID;

	/*	A season race is run under the career's own league.								*/
	gRaceSuperLeague = gLeagueSuperLeague;

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

		/*	A new driver starts with an empty Hall of Fame: the records belong to the	*/
		/*	career, not to the machine.  Written out straight away so the profile		*/
		/*	exists from the moment the name is entered, not only once a race is run.		*/
		MenuScreensClearRecords();
		ProfileSave();

		/*	main.game.selection: get.players.name, jsr R.648b2 'display opponents',		*/
		/*	then jsr R.5baea, the SELECT menu.  The port holds the ladder picture		*/
		/*	back until the season is actually being started - see ActivateLeagueChoice	*/
		/*	- so the name leads straight to the menu.									*/
		MenuScreensGoto(MS_SELECT);
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
			/*	A saved career skips the NAME? screen: the driver is already	*/
			/*	known, so go where the name entry would have gone - the		*/
			/*	SELECT menu.  'New driver' on the Load/Save screen is how you	*/
			/*	get the question back.											*/
			if (ProfileExists())
				{
				MenuScreensGoto(MS_SELECT);
				break;
				}

			gNameBuffer[0] = '\0';
			gNameLength    = 0;
			MenuScreensGoto(MS_NAME_ENTRY);
			break;

		case 1:	MenuScreensGoto(MS_MP_MENU);		break;	// Multiplayer
		}
	}

/*	======================================================================================= */
/*	Multiplayer input																		*/
/*	======================================================================================= */

/*	Both peers must draw the same opponent car, so the driver is fixed rather than rolled:	*/
/*	SetRaceOpponent(RANDOM_OPPONENT) would draw from the RNG on each machine separately,		*/
/*	before the shared seed is even in place.  It must also not be NO_OPPONENT - that is the	*/
/*	practise-run value, and the render path hides the opponent's car entirely when it is		*/
/*	set (StuntCarRacer.cpp, HideOpponentsCar).  The AI behind the car never runs; the		*/
/*	frame loop steps the remote player's physics into that slot instead.						*/
#define MP_OPPONENT_DRIVER	0

/*	The handshake has landed.  Drop into the agreed track exactly the way a league race		*/
/*	does, but with the opponent slot standing in for the other player.						*/
static void EnterMultiplayerRace( void )
	{
	gRaceIsLeague = false;
	gRaceIsSingle = false;
	gRaceTrack    = scr::NetGameTrack();

	/*	Both peers have to agree on the league, and nothing in the handshake carries it,	*/
	/*	so a network race is run under the career's own - as it always has been.			*/
	gRaceSuperLeague = gLeagueSuperLeague;

	SetRaceOpponent(MP_OPPONENT_DRIVER);

	if (MenuStartTrack(gRaceTrack))
		{
		/*	Same pair the simtrace path uses: while the menus are up OnFrameRender		*/
		/*	returns before the track-preview input is reached, so without the			*/
		/*	deactivate the race would never actually be entered.						*/
		gActive = false;
		}
	else
		{
		scr::NetGameCancel();
		MenuScreensGoto(MS_MP_MENU);
		}
	}

static void HandleAddressEntry( int key )
	{
	if (key == KEY_ENTER)
		{
		gAddressError[0] = '\0';
		if (scr::NetGameJoin(gAddressBuffer, DXUTGetTime()))
			MenuScreensGoto(MS_MP_WAIT);
		else
			snprintf(gAddressError, sizeof(gAddressError), "%s", scr::NetGameStatusLine());
		return;
		}

	if ((key == KEY_BACK) && (gAddressLength > 0))
		{
		gAddressBuffer[--gAddressLength] = '\0';
		return;
		}

	/*	Addresses are printable ASCII - digits, dots, colons for IPv6, letters and		*/
	/*	hyphens for a hostname.  Take the lot rather than validating here; the resolver	*/
	/*	is the thing that actually knows what is valid, and it reports back.				*/
	if ((key >= ' ') && (key < 127) &&
		(gAddressLength < (int)sizeof(gAddressBuffer) - 1))
		{
		gAddressBuffer[gAddressLength++] = (char)key;
		gAddressBuffer[gAddressLength]   = '\0';
		}
	}

static void ActivateMultiplayerMenu( void )
	{
	switch (gSelection)
		{
		case 0:												// Host a Race
			MenuScreensGoto(MS_MP_TRACK);
			break;

		case 1:												// Join a Race
			gAddressError[0] = '\0';
			MenuScreensGoto(MS_MP_JOIN);
			break;
		}
	}

int  gNetAutoHostTrack       = -1;
char gNetAutoJoinAddress[64] = "";

void MenuScreensTick( double now )
	{
	/*	--net-host / --net-join: open the session the menu would have opened, once.	*/
	static bool autoDone = false;
	if (!autoDone && !scr::NetGameSessionActive())
		{
		if (gNetAutoHostTrack >= 0)
			{
			autoDone = true;
			if (scr::NetGameHost(gNetAutoHostTrack, now))
				MenuScreensGoto(MS_MP_WAIT);
			}
		else if (gNetAutoJoinAddress[0] != '\0')
			{
			autoDone = true;
			snprintf(gAddressBuffer, sizeof(gAddressBuffer), "%s", gNetAutoJoinAddress);
			gAddressLength = (int)strlen(gAddressBuffer);
			if (scr::NetGameJoin(gAddressBuffer, now))
				MenuScreensGoto(MS_MP_WAIT);
			}
		}

	if (!scr::NetGameSessionActive())
		return;

	/*	Pump whether or not the waiting screen is up: the session also has to be kept	*/
	/*	alive across the frames between the handshake and the race starting.				*/
	if (scr::NetGamePoll(now) && (gScreen == MS_MP_WAIT))
		EnterMultiplayerRace();
	}

static void ActivateSelect( void )
	{
	switch (gSelection)
		{
		case 0:	MenuScreensGoto(MS_PRACTISE_TRACK);	break;	// Time Trial
		case 1:												// Single Race
			/*	The league starts on the career's own, which is what a player who	*/
			/*	just wants "this track again, properly" expects to get.				*/
			gSingleSuper = gLeagueSuperLeague;
			MenuScreensGoto(MS_SINGLE_RACE);
			gSingleField = 0;
			break;
		case 2:												// Start the Racing Season
			/*	mgs9 goes straight to the fixture screen (R.64664) and from there	*/
			/*	into set.and.preview.road - there is no division screen in between.	*/
			/*	The port asks which league first; the fixture screen follows from	*/
			/*	there.  Start on whichever the season is already set to, so a player	*/
			/*	who earned the Super League keeps it by just pressing RETURN.		*/
			/*																		*/
			/*	A season already part-run goes straight back to its next fixture:	*/
			/*	the league choice would call LeagueStartSeason and throw away the	*/
			/*	races already in the table.											*/
			if ((gLeagueRace > 0) && !LeagueSeasonComplete())
				{
				MenuScreensGoto(MS_FIXTURE);
				break;
				}

			MenuScreensGoto(MS_LEAGUE_CHOICE);
			gSelection = gLeagueSuperLeague ? 1 : 0;
			break;
		case 3:												// Hall of Fame
			gHallFromMenu = true;
			MenuScreensGoto(MS_HALL_OF_FAME);
			break;
		case 4:	MenuScreensGoto(MS_LOADSAVE);		break;	// Load/Save/Replay
		}
	}

/*	Start the race the Single Race screen has been set up for.  Not a league race: nothing	*/
/*	it does reaches the season table, and it comes back to this screen so the same race		*/
/*	can be run again with one number changed.												*/
static void StartSingleRace( void )
	{
	/*	Random settles on a track here rather than on the menu, so racing the same		*/
	/*	setting twice gives two different tracks.										*/
	const int track = (gSingleTrack == RANDOM_TRACK) ? (int)(SCR_Rand() % 8) : gSingleTrack;

	gRaceIsLeague    = false;
	gRaceIsSingle    = true;
	gRaceTrack       = track;
	gRaceSuperLeague = gSingleSuper;

	SetRaceOpponent(gSingleOpponent);

	if (MenuStartTrack(track))
		gActive = false;
	}

/*	The Single Race screen's keys: up/down pick a row, left/right change what is on it.		*/
static bool HandleSingleRace( int key )
	{
	int step = 0;

	switch (key)
		{
		case KEY_UP:
			gSingleField = (gSingleField + 2) % 3;
			return true;
		case KEY_DOWN:
			gSingleField = (gSingleField + 1) % 3;
			return true;
		case KEY_LEFT:		step = -1;	break;
		case KEY_RIGHT:		step = +1;	break;

		case KEY_ENTER:
		case ' ':
			StartSingleRace();
			return true;

		default:
			return false;
		}

	switch (gSingleField)
		{
		case 0:
			/*	The list runs Random and then the eight tracks, the same shape as	*/
			/*	the opponent row below it: RANDOM_TRACK is index 0 and track t is	*/
			/*	index t+1.  The track is drawn when RETURN is pressed, so leaving	*/
			/*	the row on Random and racing again gives a different one.			*/
			{
			int index = (gSingleTrack == RANDOM_TRACK) ? 0 : gSingleTrack + 1;
			index = (index + 9 + step) % 9;
			gSingleTrack = (index == 0) ? RANDOM_TRACK : index - 1;
			}
			break;

		case 1:
			{
			/*	The list runs Nobody, Random, then the eleven drivers, so all three	*/
			/*	kinds of race - solo, pot luck and a named driver - are one row.		*/
			int index = (gSingleOpponent == NO_OPPONENT)     ? 0
					  : (gSingleOpponent == RANDOM_OPPONENT) ? 1
															 : (int)gSingleOpponent + 2;
			const int count = NUM_LEAGUE_DRIVERS - 1 + 2;	// the player is not an opponent
			index = (index + count + step) % count;
			gSingleOpponent = (index == 0) ? NO_OPPONENT
							: (index == 1) ? RANDOM_OPPONENT
										   : (long)(index - 2);
			}
			break;

		case 2:
			gSingleSuper = !gSingleSuper;
			break;
		}
	return true;
	}

static void ActivateLeagueChoice( void )
	{
	/*	Same flag the original sets on winning Division I, so the season runs exactly as	*/
	/*	a promoted one would - only the choosing is new.  It is set before				*/
	/*	LeagueStartSeason so the division heading and fixtures are drawn in the right	*/
	/*	league from the first screen on.												*/
	gLeagueSuperLeague = (gSelection == 1);

	LeagueStartSeason();

	/*	Show the ladder before the first fixture: with the league settled and the season	*/
	/*	just laid out, this is the point at which who is in which division is worth		*/
	/*	looking at.  Fire carries on into the fixture.									*/
	gOppToFixture = true;
	MenuScreensGoto(MS_OPPONENTS);
	}

/*	The tuning screen's own keys.  True when the key has been dealt with; anything left		*/
/*	over falls through to the shared up/down handling, which walks the eight tracks.			*/
static bool HandleTuning( int key )
	{
	int step = 0;

	switch (key)
		{
		case KEY_LEFT:		step = -1;	break;
		case KEY_RIGHT:		step = +1;	break;

		/*	The coarse step is on the minus and equals keys, which are next to each	*/
		/*	other and - unlike the comma and full stop this used to use - can both be	*/
		/*	printed in the key list at the bottom: the Amiga font's comma slot holds	*/
		/*	a junk glyph.  '+' as well, for anyone who reaches for shift.				*/
		case '-':			step = -8;	break;
		case '=':
		case '+':			step = +8;	break;

		/*	Still accepted, just no longer advertised.								*/
		case ',':			step = -8;	break;
		case '.':			step = +8;	break;

		case KEY_TAB:
			gTuningColumnSuper = !gTuningColumnSuper;
			return true;

		case 'r':
		case 'R':
			OpponentTuningClear();
			return true;

		case KEY_ENTER:
		case ' ':
			MenuScreensGoto(gTuningReturn);
			return true;
		}

	if (step == 0)
		return false;

	OpponentTuningSet(gSelection, gTuningColumnSuper,
					  OpponentTuningGet(gSelection, gTuningColumnSuper) + step);
	return true;
	}

/*	======================================================================================= */
/*	Function:		MenuScreensBack															*/
/*																							*/
/*	Description:	Escape backs out one screen rather than quitting the game outright,		*/
/*					so every screen the flow can reach is reachable again without			*/
/*					restarting.  True when the key was dealt with; false only at the two		*/
/*					screens with nowhere left to go back to (NAME? and the top menu),		*/
/*					where the caller quits as the original always did.						*/
/*	======================================================================================= */

bool MenuScreensBack( void )
	{
	switch (gScreen)
		{
		case MS_NAME_ENTRY:
		case MS_MAIN:
			return false;						// nowhere above these: let the caller quit

		case MS_SELECT:
		case MS_MP_MENU:
			MenuScreensGoto(MS_MAIN);
			return true;

		case MS_TUNING:
			MenuScreensGoto(gTuningReturn);
			return true;

		case MS_LEAGUE_CHOICE:
		case MS_PRACTISE_TRACK:
		case MS_SINGLE_RACE:
		case MS_LOADSAVE:
		case MS_DIVISION:
		case MS_FIXTURE:		/*	the season keeps its place; SELECT returns to it	*/
			MenuScreensGoto(MS_SELECT);
			return true;

		case MS_HALL_OF_FAME:
			/*	Only when it was opened from the menu.  On the way out of a season	*/
			/*	it is part of the end-of-season run, which fire walks through.		*/
			if (gHallFromMenu)
				{
				gHallFromMenu = false;
				MenuScreensGoto(MS_SELECT);
				}
			return true;

		case MS_MP_TRACK:
		case MS_MP_JOIN:
			MenuScreensGoto(MS_MP_MENU);
			return true;

		case MS_MP_WAIT:
			scr::NetGameCancel();
			MenuScreensGoto(MS_MP_MENU);
			return true;

		default:
			/*	The result and end-of-season screens are a sequence, not a choice:	*/
			/*	swallow escape rather than letting it drop out of the game.			*/
			return true;
		}
	}

void MenuScreensKey( int key )
	{
	if (key == 0)
		return;

	/*	Escape means "back" on every screen, including the ones that read text.			*/
	if (key == KEY_ESCAPE)
		{
		MenuScreensBack();
		return;
		}

	if (gScreen == MS_NAME_ENTRY)
		{
		HandleNameEntry(key);
		return;
		}

	if (gScreen == MS_MP_JOIN)
		{
		HandleAddressEntry(key);
		return;
		}

	/*	The waiting screen has nothing to select - the only key that does anything is	*/
	/*	the one that gives up.															*/
	if (gScreen == MS_MP_WAIT)
		{
		if ((key == KEY_ENTER) || (key == ' '))
			{
			scr::NetGameCancel();
			MenuScreensGoto(MS_MP_MENU);
			}
		return;
		}

	if (gScreen == MS_TUNING)
		{
		if (HandleTuning(key))
			return;
		}

	if (gScreen == MS_SINGLE_RACE)
		{
		if (HandleSingleRace(key))
			return;
		}

	/*	Up and down walk the tracks on the time trial screen, so the league it runs		*/
	/*	under is on left/right - the only keys that screen has left.						*/
	if ((gScreen == MS_PRACTISE_TRACK) && ((key == KEY_LEFT) || (key == KEY_RIGHT)))
		{
		gPractiseSuper = !gPractiseSuper;
		return;
		}

	/*	The Hall of Fame is two tables, one per league, and left/right turns the page.	*/
	if ((gScreen == MS_HALL_OF_FAME) && ((key == KEY_LEFT) || (key == KEY_RIGHT)))
		{
		gHallSuper = !gHallSuper;
		return;
		}

	/*	T opens the opponent tuning bench.  Deliberately a key and not a menu entry - it	*/
	/*	is a developer's screen, and the menus it hangs off are the original's.  It is	*/
	/*	reachable from the Single Race screen as well as from SELECT, because that is	*/
	/*	the screen a tester goes back and forth to: set a number here, race it there.	*/
	if (((gScreen == MS_SELECT) || (gScreen == MS_SINGLE_RACE))
		&& ((key == 't') || (key == 'T')))
		{
		gTuningReturn = gScreen;
		MenuScreensGoto(MS_TUNING);

		/*	Open on the track and the league the next race will actually use, so		*/
		/*	the keys are on the live cell before anything is typed - editing the		*/
		/*	other league's column and seeing no change is the mistake this screen	*/
		/*	otherwise invites.														*/
		bool super = false;
		int  track = 0;
		NextRaceIntent(&super, &track);
		gTuningColumnSuper = super;
		gSelection         = track;
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

		/*	Shown on the way into a season, and again once one is over.				*/
		case MS_OPPONENTS:
			if (gOppToFixture)
				{
				gOppToFixture = false;
				MenuScreensGoto(MS_FIXTURE);
				}
			else
				MenuScreensGoto(MS_SELECT);
			break;

		case MS_PRACTISE_TRACK:
			gRaceIsLeague    = false;
			gRaceIsSingle    = false;
			gRaceTrack       = gSelection;
			gRaceSuperLeague = gPractiseSuper;
			SetRaceOpponent(NO_OPPONENT);		// a time trial is solo
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
			/*	gRecordScreenReturn is the RESULT screen for a league race and	*/
			/*	the menu for anything else - a single race has no season figures	*/
			/*	behind it to show.												*/
			MenuScreensGoto((gNewRecordRace || gNewRecordLap) ? MS_TRACK_RECORD
															  : gRecordScreenReturn);
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
			SaveCareer();
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

		case MS_LOADSAVE:
			if (gSelection == 1)
				{
				/*	Start again: throw the saved career away and go back to	*/
				/*	the NAME? screen through the main menu, which is where	*/
				/*	a driver with no profile starts.						*/
				ProfileDelete();
				LeagueNewCareer("");
				MenuScreensClearRecords();
				gNameBuffer[0] = '\0';
				gNameLength    = 0;
				MenuScreensGoto(MS_NAME_ENTRY);
				}
			else
				MenuScreensGoto(MS_SELECT);
			break;

		case MS_MP_MENU:		ActivateMultiplayerMenu();			break;

		case MS_MP_TRACK:
			/*	The host's last choice before it opens a port: track, seed and dt	*/
			/*	all go out in the handshake and the joiner adopts them wholesale.	*/
			if (scr::NetGameHost(gSelection, DXUTGetTime()))
				MenuScreensGoto(MS_MP_WAIT);
			break;

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

	/*	Quitting a race quits the session with it - the other machine is told rather		*/
	/*	than left stalled.  Done here rather than at the two 'M' key handlers in			*/
	/*	StuntCarRacer.cpp, which are separate switch statements hundreds of lines apart	*/
	/*	and easy to update only one of.													*/
	if (scr::NetGameSessionActive())
		{
		scr::NetGameCancel();
		MenuScreensGoto(MS_MAIN);
		return;
		}

	if (gRaceIsLeague)
		{
		gRaceIsLeague = false;
		MenuScreensGoto(MS_FIXTURE);		// the fixture stands, run it again
		}
	else if (gRaceIsSingle)
		{
		gRaceIsSingle = false;
		MenuScreensGoto(MS_SINGLE_RACE);	// back to the set-up, unchanged
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

	/*	A multiplayer race is over as far as the network is concerned: stop stepping and	*/
	/*	unlock the sim settings.  The session itself is dropped when the player leaves	*/
	/*	the result screens.																*/
	if (scr::NetGameSessionActive())
		scr::NetGameRaceEnded();

	/*	Note what this run took before handing the times to the record table, so the		*/
	/*	'New track records' screen knows whether it has anything to say.					*/
	/*	A race run against tuned opposition is not a race anyone can hold a record from,	*/
	/*	so the Hall of Fame ignores it entirely - no time taken, and nothing claimed on	*/
	/*	the record screen.  The season still scores: the tuning bench is for driving the	*/
	/*	same fixture over and over, and refusing to advance it would defeat that.		*/
	const bool tuned = OpponentTuningActive();

	/*	The record this race is measured against is the one from its own league.			*/
	const int raceLeague = gRaceSuperLeague ? 1 : 0;

	gNewRecordTrack  = gRaceTrack;
	gNewRecordSuper  = gRaceSuperLeague;
	gNewRecordLap    = !tuned && (playerLapTime  > 0.0) &&
					   ((gRecordLap[raceLeague][gRaceTrack]  == 0.0) ||
						(playerLapTime  < gRecordLap[raceLeague][gRaceTrack]));
	gNewRecordRace   = !tuned && (playerRaceTime > 0.0) &&
					   ((gRecordRace[raceLeague][gRaceTrack] == 0.0) ||
						(playerRaceTime < gRecordRace[raceLeague][gRaceTrack]));

	if (!tuned)
		MenuScreensRecordTimes(gRaceTrack, gRaceSuperLeague, PLAYER_DRIVER,
							   playerLapTime, playerRaceTime);

	if (!gRaceIsLeague)
		{
		/*	A time trial still counts for the Hall of Fame, and still gets told when		*/
		/*	it has set a record - there is just no result to score behind it.			*/
		SaveCareer();

		/*	A single race comes back to the screen it was set up on, so the same		*/
		/*	race can be run again with one thing changed.  It had an opponent, so		*/
		/*	it gets the winner's or loser's picture first - just not the RESULT			*/
		/*	screen, which is the season's.											*/
		const bool single    = gRaceIsSingle;
		gRaceIsSingle        = false;
		gRecordScreenReturn  = single ? MS_SINGLE_RACE : MS_SELECT;

		if (single && (gSingleOpponent != NO_OPPONENT))
			{
			MenuScreensGoto(playerWon ? MS_RACE_WIN : MS_RACE_LOST);
			return;
			}

		MenuScreensGoto((gNewRecordRace || gNewRecordLap) ? MS_TRACK_RECORD
														  : gRecordScreenReturn);
		return;
		}

	gRecordScreenReturn = MS_RESULT;

	LeagueRecordResult(playerWon, playerBestLap);
	gRaceIsLeague = false;

	/*	Saved here rather than at the end of the season, so a career put down halfway		*/
	/*	through picks up at the next fixture with the table as it stood.					*/
	SaveCareer();

	/*	The picture comes first and the RESULT screen behind it, so you see how the race	*/
	/*	went before you are told what it was worth.										*/
	MenuScreensGoto(playerWon ? MS_RACE_WIN : MS_RACE_LOST);
	}

/*	Record a lap or race time against a track, for the Hall of Fame.						*/
void MenuScreensRecordTimes( int trackID, bool superLeague, int driver,
							 double lapTime, double raceTime )
	{
	if ((trackID < 0) || (trackID >= 8))
		return;

	const int league = superLeague ? 1 : 0;

	if ((lapTime > 0.0) &&
		((gRecordLap[league][trackID] == 0.0) || (lapTime < gRecordLap[league][trackID])))
		{
		gRecordLap[league][trackID]       = lapTime;
		gRecordLapDriver[league][trackID] = driver;
		}

	if ((raceTime > 0.0) &&
		((gRecordRace[league][trackID] == 0.0) || (raceTime < gRecordRace[league][trackID])))
		{
		gRecordRace[league][trackID]       = raceTime;
		gRecordRaceDriver[league][trackID] = driver;
		}
	}

/*	The record table as the profile sees it - straight reads and writes, no comparing.		*/
void MenuScreensGetRecord( int trackID, int league, double *lapTime, int *lapDriver,
													double *raceTime, int *raceDriver )
	{
	if ((trackID < 0) || (trackID >= MENU_RECORD_TRACKS) ||
		(league  < 0) || (league  >= MENU_RECORD_LEAGUES))
		return;

	if (lapTime)    *lapTime    = gRecordLap[league][trackID];
	if (lapDriver)  *lapDriver  = gRecordLapDriver[league][trackID];
	if (raceTime)   *raceTime   = gRecordRace[league][trackID];
	if (raceDriver) *raceDriver = gRecordRaceDriver[league][trackID];
	}

void MenuScreensSetRecord( int trackID, int league, double lapTime, int lapDriver,
													double raceTime, int raceDriver )
	{
	if ((trackID < 0) || (trackID >= MENU_RECORD_TRACKS) ||
		(league  < 0) || (league  >= MENU_RECORD_LEAGUES))
		return;

	gRecordLap[league][trackID]        = lapTime;
	gRecordLapDriver[league][trackID]  = lapDriver;
	gRecordRace[league][trackID]       = raceTime;
	gRecordRaceDriver[league][trackID] = raceDriver;
	}

void MenuScreensClearRecords( void )
	{
	for (int league = 0; league < MENU_RECORD_LEAGUES; league++)
		for (int i = 0; i < MENU_RECORD_TRACKS; i++)
			{
			gRecordLap[league][i]        = 0.0;
			gRecordRace[league][i]       = 0.0;
			gRecordLapDriver[league][i]  = -1;
			gRecordRaceDriver[league][i] = -1;
			}
	}
