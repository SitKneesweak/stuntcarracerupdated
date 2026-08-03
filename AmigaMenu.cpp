/*	======================================================================================= */
/*	File:			AmigaMenu.cpp															*/
/*																							*/
/*	Description:	Implementation of the Amiga menu presentation layer - see AmigaMenu.h	*/
/*					for the approach and for where the coordinates come from.				*/
/*	======================================================================================= */

#include "dx_linux.h"
#include "StuntCarRacer.h"
#include "AmigaMenu.h"
#include "AmigaFont.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*	======================================================================================= */
/*	Palette																					*/
/*																							*/
/*	Sampled from Bitmap/menu.png so the text sits in the same colour space as the frame.	*/
/*	======================================================================================= */

const AmigaPen AMIGA_INK_BLACK   = {  10,  10,  10 };
const AmigaPen AMIGA_INK_WHITE   = { 255, 255, 255 };
const AmigaPen AMIGA_INK_RED     = { 204,  51,  17 };
const AmigaPen AMIGA_INK_DARKRED = { 109,  43,  10 };
const AmigaPen AMIGA_INK_BROWN   = { 143,  76,  43 };
/*	The pen underline.text is called with on the name entry screen (d0=10).  Sampled off a	*/
/*	screen shot of the real thing and rounded to the 4-bits-per-channel the Amiga had.		*/
const AmigaPen AMIGA_INK_GREEN   = {  34,  85,  68 };

/*	The panel is a mid grey, not white: the frame art leaves a hole for it and the game		*/
/*	fills the hole behind the art, so the STUNT CAR RACER logo - which overhangs the top		*/
/*	of the hole by a dozen pixels - stays visible.  Entries sit on light grey bars with		*/
/*	the selected one in amber, and text on a bar is black.									*/
const AmigaPen AMIGA_PAPER         = {  76,  76,  76 };
const AmigaPen AMIGA_INK_TEXT      = { 255, 255, 255 };
const AmigaPen AMIGA_BAR           = { 176, 176, 176 };
const AmigaPen AMIGA_BAR_SELECTED  = { 242, 176,  43 };
const AmigaPen AMIGA_INK_BAR_TEXT  = {  10,  10,  10 };

/*	======================================================================================= */
/*	The surface																				*/
/*	======================================================================================= */

static unsigned char gSurface[AMIGA_SCREEN_HEIGHT][AMIGA_SCREEN_WIDTH][4];
static bool  gSurfaceDirty = true;

static AmigaPen gInk = { 255, 255, 255 };
static int gPrintCol = AMIGA_PANEL_COL0;
static int gPrintRow = AMIGA_PANEL_ROW0;

/*	======================================================================================= */
/*	Function:		AmigaFontInit															*/
/*																							*/
/*	Description:	Redraw the three glyphs the game patches during start-up, so the font	*/
/*					matches what the Amiga actually printed with:							*/
/*																							*/
/*						move.b	#0,font7+14*8+5		change decimal point					*/
/*						move.b	#$10,font7+14*8+6											*/
/*						move.b	#$7e,font7+13*8+3	change minus sign						*/
/*						move.l	#0,font7+63*8		change underscore						*/
/*						move.l	#0,font7+63*8+4												*/
/*						move.b	#$7e,font7+63*8+6											*/
/*																							*/
/*					Glyph 13 is '-', 14 is '.' and 63 is '_' (index = character - 32).		*/
/*	======================================================================================= */

static void AmigaFontInit( void )
	{
	static bool done = false;
	if (done)
		return;
	done = true;

	AmigaFont7[14][5] = 0x00;			// decimal point
	AmigaFont7[14][6] = 0x10;

	AmigaFont7[13][3] = 0x7e;			// minus sign

	for (int i = 0; i < 8; i++)			// underscore: cleared, then a bar on row 6
		AmigaFont7[63][i] = 0x00;
	AmigaFont7[63][6] = 0x7e;
	}

