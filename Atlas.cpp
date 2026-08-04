#include "dxstdafx.h"
#include "Atlas.h"

/* ---- Tweakable: see ROAD_LINE_WIDTH_TEXELS in Atlas.h ---- */

float atlas_tx1[eLAST] = {0};
float atlas_tx2[eLAST] = {0};
float atlas_ty1[eLAST] = {0};
float atlas_ty2[eLAST] = {0};

int atlas_px[eLAST] = {0};
int atlas_py[eLAST] = {0};
int atlas_pw[eLAST] = {0};
int atlas_ph[eLAST] = {0};

void InitAtlasCoord() {
    const int x[eLAST] = { 
        160, 128, 96, 64, 32, 0,    //eWheel
        0, 16, 0,  //eHole, eNotHole, eCracking
        41, 0, 279, 0, // eCockpit
        340, 400,   // eCockpitWL, eCockpitWR,
        32, 48, 0,  //eHole2, eNotHole2, eCracking2
        41, 0, 279, 0, // eCockpit2
        340, 400,   // eCockpitWL2, eCockpitWR2,
        42, 42, 42, 362, // eEgines...
        620, 620, 620, 620 ,// eRoad...
        620, 620,           // eRoad Black / White
        620, 620, 620, 620  // eRoad...2
    };
    const int y[eLAST] = { 
        0, 0, 0, 0, 0, 0,    //eWheel
        64, 64, 128,
        160, 160, 160, 313,
        259, 259,
        64, 64, 140,
        364, 364, 364, 517,
        463, 463,
        592, 784, 976, 123,
        441, 551, 111, 221, 
        1, 331,
        771, 826, 661, 716
    };
    const int w[eLAST]  = {
        24, 24, 24, 24, 24, 24,
        12, 12, 238,
        238, 41, 41, 320,
        40, 40,
        12, 12, 238,
        238, 41, 41, 320,
        40, 40,
        235, 235 ,235, 235,
        400, 400, 400, 400,
        400, 400, 400, 400,
        400, 400
    };
    const int h[eLAST] = {
        56, 56, 56, 56, 56, 56,
        8, 8, 8,
        16, 153, 153, 47,
        101, 101,
        8, 8, 8,
        16, 153, 153, 47,
        101, 101,
        35, 35 ,35, 35,
        98, 98, 98, 98,
        98, 98,
        48, 48, 48, 48

    };

    // The road cells in the atlas are a single horizontal strip spanning the road's
    // width, with the coloured side lines baked into the outermost 9 texels of the
    // 400.  Insetting the u range trims texels off those side lines, which makes the
    // stripes narrower on screen (the middle is a flat colour, so stretching it to
    // compensate is invisible).  9 = the atlas as authored, 0 = no side lines at all.
    // The Amiga original plots these as single pixel lines, so lower looks truer.
    const float roadLineInset = ROAD_ATLAS_LINE_TEXELS - ROAD_LINE_WIDTH_TEXELS;

    for (int i=0; i<eLAST; i++) {
        atlas_px[i] = x[i];
        atlas_py[i] = y[i];
        atlas_pw[i] = w[i];
        atlas_ph[i] = h[i];

        float inset = (i >= eRoadYellowDark) ? roadLineInset : 0.0f;
        atlas_tx1[i] = ((float)x[i] + inset) / 1024.0f;
        atlas_tx2[i] = ((float)(x[i]+w[i]) - inset) / 1024.0f;
        #ifdef SCR_PORTABLE
        atlas_ty1[i] = 1.0f-(float)y[i] / 1024.0f;
        atlas_ty2[i] = 1.0f-(float)(y[i]+h[i]) / 1024.0f;
        #else
        atlas_ty1[i] = (float)y[i] / 1024.0f;
        atlas_ty2[i] = (float)(y[i]+h[i]) / 1024.0f;
        #endif
    }

}