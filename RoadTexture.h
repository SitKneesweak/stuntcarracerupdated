#ifndef __ROADTEXTURE_H_
#define __ROADTEXTURE_H_

#include "Atlas.h"

#ifdef SCR_ROAD_TEXTURE

/*
 * One generated texture per road colour variant (indexed by the eRoad* entries of eAtlas).
 * Entries outside that range stay NULL.  DrawTrack() binds these instead of g_pAtlas.
 */
extern IDirect3DTexture9 *g_pRoadTexture[eLAST];

// Number of texels along the road's length in one tile of the generated texture.  Together
// with the road's width in texels this fixes the texel aspect - see RoadTexture.cpp.
extern int GetRoadTextureWidth (void);
extern int GetRoadTextureHeight (void);

extern void CreateRoadTextures (void);
extern void FreeRoadTextures (void);

#endif	// SCR_ROAD_TEXTURE

#endif	//__ROADTEXTURE_H_