static inline void PutPixel( int x, int y, const AmigaPen &pen )
	{
	if ((x < 0) || (y < 0) || (x >= AMIGA_SCREEN_WIDTH) || (y >= AMIGA_SCREEN_HEIGHT))
		return;
	gSurface[y][x][0] = pen.r;
	gSurface[y][x][1] = pen.g;
	gSurface[y][x][2] = pen.b;
	gSurface[y][x][3] = 255;
	}

void AmigaMenuFillRect( int x, int y, int w, int h, const AmigaPen &pen )
	{
	for (int yy = 0; yy < h; yy++)
		for (int xx = 0; xx < w; xx++)
			PutPixel(x + xx, y + yy, pen);
	gSurfaceDirty = true;
	}

void AmigaMenuClear( const AmigaPen &pen )
	{
	for (int y = 0; y < AMIGA_SCREEN_HEIGHT; y++)
		for (int x = 0; x < AMIGA_SCREEN_WIDTH; x++)
			{
			gSurface[y][x][0] = pen.r;
			gSurface[y][x][1] = pen.g;
			gSurface[y][x][2] = pen.b;
			gSurface[y][x][3] = 255;
			}
	gSurfaceDirty = true;
	}

/*	======================================================================================= */
/*	Image cache																				*/
/*																							*/
/*	The menu art is a handful of small 320x200 PNGs; load each once and keep the pixels		*/
/*	around, since a menu screen may reblit them every frame.									*/
/*	======================================================================================= */

#define MAX_CACHED_IMAGES	8

struct CachedImage
	{
	char			name[64];
	unsigned char  *pixels;		// always 4 channels
	int				w, h;
	};

static CachedImage gImages[MAX_CACHED_IMAGES];
static int gNumImages = 0;

static const CachedImage *GetImage( const char *image )
	{
	for (int i = 0; i < gNumImages; i++)
		if (strcmp(gImages[i].name, image) == 0)
			return gImages[i].pixels ? &gImages[i] : NULL;

	if (gNumImages >= MAX_CACHED_IMAGES)
		return NULL;

	CachedImage *slot = &gImages[gNumImages++];
	snprintf(slot->name, sizeof(slot->name), "%s", image);

	char path[128];
	snprintf(path, sizeof(path), "Bitmap/%s", image);

	int n = 0;
	slot->pixels = stbi_load(path, &slot->w, &slot->h, &n, STBI_rgb_alpha);
	if (slot->pixels == NULL)
		{
		printf("AmigaMenu: could not load %s\n", path);
		return NULL;
		}
	return slot;
	}

void AmigaMenuBlitRect( const char *image, int sx, int sy, int w, int h, int dx, int dy )
	{
	const CachedImage *img = GetImage(image);
	if (img == NULL)
		return;

	for (int y = 0; y < h; y++)
		{
		const int syy = sy + y;
		if ((syy < 0) || (syy >= img->h))
			continue;
		for (int x = 0; x < w; x++)
			{
			const int sxx = sx + x;
			if ((sxx < 0) || (sxx >= img->w))
				continue;

			const unsigned char *src = img->pixels + (syy * img->w + sxx) * 4;
			if (src[3] == 0)			// the frame art is keyed, not blended
				continue;

			PutPixel(dx + x, dy + y, *(const AmigaPen *)src);
			}
		}
	gSurfaceDirty = true;
	}

void AmigaMenuBlit( const char *image, int dx, int dy )
	{
	const CachedImage *img = GetImage(image);
	if (img == NULL)
		return;
	AmigaMenuBlitRect(image, 0, 0, img->w, img->h, dx, dy);
	}

/*	======================================================================================= */
/*	Function:		AmigaMenuFrame															*/
/*																							*/
/*	Description:	The 68k's clear.menu - paint the title frame and blank the panel.		*/
/*	======================================================================================= */

