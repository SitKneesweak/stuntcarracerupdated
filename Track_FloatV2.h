// Track_FloatV2 — presents our existing Amiga track data in the shape the
// FloatV2 physics port expects.
//
// The .NET build keeps track data in a separate assembly
// (StuntCarRacer.TrackData, decompiled to reference/floatv2-decompiled/TrackData.cs)
// built around RoadSection / RoadPiece / YCoordBlock. Track.cpp already loads
// the *same* Amiga track binaries into equivalent-but-differently-named
// statics, so this is a view over that data, not a second copy of it:
//
//   C# RoadSection.PieceIndex/Angle  <- Piece_Angle_And_Template[]  (packed)
//   C# RoadSection.Left/RightYCoordId <- Left/Right_Y_Coordinate_ID[]
//   C# RoadSection.OverallLeft/RightYShift <- Left/Right_Overall_Y_Shift[]
//   C# RoadPiece                     <- Piece_Templates[] (+ WidthReduction)
//   C# YCoordBlock                   <- Amiga_Piece_Y[] / decoded Piece_Y[][]
//   C# Track.SectionCount            <- NumTrackPieces
//
// "Section" (C#) and "piece" (C++) are the same thing: one 512x512 track cube.
// "Piece" in C# means what C++ calls a piece *template*.

#pragma once

#include <cstdint>

namespace scr {

// Mirrors TrackData.cs RoadSection. Only the fields the physics reads are
// populated; DistanceAroundRoad / OpponentSpeed are derived in the C#
// TrackLoader and are not needed until the opponent AI port (Phase 1b).
struct FV2RoadSection
{
    uint8_t PieceIndex;         // template number, 0..15 (index into PieceDataMap)
    uint8_t Angle;              // rough angle + reverse-order bit (bit 4)
    uint8_t LeftYCoordId;       // index into the Y-coord blocks
    uint8_t RightYCoordId;
    int16_t OverallLeftYShift;
    int16_t OverallRightYShift;
};

// Mirrors TrackData.cs RoadPiece (the per-template constants). Values verified
// against Piece_Templates[] in Track.cpp — CoordCount == numSegments + 1.
struct FV2RoadPiece
{
    uint8_t SectionByte1;       // 0x00 straight, 0x40 diagonal, 0x80/0xc0 curve
    uint8_t CoordCount;
    uint8_t CurveDirection;     // 3 == curves left, 0 otherwise (bit 0 is what's read)
    uint8_t WidthReduction;
    uint8_t LengthReduction;
    uint8_t SteeringAmount;
};

struct FV2Track
{
    const FV2RoadSection* Sections;
    int                   SectionCount;
};

// Maps a section's PieceIndex (template number 0..15) to an FV2RoadPiece.
// Equivalent to Tracks.PieceDataMap[] + Tracks.Pieces[] in the C#.
const FV2RoadPiece& FV2_GetPiece(int pieceIndex);

// The current track, or nullptr before ConvertAmigaTrack() has run.
const FV2Track* FV2_GetTrack();

// --- RoadSurface (TrackData.cs:147) ---------------------------------------
// Bilinear-ish height lookup over one road segment, in Amiga integer units.
//   distanceIntoSection: 8.8 fixed point — high byte = coord index, low = fraction
//   wheelXPosition:      0..255 across the road width
int FV2_GetRoadHeight(const FV2Track& track, int section,
                      int distanceIntoSection, int wheelXPosition);

int16_t FV2_FetchSurfaceYCoord(int yCoordId, int coordIndex, int16_t overallYShift);

int FV2_InterpolateHeight(int16_t y1, int16_t y2, int16_t y3, int16_t y4,
                          int xFraction, int zFraction);

// --- Provided by Track.cpp -------------------------------------------------
// Raw (pre-shift, pre->>5) Y coordinate from the decoded Amiga Y tables.
// Lives in Track.cpp because Piece_Y[][] is static there.
long FV2_GetRawYCoord(int yCoordId, int coordIndex);

// Called by ConvertAmigaTrack() once the Amiga track data is loaded.
void FV2_BuildTrackView(int sectionCount,
                        const char*  pieceAngleAndTemplate,
                        const char*  leftYCoordId,
                        const char*  rightYCoordId,
                        const short* leftOverallYShift,
                        const short* rightOverallYShift);

} // namespace scr
