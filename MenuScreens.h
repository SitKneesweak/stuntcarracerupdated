/*	======================================================================================= */
/*	File:			MenuScreens.h															*/
/*																							*/
/*	Description:	The Amiga menu flow: which screen is up, what it draws, and what the		*/
/*					keys do.  Rendering primitives live in AmigaMenu.*, the season model		*/
/*					in League.*; this file is the state machine that joins them and hands	*/
/*					control over to the game when a race starts.								*/
/*	======================================================================================= */

#ifndef __MENUSCREENS_H_
#define __MENUSCREENS_H_

#include "dx_linux.h"

enum MenuScreenType
	{
	MS_NAME_ENTRY = 0,	// 'NAME?'
	MS_OPPONENTS,		// the twelve drivers, four divisions across - drawn full screen
	MS_MAIN,			// 'SELECT' - Single Player League / Multiplayer / ...
	MS_SELECT,			// 'SELECT' - Practise / Start the Racing Season / Load-Save-Replay
	MS_LEAGUE_CHOICE,	// 'SELECT' - League or Super League, on the way into a season
	MS_PRACTISE_TRACK,	// 'TIME TRIAL' - pick any of the eight tracks, raced solo
	MS_SINGLE_RACE,		// 'SINGLE RACE' - pick track, opponent and league, then race
	MS_DIVISION,		// 'DIVISION n' - the division's drivers and tracks (not in the flow)
	MS_FIXTURE,			// 'RACE n' - The X V The Y, and the track
	MS_RACE_WIN,		// the winner's picture - drawn full screen
	MS_RACE_LOST,		// the loser's picture - drawn full screen
	MS_TRACK_RECORD,	// 'New track records' - only when the race just beat one
	MS_RESULT,			// 'RESULT' - the two portraits, Winner 2pts / Best Lap 1pt
	MS_TABLE,			// the division table, as three portraits with their figures
	MS_CHAMPIONSHIP,	// 'DRIVERS CHAMPIONSHIP' - the whole ladder
	MS_CHANGES,			// 'DIVISION n CHANGES' - promotion and relegation
	MS_SUPER_LEAGUE,	// promotion to the SUPER LEAGUE
	MS_HALL_OF_FAME,	// track records - drawn full screen, not inside the menu panel
	MS_LOADSAVE,		// 'Load/Save/Replay', which this port does not implement
	MS_MP_MENU,			// 'Multiplayer' - Host a Race / Join a Race
	MS_MP_TRACK,		// the host picks the track before hosting
	MS_MP_JOIN,			// type the host's address
	MS_MP_WAIT,			// hosting / connecting, and what went wrong if it did
	MS_TUNING			// opponent speed tuning - not on any menu, see DrawTuning
	};

/*	Enter the menus, at the name entry screen.  Safe to call more than once.					*/
void MenuScreensInit( void );

/*	Jump straight to a screen (used when coming back out of a race).							*/
void MenuScreensGoto( MenuScreenType screen );

/*	True while the menus own the display and the keyboard.									*/
bool MenuScreensActive( void );

/*	Hand the display back to the 3D scene without going through a menu selection.			*/
void MenuScreensDeactivate( void );

/*	Feed a key press in (an SDL keysym under linux, a virtual key under Windows).			*/
void MenuScreensKey( int key );

/*	Escape: back out one screen.  False when the screen up is one of the two with nothing	*/
/*	above it, where escape still means "leave the game".										*/
bool MenuScreensBack( void );

/*	Called once per rendered frame while the menus are up.  Only the multiplayer screens
	need it: a session that is listening or connecting has to be pumped whether or not the
	player is touching the keyboard, and it is the poll that starts the race when the
	handshake lands.  `now` is DXUTGetTime().											*/
void MenuScreensTick( double now );

/*	The --net-host / --net-join command-line shortcuts.  They open the same session the
	Multiplayer menu would, without the menu-driving, which is what makes a two-machine
	session one command per machine.  -1 and "" mean "not asked for".					*/
extern int  gNetAutoHostTrack;
extern char gNetAutoJoinAddress[64];

/*	Draw the current screen.																*/
void MenuScreensRender( IDirect3DDevice9 *pd3dDevice );

/*	Called when a race ends, so a league race can be scored and the result shown.  For a		*/
/*	practise run this just returns to the SELECT menu.  The two times are the player's		*/
/*	fastest lap and total race time, which feed the Hall of Fame.							*/
void MenuScreensRaceFinished( bool playerWon, bool playerBestLap,
							  double playerLapTime, double playerRaceTime );

/*	True if the race that is running (or about to) counts towards the season.				*/
bool MenuScreensRaceIsLeague( void );

/*	Give up on the race in progress without scoring it, and come back to the menus - a		*/
/*	league race returns to its fixture screen so it can be run again.						*/
void MenuScreensAbandonRace( void );

/*	Record a lap and/or race time against a track for the Hall of Fame.  The league the		*/
/*	race ran under is part of the record: the Super League is a different car over the		*/
/*	same track, so its times are kept apart from the league's rather than beating them.		*/
void MenuScreensRecordTimes( int trackID, bool superLeague, int driver,
							 double lapTime, double raceTime );

/*	The Hall of Fame table, for Profile.cpp to read and write.  A zero time means the		*/
/*	record is unset and its driver is -1; that pair round-trips through the profile as		*/
/*	it stands.  Setting a record here overwrites rather than compares, so a loaded			*/
/*	profile restores exactly what was saved.  league is 0 for the league proper and 1		*/
/*	for the Super League.																	*/
#define MENU_RECORD_TRACKS	8
#define MENU_RECORD_LEAGUES	2
void MenuScreensGetRecord( int trackID, int league, double *lapTime, int *lapDriver,
													double *raceTime, int *raceDriver );
void MenuScreensSetRecord( int trackID, int league, double lapTime, int lapDriver,
													double raceTime, int raceDriver );
void MenuScreensClearRecords( void );

/*	--- Provided by StuntCarRacer.cpp ---------------------------------------------------	*/

/*	Load a track and drop into its preview, the way the old track menu did.  Returns false	*/
/*	if the track could not be converted or its vertex buffer built.							*/
bool MenuStartTrack( int trackID );

/*	--- Read by StuntCarRacer.cpp -------------------------------------------------------	*/

/*	Which league the race that is starting is run under.  Normally the career's own			*/
/*	(gLeagueSuperLeague), but the Single Race screen picks it per race, and everything		*/
/*	that reads bSuperLeague - engine power, boost, car and track colours, and the opponent	*/
/*	speed table - follows from what MenuStartTrack sets out of this.							*/
bool MenuScreensRaceSuperLeague( void );

#endif //__MENUSCREENS_H_