void AmigaMenuFrame( void )
	{
	/*	The panel goes down first and the frame art over the top of it: menu.png has the		*/
	/*	panel area punched out (alpha 0) and the logo hangs over the top edge of that		*/
	/*	hole, so painting the panel afterwards would slice the bottom off the logo.			*/
	AmigaMenuClear(AMIGA_INK_BLACK);

	for (int y = AMIGA_PANEL_Y; y < AMIGA_PANEL_Y + AMIGA_PANEL_H; y++)
		for (int x = AMIGA_PANEL_X; x < AMIGA_PANEL_X + AMIGA_PANEL_W; x++)
			PutPixel(x, y, AMIGA_PAPER);

	AmigaMenuBlit("menu.png", 0, 0);

	AmigaMenuSetInk(AMIGA_INK_TEXT);
	gPrintCol = AMIGA_PANEL_COL0;
	gPrintRow = AMIGA_PANEL_ROW0;
	gSurfaceDirty = true;
	}

/*	======================================================================================= */
/*	Text																					*/
/*	======================================================================================= */

void AmigaMenuSetInk( const AmigaPen &pen )
	{
	gInk = pen;
	}

/*	Draw one glyph at a pixel position.  Only the top 7 bits of each font row are used: the	*/
/*	original advances the column by 7 pixels, so bit 0 is the gap between characters.		*/
static void DrawCharAtPixel( int x0, int y0, unsigned char c )
	{
	AmigaFontInit();

	if ((c < AMIGA_FONT_FIRST_CHAR) || (c >= AMIGA_FONT_FIRST_CHAR + AMIGA_FONT_NUM_CHARS))
		c = ' ';

	const unsigned char *glyph = AmigaFont7[c - AMIGA_FONT_FIRST_CHAR];

	for (int y = 0; y < AMIGA_FONT_HEIGHT; y++)
		{
		const unsigned char bits = glyph[y];
		for (int x = 0; x < AMIGA_FONT_ADVANCE; x++)
			if (bits & (0x80 >> x))
				PutPixel(x0 + x, y0 + y, gInk);
		}
	}

static void DrawChar( int col, int row, unsigned char c )
	{
	DrawCharAtPixel(col * AMIGA_FONT_ADVANCE, row * AMIGA_FONT_HEIGHT, c);
	}

void AmigaMenuPrintPixel( int x, int y, const char *text )
	{
	for (; *text; text++)
		{
		DrawCharAtPixel(x, y, (unsigned char)*text);
		x += AMIGA_FONT_ADVANCE;
		}
	gSurfaceDirty = true;
	}

void AmigaMenuPrint( const char *text )
	{
	for (; *text; text++)
		{
		if (*text == '\n')
			{
			gPrintCol = AMIGA_PANEL_COL0;
			gPrintRow++;
			continue;
			}
		DrawChar(gPrintCol++, gPrintRow, (unsigned char)*text);
		}
	gSurfaceDirty = true;
	}

void AmigaMenuPrintAt( int col, int row, const char *text )
	{
	gPrintCol = col;
	gPrintRow = row;
	AmigaMenuPrint(text);
	}

void AmigaMenuPrintF( int col, int row, const char *fmt, ... )
	{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	AmigaMenuPrintAt(col, row, buf);
	}

void AmigaMenuPrintEsc( const char *text )
	{
	const unsigned char *p = (const unsigned char *)text;
	while (*p)
		{
		if (*p == 31)					// 31, column, row
			{
			if (p[1] && p[2])
				{
				gPrintCol = p[1];
				gPrintRow = p[2];
				p += 3;
				continue;
				}
			break;
			}
		DrawChar(gPrintCol++, gPrintRow, *p++);
		}
	gSurfaceDirty = true;
	}

void AmigaMenuPrintCentred( int row, const char *text )
	{
	const int len  = (int)strlen(text);
	const int cols = AMIGA_PANEL_COL1 - AMIGA_PANEL_COL0 + 1;
	int col = AMIGA_PANEL_COL0 + (cols - len) / 2;
	if (col < AMIGA_PANEL_COL0)
		col = AMIGA_PANEL_COL0;
	AmigaMenuPrintAt(col, row, text);
	}

