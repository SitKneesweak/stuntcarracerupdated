/*	======================================================================================= */
/*	File:			AmigaMenu.h																*/
/*																							*/
/*	Description:	The original Amiga menu presentation layer.								*/
/*																							*/
/*					The Amiga drew its menus straight into the 320x200 chip-RAM bitmap:		*/
/*					the "STUNT CAR RACER" frame (Bitmap/menu.png) with a white panel cut		*/
/*					out of the middle, and 7x8 font characters blitted into that panel on	*/
/*					a fixed character grid.  Rather than approximate that with quads per		*/
/*					glyph, this module rebuilds the Amiga's framebuffer literally: a			*/
/*					320x200 RGBA surface in RAM that everything is blitted into, uploaded	*/
/*					once per frame as a single texture and drawn as one full-screen quad		*/
/*					through the existing sharp-bilinear filter.  That gets pixel accuracy	*/
/*					for free and makes the compositing trivial.								*/
/*																							*/
/*	Coordinates:	Character cell (col,row) sits at pixel (col*AMIGA_FONT_ADVANCE,			*/
/*					row*AMIGA_FONT_HEIGHT) = (col*7, row*8), exactly as print.character		*/
/*					in the 68k does it.  The white panel in menu.png measures out at			*/
/*					x 32..255, y 54..194, so the usable text area is columns 5..36 and		*/
/*					rows 7..24 - which is precisely where every coordinate baked into the	*/
/*					disassembly's text tables lands.  Nothing here is fudged to fit.			*/
/*	======================================================================================= */

#ifndef __AMIGAMENU_H_
#define __AMIGAMENU_H_

#include "dx_linux.h"

/*	The Amiga display the menus were authored for. */
#define AMIGA_SCREEN_WIDTH		320
#define AMIGA_SCREEN_HEIGHT		200

/*	One character cell.  The glyphs are eight pixels wide but the print column steps by		*/
/*	seven, so the last column is the gap between characters.									*/
#define AMIGA_CHAR_WIDTH		7
#define AMIGA_CHAR_HEIGHT		8

/*	The white panel inside the menu.png frame, in surface pixels, and the character cells	*/
/*	that fall inside it.  PANEL_COL0/ROW0 are the first cell fully within the panel.			*/
#define AMIGA_PANEL_X			32
#define AMIGA_PANEL_Y			54
#define AMIGA_PANEL_W			224
#define AMIGA_PANEL_H			141
#define AMIGA_PANEL_COL0		5
#define AMIGA_PANEL_COL1		36
#define AMIGA_PANEL_ROW0		7
#define AMIGA_PANEL_ROW1		24

/*	A colour to draw with.  The Amiga picked a pen from a 16-entry palette; we just carry	*/
/*	the RGB, since the menu palette never animates.											*/
struct AmigaPen
	{
	unsigned char r, g, b;
	};

/*	The menu palette, matched to the colours actually present in Bitmap/menu.png.			*/
extern const AmigaPen AMIGA_INK_BLACK;
extern const AmigaPen AMIGA_INK_WHITE;
extern const AmigaPen AMIGA_INK_RED;		// headings
extern const AmigaPen AMIGA_INK_DARKRED;
extern const AmigaPen AMIGA_INK_BROWN;
extern const AmigaPen AMIGA_INK_GREEN;		// pen 10 - underline.text's line colour

/*	The panel and its bars.  The panel is not white - the original's menu panel is a mid		*/
/*	grey, entries sit on light grey bars, and the selected entry's bar is amber.  Text is	*/
/*	white on the panel and black on a bar.													*/
extern const AmigaPen AMIGA_PAPER;			// the panel background
extern const AmigaPen AMIGA_INK_TEXT;		// body text on the panel
extern const AmigaPen AMIGA_BAR;			// an unselected entry's bar
extern const AmigaPen AMIGA_BAR_SELECTED;	// the highlighted entry's bar
extern const AmigaPen AMIGA_INK_BAR_TEXT;	// text on top of either bar

/*	--- Surface ------------------------------------------------------------------------	*/

/*	Wipe the whole 320x200 surface to a flat colour (normally black - the frame art covers	*/
/*	everything that matters).																*/
void AmigaMenuClear( const AmigaPen &pen );

