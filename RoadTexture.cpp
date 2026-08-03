/*	=========================================================================================== */
/*	Procedural road surface texture.															*/
/*																								*/
/*	The road cells in Bitmap/atlas.png are 400x98 blocks of flat colour with the yellow/red		*/
/*	side lines painted into their outer 9 texels.  The track vertex buffer only ever sampled	*/
/*	a single row of one (every road corner took atlas_ty1), so the cell acted purely as a		*/
/*	way of drawing the side lines and the road surface between them was flat.					*/
/*																								*/
/*	Here we build one small texture per colour variant instead: the same cross-section read		*/
/*	straight out of the atlas so the colours and side lines stay pixel-identical, with			*/
/*	quantised grey noise multiplied into the surface between the lines.  The texture repeats	*/
/*	along the road's length (Track.cpp drives V from distance travelled), and is point			*/
/*	sampled, so the grain reads as chunky Amiga-era pixels rather than a blurred photo.			*/
/*																								*/
/*	Generating rather than shipping a PNG keeps the grain tileable by construction and makes	*/
/*	the knobs below real knobs - rebuild with "make MACOS=1" after changing them.				*/
/*	=========================================================================================== */

#include "dxstdafx.h"
#include "RoadTexture.h"

#ifdef SCR_ROAD_TEXTURE

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*	---- Tweakable ---- */
#define ROAD_TEX_HEIGHT			256		// texels along the road in one tile
#define ROAD_NOISE_BLOCK		8		// texels per noise sample; 1 = one sample per texel
#define ROAD_NOISE_LEVELS		4		// grey steps the noise is quantised to (Amiga-ish)
#define ROAD_NOISE_CONTRAST		0.06f	// +/- fraction of the base colour at the extremes
#define ROAD_NOISE_SEED			0x5ca1ab1eu

IDirect3DTexture9 *g_pRoadTexture[eLAST] = {NULL};

// The generated width is the atlas road width with the side line inset already applied, so
// Track.cpp can map u straight across 0..1 and still get ROAD_LINE_WIDTH_TEXELS-wide lines.
static int roadTexWidth  = 0;
static int roadTexHeight = 0;

int GetRoadTextureWidth (void)  {return roadTexWidth;}
int GetRoadTextureHeight (void) {return roadTexHeight;}


/*	=========================================================================================== */
/*	Flat noise																					*/
/*																								*/
/*	Deliberately not fractal.  An interpolated value-noise field, however many octaves it has,	*/
/*	always reads as smeared blobs at this contrast, and quantising it just outlines them.  One	*/
/*	independent sample per texel has no shapes in it at all, which is what "gravel seen from a	*/
/*	distance" actually looks like - and it tiles for free, since every sample is keyed off the	*/
/*	texel coordinate alone.																		*/
/*	=========================================================================================== */

static unsigned int HashLattice (int x, int y, unsigned int seed)
{
unsigned int h = seed;

	h ^= (unsigned int)x * 0x8da6b343u;
	h ^= (unsigned int)y * 0xd8163841u;
	h ^= h >> 15;	h *= 0x2c1b3c6du;
	h ^= h >> 12;	h *= 0x297a2d39u;
	h ^= h >> 15;
	return h;
}


// Multiplier to apply to the base road colour at this texel: 1.0 +/- ROAD_NOISE_CONTRAST.
static float NoiseAt (int px, int py, int width, int height)
{
	(void)width; (void)height;

	// Snap to the block grid first so ROAD_NOISE_BLOCK > 1 gives chunkier grain without
	// changing anything else; at 1 this is the identity.
	const int bx = px / ROAD_NOISE_BLOCK;
	const int by = py / ROAD_NOISE_BLOCK;

	float n = (float)(HashLattice(bx, by, ROAD_NOISE_SEED) & 0xffff) / 65535.0f;

	// Quantise to a handful of steps.  This is what stops it reading as film grain: the
	// Amiga's road was flat colour, so a few hard grey levels sit better than a smooth ramp.
	int step = (int)(n * (float)ROAD_NOISE_LEVELS);
	if (step >= ROAD_NOISE_LEVELS) step = ROAD_NOISE_LEVELS - 1;
	if (step < 0)                  step = 0;
	n = (ROAD_NOISE_LEVELS > 1) ? ((float)step / (float)(ROAD_NOISE_LEVELS - 1)) : 0.5f;

	return 1.0f + (n - 0.5f) * 2.0f * ROAD_NOISE_CONTRAST;
}