/*	======================================================================================= */
/*	Function:		AmigaMenuBar															*/
/*																							*/
/*	Description:	The selection bar.  fill.bar in the 68k blits a 224x17 chunk of the		*/
/*					title bitmap at x=32, y=row*8-9.  The title bitmap is LZ-packed in the	*/
/*					disassembly and not recoverable, so the bar is rebuilt here: light grey	*/
/*					for an entry, amber for the selected one, with a black rule underneath	*/
/*					to separate it from the next.  Geometry is the original's, unchanged.	*/
/*	======================================================================================= */

#define MENU_BAR_HEIGHT		17

/*	fill.bar's address arithmetic works out at row*8-9, but measured against a screenshot	*/
/*	of the real thing the bar sits four pixels lower: the entry's glyphs start six pixels	*/
/*	down the bar with four spare below, rather than being jammed against the bottom rule		*/
/*	with everything spare above.  Screen2 evidently isn't quite where the address maths		*/
/*	assumes.  The screenshot wins - this is the offset that reproduces it.					*/
#define MENU_BAR_Y_OFFSET	(-5)

int AmigaMenuBarY( int row )
	{
	return row * AMIGA_FONT_HEIGHT + MENU_BAR_Y_OFFSET;
	}

void AmigaMenuBar( int row, bool selected )
	{
	const int y0 = AmigaMenuBarY(row);

	for (int y = 0; y < MENU_BAR_HEIGHT; y++)
		{
		/*	The bar is bevelled: a white rule along the top and a black one along the	*/
		/*	bottom, which is what separates one bar from the next once every entry has	*/
		/*	one.  Both are one pixel, measured off a screen shot of the real thing.		*/
		const AmigaPen &pen = (y == 0)						? AMIGA_INK_WHITE
							: (y == MENU_BAR_HEIGHT - 1)	? AMIGA_INK_BLACK
							: (selected ? AMIGA_BAR_SELECTED : AMIGA_BAR);

		for (int x = 0; x < AMIGA_PANEL_W; x++)
			PutPixel(AMIGA_PANEL_X + x, y0 + y, pen);
		}
	gSurfaceDirty = true;
	}

/*	======================================================================================= */
/*	Presentation																			*/
/*	======================================================================================= */

struct MENUVERTEX
	{
	float x, y, z, rhw;
	float u, v;
	};
#define D3DFVF_MENUVERTEX	(D3DFVF_XYZRHW|D3DFVF_TEX1)

static IDirect3DTexture9		*pMenuTexture = NULL;
static IDirect3DVertexBuffer9	*pMenuVB      = NULL;

void AmigaMenuRelease( void )
	{
	if (pMenuTexture) { delete pMenuTexture; pMenuTexture = NULL; }
	if (pMenuVB)      { pMenuVB->Release();  pMenuVB      = NULL; }

	for (int i = 0; i < gNumImages; i++)
		if (gImages[i].pixels)
			{
			stbi_image_free(gImages[i].pixels);
			gImages[i].pixels = NULL;
			}
	gNumImages = 0;
	gSurfaceDirty = true;
	}

void AmigaMenuWritePPM( const char *path )
	{
	FILE *f = fopen(path, "wb");
	if (f == NULL)
		{
		printf("AmigaMenu: could not write %s\n", path);
		return;
		}

	fprintf(f, "P6\n%d %d\n255\n", AMIGA_SCREEN_WIDTH, AMIGA_SCREEN_HEIGHT);
	for (int y = 0; y < AMIGA_SCREEN_HEIGHT; y++)
		for (int x = 0; x < AMIGA_SCREEN_WIDTH; x++)
			fwrite(gSurface[y][x], 1, 3, f);
	fclose(f);
	}

