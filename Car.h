
#ifndef	_CAR
#define	_CAR

#ifdef SCR_PORTABLE
#include "dx_linux.h"
#endif

/*	========= */
/*	Constants */
/*	========= */
// VCAR is short for VISIBLE_CAR
#define	VCAR_WIDTH	162		// ((width 27+27 * segment width 384) / surface factor 256) * PC_FACTOR
#define	VCAR_LENGTH	256		// ((length 128 * segment length 256) / surface factor 256) * PC_FACTOR
#define	VCAR_HEIGHT	162		// chosen to look ok with the above

/*	How far the outer face of each wheel stands from the car's centre line. The car used to
	be as wide as its road footprint at both ends (VCAR_WIDTH/2), and the opponent's shadow
	is still built from that footprint - so these are what the shadow has to be scaled by to
	sit under the car that is actually drawn. See the wheel size note in Car.cpp.		*/
#define	WHEEL_REAR_OUTER	((17*VCAR_WIDTH)/40)	// 0.85 of the road footprint
#define	WHEEL_FRONT_OUTER	((7*VCAR_WIDTH)/32)		// 0.44 - the fronts are tucked well in

/*	The shadow floats this far above the road so it isn't clipped by it. The car is lifted
	to match, so its wheels stand on the shadow's plane rather than hovering over it. The
	car's y is in Amiga units, which are halved for display - hence the doubling.	*/
#define	SHADOW_ABOVE_ROAD		7
#define	CAR_LIFT_ABOVE_ROAD		(2 * SHADOW_ABOVE_ROAD)

// Cockpit rendering constants (320x200 base space)
#define COCKPIT_WIDESCREEN_OFFSET   40.0f   // Additional X offset for widescreen mode
#define COCKPIT_WHEEL_WIDTH         24.0f   // Width of wheel graphic (half)
#define COCKPIT_WHEEL_HEIGHT        56.0f   // Height of wheel graphic
#define COCKPIT_WHEEL_BOTTOM_GAP    20.0f   // Gap from bottom of screen
#define COCKPIT_WHEEL_LEFT_OFFSET   31.0f   // Left wheel X offset from edge
#define COCKPIT_ENGINE_X_OFFSET     42.0f   // Engine flame X offset
#define COCKPIT_ENGINE_Y_OFFSET     123.0f  // Engine flame Y offset
#define COCKPIT_ENGINE_WIDTH        235.0f  // Engine flame width
#define COCKPIT_ENGINE_HEIGHT       35.0f   // Engine flame height
#define COCKPIT_TOP_X_OFFSET        41.0f   // Top panel X offset
#define COCKPIT_TOP_WIDTH           238.0f  // Top panel width
#define COCKPIT_TOP_HEIGHT          16.0f   // Top panel height
#define COCKPIT_SIDE_HEIGHT         153.0f  // Side panel height
#define COCKPIT_RIGHT_X_OFFSET      279.0f  // Right panel X offset
#define COCKPIT_DAMAGE_HEIGHT       8.0f    // Damage indicator height
// Hole slots: copy.damage.graphic places slot n at word ((3n+6)>>1), i.e. x = 24n+48,
// and there are ten of them.  Holes are filled from slot 9 leftwards - see Car.cpp.
#define COCKPIT_HOLE_X_OFFSET       48.0f   // X offset of hole slot 0
#define COCKPIT_HOLE_SPACING        24.0f   // Spacing between holes
#define COCKPIT_HOLE_WIDTH          12.0f   // Width of hole graphic (half)
#define COCKPIT_HOLE_SLOTS          10      // Number of hole positions along the bar
#define COCKPIT_SPEEDBAR_X_OFFSET   196.0f  // Speed bar X offset
#define COCKPIT_SPEEDBAR_Y_OFFSET   61.0f   // Speed bar Y offset from bottom
#define COCKPIT_SPEEDBAR_WIDTH      242.0f  // Speed bar maximum width
#define COCKPIT_SPEEDBAR_HEIGHT     3.0f    // Speed bar height
#define COCKPIT_SPEEDBAR_MAX        240     // Maximum speed value for normal color
#define COCKPIT_WLEFT_X_OFFSET      40.0f   // Widescreen left panel width
#define COCKPIT_WRIGHT_X_OFFSET     82.0f   // Widescreen right panel offset from edge
#define COCKPIT_WRIGHT_Y_OFFSET     98.0f   // Widescreen right panel Y offset
#define COCKPIT_WLEFT_Y_OFFSET      99.0f   // Widescreen left panel Y offset

// The hole the cockpit art leaves for the world, measured off the alpha of the four cockpit
// pieces in the atlas (320x200 space).  This is wider than the Amiga's 238x137 playfield at
// (41,16) that SCR_WINDOW_* describes: the frame's inner bevel is transparent out to x 31
// and 287, and down to y 159 where the bonnet slopes away.  The bevel and the bonnet edge
// are diagonals, so this is their bounding box - the cockpit is drawn over the top of the
// scene, so over-reaching into art that is opaque anyway costs nothing.
#define COCKPIT_WINDOW_X            31.0f
#define COCKPIT_WINDOW_Y            16.0f
#define COCKPIT_WINDOW_WIDTH        257.0f  // x 31..287 inclusive
#define COCKPIT_WINDOW_HEIGHT       144.0f  // y 16..159 inclusive

