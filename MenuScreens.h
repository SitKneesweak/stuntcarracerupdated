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
	MS_PRACTISE_TRACK,	// pick any of the eight tracks
	MS_DIVISION,		// 'DIVISION n' - the division's drivers and tracks (not in the flow)
	MS_FIXTURE,			// 'RACE n' - The X V The Y, and the track
	MS_RESULT,			// 'RESULT' - Race Winner / Fastest Lap
	MS_TABLE,			// 'RESULTS TABLE'
	MS_CHAMPIONSHIP,	// 'DRIVERS CHAMPIONSHIP' - the whole ladder
	MS_CHANGES,			// 'DIVISION n CHANGES' - promotion and relegation
	MS_SUPER_LEAGUE,	// promotion to the SUPER LEAGUE
	MS_HALL_OF_FAME,	// track records - drawn full screen, not inside the menu panel
	MS_LOADSAVE,		// 'Load/Save/Replay', which this port does not implement
	MS_LINK			 	// the two-player serial link, which this port cannot offer
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

/*	Record a lap and/or race time against a track for the Hall of Fame.						*/
void MenuScreensRecordTimes( int trackID, int driver, double lapTime, double raceTime );

/*	--- Provided by StuntCarRacer.cpp ---------------------------------------------------	*/

/*	Load a track and drop into its preview, the way the old track menu did.  Returns false	*/
/*	if the track could not be converted or its vertex buffer built.							*/
bool MenuStartTrack( int trackID );

#endif //__MENUSCREENS_H_