/*	Where a rectangle of the 320x200 surface lands on screen, in the 640x480 (or 800x480)
	base space everything else is laid out in.  The surface is letterboxed into the window
	at the 4:3 shape the original displayed at, so a widescreen window pillarboxes rather
	than stretching.  The track preview needs this to know where to put the 3D view.		*/
void AmigaMenuGetScreenRect( int sx, int sy, int sw, int sh,
							 float *out_x, float *out_y, float *out_w, float *out_h )
	{
	long screen_width, screen_height;
	GetScreenDimensions(&screen_width, &screen_height);

	const float target = 4.0f / 3.0f;
	float w = (float)screen_width;
	float h = w / target;
	if (h > (float)screen_height)
		{
		h = (float)screen_height;
		w = h * target;
		}
	const float x0 = ((float)screen_width  - w) * 0.5f;
	const float y0 = ((float)screen_height - h) * 0.5f;

	const float scale_x = w / (float)AMIGA_SCREEN_WIDTH;
	const float scale_y = h / (float)AMIGA_SCREEN_HEIGHT;

	*out_x = x0 + (float)sx * scale_x;
	*out_y = y0 + (float)sy * scale_y;
	*out_w = (float)sw * scale_x;
	*out_h = (float)sh * scale_y;
	}

void AmigaMenuPresent( IDirect3DDevice9 *pd3dDevice )
	{
	if (pMenuTexture == NULL)
		{
		pMenuTexture  = new IDirect3DTexture9();
		gSurfaceDirty = true;
		}

	if (gSurfaceDirty)
		{
		/*	CreateFromMemory uploads rows in the order given, so surface row 0 becomes		*/
		/*	v=0.  The quad below therefore maps v=0 to the top of the screen.  (Note		*/
		/*	LoadTexture flips on the way in, so its textures want the opposite mapping.)		*/
		pMenuTexture->CreateFromMemory((const unsigned char *)gSurface,
									   AMIGA_SCREEN_WIDTH, AMIGA_SCREEN_HEIGHT, 4,
									   false, false);
		gSurfaceDirty = false;
		}

	if (pMenuVB == NULL)
		{
		if (FAILED(pd3dDevice->CreateVertexBuffer(4 * sizeof(MENUVERTEX), D3DUSAGE_WRITEONLY,
												  D3DFVF_MENUVERTEX, D3DPOOL_DEFAULT,
												  &pMenuVB, NULL)))
			return;
		}

	float x0, y0, w, h;
	AmigaMenuGetScreenRect(0, 0, AMIGA_SCREEN_WIDTH, AMIGA_SCREEN_HEIGHT, &x0, &y0, &w, &h);

	MENUVERTEX *pVertices;
	if (FAILED(pMenuVB->Lock(0, 0, (void **)&pVertices, 0)))
		return;

	pVertices[0].x = x0;     pVertices[0].y = y0;     pVertices[0].u = 0.0f; pVertices[0].v = 0.0f;
	pVertices[1].x = x0 + w; pVertices[1].y = y0;     pVertices[1].u = 1.0f; pVertices[1].v = 0.0f;
	pVertices[2].x = x0 + w; pVertices[2].y = y0 + h; pVertices[2].u = 1.0f; pVertices[2].v = 1.0f;
	pVertices[3].x = x0;     pVertices[3].y = y0 + h; pVertices[3].u = 0.0f; pVertices[3].v = 1.0f;
	for (int i = 0; i < 4; i++)
		{
		pVertices[i].z   = 0.5f;
		pVertices[i].rhw = 1.0f;
		}
	pMenuVB->Unlock();

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTSS_COLORARG1);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	pd3dDevice->SetTexture(0, pMenuTexture);
	pd3dDevice->SetStreamSource(0, pMenuVB, 0, sizeof(MENUVERTEX));
	pd3dDevice->SetFVF(D3DFVF_MENUVERTEX);
	pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	}
