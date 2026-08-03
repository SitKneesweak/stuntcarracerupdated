#ifndef __ATLAS_H_
#define __ATLAS_H_

enum eAtlas {
    eWheel0 = 0,
    eWheel1,
    eWheel2,
    eWheel3,
    eWheel4,
    eWheel5,
    eHole,
    eNoHole,
    eCracking,
    eCockpitTop,
    eCockpitLeft,
    eCockpitRight,
    eCockpitBottom,
    eCockpitWL,
    eCockpitWR,
    eHole2,
    eNoHole2,
    eCracking2,
    eCockpitTop2,
    eCockpitLeft2,
    eCockpitRight2,
    eCockpitBottom2,
    eCockpitWL2,
    eCockpitWR2,
    eEngine,
    eEngineFlames0,
    eEngineFlames1,
    eEngineFlames2,
    eRoadYellowDark,
    eRoadYellowLight,
    eRoadRedDark,
    eRoadRedLight,
    eRoadBlack,
    eRoadWhite,
    eRoadYellowDark2,
    eRoadYellowLight2,
    eRoadRedDark2,
    eRoadRedLight2,
    eLAST
};

extern float atlas_tx1[eLAST], atlas_tx2[eLAST], atlas_ty1[eLAST], atlas_ty2[eLAST];

// The cells' raw pixel rects within atlas.png, kept alongside the UVs above so code that
// needs to read the authored artwork (see RoadTexture.cpp) doesn't have to undo the UV
// normalisation, the linux V flip and the road line inset to get back to texel space.
extern int atlas_px[eLAST], atlas_py[eLAST], atlas_pw[eLAST], atlas_ph[eLAST];

// Width of the red/yellow road side lines as authored into the atlas, in texels out of
// the road cells' 400, and the width actually wanted on screen.  See Atlas.cpp.
#define ROAD_ATLAS_LINE_TEXELS  9.0f
#define ROAD_LINE_WIDTH_TEXELS  5.0f

void InitAtlasCoord();

#endif //__ATLAS_H_