/*	--- Crane chains ---------------------------------------------------------------------
	The two chains the crane hangs the car from, in the Amiga's 320x200 screen space.

	draw.chains ("Reference only/StuntCarRacer.s":21821) blits a 16-pixel-tall link as two
	8-line halves, stepping up the screen from the bottom of the chain until the next tile
	would cross the top of the playfield window at y 16.  The blit is word aligned - see
	copy.graphic, which adds (x position in words) * 2 bytes to the screen pointer and never
	shifts - so the chains sit in words 4 and 15, at x 64 and 240.  Where the 10-pixel-wide
	artwork sits inside its 16-pixel word is baked into the graphic data rather than the
	table, and is not the same on both sides: measured off an Amiga screenshot the left
	chain runs x 69..78 and the right x 240..249.

	The bottom end comes from B.1bbea, which draw.chains calls d2: the lowest tile's top
	edge is d2-40, so the chain ends at d2-32.  It holds at 176 while the car is on the
	chains and, once released, is whipped upwards by a step that grows 8 a frame -
	176, 168, 152, 128, 96 - after which the chains are gone.									*/
#define CHAIN_LEFT_X                69.0f   // left chain, x 69..78 within word 4  (64..79)
#define CHAIN_RIGHT_X               240.0f  // right chain, x 240..249 within word 15 (240..255)
#define CHAIN_WIDTH                 10.0f   // as authored in Bitmap/ChainLeft.png
#define CHAIN_LINK_HEIGHT           16      // the artwork's vertical period, in Amiga lines
#define CHAIN_TILE_HEIGHT           8       // the Amiga blits a link as two halves
#define CHAIN_TEX_HEIGHT            116.0f  // Bitmap/ChainLeft.png is 7 links plus a stub
#define CHAIN_HELD_D2               176     // B.1bbea while the car is on the chains
#define CHAIN_RETRACTED_D2          96      // the value at which draw.chains gives up
#define CHAIN_RETRACT_STEP          8       // B.1bbe9, and how much it grows each frame
#define CHAIN_D2_TO_BOTTOM          32      // chain bottom edge = d2 - this
#define CHAIN_TOP_CLIP              16      // top of the playfield window; tiles above it go

// Dashboard readout positions, in the Amiga's 320x200 screen space.  Each is the original's
// print column/row scaled by the 7x8 font cell plus its fine.x/fine.y nudge - see
// print.lap.boost.text, boost.print and display.opponents.distance in the 68k source.
// The top-left grey box spans x 36..85, y 178..185; the one below it y 188..195.
#define HUD_LAP_LABEL_X             37.0f   // 'L'   column 5, fine.x 2
#define HUD_LAP_VALUE_X             45.0f   // digit column 6, fine.x 2 (+1 from print.dec.digit1)
#define HUD_BOOST_LABEL_X           60.0f   // 'B'   column 8, fine.x 4
#define HUD_BOOST_VALUE_X           68.0f   // digits column 9, fine.x 4 (+1)
#define HUD_TOP_Y                  178.0f   // row 22, fine.y 2
#define HUD_DIST_X                  44.0f   // sign + 4 digits, column 6, fine.x 1 (+1)
#define HUD_DIST_Y                 188.0f   // row 23, fine.y 4

// Lap stopwatch, "M:SS.hh", from print.lap.time.  It starts at column 34 and then walks
// the print column and fine.x about, so the pieces are not evenly spaced: minutes at
// column 33 fine.x 6, colon at 34 fine.x 5, the two seconds digits at columns 35/36
// fine.x 3, the point at column 37 fine.x 2, the hundredths at columns 37/38 fine.x 7.
// The point is also lifted two scanlines (subq.b #2,print.fine.y).
#define HUD_TIME_MINS_X            237.0f
#define HUD_TIME_COLON_X           243.0f
#define HUD_TIME_SECS_X            248.0f
#define HUD_TIME_POINT_X           261.0f
#define HUD_TIME_HUNDREDTHS_X      266.0f
#define HUD_TIME_POINT_Y_OFFSET     -2.0f
// Row 22 fine.y 2 for the running / just-completed lap (TAB.5e46c entry 2), row 23
// fine.y 4 for the best lap (entry 3).
#define HUD_TIME_Y                 178.0f
#define HUD_BEST_TIME_Y            188.0f

/*	===================== */
/*	Structure definitions */
/*	===================== */

/*	============================== */
/*	External function declarations */
/*	============================== */
extern HRESULT CreateCarVertexBuffer (IDirect3DDevice9 *pd3dDevice);

extern void FreeCarVertexBuffer (void);

// Refills both cars' vertex buffers at their current suspension compression. Once per frame,
// before either car is drawn - they ride at different heights so they cannot share a buffer.
extern void UpdateCarSuspension (IDirect3DDevice9 *pd3dDevice, float fElapsedTime);

extern void DrawCar (IDirect3DDevice9 *pd3dDevice);

extern void DrawOpponentCar (IDirect3DDevice9 *pd3dDevice);

extern HRESULT CreateCockpitVertexBuffer (IDirect3DDevice9 *pd3dDevice);

extern void FreeCockpitVertexBuffer (void);

extern void DrawCockpit (IDirect3DDevice9 *pd3dDevice);

#endif	/* _CAR */