/*	=========================================================================================== */
/*	Texture construction																		*/
/*	=========================================================================================== */

static unsigned char ScaleChannel (unsigned char c, float mul)
{
	float f = (float)c * mul;
	if (f < 0.0f)   f = 0.0f;
	if (f > 255.0f) f = 255.0f;
	return (unsigned char)(f + 0.5f);
}


// Build one variant.  atlasPixels is the whole of atlas.png, atlasW/H its dimensions and
// channels its component count; cell is the eRoad* entry to read the cross-section from.
static void CreateOneRoadTexture (const unsigned char *atlasPixels, int atlasW, int atlasH,
                                  int channels, int cell)
{
	const int inset = (int)(ROAD_ATLAS_LINE_TEXELS - ROAD_LINE_WIDTH_TEXELS + 0.5f);
	const int srcX  = atlas_px[cell] + inset;
	const int srcW  = atlas_pw[cell] - inset * 2;
	// Any row of the cell will do - they're all the same - but take one from the middle
	// rather than the edge in case of bleed from a neighbouring cell.
	const int srcY  = atlas_py[cell] + atlas_ph[cell] / 2;

	if (srcW <= 0 || srcX < 0 || srcX + srcW > atlasW || srcY < 0 || srcY >= atlasH)
	{
		printf("RoadTexture: atlas cell %d out of range, skipping\n", cell);
		return;
	}

	roadTexWidth  = srcW;
	roadTexHeight = ROAD_TEX_HEIGHT;

	// Texels whose distance from either edge is less than the line width are the painted
	// side lines - leave those exactly as authored, they're the one part that isn't surface.
	const int lineTexels = (int)(ROAD_LINE_WIDTH_TEXELS + 0.5f);

	unsigned char *pixels = (unsigned char *)malloc((size_t)srcW * ROAD_TEX_HEIGHT * 3);
	if (pixels == NULL)
		return;

	for (int py = 0; py < ROAD_TEX_HEIGHT; py++)
	{
		for (int px = 0; px < srcW; px++)
		{
			const unsigned char *src = atlasPixels + ((size_t)srcY * atlasW + (srcX + px)) * channels;
			unsigned char *dst = pixels + ((size_t)py * srcW + px) * 3;

			const bool isSideLine = (px < lineTexels) || (px >= srcW - lineTexels);
			const float mul = isSideLine ? 1.0f : NoiseAt(px, py, srcW, ROAD_TEX_HEIGHT);

			dst[0] = ScaleChannel(src[0], mul);
			dst[1] = ScaleChannel(channels > 1 ? src[1] : src[0], mul);
			dst[2] = ScaleChannel(channels > 2 ? src[2] : src[0], mul);
		}
	}

	if (g_pRoadTexture[cell] == NULL)
		g_pRoadTexture[cell] = new IDirect3DTexture9();

	g_pRoadTexture[cell]->CreateFromMemory(pixels, srcW, ROAD_TEX_HEIGHT, 3,
	                                       true /*nearest*/, true /*repeat V*/);
	free(pixels);
}


void CreateRoadTextures (void)
{
int x, y, n;

	// InitAtlasCoord() has to have run first - that's what fills in atlas_px/py/pw/ph.
	unsigned char *atlasPixels = stbi_load("Bitmap/atlas.png", &x, &y, &n, 0);
	if (atlasPixels == NULL)
	{
		printf("RoadTexture: could not read Bitmap/atlas.png, road stays flat\n");
		return;
	}

	for (int cell = eRoadYellowDark; cell < eLAST; cell++)
		CreateOneRoadTexture(atlasPixels, x, y, n, cell);

	stbi_image_free(atlasPixels);

	printf("Road textures generated (%dx%d, %d grey levels)\n",
	       roadTexWidth, roadTexHeight, ROAD_NOISE_LEVELS);
	fflush(stdout);
}


void FreeRoadTextures (void)
{
	for (int cell = eRoadYellowDark; cell < eLAST; cell++)
	{
		if (g_pRoadTexture[cell])
		{
			delete g_pRoadTexture[cell];
			g_pRoadTexture[cell] = NULL;
		}
	}
}

#endif	// SCR_ROAD_TEXTURE
