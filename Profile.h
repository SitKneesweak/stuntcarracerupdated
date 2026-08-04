/*	======================================================================================= */
/*	File:			Profile.h																*/
/*																							*/
/*	Description:	Career persistence - the one thing the Amiga's Load/Save/Replay menu		*/
/*					offered that this port never had.  A single driver profile holds the		*/
/*					name typed on the NAME? screen, the league ladder and season in			*/
/*					progress, and that driver's Hall of Fame records; it is written after	*/
/*					every race and read back at start-up, so quitting and coming back		*/
/*					continues where you left off.											*/
/*																							*/
/*					One profile only, and the records belong to it: the Hall of Fame is		*/
/*					that driver's, not the machine's.  Starting a new driver from the		*/
/*					Load/Save screen wipes both together.									*/
/*	======================================================================================= */

#ifndef __PROFILE_H_
#define __PROFILE_H_

/*	Where the profile lives - a per-user data directory, made on demand:					*/
/*																							*/
/*		macOS		~/Library/Application Support/StuntCarRacer/profile.txt				*/
/*		Linux		$XDG_DATA_HOME/StuntCarRacer/profile.txt, or ~/.local/share/...		*/
/*		Windows		%APPDATA%\StuntCarRacer\profile.txt									*/
/*																							*/
/*	Returns NULL if none of the environment variables it needs are set, which disables		*/
/*	saving rather than writing somewhere unexpected.										*/
const char *ProfilePath( void );

/*	The same directory, with a different file in it.  Returns NULL on the same terms as		*/
/*	ProfilePath, and into a buffer that the next call overwrites - use it and be done.		*/
/*	The opponent tuning bench keeps its offsets alongside the profile this way, without		*/
/*	putting a developer's numbers inside a player's career file.							*/
const char *ProfileDataPath( const char *filename );

/*	Read the profile into the league and record tables.  False if there is no profile		*/
/*	yet, or if the file is unreadable or from a newer version - in every one of those		*/
/*	cases the caller carries on with a fresh career, as the port did before.					*/
bool ProfileLoad( void );

/*	Write the current league and record tables out.  Silent on failure: a career that		*/
/*	cannot be saved is still a career worth playing.										*/
void ProfileSave( void );

/*	Remove the saved profile from disk.  The in-memory tables are left alone; the caller	*/
/*	decides what to put in their place.														*/
void ProfileDelete( void );

/*	True once a profile has been loaded or saved this run - i.e. there is a driver whose	*/
/*	career is being kept.  Drives whether the main menu asks for a name or resumes.			*/
bool ProfileExists( void );

#endif //__PROFILE_H_