/*	Repaint the menu.png frame, then blank the white panel back to white.  This is the		*/
/*	equivalent of the 68k's clear.menu: the start of every menu screen.						*/
void AmigaMenuFrame( void );

/*	Blit a PNG from Bitmap/ (cached after first load).  The "Rect" form takes a source		*/
/*	rectangle, which is how the individual driver portraits get pulled out of heads.png.	*/
void AmigaMenuBlit( const char *image, int dx, int dy );
void AmigaMenuBlitRect( const char *image, int sx, int sy, int w, int h, int dx, int dy );

/*	--- Text ---------------------------------------------------------------------------	*/

/*	Ink for subsequent text.  Defaults to black.											*/
void AmigaMenuSetInk( const AmigaPen &pen );

/*	Print at an explicit cell, and print continuing from wherever the last print ended.		*/
void AmigaMenuPrintAt( int col, int row, const char *text );
void AmigaMenuPrint( const char *text );
void AmigaMenuPrintF( int col, int row, const char *fmt, ... );

/*	Print a string in the original's own escape encoding, so the tables lifted out of the	*/
/*	disassembly can be used verbatim: a 31 byte means "the next two bytes are the column		*/
/*	and row to move to" (in that order - the 68k stores print.column first, then				*/
/*	print.row).  Printing stops at a 0 terminator; the tables' own 255 separators are		*/
/*	handled by AmigaMenuText() below, which hands back one entry at a time.					*/
void AmigaMenuPrintEsc( const char *text );

/*	Centre a string on a row, within the panel.												*/
void AmigaMenuPrintCentred( int row, const char *text );

/*	Print at an arbitrary pixel position rather than on the character grid - needed where	*/
/*	text has to line up with artwork instead of with the grid (the name plate under a		*/
/*	driver portrait, for instance).															*/
void AmigaMenuPrintPixel( int x, int y, const char *text );

/*	--- Highlight bar ------------------------------------------------------------------	*/

/*	The selection bar.  The original cut this out of the (LZ-packed) title bitmap, which		*/
/*	isn't recoverable from the disassembly, so it is drawn here in the frame's own dark		*/
/*	red with a lighter edge top and bottom.  Geometry is the original's: full panel width,	*/
/*	17 pixels tall, top edge at row*8-5.  Note that height spans two character rows, which	*/
/*	is why the original spaces menu entries three rows apart - put entries any closer than	*/
/*	two rows and the bar swallows its neighbour.											*/
/*																							*/
/*	Every entry gets a bar; the selected one gets the amber one.							*/
void AmigaMenuBar( int row, bool selected = true );

/*	How tall that bar is, for anything that has to fill its own slab to match.				*/
#define MENU_BAR_HEIGHT		17

/*	The pixel row the top of that bar lands on, for anything that has to be positioned		*/
/*	against the bar rather than against the character grid.									*/
int AmigaMenuBarY( int row );

/*	Flat rectangle fill, in surface pixels.													*/
void AmigaMenuFillRect( int x, int y, int w, int h, const AmigaPen &pen );

/*	--- Presentation -------------------------------------------------------------------	*/

/*	Upload the surface and draw it over the whole window, letterboxed to preserve the		*/
/*	4:3 shape of the original 320x200 display.												*/
void AmigaMenuPresent( IDirect3DDevice9 *pd3dDevice );

/*	Where a rectangle of the 320x200 surface lands on screen, in the 640x480 / 800x480 base
	space.  The track preview uses this to place the 3D view inside the picture window of
	Bitmap/trackpreview.png.																*/
void AmigaMenuGetScreenRect( int sx, int sy, int sw, int sh,
							 float *out_x, float *out_y, float *out_w, float *out_h );

/*	Drop cached textures/images (device teardown).											*/
void AmigaMenuRelease( void );

/*	Write the current surface out as a binary PPM.  The menus are the one part of the game	*/
/*	whose output can be checked exactly - against the Amiga's own character grid - so being	*/
/*	able to get at the raw 320x200 surface is worth having.  See SCR_MENU_DUMPALL in			*/
/*	MenuScreens.cpp, which uses this to render every screen to a file in one go.				*/
void AmigaMenuWritePPM( const char *path );

#endif //__AMIGAMENU_H_
