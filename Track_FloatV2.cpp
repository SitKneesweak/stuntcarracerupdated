// Track_FloatV2 — see header for context.
//
// Ported from reference/floatv2-decompiled/TrackData.cs (RoadSurface, Tracks).
// The arithmetic here is deliberately integer-exact: these are 68k routines
// carried through two ports, and the truncation/wrapping *is* the behaviour.

#include "Track_FloatV2.h"

#include <cstdlib>

namespace scr {

namespace {

// Tracks.PieceDataMap — template number (low nibble of the section byte) to
// index in Pieces[]. Only 7 of the 16 templates exist; the rest are unused in
// the shipped tracks and map to the straight piece, as in the C#.
// Order matches Track.cpp's Piece_Templates[]: templates 0,1,3,4,6,7,10.
constexpr int kPieceDataMap[16] = {
    0, 1, 0, 2, 3, 0, 4, 5, 0, 0, 6, 0, 0, 0, 0, 0
};

// Tracks.Pieces — verified field-by-field against Piece_Templates[] in
// Track.cpp (CoordCount == numSegments + 1). WidthReduction is a constant 171
// for every piece (TrackConstants.WidthReduction) and has no C++ counterpart,
// so it comes from the reference.
constexpr FV2RoadPiece kPieces[7] = {
    //  byte1  coords  curve  width  length  steering
    { 0x00,  9, 0, 171, 128, 0x20 },   // template 0  — straight
    { 0x80,  9, 0, 171, 135, 0x3e },   // template 1  — curve right
    { 0xc0,  9, 3, 171, 135, 0x3e },   // template 3  — curve left
    { 0x40, 14, 0, 171, 128, 0x20 },   // template 4  — diagonal
    { 0x80, 10, 0, 171, 122, 0x32 },   // template 6  — curve right (tight)
    { 0xc0, 10, 3, 171, 122, 0x32 },   // template 7  — curve left (tight)
    { 0x40, 12, 0, 171, 124, 0x20 },   // template 10 — diagonal (long)
};

FV2RoadSection gSections[100];      // MAX_PIECES_PER_TRACK
FV2Track       gTrack = { gSections, 0 };
bool           gTrackValid = false;

// RoadSurface.ScaleByZFraction (TrackData.cs:222)
int ScaleByZFraction(int d0, int zFraction)
{
    int16_t v = static_cast<int16_t>(d0);
    if (v < 0)
        return -static_cast<int>((static_cast<uint16_t>(-v) * zFraction) & ~255u);
    return static_cast<int>(static_cast<uint16_t>(v)) * zFraction;
}

} // anonymous namespace

const FV2RoadPiece& FV2_GetPiece(int pieceIndex)
{
    return kPieces[kPieceDataMap[pieceIndex & 0x0f]];
}

const FV2Track* FV2_GetTrack()
{
    return gTrackValid ? &gTrack : nullptr;
}

// RoadSurface.FetchSurfaceYCoord (TrackData.cs:180)
// Track.cpp's ConvertAmigaPieceY() already performs the byte/word unpacking
// (identical bit maths), so we take its decoded value and apply the section's
// overall shift plus the >>5 the physics expects. The int16_t cast matters:
// the sum wraps in 16 bits on the Amiga and some tracks rely on it.
int16_t FV2_FetchSurfaceYCoord(int yCoordId, int coordIndex, int16_t overallYShift)
{
    long raw = FV2_GetRawYCoord(yCoordId, coordIndex);
    return static_cast<int16_t>(static_cast<int16_t>(raw + overallYShift) >> 5);
}

// RoadSurface.InterpolateHeight (TrackData.cs:199)
// Lerp across the road (xFraction) on the near and far edges of the segment,
// then lerp between them along it (zFraction). The >>3 dance keeps the
// intermediate in range for the 16-bit multiply the original used.
int FV2_InterpolateHeight(int16_t y1, int16_t y2, int16_t y3, int16_t y4,
                          int xFraction, int zFraction)
{
    int nearEdge = (y2 - y1) * xFraction + (y1 << 8);
    int delta    = (y4 - y3) * xFraction + (y3 << 8) - nearEdge;

    if (std::abs(delta) >= 32768)
    {
        delta >>= 3;
        delta = ScaleByZFraction(delta, zFraction);
        delta <<= 3;
    }
    else
    {
        delta = ScaleByZFraction(delta, zFraction);
    }

    delta >>= 8;
    return delta + nearEdge;
}

// RoadSurface.GetRoadHeight (TrackData.cs:149)
int FV2_GetRoadHeight(const FV2Track& track, int section,
                      int distanceIntoSection, int wheelXPosition)
{
    const FV2RoadSection& sec   = track.Sections[section];
    const FV2RoadPiece&   piece = FV2_GetPiece(sec.PieceIndex);

    const int leftId  = sec.LeftYCoordId  & 0x7f;
    const int rightId = sec.RightYCoordId & 0x7f;

    const bool reversed  = (sec.Angle & 0x10) != 0;
    const int  coordIdx  = (distanceIntoSection >> 8) & 0xff;
    const int  zFraction = distanceIntoSection & 0xff;

    int16_t y1, y2, y3, y4;
    if (!reversed)
    {
        y1 = FV2_FetchSurfaceYCoord(leftId,  coordIdx,     sec.OverallLeftYShift);
        y2 = FV2_FetchSurfaceYCoord(rightId, coordIdx,     sec.OverallRightYShift);
        y3 = FV2_FetchSurfaceYCoord(leftId,  coordIdx + 1, sec.OverallLeftYShift);
        y4 = FV2_FetchSurfaceYCoord(rightId, coordIdx + 1, sec.OverallRightYShift);
    }
    else
    {
        // Travelling the piece backwards: walk the coords from the far end and
        // swap the near/far pairs so the interpolation still runs "forwards".
        const int idx = piece.CoordCount - 1 - coordIdx - 1;
        y4 = FV2_FetchSurfaceYCoord(leftId,  idx,     sec.OverallLeftYShift);
        y3 = FV2_FetchSurfaceYCoord(rightId, idx,     sec.OverallRightYShift);
        y2 = FV2_FetchSurfaceYCoord(leftId,  idx + 1, sec.OverallLeftYShift);
        y1 = FV2_FetchSurfaceYCoord(rightId, idx + 1, sec.OverallRightYShift);
    }

    return FV2_InterpolateHeight(y1, y2, y3, y4, wheelXPosition, zFraction);
}

void FV2_BuildTrackView(int sectionCount,
                        const char*  pieceAngleAndTemplate,
                        const char*  leftYCoordId,
                        const char*  rightYCoordId,
                        const short* leftOverallYShift,
                        const short* rightOverallYShift)
{
    for (int i = 0; i < sectionCount; ++i)
    {
        const uint8_t sectionByte = static_cast<uint8_t>(pieceAngleAndTemplate[i]);

        // Our .bin tracks are already RLE-expanded, so the piece byte and the
        // angle byte the C# TrackLoader tracks separately are the same byte.
        gSections[i].PieceIndex         = static_cast<uint8_t>(sectionByte & 0x0f);
        gSections[i].Angle              = sectionByte;
        gSections[i].LeftYCoordId       = static_cast<uint8_t>(leftYCoordId[i]);
        gSections[i].RightYCoordId      = static_cast<uint8_t>(rightYCoordId[i]);
        gSections[i].OverallLeftYShift  = leftOverallYShift[i];
        gSections[i].OverallRightYShift = rightOverallYShift[i];
    }

    gTrack.SectionCount = sectionCount;
    gTrackValid = true;
}

} // namespace scr
