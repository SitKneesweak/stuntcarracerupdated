/**************************************************************************

    Track.cpp - Functions for manipulating track

	24/04/1998 - all Track data from Amiga StuntCarRacer is now made 2
				 (PC_FACTOR) times bigger for use by PC StuntCarRacer

 **************************************************************************/

/*	============= */
/*	Include files */
/*	============= */
#include "dxstdafx.h"

#include "Track.h"
#include "Track_FloatV2.h"
#include "StuntCarRacer.h"
#include "3D_Engine.h"
#include "Atlas.h"
#include "RoadTexture.h"

/*	===== */
/*	Debug */
/*	===== */
#if defined(DEBUG) || defined(_DEBUG)
extern FILE *out;
extern long	VALUE1, VALUE2;
#endif

/*	========= */
/*	Constants */
/*	========= */
#define DISTANT_OBJECT_BOUNDARY (0x18000000)

#define SCR_BASE_COLOUR	26

/*	=========== */
/*	Global data */
/*	=========== */
extern GameModeType GameMode;

/*	The Amiga preview screen draws the road the way the game does, lines and all - see		*/
/*	DrawTrack.  The PC port's own preview and the track menu keep the plain flat road.		*/
extern bool bAmigaTrackPreview;
extern bool bAmigaPreviewScreen;
extern long bTrackDrawMode;
extern long gTrackLighting;
extern bool bSuperLeague;

unsigned char sections_car_can_be_put_on[] =
{
	0x00,0x80,0x20,0xc0,0x00,0x73,0x80,0xc0,0xa9,0x59,0x00,0x02,0xa9,0x5e,0x85,0x4b

// These 16 bytes are flags for each of the near sections.  The actual values used are as follows :-
//
//	0x00,0x80,0x00,0xc0,0x00,0x00,0x80,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
//
// If bit 7 is set then the car cannot be lowered onto this section.
};

/*	=========== */
/*	Static data */
/*	=========== */

//
//road.section.x.z.positions
//
static char Piece_X_Z_Position[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(char)0xcf,(char)0xbf,(char)0xaf,(char)0x9f,(char)0x8f,(char)0x7f,(char)0x6f,(char)0x5f,
	(char)0x4f,(char)0x3f,(char)0x2f,(char)0x1f,(char)0x0e,(char)0x0d,(char)0x0c,(char)0x0b,
	(char)0x0a,(char)0x09,(char)0x08,(char)0x07,(char)0x06,(char)0x05,(char)0x04,(char)0x03,
	(char)0x02,(char)0x01,(char)0x10,(char)0x20,(char)0x31,(char)0x42,(char)0x53,(char)0x64,
	(char)0x75,(char)0x86,(char)0x97,(char)0xa8,(char)0xb9,(char)0xca,(char)0xdb,(char)0xec,
	(char)0xfd,(char)0xfe,(char)0xef,(char)0xdf
};
*/

//
//road.section.angle.and.piece
//
//* Top two bits are the rough angle for the piece (0, 90, 180 or 270 degrees).
//*
//* Bit 4 indicates that piece is rotated through a further 180 degrees.
//*
//* Bottom nibble is the near section piece to use.
//
char Piece_Angle_And_Template[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(char)0xa0,(char)0xa0,(char)0xa0,(char)0xa0,(char)0xa0,(char)0xa0,(char)0xa0,(char)0xa0,
	(char)0xa0,(char)0xa0,(char)0x80,(char)0x86,(char)0x57,(char)0xc0,(char)0xe0,(char)0xe0,
	(char)0xe0,(char)0xe0,(char)0xe0,(char)0xe0,(char)0xe0,(char)0xe0,(char)0xe0,(char)0xe0,
	(char)0xc0,(char)0xc6,(char)0xb7,(char)0x01,(char)0x94,(char)0x2a,(char)0x2a,(char)0x2a,
	(char)0x2a,(char)0x2a,(char)0x2a,(char)0x2a,(char)0x2a,(char)0x2a,(char)0x2a,(char)0x04,
	(char)0xd3,(char)0x66,(char)0x17,(char)0x80
};
*/

//
//left.y.coordinate.IDs
//
//* Bit 7 indicates that the y coords for that section are stored as words
//* e.g. for steeper sections on the roller coaster or the high jump
//
static char Left_Y_Coordinate_ID[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(char)0x6a,(char)0x6b,(char)0x24,(char)0x50,(char)0x50,(char)0x25,(char)0x00,(char)0x00,
	(char)0x19,(char)0x63,(char)0x04,(char)0x65,(char)0x68,(char)0x64,(char)0x69,(char)0x17,
	(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,
	(char)0x03,(char)0x16,(char)0x00,(char)0x19,(char)0x04,(char)0x00,(char)0x00,(char)0x00,
	(char)0x28,(char)0x29,(char)0x00,(char)0x2a,(char)0x2b,(char)0x00,(char)0x00,(char)0x09,
	(char)0x16,(char)0x00,(char)0x1b,(char)0x04
};
*/

//
//right.y.coordinate.IDs
//
//* Bit 7 goes to other.road.line.colour
//
static char Right_Y_Coordinate_ID[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(char)0x6a,(char)0x6b,(char)0x24,(char)0x50,(char)0x50,(char)0x25,(char)0x00,(char)0x00,
	(char)0x19,(char)0x63,(char)0x64,(char)0x66,(char)0xe7,(char)0x04,(char)0x69,(char)0x17,
	(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,(char)0x00,
	(char)0x04,(char)0x17,(char)0x80,(char)0x18,(char)0x03,(char)0x80,(char)0x00,(char)0x80,
	(char)0x28,(char)0xa9,(char)0x00,(char)0xaa,(char)0x2b,(char)0x80,(char)0x00,(char)0x8a,
	(char)0x17,(char)0x00,(char)0x9a,(char)0x03
};
*/

//
//overall.left.y.shifts
//
//* A value for each road section, used to shift all the left side y
//* co-ordinates up by the same amount.
//
static short Left_Overall_Y_Shift[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(short)0x0280,(short)0x0280,(short)0x0780,(short)0x0a60,
	(short)0x1260,(short)0x1a60,(short)0x1d40,(short)0x1d40,
	(short)0x1ce0,(short)0x1920,(short)0x17a0,(short)0x1380,
	(short)0x0ea0,(short)0x0660,(short)0x0560,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0700,(short)0x0760,(short)0x0700,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0700,(short)0x0760,(short)0x0700,(short)0x0500
};
*/

//
//overall.right.y.shifts
//
//* Same as above, but for the right side y co-ordinates.
//
static short Right_Overall_Y_Shift[MAX_PIECES_PER_TRACK];/* Little Ramp data, from before it was loaded =
{
	(short)0x0280,(short)0x0280,(short)0x0780,(short)0x0a60,
	(short)0x1260,(short)0x1a60,(short)0x1d40,(short)0x1d40,
	(short)0x1ce0,(short)0x1920,(short)0x1160,(short)0x0ec0,
	(short)0x0aa0,(short)0x08a0,(short)0x0560,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0300,(short)0x02a0,(short)0x02a0,(short)0x02a0,
	(short)0x0300,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0500,
	(short)0x0500,(short)0x0500,(short)0x0500,(short)0x0300,
	(short)0x02a0,(short)0x02a0,(short)0x02a0,(short)0x0300
};
*/

//
//******** Start of y co-ordinates for near sections ********
//
//	B means co-ords are stored as bytes, W means words.
//
static unsigned char B1037[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1051[] =
{
	0x00,0x60,0x61,0x03,0x44,
	0x26,0x28,0x2a,0x2c
};
static unsigned char W1060[] =
{
	0x00,0x00,0x02,0x00,0x04,0x00,
	0x06,0x00,0x08,0x00,0x0a,0x00,
	0x0c,0x00,0x0e,0x00,0x10,0x00
};
static unsigned char B1078[] =
{
	0x00,0x20,0x40,0x60,0x01,0x21,0x41,
	0x61,0x02,0x02,0x02,0x02,0x02,0x02
};
static unsigned char B1092[] =
{
	0x02,0x61,0x41,0x21,0x01,0x60,0x40,
	0x20,0x00,0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1106[] =
{
	0x00,0x60,0x21,0x51,0x02,
	0x22,0x42,0x62,0x03,0x13
};
static unsigned char B1116[] =
{
	0x00,0x20,0x40,0x70,0x21,
	0x41,0x61,0x02,0x22,0x32
};
static unsigned char B1126[] =
{
	0x00,0x02,0x04,0x06,0xe7,
	0x29,0xca,0x4b,0x2c
};
static unsigned char B1135[] =
{
	0x46,0x96,0x55,0x85,0x24,
	0x33,0xb2,0x21,0x00
};
static unsigned char B1144[] =
{
	0x00,0x00,0x00,0x00,0x00,0x10,0x20,
	0x40,0x60,0x01,0x21,0x41,0x61,0x02
};
static unsigned char B1158[] =
{
	0x02,0x02,0x02,0x02,0x02,0x71,0x61,
	0x41,0x21,0x01,0x60,0x40,0x20,0x00
};

static unsigned char B1172[] =
{
	0x00,0x10,0x10,0x10,0x10,
	0x10,0x10,0x90,0x80
};
static unsigned char B1181[] =
{
	0x10,0x00,0x00,0x00,0x00,
	0x00,0x00,0x80,0x90
};

static unsigned char B1190[] =
{
	0x00,0x01,0x02,0x03,0x04,0x05,
	0x06,0x07,0x08,0x09,0x0a,0x0b
};
static unsigned char W1202[] =
{
	0x1b,0x80,0x1c,0x80,0x1d,0x80,
	0x1e,0x80,0x1f,0x80,0x20,0x80,
	0xa1,0x80,0x80,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1226[] =
{
	0x4e,0x1d,0xdb,0x0a,0xa8,
	0x36,0x34,0x22,0x00
};
static unsigned char W1235[] =
{
	0x00,0x00,0x9b,0x20,0x19,0xe0,
	0x18,0xa0,0x17,0x60,0x16,0x20,
	0x14,0xe0,0x13,0xa0,0x12,0x60,
	0x11,0x20,0x0f,0xe0,0x0e,0xa0
};
static unsigned char B1259[] =
{
	0x48,0x27,0x26,0x35,0x44,0x63,
	0x13,0x42,0x71,0x21,0x50,0x00
};
static unsigned char B1271[] =
{
	0x13,0x03,0x62,0x42,0x22,
	0x02,0x51,0x21,0xe0,0x80
};
static unsigned char B1281[] =
{
	0x05,0x05,0x85,0x00,0x00,
	0x85,0x05,0x05,0x05
};
static unsigned char B1290[] =
{
	0x32,0x22,0x02,0x61,0x41,
	0x21,0x70,0x40,0xa0,0x80
};
static unsigned char B1300[] =
{
	0x00,0x40,0x01,0x41,0x02,
	0x42,0x03,0x33,0x63
};
static unsigned char B1309[] =
{
	0x00,0x20,0x30,0x30,0x30,
	0x30,0x30,0x30,0x30,0x30
};
static unsigned char B1319[] =
{
	0x30,0x10,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1329[] =
{
	0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x90,0xb0
};
static unsigned char B1338[] =
{
	0x30,0x30,0x30,0x30,0x30,
	0x30,0x30,0xa0,0x80
};
static unsigned char B1347[] =
{
	0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x90,0xb0
};
static unsigned char B1357[] =
{
	0x30,0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0xa0,0x80
};
static unsigned char B1367[] =
{
	0x00,0x21,0x42,0x53,0xe4,
	0x65,0xe6,0x57,0x48
};
static unsigned char B1376[] =
{
	0x00,0x60,0x41,0x92,0x62,
	0xa3,0x63,0x14,0x44
};
static unsigned char B1385[] =
{
	0x00,0x20,0x40,0xd0,0x60,
	0x60,0xd0,0x40,0x20
};
static unsigned char B1394[] =
{
	0x04,0x63,0xb3,0x03,0x42,
	0x82,0x31,0x60,0x00
};
static unsigned char B1403[] =
{
	0xa6,0x80,0x00,0x00,0x00,
	0x00,0x00,0x80,0x35
};
static unsigned char B1412[] =
{
	0x47,0x87,0x46,0x75,0x25,0x44,
	0x63,0x03,0x22,0x41,0x60,0x00
};
static unsigned char B1424[] =
{
	0x08,0x27,0x36,0xc5,0x44,
	0x43,0x32,0x21,0x00
};
static unsigned char B1433[] =
{
	0x50,0x50,0x50,0x50,0xc0,
	0x30,0x20,0x10,0x00
};
static unsigned char B1442[] =
{
	0x00,0x00,0x10,0x30,0x60,
	0x11,0x51,0x22,0x72
};
static unsigned char B1451[] =
{
	0x00,0x60,0x41,0xa2,0xd2,0x62,
	0xf2,0x72,0x72,0x72,0x72,0x72
};
static unsigned char B1463[] =
{
	0x22,0xb2,0x32,0xa2,0x12,
	0xf1,0x31,0x60,0x00
};
static unsigned char B1472[] =
{
	0x0a,0x68,0x47,0x26,0x05,
	0x63,0x42,0x21,0x00
};
static unsigned char B1481[] =
{
	0x00,0x10,0x30,0x60,0x21,0x71,
	0x42,0x13,0x63,0x34,0x05,0x55
};
static unsigned char B1493[] =
{
	0x55,0x26,0x76,0x47,0x18,0x68,
	0x39,0x8a,0x00,0x00,0x00,0x00
};
static unsigned char B1505[] =
{
	0x00,0xc7,0x76,0x26,0x55,0x05,
	0x34,0x63,0x13,0x42,0x71,0x21
};
static unsigned char B1517[] =
{
	0x21,0x60,0x30,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1529[] =
{
	0x8a,0x80,0x00,0x00,0x00,
	0x00,0x00,0x80,0x4c
};
static unsigned char B1538[] =
{
	0x00,0x41,0x03,0x44,0x06,
	0x47,0x09,0x4a,0x0c
};
static unsigned char B1547[] =
{
	0x70,0x50,0x30,0x10,0x00,
	0x10,0x30,0x50,0x70
};
static unsigned char B1556[] =
{
	0xaa,0x80,0x00,0x00,0x00,
	0x00,0x00,0x80,0x2a
};
static unsigned char B1565[] =
{
	0x59,0x49,0x39,0xa9,0x63,
	0x63,0x63,0x63,0x47
};
static unsigned char B1574[] =
{
	0x00,0x00,0x00,0x10,0x30,0x50,
	0x01,0x31,0x71,0x42,0x23,0x14
};
static unsigned char B1586[] =
{
	0x62,0x62,0x62,0xd2,0x42,0xa2,
	0x02,0x61,0xb1,0x01,0x40,0x00
};
static unsigned char B1598[] =
{
	0x00,0x40,0x01,0x41,0x02,0x42,0x03,
	0x43,0x04,0x64,0x45,0x26,0x07,0x67
};
static unsigned char B1612[] =
{
	0x00,0x10,0x20,0x30,0x40,
	0x40,0x40,0x40,0x40,0x40
};
static unsigned char B1622[] =
{
	0x00,0x00,0x00,0x00,0x00,
	0x10,0x30,0x60,0x21
};
static unsigned char B1631[] =
{
	0x8d,0x80,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00
};
static unsigned char W1640[] =
{
	0x00,0x00,0x00,0x00,0x80,0x00,
	0x9c,0x80,0x1c,0x80,0x9c,0x80,
	0x80,0x00,0x00,0x00,0x00,0x00
};
static unsigned char B1658[] =
{
	0x00,0x00,0x10,0x20,0x40,
	0x60,0x01,0x31,0x71
};
static unsigned char B1667[] =
{
	0x00,0x10,0x30,0x70,0x31,
	0x71,0xb2,0x52,0x62
};
static unsigned char B1676[] =
{
	0x00,0x00,0x00,0x10,0x30,
	0x60,0x21,0x02,0x03
};
static unsigned char B1685[] =
{
	0x00,0x10,0x30,0x60,0x21,
	0x71,0x62,0x53,0x44
};
static unsigned char B1694[] =
{
	0x00,0x70,0x61,0x52,0x43,
	0x34,0x25,0x16,0x07
};
static unsigned char B1703[] =
{
	0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x80,0x2e
};
static unsigned char B1712[] =
{
	0x00,0x01,0xf1,0x52,0xa3,
	0x63,0x94,0x34,0x54
};
static unsigned char B1721[] =
{
	0x00,0x30,0xd0,0x70,0x11,
	0xa1,0x31,0x41,0x41,0x41
};
static unsigned char B1731[] =
{
	0x40,0x10,0x00,0x00,0x00,
	0x10,0x40,0x11,0x61
};
static unsigned char B1740[] =
{
	0x40,0x40,0x40,0x40,0x40,
	0x40,0x30,0x20,0x10,0x00
};
static unsigned char W1750[] =
{
	0x9a,0xc0,0x80,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x80,0x00,0x0c,0x80
};
static unsigned char B1768[] =
{
	0x24,0x03,0x02,0x21,0x60,
	0x30,0x10,0x00,0x00
};
static unsigned char B1777[] =
{
	0x47,0x46,0x65,0x25,0x05,
	0x05,0x15,0x35,0x75
};
static unsigned char B1786[] =
{
	0x80,0xe6,0x16,0x45,0x74,
	0x24,0x53,0x23,0x13
};
static unsigned char B1795[] =
{
	0x46,0x25,0x14,0x13,0x22,
	0x41,0x70,0x30,0x00
};
static unsigned char B1804[] =
{
	0x00,0x01,0x12,0x33,0x54,
	0x75,0x17,0x38,0x59,0x7a
};
static unsigned char B1814[] =
{
	0x02,0x71,0xd1,0x21,0x60,
	0x30,0x10,0x00,0x00
};
static unsigned char B1823[] =
{
	0x00,0x00,0x10,0x30,0x60,
	0x21,0xd1,0x71,0x02
};
static unsigned char B1832[] =
{
	0x00,0x40,0x81,0x31,0xd1,
	0x61,0xf1,0x71,0x71
};
static unsigned char B1841[] =
{
	0x22,0x61,0x21,0x60,0x30,
	0x10,0x00,0x00,0x00
};
static unsigned char B1850[] =
{
	0x00,0x60,0x41,0x22,0x03,0x63,
	0x44,0x25,0x06,0x66,0x47,0x28
};
static unsigned char B1862[] =
{
	0x00,0x00,0x10,0x30,0x60,
	0x21,0x71,0x52,0x43
};
static unsigned char B1871[] =
{
	0x24,0x45,0xe6,0x80,0x21,
	0x42,0x63,0x05,0x26
};
static unsigned char W1880[] =
{
	0x28,0x60,0x27,0xc0,0x27,0x40,
	0x26,0xe0,0x26,0xa0,0x26,0x80,
	0x26,0x80,0x26,0xa0,0x26,0xe0,
	0x27,0x20,0xa7,0x60,0x00,0x00
};
static unsigned char B1904[] =
{
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,
	0x07,0x08,0x68,0x49,0x2a,0x0b,0x6b
};
static unsigned char B1918[] =
{
	0x00,0x70,0x51,0x32,0x13,
	0x73,0x54,0x35,0x06
};
static unsigned char B1927[] =
{
	0x00,0x50,0x31,0x12,0x72,
	0x53,0x34,0x15,0x06
};
static unsigned char B1936[] =
{
	0x00,0x60,0x41,0x22,0x03,0x73,0x64,
	0x65,0x66,0x67,0x68,0x69,0x6a,0x6b
};
static unsigned char B1950[] =
{
	0x00,0x60,0x41,0x22,0x03,0x53,0x24,
	0x64,0x25,0x65,0x26,0x66,0x27,0x67
};
static unsigned char B1964[] =
{
	0x00,0x81,0x61,0xa2,0x42,
	0x52,0x52,0x52,0x52
};
static unsigned char B1973[] =
{
	0x00,0x41,0x72,0x14,0x35,
	0x56,0x77,0x19,0x3a,0x5b
};
static unsigned char B1983[] =
{
	0x00,0x21,0x42,0x63,0x05,
	0x26,0x47,0x68,0x1a,0x5b
};
static unsigned char B1993[] =
{
	0x64,0x14,0x43,0x72,0x22,0x51,
	0x01,0x40,0x20,0x10,0x00,0x00
};
static unsigned char B2005[] =
{
	0x05,0x05,0x05,0x15,0x25,
	0x45,0xe5,0x00,0x00
};
static unsigned char B2014[] =
{
	0x22,0x12,0xf1,0x51,0x31,
	0x11,0x60,0x30,0x00
};
static unsigned char B2023[] =
{
	0x00,0x50,0x31,0x22,0x23,
	0x34,0x55,0x76,0x18
};
static unsigned char B2032[] =
{
	0x00,0x21,0x42,0x63,0x05,
	0x26,0x47,0x68,0x79,0x7a
};
static unsigned char B2042[] =
{
	0x52,0x71,0x21,0x60,0x30,
	0x10,0x00,0x00,0x00
};
// 20/08/1998 - following four arrays form the animating section of the DrawBridge
// The original Amiga StuntCarRacer data for these (large spikes) has been changed to a
// set of data from the MoveDrawBridge function (height of 15), to prevent some of
// the road surfaces being made black when this is not required.
static unsigned char W2051[] =
{
	0x00,0x00,0x02,0x60,0x04,0xc0,
	0x07,0x20,0x09,0x80,0x0b,0xe0,
	0x0e,0x40,0x10,0xa0,0x13,0x00
};
static unsigned char W2069[] =
{
	0x13,0x00,0x15,0x60,0x17,0xc0,
	0x1a,0x20,0x1c,0x80,0x1e,0xe0,
	0x21,0x40,0x23,0xa0,0x00,0x00
};
static unsigned char W2087[] =
{
	0x00,0x00,0x23,0xa0,0x21,0x40,
	0x1e,0xe0,0x1c,0x80,0x1a,0x20,
	0x17,0xc0,0x15,0x60,0x13,0x00
};
static unsigned char W2105[] =
{
	0x13,0x00,0x10,0xa0,0x0e,0x40,
	0x0b,0xe0,0x09,0x80,0x07,0x20,
	0x04,0xc0,0x02,0x60,0x00,0x00
};
static unsigned char B2123[] =
{
	0x63,0x43,0xa3,0xf2,0x42,
	0x02,0x41,0x01,0x40
};
static unsigned char B2132[] =
{
	0x28,0x47,0x66,0x06,0x25,0x44,
	0x63,0x03,0x22,0x41,0x60,0x00
};
static unsigned char B2144[] =
{
	0x14,0x73,0x43,0x03,0x42,
	0x02,0x41,0x01,0x40,0x00
};
static unsigned char B2154[] =
{
	0x74,0x14,0x43,0x03,0x42,
	0x02,0x41,0x01,0x40,0x00
};
static unsigned char B2164[] =
{
	0x14,0x53,0x13,0x52,0x12,
	0x51,0x11,0x50,0xa0,0x80
};
static unsigned char B2174[] =
{
	0x74,0x34,0x73,0x33,0x72,
	0x32,0x71,0x31,0xe0,0x80
};
static unsigned char B2184[] =
{
	0x23,0x62,0x22,0x61,0x21,
	0x70,0x40,0x20,0x00
};
static unsigned char B2193[] =
{
	0x42,0x42,0x52,0x72,0x13,
	0x43,0xf3,0x80,0x00
};
static unsigned char B2202[] =
{
	0x00,0x00,0x00,0x80,0x85,
	0x05,0x05,0x05,0x05
};
static unsigned char B2211[] =
{
	0x0c,0x59,0x47,0x55,0x04,
	0x52,0x41,0x50,0x00
};
static unsigned char B2220[] =
{
	0x00,0x10,0x30,0x50,0xe0,
	0x50,0x30,0x10,0x00
};
static unsigned char B2229[] =
{
	0x00,0x00,0x00,0x00,0x80,
	0x00,0x00,0x00,0x00
};
static unsigned char B2238[] =
{
	0x04,0x04,0x04,0x04,0x04,0x04,
	0x73,0xe3,0x33,0x52,0x41,0x00
};
static unsigned char B2250[] =
{
	0x44,0x04,0x43,0x03,0x42,
	0x02,0x41,0x01,0x40,0x00
};
static unsigned char B2260[] =
{
	0x41,0x41,0x41,0x41,0x41,0x41,
	0x31,0xa1,0x01,0xe0,0x30,0x00
};
static unsigned char W2272[] =
{
	0x18,0xc0,0x16,0x80,0x14,0x40,
	0x12,0x00,0x0f,0xc0,0x0d,0x80,
	0x0b,0x40,0x09,0x00,0x06,0xc0,
	0x04,0x80,0x02,0x40,0x00,0x00
};
static unsigned char B2296[] =
{
	0x7e,0x4c,0x1a,0x08,0x16,0x44,
	0x13,0x02,0x11,0x40,0x10,0x00
};
static unsigned char B2308[] =
{
	0x60,0x30,0x10,0x00,0x00,
	0x10,0x30,0x60,0x21
};
static unsigned char W2317[] =
{
	0x13,0x00,0x10,0xa0,0x0e,0x40,
	0x0b,0xe0,0x09,0x80,0x07,0x20,
	0x04,0xc0,0x02,0x60,0x00,0x00
};
static unsigned char B2335[] =
{
	0x00,0xe8,0x18,0x47,0x76,
	0x26,0x55,0x05,0x34
};
static unsigned char B2344[] =
{
	0x00,0x00,0x00,0x10,0x30,
	0x60,0x21,0x71,0x42
};
static unsigned char B2353[] =
{
	0x00,0x21,0x42,0x63,0x05,
	0x26,0x47,0x68,0x0a
};
static unsigned char B2362[] =
{
	0x00,0x60,0x31,0x71,0x32,
	0x72,0x33,0x73,0x34,0x74
};
static unsigned char B2372[] =
{
	0x00,0x20,0x50,0x11,0x51,
	0x12,0x52,0x13,0x53,0x14
};
static unsigned char B2382[] =
{
	0x00,0x40,0x01,0x41,0x02,
	0x42,0x03,0x43,0x94,0xf4
};
static unsigned char B2392[] =
{
	0x00,0x40,0x01,0x41,0x02,
	0x42,0x03,0x43,0xf3,0x94
};

#define	NUM_AMIGA_PIECE_Y	128

#define	MAX_Y_COORDS_PER_PIECE	(MAX_SEGMENTS_PER_PIECE + 1)
//
// y.coordinate.offsets
//
static struct AMIGA_PIECE_Y
	{
	unsigned char *amigaY;
	long words;	/* TRUE/FALSE - indicating whether or not co-ords are word sized */
	long size;
	} Amiga_Piece_Y[NUM_AMIGA_PIECE_Y] =
		{
		{B1037,FALSE,sizeof(B1037)},
		{B1051,FALSE,sizeof(B1051)},
		{W1060,TRUE,sizeof(W1060)},
		{B1078,FALSE,sizeof(B1078)},
		{B1092,FALSE,sizeof(B1092)},
		{B1106,FALSE,sizeof(B1106)},
		{B1116,FALSE,sizeof(B1116)},
		{B1126,FALSE,sizeof(B1126)},
		{B1135,FALSE,sizeof(B1135)},
		{B1144,FALSE,sizeof(B1144)},
		{B1158,FALSE,sizeof(B1158)},
		{B1172,FALSE,sizeof(B1172)},
		{B1181,FALSE,sizeof(B1181)},
		{B1190,FALSE,sizeof(B1190)},
		{W1202,TRUE,sizeof(W1202)},
		{B1226,FALSE,sizeof(B1226)},
		{W1235,TRUE,sizeof(W1235)},
		{B1259,FALSE,sizeof(B1259)},
		{B1271,FALSE,sizeof(B1271)},
		{B1281,FALSE,sizeof(B1281)},
		{B1290,FALSE,sizeof(B1290)},
		{B1300,FALSE,sizeof(B1300)},
		{B1309,FALSE,sizeof(B1309)},
		{B1319,FALSE,sizeof(B1319)},
		{B1329,FALSE,sizeof(B1329)},
		{B1338,FALSE,sizeof(B1338)},
		{B1347,FALSE,sizeof(B1347)},
		{B1357,FALSE,sizeof(B1357)},
		{B1367,FALSE,sizeof(B1367)},
		{B1376,FALSE,sizeof(B1376)},
		{B1385,FALSE,sizeof(B1385)},
		{B1394,FALSE,sizeof(B1394)},
		{B1403,FALSE,sizeof(B1403)},
		{B1412,FALSE,sizeof(B1412)},
		{B1424,FALSE,sizeof(B1424)},
		{B1433,FALSE,sizeof(B1433)},
		{B1442,FALSE,sizeof(B1442)},
		{B1451,FALSE,sizeof(B1451)},
		{B1463,FALSE,sizeof(B1463)},
		{B1472,FALSE,sizeof(B1472)},
		{B1481,FALSE,sizeof(B1481)},
		{B1493,FALSE,sizeof(B1493)},
		{B1505,FALSE,sizeof(B1505)},
		{B1517,FALSE,sizeof(B1517)},
		{B1529,FALSE,sizeof(B1529)},
		{B1538,FALSE,sizeof(B1538)},
		{B1547,FALSE,sizeof(B1547)},
		{B1556,FALSE,sizeof(B1556)},
		{B1565,FALSE,sizeof(B1565)},
		{B1574,FALSE,sizeof(B1574)},
		{B1586,FALSE,sizeof(B1586)},
		{B1598,FALSE,sizeof(B1598)},
		{B1612,FALSE,sizeof(B1612)},
		{B1622,FALSE,sizeof(B1622)},
		{B1631,FALSE,sizeof(B1631)},
		{W1640,TRUE,sizeof(W1640)},
		{B1658,FALSE,sizeof(B1658)},
		{B1667,FALSE,sizeof(B1667)},
		{B1676,FALSE,sizeof(B1676)},
		{B1685,FALSE,sizeof(B1685)},
		{B1694,FALSE,sizeof(B1694)},
		{B1703,FALSE,sizeof(B1703)},
		{B1712,FALSE,sizeof(B1712)},
		{B1721,FALSE,sizeof(B1721)},
		{B1731,FALSE,sizeof(B1731)},
		{B1740,FALSE,sizeof(B1740)},
		{W1750,TRUE,sizeof(W1750)},
		{B1768,FALSE,sizeof(B1768)},
		{B1777,FALSE,sizeof(B1777)},
		{B1786,FALSE,sizeof(B1786)},
		{B1795,FALSE,sizeof(B1795)},
		{B1804,FALSE,sizeof(B1804)},
		{B1814,FALSE,sizeof(B1814)},
		{B1823,FALSE,sizeof(B1823)},
		{B1832,FALSE,sizeof(B1832)},
		{B1841,FALSE,sizeof(B1841)},
		{B1850,FALSE,sizeof(B1850)},
		{B1862,FALSE,sizeof(B1862)},
		{B1871,FALSE,sizeof(B1871)},
		{W1880,TRUE,sizeof(W1880)},
		{B1904,FALSE,sizeof(B1904)},
		{NULL,FALSE,0},
		{B1918,FALSE,sizeof(B1918)},
		{B1927,FALSE,sizeof(B1927)},
		{B1936,FALSE,sizeof(B1936)},
		{B1950,FALSE,sizeof(B1950)},
		{B1964,FALSE,sizeof(B1964)},
		{B1973,FALSE,sizeof(B1973)},
		{B1983,FALSE,sizeof(B1983)},
		{B1993,FALSE,sizeof(B1993)},
		{B2005,FALSE,sizeof(B2005)},
		{B2014,FALSE,sizeof(B2014)},
		{B2023,FALSE,sizeof(B2023)},
		{B2032,FALSE,sizeof(B2032)},
		{B2042,FALSE,sizeof(B2042)},
		{W2051,TRUE,sizeof(W2051)},
		{W2069,TRUE,sizeof(W2069)},
		{W2087,TRUE,sizeof(W2087)},
		{W2105,TRUE,sizeof(W2105)},
		{B2123,FALSE,sizeof(B2123)},
		{B2132,FALSE,sizeof(B2132)},
		{B2144,FALSE,sizeof(B2144)},
		{B2154,FALSE,sizeof(B2154)},
		{B2164,FALSE,sizeof(B2164)},
		{B2174,FALSE,sizeof(B2174)},
		{B2184,FALSE,sizeof(B2184)},
		{B2193,FALSE,sizeof(B2193)},
		{B2202,FALSE,sizeof(B2202)},
		{B2211,FALSE,sizeof(B2211)},
		{B2220,FALSE,sizeof(B2220)},
		{B2229,FALSE,sizeof(B2229)},
		{B2238,FALSE,sizeof(B2238)},
		{B2250,FALSE,sizeof(B2250)},
		{B2260,FALSE,sizeof(B2260)},
		{W2272,TRUE,sizeof(W2272)},
		{B2296,FALSE,sizeof(B2296)},
		{B2308,FALSE,sizeof(B2308)},
		{W2317,TRUE,sizeof(W2317)},
		{B2335,FALSE,sizeof(B2335)},
		{B2344,FALSE,sizeof(B2344)},
		{NULL,FALSE,0},
		{NULL,FALSE,0},
		{B2353,FALSE,sizeof(B2353)},
		{NULL,FALSE,0},
		{B2362,FALSE,sizeof(B2362)},
		{B2372,FALSE,sizeof(B2372)},
		{B2382,FALSE,sizeof(B2382)},
		{B2392,FALSE,sizeof(B2392)}
		};

//
// straight 8
//
//O336	dc.b	4			offset for number.of.coords
//		dc.b	0			near.section.byte1
//		dc.b	$40,$03
//		dc.b	9*2			number.of.coords
//		dc.b	0			gives curve.to.left
//		dc.b	WIDTH.REDUCTION		road.width.reduction
//		dc.b	128			road.length.reduction
//		dc.b	$80,$01
//		dc.b	$20			section.steering.amount
//
static unsigned char Amiga_Piece0_XZ[9*8] =
{
	0x40,0x03,0x00,0x00,0xc0,0x04,0x00,0x00,
	0x40,0x03,0x00,0x01,0xc0,0x04,0x00,0x01,
	0x40,0x03,0x00,0x02,0xc0,0x04,0x00,0x02,
	0x40,0x03,0x00,0x03,0xc0,0x04,0x00,0x03,
	0x40,0x03,0x00,0x04,0xc0,0x04,0x00,0x04,
	0x40,0x03,0x00,0x05,0xc0,0x04,0x00,0x05,
	0x40,0x03,0x00,0x06,0xc0,0x04,0x00,0x06,
	0x40,0x03,0x00,0x07,0xc0,0x04,0x00,0x07,
	0x40,0x03,0x00,0x08,0xc0,0x04,0x00,0x08
};

//
// curve right 8
//
//O419	dc.b	12
//		dc.b	$80
//		dc.b	$a8,$0d,$00,$00,$00,$ff,$80,$68,$0a,$87
//		dc.b	9*2
//		dc.b	0
//		dc.b	WIDTH.REDUCTION
//		dc.b	135
//		dc.b	$80,$01
//		dc.b	$3e
//
static unsigned char Amiga_Piece1_XZ[9*8] =
{
	0x40,0x03,0x00,0x00,0xc0,0x04,0x00,0x00,
	0x4c,0x03,0x05,0x01,0xca,0x04,0xdf,0x00,
	0x73,0x03,0x07,0x02,0xeb,0x04,0xbc,0x01,
	0xb2,0x03,0x05,0x03,0x22,0x05,0x95,0x02,
	0x0a,0x04,0xfb,0x03,0x6d,0x05,0x68,0x03,
	0x7a,0x04,0xe7,0x04,0xcd,0x05,0x32,0x04,
	0x00,0x05,0xc8,0x05,0x40,0x06,0xf2,0x04,
	0x9c,0x05,0x9a,0x06,0xc5,0x06,0xa6,0x05,
	0x4c,0x06,0x5b,0x07,0x5b,0x07,0x4c,0x06
};

//
// curve left 8
//
//O510	dc.b	12
//		dc.b	$c0
//		dc.b	$57,$fa,$00,$00,$00,$01,$80,$e8,$08,$87
//		dc.b	9*2
//		dc.b	3
//		dc.b	WIDTH.REDUCTION
//		dc.b	135
//		dc.b	$80,$01
//		dc.b	$3e
//
static unsigned char Amiga_Piece3_XZ[9*8] =
{
	0x3f,0x03,0x00,0x00,0xbf,0x04,0x00,0x00,
	0x35,0x03,0xdf,0x00,0xb3,0x04,0x05,0x01,
	0x14,0x03,0xbc,0x01,0x8c,0x04,0x07,0x02,
	0xdd,0x02,0x95,0x02,0x4d,0x04,0x05,0x03,
	0x92,0x02,0x68,0x03,0xf5,0x03,0xfb,0x03,
	0x32,0x02,0x32,0x04,0x85,0x03,0xe7,0x04,
	0xbf,0x01,0xf2,0x04,0xff,0x02,0xc8,0x05,
	0x3a,0x01,0xa6,0x05,0x63,0x02,0x9a,0x06,
	0xa4,0x00,0x4c,0x06,0xb3,0x01,0x5b,0x07
};

//
// straight 13
//
//O601	dc.b	8
//		dc.b	$40
//		dc.b	$40,$ff,$00,$20,$80,$b5
//		dc.b	14*2
//		dc.b	0
//		dc.b	WIDTH.REDUCTION
//		dc.b	128
//		dc.b	$80,$01
//		dc.b	$20
//
static unsigned char Amiga_Piece4_XZ[14*8] =
{
	0x78,0xff,0x87,0x00,0x87,0x00,0x78,0xff,
	0x2c,0x00,0x3c,0x01,0x3c,0x01,0x2c,0x00,
	0xe1,0x00,0xf0,0x01,0xf0,0x01,0xe1,0x00,
	0x96,0x01,0xa5,0x02,0xa5,0x02,0x96,0x01,
	0x4a,0x02,0x5a,0x03,0x5a,0x03,0x4a,0x02,
	0xff,0x02,0x0e,0x04,0x0e,0x04,0xff,0x02,
	0xb3,0x03,0xc3,0x04,0xc3,0x04,0xb3,0x03,
	0x68,0x04,0x77,0x05,0x77,0x05,0x68,0x04,
	0x1d,0x05,0x2c,0x06,0x2c,0x06,0x1d,0x05,
	0xd1,0x05,0xe1,0x06,0xe1,0x06,0xd1,0x05,
	0x86,0x06,0x95,0x07,0x95,0x07,0x86,0x06,
	0x3a,0x07,0x4a,0x08,0x4a,0x08,0x3a,0x07,
	0xef,0x07,0xff,0x08,0xff,0x08,0xef,0x07,
	0xa4,0x08,0xb3,0x09,0xb3,0x09,0xa4,0x08
};

//
// curve right 9
//
//O728	dc.b	12
//		dc.b	$80
//		dc.b	$00,$10,$00,$00,$00,$ff,$90,$c0,$0c,$7a
//		dc.b	10*2
//		dc.b	0
//		dc.b	WIDTH.REDUCTION
//		dc.b	122
//		dc.b	$80,$01
//		dc.b	$32
//
static unsigned char Amiga_Piece6_XZ[10*8] =
{
	0x40,0x03,0x00,0x00,0xc0,0x04,0x00,0x00,
	0x4c,0x03,0x1c,0x01,0xca,0x04,0xfb,0x00,
	0x71,0x03,0x36,0x02,0xeb,0x04,0xf4,0x01,
	0xaf,0x03,0x4c,0x03,0x22,0x05,0xe9,0x02,
	0x04,0x04,0x5c,0x04,0x6d,0x05,0xd9,0x03,
	0x71,0x04,0x63,0x05,0xcd,0x05,0xc1,0x04,
	0xf5,0x04,0x60,0x06,0x41,0x06,0xa0,0x05,
	0x8e,0x05,0x50,0x07,0xc8,0x06,0x73,0x06,
	0x3b,0x06,0x32,0x08,0x61,0x07,0x3b,0x07,
	0xfc,0x06,0x03,0x09,0x0b,0x08,0xf4,0x07
};

//
// curve left 9
//
//O827	dc.b	12
//		dc.b	$c0
//		dc.b	$00,$f8,$00,$00,$00,$01,$90,$40,$0b,$7a
//		dc.b	10*2
//		dc.b	3
//		dc.b	WIDTH.REDUCTION
//		dc.b	122
//		dc.b	$80,$01
//		dc.b	$32
//
static unsigned char Amiga_Piece7_XZ[10*8] =
{
	0x40,0x03,0x00,0x00,0xc0,0x04,0x00,0x00,
	0x35,0x03,0xfb,0x00,0xb3,0x04,0x1c,0x01,
	0x14,0x03,0xf4,0x01,0x8e,0x04,0x36,0x02,
	0xdd,0x02,0xe9,0x02,0x50,0x04,0x4c,0x03,
	0x92,0x02,0xd9,0x03,0xfb,0x03,0x5c,0x04,
	0x32,0x02,0xc1,0x04,0x8e,0x03,0x63,0x05,
	0xbe,0x01,0xa0,0x05,0x0a,0x03,0x60,0x06,
	0x37,0x01,0x73,0x06,0x71,0x02,0x50,0x07,
	0x9e,0x00,0x3b,0x07,0xc4,0x01,0x32,0x08,
	0xf4,0xff,0xf4,0x07,0x03,0x01,0x03,0x09
};

//
// straight 11
//
//O926	dc.b	8
//		dc.b	$40
//		dc.b	$40,$ff,$00,$20,$7c,$b0
//		dc.b	12*2
//		dc.b	0
//		dc.b	WIDTH.REDUCTION
//		dc.b	124
//		dc.b	$80,$01
//		dc.b	$20
//
static unsigned char Amiga_Piece10_XZ[12*8] =
{
	0x78,0xff,0x87,0x00,0x87,0x00,0x78,0xff,
	0x32,0x00,0x41,0x01,0x41,0x01,0x32,0x00,
	0xec,0x00,0xfc,0x01,0xfc,0x01,0xec,0x00,
	0xa6,0x01,0xb6,0x02,0xb6,0x02,0xa6,0x01,
	0x60,0x02,0x70,0x03,0x70,0x03,0x60,0x02,
	0x1b,0x03,0x2a,0x04,0x2a,0x04,0x1b,0x03,
	0xd5,0x03,0xe4,0x04,0xe4,0x04,0xd5,0x03,
	0x8f,0x04,0x9f,0x05,0x9f,0x05,0x8f,0x04,
	0x49,0x05,0x59,0x06,0x59,0x06,0x49,0x05,
	0x03,0x06,0x13,0x07,0x13,0x07,0x03,0x06,
	0xbe,0x06,0xcd,0x07,0xcd,0x07,0xbe,0x06,
	0x78,0x07,0x87,0x08,0x87,0x08,0x78,0x07
};


// piece XZs after conversion from Amiga
static COORD_XZ Piece0_XZ[9*2];
static COORD_XZ Piece1_XZ[9*2];
static COORD_XZ Piece3_XZ[9*2];
static COORD_XZ Piece4_XZ[14*2];
static COORD_XZ Piece6_XZ[10*2];
static COORD_XZ Piece7_XZ[10*2];
static COORD_XZ Piece10_XZ[12*2];

// only 0,1,3,4,6,7,10 used below
static struct PIECE_TEMPLATE
    {
	long numSegments;
	// could eventually get rid of next value and just use type
	long curveToLeft;	// TRUE/FALSE
	long type;			// near.section.byte1 - 0x00 STRAIGHT, 0x40 DIAGONAL (45 degrees)
						//						0x80 CURVE RIGHT, 0Xc0 CURVE LEFT
	long lengthReduction;
	long steeringAmount;
	COORD_XZ *pieceXZ;
	} Piece_Templates[11] =
		{
		{8,  FALSE, 0x00, 128, 0x20, Piece0_XZ},
		{8,  FALSE, 0x80, 135, 0x3e, Piece1_XZ},
		{0,  FALSE, 0x00, 0,   0x00, NULL},
		{8,  TRUE,  0xc0, 135, 0x3e, Piece3_XZ},
		{13, FALSE, 0x40, 128, 0x20, Piece4_XZ},
		{0,  FALSE, 0x00, 0,   0x00, NULL},
		{9,  FALSE, 0x80, 122, 0x32, Piece6_XZ},
		{9,  TRUE,  0xc0, 122, 0x32, Piece7_XZ},
		{0,  FALSE, 0x00, 0,   0x00, NULL},
		{0,  FALSE, 0x00, 0,   0x00, NULL},
		{11, FALSE, 0x40, 124, 0x20, Piece10_XZ}
		};

// piece Ys after conversion from Amiga
// (could allocate memory individually, as some have less than MAX_Y_COORDS_PER_PIECE,
//  but this wouldn't save much memory and would have to free it before exit)
static COORD_Y Piece_Y[NUM_AMIGA_PIECE_Y][MAX_Y_COORDS_PER_PIECE];

/*	===================== */
/*	Function declarations */
/*	===================== */

static void ConvertAmigaPieceData( void );

static void GetRotatedPieceXZ( COORD_XZ coord,
							   long roughAngle,
							   long *x,
							   long *z );

static void ConvertAmigaPieceXZ( unsigned char *amiga,
								 COORD_XZ *dest,
								 long size );

static void ConvertAmigaPieceY( AMIGA_PIECE_Y *amiga,
								COORD_Y *dest );

static long CoordVisible( long *xptr,
						  long *yptr,
						  long *zptr,
						  long piece_x,
						  long piece_y,
						  long piece_z,
						  long viewpoint_x,
						  long viewpoint_y,
						  long viewpoint_z );

static void UpdateDrawBridgeYCoords( long piece,
									 long firstCoord,
									 long lastCoord,
									 long firstYIndex,
									 long direction );

static long ReadAmigaTrackData( long track );
static void *GetTRACKResource( HMODULE hModule, LPCWSTR lpResName );

/*	======================================================================================= */
/*	Function:		GetTrackName															*/
/*																							*/
/*	Description:	Provide name of required Track ID										*/
/*	======================================================================================= */

WCHAR *GetTrackName( long track )
{
static WCHAR trackNames[][32] =
    					   {L"Little Ramp",
    						L"Stepping Stones",
    						L"Hump Back",
    						L"Big Ramp",
    						L"Ski Jump",
       						L"Draw Bridge",
    						L"High Jump",
    						L"Roller Coaster"};

    return(trackNames[track]);
}

/*	======================================================================================= */
/*	Function:		GetPieceAngleAndTemplate												*/
/*																							*/
/*	Description:	Provide Piece_Angle_And_Template value for required Track piece			*/
/*	======================================================================================= */

char GetPieceAngleAndTemplate( long piece )
{
	return(Piece_Angle_And_Template[piece]);
}

/*	======================================================================================= */
/*	Function:		ConvertAmigaTrack														*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

long TrackID = NO_TRACK;

TRACK_PIECE Track[MAX_PIECES_PER_TRACK];

long Track_Map[NUM_TRACK_CUBES][NUM_TRACK_CUBES];	// [x][z]

long NumTrackPieces;		// Part of track definition (number.of.road.sections)
long NumTrackSegments;		// Calculated by ConvertAmigaTrack() below
long PlayersStartPiece;		// Part of track definition (players.start.section)
long StartLinePiece;		// Part of track definition (near.start.line.section)
long HalfALapPiece;		// Part of track definition, but currently calculated by ReadAmigaTrackData() below
long StandardBoost, SuperBoost;


long ConvertAmigaTrack( long track )
	{
	static long first_time = TRUE;
	char c;
	long piece, size, templateNum, roughAngle, reverseOrder;
	long numSegments, firstSegment, numCoords, i, j, step;
	long x, y, y1, y2, z, leftID, rightID, leftOverallShift, rightOverallShift;
	COORD_XZ *pieceXZ;

	BYTE roadColour, sidesColour;
	if ( first_time )
	    {
		ConvertAmigaPieceData();
		first_time = FALSE;
		}

	if (TrackID != NO_TRACK)
		FreeTrackData();

	// set the required track
	if (! ReadAmigaTrackData(track))
		{
		TrackID = NO_TRACK;
		return(FALSE);
		}

	// store track ID
	TrackID = track;

	// clear the Track Map
	for (x = 0; x < NUM_TRACK_CUBES; x++)
		for (z = 0; z < NUM_TRACK_CUBES; z++)
			Track_Map[x][z] = -1;

	// convert each piece of track in turn
	firstSegment = 0;
	for (piece = 0; piece < NumTrackPieces; piece++)
		{
		// store x,y,z of piece's cube
		c = Piece_X_Z_Position[piece];
		x = static_cast<long>(c) & 0x0f;
		y = 0;
		z = (static_cast<long>(c) & 0xf0) >> 4;
		Track[piece].x = x;
		Track[piece].y = y;
		Track[piece].z = z;


		// record piece within Track Map
		Track_Map[x][z] = piece;


		// get piece template information
		c = Piece_Angle_And_Template[piece];
		templateNum = static_cast<long>(c) & 0x0f;
		roughAngle = static_cast<long>(c) & 0xc0;
		reverseOrder = static_cast<long>(c) & 0x10;

		Track[piece].roughPieceAngle = (roughAngle * MAX_ANGLE) / 0x100;
		Track[piece].oppositeDirection = (reverseOrder == 0 ? FALSE : TRUE);
		Track[piece].curveToLeft = Piece_Templates[templateNum].curveToLeft;
		Track[piece].type = Piece_Templates[templateNum].type;
		Track[piece].lengthReduction = Piece_Templates[templateNum].lengthReduction;
		Track[piece].steeringAmount = Piece_Templates[templateNum].steeringAmount;


		// store number of segments
		numSegments = Piece_Templates[templateNum].numSegments;

		Track[piece].numSegments = numSegments;
		Track[piece].firstSegment = firstSegment;
		firstSegment += numSegments;
		/*
		Track[piece].minZDifference = 0;
		*/


		// store initial road lines colour
		Track[piece].initialColour = (static_cast<long>(Right_Y_Coordinate_ID[piece]) & 0x80) >> 7;


		// decide road/side surface colours
		if (piece & 1)
			{
			// odd numbered section (light)
				roadColour = SCR_BASE_COLOUR + 2;			// grey surface in both leagues
				if(bSuperLeague) {
					sidesColour = SCR_BASE_COLOUR + 16;		// blue edges
				} else {
					sidesColour = SCR_BASE_COLOUR + 10;
				}
			}
		else
			{
			// even numbered section (dark)
				roadColour = SCR_BASE_COLOUR + 1;			// grey surface in both leagues
				sidesColour = SCR_BASE_COLOUR + 15;
			}


		// allocate memory for unrotated co-ordinates
		size = sizeof(COORD_3D) * (numSegments + 1) * 4;
		Track[piece].coords = (COORD_3D *)malloc(size);
		Track[piece].coordsSize = size;
		if (Track[piece].coords == NULL)
			return(FALSE);

		/*
		// allocate memory for transformed co-ordinates
		size = sizeof(COORD_3D) * (numSegments + 1) * 4;
		Track[piece].transformed_coords = (COORD_3D *)malloc(size);
		if (Track[piece].transformed_coords == NULL)
			{
			free(Track[piece].coords);
			return(FALSE);
			}

		// allocate memory for screen co-ordinates
		size = sizeof(COORD_2D) * (numSegments + 1) * 4;
		Track[piece].screen_coords = (COORD_2D *)malloc(size);
		if (Track[piece].screen_coords == NULL)
			{
			free(Track[piece].coords);
			free(Track[piece].transformed_coords);
			return(FALSE);
			}
		*/

		// locate piece's x,z information
		pieceXZ = Piece_Templates[templateNum].pieceXZ;


		// store piece x,z using template
		numCoords = numSegments + 1;

		if (reverseOrder)
			{
			j = (numCoords * 2) - 1;
			step = -1;
			}
		else
			{
			j = 0;
			step = 1;
			}

		for (i = 0; i < numCoords; i++)
			{
			GetRotatedPieceXZ(pieceXZ[j], roughAngle, &x, &z);

			x *= PC_FACTOR; z *= PC_FACTOR;

			// top left x, z
			Track[piece].coords[(i*4)].x = x;
			Track[piece].coords[(i*4)].z = z;

			// bottom left x, z
			Track[piece].coords[(i*4)+2].x = x;
			Track[piece].coords[(i*4)+2].z = z;
			j += step;

			GetRotatedPieceXZ(pieceXZ[j], roughAngle, &x, &z);

			x *= PC_FACTOR; z *= PC_FACTOR;

			// top right x, z
			Track[piece].coords[(i*4)+1].x = x;
			Track[piece].coords[(i*4)+1].z = z;

			// bottom right x, z
			Track[piece].coords[(i*4)+3].x = x;
			Track[piece].coords[(i*4)+3].z = z;
			j += step;
			}


		// store piece y using IDs and overall shifts
		leftID = static_cast<long>(Left_Y_Coordinate_ID[piece]) & 0x7f;
		rightID = static_cast<long>(Right_Y_Coordinate_ID[piece]) & 0x7f;

		leftOverallShift = static_cast<long>(Left_Overall_Y_Shift[piece]);
		rightOverallShift = static_cast<long>(Right_Overall_Y_Shift[piece]);

		for (i = 0; i < numCoords; i++)
			{
			// top left y
			y = Piece_Y[leftID][i].y;
			y += leftOverallShift;
			Track[piece].coords[(i*4)].y = (y * PC_FACTOR);

			// bottom left y
			Track[piece].coords[(i*4)+2].y = TRACK_BOTTOM_Y;

			// top right y
			y = Piece_Y[rightID][i].y;
			y += rightOverallShift;
			Track[piece].coords[(i*4)+1].y = (y * PC_FACTOR);

			// bottom right y
			Track[piece].coords[(i*4)+3].y = TRACK_BOTTOM_Y;
			}


		// set road/side surface colours
		Track[piece].sidesColour = sidesColour;

		for (i = 0; i < numSegments; i++)
			{
			y1 = Track[piece].coords[(i*4)].y - Track[piece].coords[(i+1)*4].y;
			y2 = Track[piece].coords[(i*4)+1].y - Track[piece].coords[((i+1)*4)+1].y;

			// get maximum of two (but keep sign)
			y = (abs(y1) > abs(y2) ? y1 : y2);

			// roadColour needs to be black (colour 0)
			// if change in piece y co-ords is >= 640
			if (abs(y) >= (640 * PC_FACTOR))
				{
				Track[piece].roadColour[i] = SCR_BASE_COLOUR + 0;

				/*
				// adjust z comparison value for pieces that have a large
				// step in them - this should overcome the priority problem
				// between the low road surface and the side surface when
				// viewing the piece from one side
				if (y < 0)
					Track[piece].minZDifference = (14000000 * PC_FACTOR);
				else
					Track[piece].minZDifference = (-14000000 * PC_FACTOR);
				*/
				}
			else
				Track[piece].roadColour[i] = roadColour;
			}
		}
	NumTrackSegments = firstSegment;


	// ensure pieces join up perfectly
	// this is done by copying start co-ordinates of
	// current piece to end co-ordinates of last piece
	for (piece = 0; piece < NumTrackPieces; piece++)
		{
		long lastPiece;
		long cubeX, cubeY, cubeZ;
		long lastCubeX, lastCubeY, lastCubeZ;

		if (piece > 0)
			lastPiece = piece - 1;
		else
			lastPiece = NumTrackPieces - 1;

		// Note that pieces are in different 'cubes', so need to take
		// cube x,y,z (stored with piece) change into account

		// calculate position of current piece's bottom front left corner, within world
		cubeX = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
		cubeY = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
		cubeZ = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);

		// calculate position of last piece's bottom front left corner, within world
		lastCubeX = Track[lastPiece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
		lastCubeY = Track[lastPiece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
		lastCubeZ = Track[lastPiece].z << (LOG_CUBE_SIZE-LOG_PRECISION);

		// copy first four co-ordinates to end of last piece
		j = Track[lastPiece].numSegments * 4;
		for (i = 0; i < 4; i++, j++)
			{
		    Track[lastPiece].coords[j].x = Track[piece].coords[i].x + cubeX - lastCubeX;
		    Track[lastPiece].coords[j].y = Track[piece].coords[i].y + cubeY - lastCubeY;
		    Track[lastPiece].coords[j].z = Track[piece].coords[i].z + cubeZ - lastCubeZ;
			}
		}

	// build the C#-shaped view of this track for the FloatV2 physics port
	scr::FV2_BuildTrackView(NumTrackPieces,
							Piece_Angle_And_Template,
							Left_Y_Coordinate_ID,
							Right_Y_Coordinate_ID,
							Left_Overall_Y_Shift,
							Right_Overall_Y_Shift);

	return(TRUE);
	}

/*	======================================================================================= */
/*	Function:		FV2_GetRawYCoord														*/
/*																							*/
/*	Description:	Decoded (pre-shift) Amiga Y coordinate, for Track_FloatV2.cpp			*/
/*	======================================================================================= */

namespace scr {

long FV2_GetRawYCoord( int yCoordId, int coordIndex )
	{
	if ((yCoordId < 0) || (yCoordId >= NUM_AMIGA_PIECE_Y) ||
		(coordIndex < 0) || (coordIndex >= MAX_Y_COORDS_PER_PIECE))
		return(0);

	return(Piece_Y[yCoordId][coordIndex].y);
	}

} // namespace scr

/*	======================================================================================= */
/*	Function:		GetRotatedPieceXZ														*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static void GetRotatedPieceXZ( COORD_XZ coord,
							   long roughAngle,
							   long *x,
							   long *z )
	{
	switch(roughAngle)
		{
		case 0x00:		// 0 degrees
						*x = coord.x;
						*z = coord.z;
						break;

		case 0x40:		// 90 degrees clockwise
						*x = coord.z;
						*z = 0x800 - coord.x;
						break;

		case 0x80:		// 180 degrees
						*x = 0x800 - coord.x;
						*z = 0x800 - coord.z;
						break;

		case 0xc0:		// 270 degrees clockwise
						*x = 0x800 - coord.z;
						*z = coord.x;
						break;
		}
	}

/*	======================================================================================= */
/*	Function:		FreeTrackData															*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

void FreeTrackData( void )
	{
	long piece;

	for (piece = 0; piece < NumTrackPieces; piece++)
		{
		free(Track[piece].coords);
		/*
		free(Track[piece].transformed_coords);
		free(Track[piece].screen_coords);
		*/
		}
	}

/*	======================================================================================= */
/*	Function:		ConvertAmigaPieceData													*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static void ConvertAmigaPieceData( void )
	{
	long i;

	// convert XZ data
	ConvertAmigaPieceXZ(Amiga_Piece0_XZ, Piece0_XZ, sizeof(Piece0_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece1_XZ, Piece1_XZ, sizeof(Piece1_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece3_XZ, Piece3_XZ, sizeof(Piece3_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece4_XZ, Piece4_XZ, sizeof(Piece4_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece6_XZ, Piece6_XZ, sizeof(Piece6_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece7_XZ, Piece7_XZ, sizeof(Piece7_XZ));
	ConvertAmigaPieceXZ(Amiga_Piece10_XZ, Piece10_XZ, sizeof(Piece10_XZ));

	// convert Y data
	for (i = 0; i < NUM_AMIGA_PIECE_Y; i++)
		ConvertAmigaPieceY(&Amiga_Piece_Y[i], Piece_Y[i]);
	}

/*	======================================================================================= */
/*	Function:		ConvertAmigaPieceXZ														*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static void ConvertAmigaPieceXZ( unsigned char *amiga,
								 COORD_XZ *dest,
								 long size )
	{
	long i, number;
	char low, high;
	short value;
	long x, z;

	// calculate number of co-ordinates
	number = size / sizeof(COORD_XZ);

	for (i = 0; i < number; i++)
		{
		low = (char)*amiga++;
		high = (char)*amiga++;

		value = ((high & 0xff) << 8) | (low & 0xff);
		x = static_cast<long>(value);

		low = (char)*amiga++;
		high = (char)*amiga++;

		value = ((high & 0xff) << 8) | (low & 0xff);
		z = static_cast<long>(value);

		dest->x = x;
		dest->z = z;
		dest++;
		}
	}

/*	======================================================================================= */
/*	Function:		ConvertAmigaPieceY														*/
/*																							*/
/*	Description:																			*/
/*	======================================================================================= */

static void ConvertAmigaPieceY( AMIGA_PIECE_Y *amiga,
								COORD_Y *dest )
	{
	long i, number;
	char *yptr;
	char low, high;
	short value;
	long y, ya, yb;

	// calculate number of co-ordinates
	number = amiga->size / sizeof(char);
	yptr = (char *)amiga->amigaY;
	if (amiga->words == TRUE)
		{
		number /= 2;	// half the number of co-ordinates, if words
		for (i = 0; i < number; i++)
			{
			high = *yptr++;
			low = *yptr++;

			value = ((high & 0x7f) << 8) | (low & 0xff);
			y = static_cast<long>(value);

			dest->y = y;
			dest++;
			}
		}
	else
		{
		for (i = 0; i < number; i++)
			{
			ya = yb = static_cast<long>(*yptr++) & 0xff;

			ya <<= 1;
			ya &= 0xe0;
			yb &= 0x0f;
			yb <<= 8;

			y = ya | yb;

			dest->y = y;
			dest++;
			}
		}
	}

/*	======================================================================================= */
/*	Function:		DrawTrack																*/
/*																							*/
/*	Description:	Draw the current track using the supplied viewpoint						*/
/*	======================================================================================= */

#define	MAX_VERTICES_PER_TRACK	(MAX_PIECES_PER_TRACK * MAX_SEGMENTS_PER_PIECE * 6 * 3)	// In TRIANGLELIST mode (6 triangles per segment, road and 2 sides)
#define	MAX_VERTICES_PER_SHADOW	(2*3)	// In TRIANGLELIST mode

typedef enum
	{
	LEFT_SIDE = 0,
	RIGHT_SIDE,
	ROAD,
	NUM_TRACK_FACES
	} TrackFaceType;

static IDirect3DVertexBuffer9 *pTrackVB = NULL, *pShadowVB = NULL, *pStartLineVB = NULL;
static long trackVertices, trackSegments;
static long numShadowVertices;
static long PieceFirstVertex[NUM_TRACK_FACES][MAX_PIECES_PER_TRACK];
static long SegmentRoadTexture[MAX_PIECES_PER_TRACK * MAX_SEGMENTS_PER_PIECE];

#ifdef SCR_ROAD_TEXTURE
/*
 * Texture V at the near edge of each segment, accumulated along the track so the generated
 * road texture (RoadTexture.cpp) tiles continuously instead of restarting per segment - a
 * per-segment 0..1 would make the grain stretch and snap as segment lengths change.
 * One extra entry holds the far edge of the last segment.
 */
static float SegmentTexV[MAX_PIECES_PER_TRACK * MAX_SEGMENTS_PER_PIECE + 1];

// Set either side of each road quad by the ROAD cases of the two CreateUpdatePieceInVB
// functions, so the vertex-storing helpers below don't need another two arguments.
static float roadTexVNear = 0.0f, roadTexVFar = 1.0f;

static void SetSegmentTexCoords (void)
{
long  piece, s, numSegments, segment = 0;
double distance = 0.0, roadWidth = 0.0;

	if (NumTrackPieces <= 0)
		return;

	/*
	 * 1) Walk the track accumulating the world-space distance along it.  A segment's length
	 *    is the gap between the midpoints of its near (coords 0,1) and far (coords 4,5)
	 *    edges; the piece origin is common to both so it cancels and can be ignored.
	 */
	for (piece = 0; piece < NumTrackPieces; piece++)
	{
		numSegments = Track[piece].numSegments;

		for (s = 0; s < numSegments; s++)
		{
			const long o = s * 4;
			const COORD_3D *c = Track[piece].coords;

			// y is divided by 4 for display (see GetPieceVertex), so do the same here or
			// steep sections would stretch the grain.
			const double nx = (double)(c[o+0].x + c[o+1].x) * 0.5;
			const double ny = (double)(c[o+0].y + c[o+1].y) * 0.5 * 0.25;
			const double nz = (double)(c[o+0].z + c[o+1].z) * 0.5;
			const double fx = (double)(c[o+4].x + c[o+5].x) * 0.5;
			const double fy = (double)(c[o+4].y + c[o+5].y) * 0.5 * 0.25;
			const double fz = (double)(c[o+4].z + c[o+5].z) * 0.5;

			const double dx = fx - nx, dy = fy - ny, dz = fz - nz;

			SegmentTexV[segment] = (float)distance;
			distance += sqrt(dx*dx + dy*dy + dz*dz);

			if (roadWidth == 0.0)
			{
				const double wx = (double)(c[o+1].x - c[o+0].x);
				const double wy = (double)(c[o+1].y - c[o+0].y) * 0.25;
				const double wz = (double)(c[o+1].z - c[o+0].z);
				roadWidth = sqrt(wx*wx + wy*wy + wz*wz);
			}

			segment++;
		}
	}
	SegmentTexV[segment] = (float)distance;

	if ((distance <= 0.0) || (roadWidth <= 0.0) || (GetRoadTextureWidth() <= 0))
		return;

	/*
	 * 2) Convert distance to tiles.  One tile spans as much road length as it does width,
	 *    scaled by its own texel dimensions, which keeps the generated texels square.
	 */
	const double worldPerTexel = roadWidth / (double)GetRoadTextureWidth();
	double tileLength = worldPerTexel * (double)GetRoadTextureHeight();
	if (tileLength <= 0.0)
		return;

	/*
	 * 3) The track is a loop, so the last segment's far edge is the first segment's near
	 *    edge.  Stretch the tile length very slightly so a whole number of tiles fits and
	 *    the grain doesn't jump at the start line.
	 */
	double tiles = distance / tileLength;
	double wholeTiles = floor(tiles + 0.5);
	if (wholeTiles >= 1.0)
		tileLength = distance / wholeTiles;

	for (long i = 0; i <= segment; i++)
		SegmentTexV[i] = (float)((double)SegmentTexV[i] / tileLength);
}
#endif	// SCR_ROAD_TEXTURE

static void SetSegmentTextures (void)
{
long piece, rlc, s, numSegments, t;
BYTE roadColourIndex;

	trackSegments = 0;
	for (piece = 0; piece < NumTrackPieces; piece++)
	{
		numSegments = Track[piece].numSegments;
		rlc = Track[piece].initialColour;	// get initial colour of road side lines

		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			roadColourIndex = Track[piece].roadColour[s];
			// (The start line itself is not a whole white segment - it is drawn
			//  separately as a thin white band, see DrawStartLine())
			if (roadColourIndex == SCR_BASE_COLOUR + 1)	// darker
			{
				if (rlc == 0)
					t = 0;
				else
					t = 2;
			}
			else if (roadColourIndex == SCR_BASE_COLOUR + 2)	// lighter
			{
				if (rlc == 0)
					t = 1;
				else
					t = 3;
			}
			else if (roadColourIndex == SCR_BASE_COLOUR + 18)
			{
				if (rlc == 0)
					t = 6+0;
				else
					t = 6+2;
			}
			else if (roadColourIndex == SCR_BASE_COLOUR + 17)	// lighter
			{
				if (rlc == 0)
					t = 6+1;
				else
					t = 6+3;
			}
			else// if (roadColourIndex == SCR_BASE_COLOUR)	// black
			{
				t = 4;
			}
			SegmentRoadTexture[trackSegments++] = eRoadYellowDark + t;

			// switch to other road side lines colour
			rlc = (rlc == 0) ? 1 : 0;
		}
	}
}


static D3DXVECTOR3 GetPieceVertex( long piece, long piece_x, long piece_y, long piece_z, long offset )
{
long x, y, z;
D3DXVECTOR3 v;

	x = Track[piece].coords[offset].x;
	y = Track[piece].coords[offset].y;
	// y co-ordinates need to be divided by 4 for display
	y = y/4;
	z = Track[piece].coords[offset].z;

	x += piece_x;
	y += piece_y;
	z += piece_z;

	v = D3DXVECTOR3( static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) );
	return(v);
}


/*
 * Texture coordinates for one corner of a road quad.  uRight picks the far side of the road
 * across its width, vFar the far end along its length.
 *
 * With the generated road texture the quad maps the whole texture across its width and takes
 * V from distance along the track, so the surface grain tiles continuously.  Without it we
 * keep the original behaviour: the atlas road cell is only there to draw the side lines, so
 * every corner samples the same single row of it (atlas_ty1) and the surface stays flat.
 */
static void SetRoadVertexUV( UTVERTEX *pVertex, long piece, long s, bool uRight, bool vFar )
{
#ifdef SCR_ROAD_TEXTURE
	pVertex->tu = uRight ? 1.0f : 0.0f;
	pVertex->tv = vFar ? roadTexVFar : roadTexVNear;
#else
	const long t = SegmentRoadTexture[Track[piece].firstSegment+s];
	pVertex->tu = uRight ? atlas_tx2[t] : atlas_tx1[t];
	pVertex->tv = atlas_ty1[t];
	(void)vFar;
#endif
}


/*
 * ------------------------------------------------------------------------------------------
 * Directional shading of the track faces.
 *
 * A single fixed sun, no per-frame cost: the track is static geometry, so the shade is baked
 * into the vertex colours when the vertex buffer is built.  The lambert term is quantised to
 * a handful of steps so faces stay flat-shaded and the Amiga look survives.
 *
 * The sun is deliberately well off vertical.  An overhead sun would agree more neatly with the
 * opponent's shadow (which drops straight down), but it puts a flat road at full brightness -
 * and then nothing can be brighter than flat, so climbs and banking can only ever darken and
 * mostly land in the same quantisation bucket as flat.  Tilting the sun sits a flat road in
 * mid-range and lets the track deviate in both directions.
 *
 * GetPieceVertex returns display vertices, which have y/4 applied.  Shading those directly
 * flattens every normal by 4x and the road comes out all one tone, so the y component of each
 * edge is scaled back up by SHADE_Y_UNSQUASH first - the lighting follows the track's true
 * slope rather than its foreshortened drawn slope.
 *
 * These numbers were picked against a spread of representative faces (flat, banked both ways,
 * climbing, descending, vertical sides) to keep as many distinct shade steps as possible.
 * They are a starting point tuned on paper, not by eye - adjust to taste.
 * ------------------------------------------------------------------------------------------
 */
#define SHADE_LEVELS		(8)			// quantisation steps - keep low to stay flat-shaded
#define SHADE_AMBIENT		(0.40f)		// floor, so an unlit face is never black
#define SHADE_Y_UNSQUASH	(4.0f)		// undoes the y/4 in GetPieceVertex

// Direction from a surface toward the sun (need not be unit length, we normalise below)
#define SUN_X	(0.45f)
#define SUN_Y	(0.80f)
#define SUN_Z	(-0.40f)

// Shade a level road (normal 0,1,0) would get - everything is scaled so that comes out at 1.0
#define SUN_LENGTH				(sqrtf((SUN_X*SUN_X) + (SUN_Y*SUN_Y) + (SUN_Z*SUN_Z)))
#define SHADE_LEVEL_REFERENCE	(SHADE_AMBIENT + ((1.0f - SHADE_AMBIENT) * (SUN_Y / SUN_LENGTH)))

static float FaceShade( const D3DXVECTOR3 &v1, const D3DXVECTOR3 &v2, const D3DXVECTOR3 &v3 )
{
float e1x, e1y, e1z, e2x, e2y, e2z;
float nx, ny, nz, n_len;
float lambert, shade;

	if (!gTrackLighting)
		return(1.0f);

	// Surface normal, by hand - the D3DXVec3 helpers don't exist in the non-Windows shim
	e1x = v2.x - v1.x;	e1y = (v2.y - v1.y) * SHADE_Y_UNSQUASH;		e1z = v2.z - v1.z;
	e2x = v3.x - v2.x;	e2y = (v3.y - v2.y) * SHADE_Y_UNSQUASH;		e2z = v3.z - v2.z;

	nx = (e1y * e2z) - (e1z * e2y);
	ny = (e1z * e2x) - (e1x * e2z);
	nz = (e1x * e2y) - (e1y * e2x);

	n_len = sqrtf((nx*nx) + (ny*ny) + (nz*nz));

	// Degenerate triangle (repeated vertices do occur in the strips) - leave it unshaded
	if (n_len == 0.0f)
		return(1.0f);

	// Winding is not consistent between the road and the two sides (and the sides are built
	// facing outwards in opposite directions), so take the magnitude rather than trusting the
	// normal's sign.  A track face is lit the same from either side, which is what we want -
	// there is no "underneath" of the track that the player ever sees lit differently.
	lambert = ((nx*SUN_X) + (ny*SUN_Y) + (nz*SUN_Z))
				/ (n_len * sqrtf((SUN_X*SUN_X) + (SUN_Y*SUN_Y) + (SUN_Z*SUN_Z)));
	if (lambert < 0.0f) lambert = -lambert;

	shade = SHADE_AMBIENT + ((1.0f - SHADE_AMBIENT) * lambert);

	/*
	 * Normalise against a level road, so a flat piece keeps its palette colour exactly and only
	 * the deviations from level darken.  Without this every face comes out below full brightness
	 * and the whole track just looks washed out rather than lit - the palette is the Amiga's and
	 * it wants to stay vivid.  The cost is that faces tilted further into the sun than level
	 * can't go brighter than the base colour; they clamp.  That is the right trade here, as
	 * blowing the road out toward white would look far worse than losing a step at the top.
	 */
	shade /= SHADE_LEVEL_REFERENCE;

	// Quantise
	shade = static_cast<float>(static_cast<long>((shade * SHADE_LEVELS) + 0.5f)) / SHADE_LEVELS;
	if (shade > 1.0f) shade = 1.0f;

	return(shade);
}


static void StorePieceTriangle( long piece, long piece_x, long piece_y, long piece_z, long offset1, long offset2, long offset3, UTVERTEX *pVertices, long colour_index, short txind, long s )
{
D3DXVECTOR3 v1, v2, v3;
DWORD colour;

	v1 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset1 );
	v2 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset2 );
	v3 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset3 );

	colour = SCRGBShaded( colour_index, FaceShade( v1, v2, v3 ) );

	pVertices[trackVertices].pos = v1;
	pVertices[trackVertices].color = colour;
	// triangle 1 is (near left, far left, far right), triangle 2 (near left, far right, near right)
	if (txind != 0)
		SetRoadVertexUV( &pVertices[trackVertices], piece, s, false, false );
	++trackVertices;

	pVertices[trackVertices].pos = v2;
	pVertices[trackVertices].color = colour;
	if (txind != 0)
		SetRoadVertexUV( &pVertices[trackVertices], piece, s, (txind == 2), true );
	++trackVertices;

	pVertices[trackVertices].pos = v3;
	pVertices[trackVertices].color = colour;
	if (txind != 0)
		SetRoadVertexUV( &pVertices[trackVertices], piece, s, true, (txind == 1) );
	++trackVertices;
}


// Fetch and store the piece vertex identified by offset1 (offset2 and 3 are just used to calculate the surface normal)
static void StorePieceVertex1( long piece, long piece_x, long piece_y, long piece_z, long offset1, long offset2, long offset3, UTVERTEX *pVertices, long colour_index, short txind, long s )
{
D3DXVECTOR3 v1, v2, v3;

	v1 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset1 );
	v2 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset2 );
	v3 = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset3 );

	pVertices[trackVertices].pos = v1;
	pVertices[trackVertices].color = SCRGBShaded( colour_index, FaceShade( v1, v2, v3 ) );
	// Strip mode stores one vertex at a time; the road ones are always a far-edge corner,
	// txind picking which side of the road (except for the strip's two priming vertices,
	// where the caller points roadTexVFar at the near edge instead - see below).
	if (txind != 0)
		SetRoadVertexUV( &pVertices[trackVertices], piece, s, (txind == 2), true );
	++trackVertices;
}


void RemoveShadowTriangles( void )
{
	numShadowVertices = 0;
	return;
}

void StoreShadowTriangle( D3DXVECTOR3 v1, D3DXVECTOR3 v2, D3DXVECTOR3 v3, long other_colour )
{
//D3DXVECTOR3 edge1, edge2, surface_normal;
DWORD colour;

	UTVERTEX *pVertices;
	if( FAILED( pShadowVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock shadow vertex buffer for drawing\n");
		return;
	}

	/*
	// Calculate surface normal
	edge1 = v2-v1; edge2 = v3-v2;
	D3DXVec3Cross( &surface_normal, &edge1, &edge2 );
	D3DXVec3Normalize( &surface_normal, &surface_normal );
	*/

	if (other_colour)
		colour = SCRGB(SCR_BASE_COLOUR + 15);
	else
		colour = SCRGB(SCR_BASE_COLOUR + 5);

	pVertices[numShadowVertices].pos = v1;
//	pVertices[numShadowVertices].normal = surface_normal;
	pVertices[numShadowVertices].color = colour;
	++numShadowVertices;

	pVertices[numShadowVertices].pos = v2;
//	pVertices[numShadowVertices].normal = surface_normal;
	pVertices[numShadowVertices].color = colour;
	++numShadowVertices;

	pVertices[numShadowVertices].pos = v3;
//	pVertices[numShadowVertices].normal = surface_normal;
	pVertices[numShadowVertices].color = colour;
	++numShadowVertices;

	pShadowVB->Unlock();
}


// Create piece in vertex buffer as TRIANGLELIST
static void CreateUpdatePieceInVBMode1( long piece, long face, UTVERTEX *pVertices, bool create )		// piece was called roadSection
{
	long piece_x, piece_y, piece_z;
	long s, numSegments = Track[piece].numSegments, offset;
	long colour;			// palette index; shading is applied per face when stored
	/*
	BYTE roadLineColours[2] = {SCR_BASE_COLOUR + 3,	// yellow
								SCR_BASE_COLOUR + 10};	// red
	BYTE sidesLinesColour = SCR_BASE_COLOUR + 9;	// 0 for SUPER LEAGUE
	*/

	// Store index of piece's first vertex, only when creating (because update doesn't lock entire vertex buffer)
	// (used to update vertices, e.g. by MoveDrawBridge)
	if (create)
		PieceFirstVertex[face][piece] = trackVertices;

	// Calculate position of piece's bottom front left corner, within world
	piece_x = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_y = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_z = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);

	if (face == ROAD)	// create road (i.e. top)
	{
		BYTE roadColourIndex;

		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			roadColourIndex = Track[piece].roadColour[s];

			colour = roadColourIndex;
#ifdef SCR_ROAD_TEXTURE
			roadTexVNear = SegmentTexV[Track[piece].firstSegment + s];
			roadTexVFar  = SegmentTexV[Track[piece].firstSegment + s + 1];
#endif
			// triangle 1 (offsets 0,4,5)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset, offset+4, offset+5, pVertices, colour, 1, s);
			// triangle 2 (offsets 0,5,1)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset, offset+5, offset+1, pVertices, colour, 2, s);
		}
	}
	else if (face == LEFT_SIDE)	// create left side
	{
		colour = Track[piece].sidesColour;
		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			// triangle 1 (offsets 0,2,6)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset, offset+2, offset+6, pVertices, colour, 0, s);
			// triangle 2 (offsets 0,6,4)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset, offset+6, offset+4, pVertices, colour, 0, s);
		}
	}
	else	// create right side
	{
		colour = Track[piece].sidesColour;
		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			// triangle 1 (offsets 5,7,3)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset+5, offset+7, offset+3, pVertices, colour, 0, s);
			// triangle 2 (offsets 5,3,1)
			StorePieceTriangle(piece, piece_x, piece_y, piece_z, offset+5, offset+3, offset+1, pVertices, colour, 0, s);
		}
	}
}


// Create piece in vertex buffer as TRIANGLESTRIP
static void CreateUpdatePieceInVBMode2( long piece, long face, UTVERTEX *pVertices, bool create )		// piece was called roadSection
{
	long piece_x, piece_y, piece_z;
	long s, numSegments = Track[piece].numSegments, offset;
	BYTE roadColourIndex;
	long colour;			// palette index; shading is applied per face when stored
	/*
	BYTE roadLineColours[2] = {SCR_BASE_COLOUR + 3,	// yellow
								SCR_BASE_COLOUR + 10};	// red
	BYTE sidesLinesColour = SCR_BASE_COLOUR + 9;	// 0 for SUPER LEAGUE
	*/

	// Store index of piece's first vertex, only when creating (because update doesn't lock entire vertex buffer)
	// (used to update vertices, e.g. by MoveDrawBridge)
	if (create)
		PieceFirstVertex[face][piece] = trackVertices;

	// Calculate position of piece's bottom front left corner, within world
	piece_x = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_y = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
	piece_z = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);

	if (piece == 0)
	{
		// TRIANGLESTRIP begins with three vertices, so store first two vertices of triangle 1 here
		// Note that this causes two extra triangles to be drawn in the transition from left to right
		// and right to top, but this doesn't matter as these aren't visible.  (The alternative is
		// to create three separate strips and call DrawPrimitive separately for each.)
		if (face == ROAD)	// road (i.e. top)
		{
			roadColourIndex = Track[piece].roadColour[0];
			colour = roadColourIndex;
#ifdef SCR_ROAD_TEXTURE
			// These two prime the strip with the near edge of the very first segment, so
			// point the "far" V that StorePieceVertex1 uses at the start of the track.
			roadTexVFar = SegmentTexV[0];
#endif
			// triangle 1 (offsets 1,0,5)
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 1, 0, 5, pVertices, colour, 2, 0);
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 0, 5, 1, pVertices, colour, 1, 0);
		}
		else if (face == LEFT_SIDE)
		{
			colour = Track[piece].sidesColour;
			// triangle 1 (offsets 0,2,4)
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 0, 2, 4, pVertices, colour, 0, 0);
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 2, 4, 0, pVertices, colour, 0, 0);
		}
		else	// right side
		{
			colour = Track[piece].sidesColour;
			// triangle 1 (offsets 3,1,7)
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 3, 1, 7, pVertices, colour, 0, 0);
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, 1, 7, 3, pVertices, colour, 0, 0);
		}
	}

	// Direct3D TRIANGLESTRIP drawing appears to use the colour from the first vertex
	// of the three, so below we need to take the colours from the next segment along

	if (face == ROAD)	// create road (i.e. top)
	{
		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			if (s < numSegments-1)
			{
				roadColourIndex = Track[piece].roadColour[s+1];
			}
			else
			{
				if (piece == (NumTrackPieces-1))
					roadColourIndex = Track[0].roadColour[0];
				else
					roadColourIndex = Track[piece+1].roadColour[0];
			}

			colour = roadColourIndex;
#ifdef SCR_ROAD_TEXTURE
			roadTexVFar = SegmentTexV[Track[piece].firstSegment + s + 1];
#endif
			// store last vertex of triangle 1 (offsets 1,0,5) i.e. offset 5
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+5, offset+1, offset, pVertices, colour, 2, s);
			// store last vertex of triangle 2 (offsets 0,5,4) i.e. offset 4
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+4, offset, offset+5, pVertices, colour, 1, s);
		}
	}
	else if (face == LEFT_SIDE)	// create left side
	{
		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			if (s < numSegments-1)
			{
				colour = Track[piece].sidesColour;
			}
			else
			{
				if (piece == (NumTrackPieces-1))
					colour = Track[0].sidesColour;
				else
					colour = Track[piece+1].sidesColour;
			}

			// store last vertex of triangle 1 (offsets 0,2,4) i.e. offset 4
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+4, offset+0, offset+2, pVertices, colour, 0, s);
			// store last vertex of triangle 2 (offsets 2,4,6) i.e. offset 6
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+6, offset+2, offset+4, pVertices, colour, 0, s);
		}
	}
	else	// create right side
	{
		// loop through piece segments
		for (s = 0; s < numSegments; s++)
		{
			offset = s * 4;
			if (s < numSegments-1)
			{
				colour = Track[piece].sidesColour;
			}
			else
			{
				if (piece == (NumTrackPieces-1))
					colour = Track[0].sidesColour;
				else
					colour = Track[piece+1].sidesColour;
			}

			// store last vertex of triangle 1 (offsets 3,1,7) i.e. offset 7
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+7, offset+3, offset+1, pVertices, colour, 0, s);
			// store last vertex of triangle 2 (offsets 7,1,5) i.e. offset 5
			StorePieceVertex1(piece, piece_x, piece_y, piece_z, offset+5, offset+7, offset+1, pVertices, colour, 0, s);
		}
	}
}


/*
 * The functions to render the opponent's car shadow are in this module because they
 * need the same transformations as for the track (as the shadow is on the track)
 */
HRESULT CreateShadowVertexBuffer (IDirect3DDevice9 *pd3dDevice)
{
	if (pShadowVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( MAX_VERTICES_PER_SHADOW*sizeof(UTVERTEX),
				D3DUSAGE_WRITEONLY, D3DFVF_UTVERTEX, D3DPOOL_DEFAULT, &pShadowVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create shadow vertex buffer\n");
			return E_FAIL;
		}
	}

	UTVERTEX *pVertices;
	if( FAILED( pShadowVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock shadow vertex buffer\n");
		return E_FAIL;
	}

	numShadowVertices = 0;

	pShadowVB->Unlock();
	return S_OK;
}


void FreeShadowVertexBuffer (void)
{
	if (pShadowVB) pShadowVB->Release(), pShadowVB = NULL;
}

/*
 * The start/finish line.
 *
 * On the Amiga this is a single plotted line across the road at the end of the
 * start line section (draw.start.line), i.e. one pixel thick.  Colouring the
 * whole last road segment white (as this port used to do) makes it look far too
 * fat, so instead we build a thin white band sitting on the road surface at the
 * section boundary.
 */
/* ---- Tweakables (rebuild with "make MACOS=1" after changing) ---- */
#define	START_LINE_WIDTH	(0.06f)		// thickness, as a fraction of the segment's length (smaller = thinner)
#define	START_LINE_LIFT		(0.004f)	// raise off the road, as a fraction of segment length, to avoid z-fighting
										// (smaller = flatter on the road; too small and it will shimmer)

static HRESULT CreateStartLineVertexBuffer (IDirect3DDevice9 *pd3dDevice)
{
	if (pStartLineVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( 6*sizeof(UTVERTEX),
				D3DUSAGE_WRITEONLY, D3DFVF_UTVERTEX, D3DPOOL_DEFAULT, &pStartLineVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create start line vertex buffer\n");
			return E_FAIL;
		}
	}

	UTVERTEX *pVertices;
	if( FAILED( pStartLineVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock start line vertex buffer\n");
		return E_FAIL;
	}

	long piece = StartLinePiece;
	long offset = (Track[piece].numSegments - 1) * 4;	// last segment of the start line piece
	long piece_x = Track[piece].x << (LOG_CUBE_SIZE-LOG_PRECISION);
	long piece_y = Track[piece].y << (LOG_CUBE_SIZE-LOG_PRECISION);
	long piece_z = Track[piece].z << (LOG_CUBE_SIZE-LOG_PRECISION);

	// The segment's road quad: offsets 0/1 are the near edge, 4/5 the far edge
	D3DXVECTOR3 nearL = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset+0 );
	D3DXVECTOR3 nearR = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset+1 );
	D3DXVECTOR3 farL  = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset+4 );
	D3DXVECTOR3 farR  = GetPieceVertex( piece, piece_x, piece_y, piece_z, offset+5 );

	// Band runs back from the far edge (i.e. the section boundary the Amiga draws its line at)
	D3DXVECTOR3 backL = D3DXVECTOR3( farL.x + (nearL.x - farL.x) * START_LINE_WIDTH,
									 farL.y + (nearL.y - farL.y) * START_LINE_WIDTH,
									 farL.z + (nearL.z - farL.z) * START_LINE_WIDTH );
	D3DXVECTOR3 backR = D3DXVECTOR3( farR.x + (nearR.x - farR.x) * START_LINE_WIDTH,
									 farR.y + (nearR.y - farR.y) * START_LINE_WIDTH,
									 farR.z + (nearR.z - farR.z) * START_LINE_WIDTH );

	float dx = farL.x - nearL.x, dy = farL.y - nearL.y, dz = farL.z - nearL.z;
	float lift = sqrtf(dx*dx + dy*dy + dz*dz) * START_LINE_LIFT;
	farL.y += lift; farR.y += lift; backL.y += lift; backR.y += lift;

	DWORD colour = SCRGB(SCR_BASE_COLOUR + 15);		// white

	// Same winding as the road quad it sits on: (0,4,5) and (0,5,1)
	const D3DXVECTOR3 v[6] = { backL, farL, farR, backL, farR, backR };
	for (int i = 0; i < 6; i++)
	{
		pVertices[i].pos = v[i];
		pVertices[i].color = colour;
		pVertices[i].tu = pVertices[i].tv = 0.0f;
	}

	pStartLineVB->Unlock();
	return S_OK;
}


static void DrawStartLine (IDirect3DDevice9 *pd3dDevice)
{
	if (pStartLineVB == NULL)
		return;

	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
	pd3dDevice->SetStreamSource( 0, pStartLineVB, 0, sizeof(UTVERTEX) );
	pd3dDevice->SetFVF( D3DFVF_UTVERTEX );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );
}


HRESULT CreateTrackVertexBuffer (IDirect3DDevice9 *pd3dDevice)
{
	if (pTrackVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( MAX_VERTICES_PER_TRACK*sizeof(UTVERTEX),
				D3DUSAGE_WRITEONLY, D3DFVF_UTVERTEX, D3DPOOL_DEFAULT, &pTrackVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create track vertex buffer\n");
			return E_FAIL;
		}
	}

	UTVERTEX *pVertices;
	if( FAILED( pTrackVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock track vertex buffer\n");
		return E_FAIL;
	}

	SetSegmentTextures();
#ifdef SCR_ROAD_TEXTURE
	SetSegmentTexCoords();
#endif

	long face, piece;
	// convert each piece of track in turn
	trackVertices = 0;
	for (face = LEFT_SIDE; face < NUM_TRACK_FACES; face++)
	{
		for (piece = 0; piece < NumTrackPieces; piece++)
		{
			if (bTrackDrawMode == 0)
				CreateUpdatePieceInVBMode1(piece, face, pVertices, true);
			else
				CreateUpdatePieceInVBMode2(piece, face, pVertices, true);
		}
	}
/*
#if defined(DEBUG) || defined(_DEBUG)
	VALUE1 = trackVertices;
	VALUE2 = trackSegments;
#endif
*/

	pTrackVB->Unlock();

	CreateStartLineVertexBuffer(pd3dDevice);

	return S_OK;
}


void FreeTrackVertexBuffer (void)
{
	if (pTrackVB) pTrackVB->Release(), pTrackVB = NULL;
	if (pStartLineVB) pStartLineVB->Release(), pStartLineVB = NULL;
}


// Used to update vertices, e.g. by MoveDrawBridge
HRESULT UpdatePieceInVB (IDirect3DDevice9 *pd3dDevice, long piece)
{
long firstVertex, face;//, numVertices;
long savedTrackVertices = trackVertices;

	if (pTrackVB == NULL)
	{
		return E_FAIL;
	}
	/*
	if (piece == (NumTrackPieces-1))
		numVertices = trackVertices - firstVertex;
	else
		numVertices = PieceFirstVertex[piece+1] - firstVertex;

	if( FAILED( pTrackVB->Lock( firstVertex*sizeof(UTVERTEX), numVertices*sizeof(UTVERTEX), (void**)&pVertices, 0 ) ) )
		return E_FAIL;

	trackVertices = 0;
	CreateUpdatePieceInVB(piece, pVertices, false);
	trackVertices = savedTrackVertices;
	*/

	// Simpler just to lock the whole VB
	UTVERTEX *pVertices;
	if( FAILED( pTrackVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock track vertex buffer for update\n");
		return E_FAIL;
	}
	for (face = LEFT_SIDE; face < NUM_TRACK_FACES; face++)
	{
		firstVertex = PieceFirstVertex[face][piece];
		trackVertices = firstVertex;

		if (bTrackDrawMode == 0)
			CreateUpdatePieceInVBMode1(piece, face, pVertices, false);
		else
			CreateUpdatePieceInVBMode2(piece, face, pVertices, false);
	}

	trackVertices = savedTrackVertices;
	pTrackVB->Unlock();
	return S_OK;
}

extern long player_current_piece;	// use as players_road_section
extern long player_current_segment;

#define TEXTURED_SEGMENTS_AROUND_PLAYER	11
extern IDirect3DTexture9 *g_pAtlas;

void DrawTrack (IDirect3DDevice9 *pd3dDevice)
{
	long segmentsRendered = 0;

	if (TrackID == NO_TRACK)
		return;

//	VALUE1 = player_current_piece;
//	VALUE2 = player_current_segment;

	pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

	pd3dDevice->SetStreamSource( 0, pTrackVB, 0, sizeof(UTVERTEX) );
	pd3dDevice->SetFVF( D3DFVF_UTVERTEX );
	/*	The Amiga's preview showed the road with its markings on - the loop reads as a	*/
	/*	yellow-and-black ribbon in the arena, not a bare grey one - so it takes the game's	*/
	/*	drawing path below.															*/
	const bool bPlainRoad = (GameMode == TRACK_MENU) ||
							((GameMode == TRACK_PREVIEW) && !(bAmigaTrackPreview && bAmigaPreviewScreen));

	if (bPlainRoad)
	{
		/*
		 * Draw track without road lines
		 */
		if (bTrackDrawMode == 0)		// Use D3DPT_TRIANGLELIST
		{
			pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, NumTrackSegments*6 );	// 6 triangles per segment (road and two sides)
		}
		else if (bTrackDrawMode == 1)	// Use D3DPT_TRIANGLESTRIP (so there are less vertices in total)
		{
			pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, trackVertices-2 );
		}
		else
		{
			// Another (possibly faster and more efficient) option would be to draw each piece of track individually
			// (but using DrawIndexedPrimitive / triangle strip so there are less vertices in total)
			return;
		}
	}
	else //	GAME_IN_PROGRESS or GAME_OVER
	{
		/*
		 * Draw track with road lines
		 */
		D3DPRIMITIVETYPE primitiveType;
		long verticesPerSegment, s, v;
#ifndef SCR_ROAD_TEXTURE
		long firstTexturedSegment, lastTexturedSegment, i, count;
#endif

		if (bTrackDrawMode == 0)		// Use D3DPT_TRIANGLELIST
		{
			primitiveType = D3DPT_TRIANGLELIST;
			verticesPerSegment = 6;
		}
		else if (bTrackDrawMode == 1)	// Use D3DPT_TRIANGLESTRIP (so there are less vertices in total)
		{
			primitiveType = D3DPT_TRIANGLESTRIP;
			verticesPerSegment = 2;
		}
		else
		{
			// Another (possibly faster and more efficient) option would be to draw each piece of track individually
			// (but using DrawIndexedPrimitive / triangle strip so there are less vertices in total)
			return;
		}

		/*
		 * 1) Disable texture mapping then draw left and right sides
		 */
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
		v = PieceFirstVertex[LEFT_SIDE][0];	// first left side vertex
		pd3dDevice->DrawPrimitive( primitiveType, v, NumTrackSegments*2 );	// 2 side triangles per segment
		v = PieceFirstVertex[RIGHT_SIDE][0];	// first right side vertex
		pd3dDevice->DrawPrimitive( primitiveType, v, NumTrackSegments*2 );	// 2 side triangles per segment

#ifdef SCR_ROAD_TEXTURE
		/*
		 * 2) Draw the whole road textured.
		 *
		 * The original only textured the segments around the player (see the #else below),
		 * which was invisible when the texture was flat colour plus side lines but would
		 * show as a hard band of grain sliding towards you now that it carries a surface.
		 * The colour variant alternates per segment so consecutive segments rarely share a
		 * texture anyway - batching wouldn't buy much, and these are two triangles each.
		 */
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

		v = PieceFirstVertex[ROAD][0];	// first road vertex
		for (s = 0; s < NumTrackSegments; s++, v += verticesPerSegment)
		{
			IDirect3DTexture9 *pTexture = g_pRoadTexture[SegmentRoadTexture[s]];
			pd3dDevice->SetTexture( 0, pTexture ? pTexture : g_pAtlas );
			pd3dDevice->DrawPrimitive( primitiveType, v, 2 );	// 2 road triangles per segment
		}

		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
		(void)segmentsRendered;
#else
		/*
		 * 2) Draw first part of road untextured (up to where road lines begin, in region surrounding player)
		 */
		firstTexturedSegment = lastTexturedSegment = Track[player_current_piece].firstSegment + player_current_segment;
		firstTexturedSegment -= TEXTURED_SEGMENTS_AROUND_PLAYER;
		lastTexturedSegment += TEXTURED_SEGMENTS_AROUND_PLAYER;

		v = PieceFirstVertex[ROAD][0];	// first road vertex
		if (firstTexturedSegment > 0)
		{
			pd3dDevice->DrawPrimitive( primitiveType, v, firstTexturedSegment*2 );	// 2 road triangles per segment
			segmentsRendered += firstTexturedSegment;
		}

		/*
		 * 3) Enable texture mapping then draw textured road region surrounding player (i.e. with road lines)
		 */
//		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

		pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

		count = lastTexturedSegment - firstTexturedSegment + 1;

		// Limit first and last to track boundaries
		if (firstTexturedSegment < 0)
			firstTexturedSegment += NumTrackSegments;

		if (lastTexturedSegment >= NumTrackSegments)
			lastTexturedSegment -= NumTrackSegments;

		s = firstTexturedSegment;
		v += s * verticesPerSegment;

		// Setup texture 1
		pd3dDevice->SetTexture( 0, g_pAtlas );

		for (i = 0; i < count; i++, s++, v += verticesPerSegment)
		{
			if (s == NumTrackSegments)
			{
				s = 0;
				v = PieceFirstVertex[ROAD][0];	// first road vertex
			}

			// Setup texture 1
			//pd3dDevice->SetTexture( 0, g_pRoadTexture[SegmentRoadTexture[s]] );

			pd3dDevice->DrawPrimitive( primitiveType, v, 2 );	// 2 road triangles per segment
			segmentsRendered++;
		}

		/*
		 * 4) Disable texture mapping then (optionally) draw third part of road untextured (after end of road lines)
		 */
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );

		// v is already set correctly from 3) above
		if (segmentsRendered < NumTrackSegments)
		{
			s = NumTrackSegments - segmentsRendered;
			pd3dDevice->DrawPrimitive( primitiveType, v, s*2 );	// 2 road triangles per segment
		}
#endif	// SCR_ROAD_TEXTURE
	}

	/* Draw the start/finish line on top of the road */
	DrawStartLine(pd3dDevice);

	/* Finally draw the opponent's car shadow */
	if ((GameMode != TRACK_MENU) && (numShadowVertices > 0))
	{
		pd3dDevice->SetStreamSource( 0, pShadowVB, 0, sizeof(UTVERTEX) );
		pd3dDevice->SetFVF( D3DFVF_UTVERTEX );
		pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, numShadowVertices/3 );
	}
}


/*	======================================================================================= */
/*	Function:		MoveDrawBridge,															*/
/*					ResetDrawBridge															*/
/*																							*/
/*	Description:	Animate the relevant pieces of the DrawBridge							*/
/*	======================================================================================= */

#define	NUM_DRAW_BRIDGE_Y_VALUES	15


static long on_draw_bridge_offset = 0;
static long draw_bridge_frame_count = 0;
static long draw_bridge_y_list[NUM_DRAW_BRIDGE_Y_VALUES];

extern long opponents_current_piece;	// use as opponents_road_section

extern unsigned char opponents_speed_values[NUM_TRACKS][MAX_PIECES_PER_TRACK];

//TODO: what are those value in SuperLeague mode? Seems to be the same, but need more checks
// Opponent's speed values for driving up the Draw Bridge (one value for each height)
static unsigned char TAB5a996[16] = {0xd2,0xbb,0xb7,0xb3,0xb1,0xad,0xab,0xa7,0xa6,0xa4,0xa2,0xa1,0x9f,0x9f,0x9f,0x9e};
// Opponent's speed values for approaching the Draw Bridge
static unsigned char TAB5a9a6[16] = {0xf7,0xf7,0xf6,0xf6,0xf5,0xf5,0xf6,0xf7,0xf8,0xf9,0xfb,0xfd,0xff,0x02,0x05,0xfd};


void MoveDrawBridge( void )
{
long f, i, height, y, yinc;
	IDirect3DDevice9 *pd3dDevice = DXUTGetD3DDevice();

	if (TrackID != DRAW_BRIDGE)
		return;

	if (player_current_piece >= 56)
		goto player_not_on_draw_bridge;
	if (player_current_piece >= 51)
		goto on_draw_bridge;

player_not_on_draw_bridge:
	if (opponents_current_piece >= 56)
		goto not_on_draw_bridge;
	if (opponents_current_piece >= 51)
		goto on_draw_bridge;

	if (on_draw_bridge_offset == 0)
		goto not_on_draw_bridge;
	if (opponents_current_piece < 48)
		goto not_on_draw_bridge;

on_draw_bridge:	// player or opponent are on Draw Bridge section, or opponent is approaching it (is on piece 48 to 50)
	// NOTE: draw bridge doesn't move in this case
	on_draw_bridge_offset = 12;
	f = on_draw_bridge_offset + draw_bridge_frame_count;
	goto set_opponent_approach_speed;

not_on_draw_bridge:	// neither player or opponent are on Draw Bridge section
	draw_bridge_frame_count++;	// draw bridge does move in this case
	on_draw_bridge_offset = 0;

	// get height value between 0 and 15
	height = ((draw_bridge_frame_count & 0x1f) - 0x10);
	if (height < 0) height = abs(height) - 1;

	// Set opponent's required speed values for driving up the Draw Bridge
	opponents_speed_values[DRAW_BRIDGE][51] = TAB5a996[height];
	opponents_speed_values[DRAW_BRIDGE][52] = TAB5a996[height];

	// populate y value array for current height
	y = yinc = (height + 4) << 5;
	for (i = 0; i < NUM_DRAW_BRIDGE_Y_VALUES; i++)
		{
		draw_bridge_y_list[i] = y;
		//fprintf(out, "y %04x\n", y);
		y += yinc;
		}

	// update piece 51
	UpdateDrawBridgeYCoords(51, 1,8, 0, 1);

	// update piece 52
	UpdateDrawBridgeYCoords(52, 0,7, 7, 1);

	// update piece 54
	UpdateDrawBridgeYCoords(54, 1,8, (NUM_DRAW_BRIDGE_Y_VALUES-1), -1);

	// update piece 55
	UpdateDrawBridgeYCoords(55, 0,7, (NUM_DRAW_BRIDGE_Y_VALUES-1-7), -1);

	// 29/06/2007 also update pieces in Direct3D vertex buffer
	UpdatePieceInVB(pd3dDevice, 51);
	UpdatePieceInVB(pd3dDevice, 52);
	UpdatePieceInVB(pd3dDevice, 54);
	UpdatePieceInVB(pd3dDevice, 55);

	if (opponents_current_piece != 47)
		return;

	f = draw_bridge_frame_count;

	// Set opponent's required speed values for approaching the Draw Bridge
set_opponent_approach_speed:
	long idx = (f & 0x1f) >> 1;
	long speed = 0xc6;	// -'ve to cause double acceleration, to change speed more quickly
	for (i = 0; i < 3; i++)
	{
		speed += TAB5a9a6[idx];
		opponents_speed_values[DRAW_BRIDGE][48+i] = (unsigned char)speed;
	}
	return;
}


static void UpdateDrawBridgeYCoords( long piece,
									 long firstCoord,
									 long lastCoord,
									 long firstYIndex,
									 long direction )		// 1 or -1
	{
	long i, j, leftID, rightID, leftOverallShift, rightOverallShift, y;

	leftID = static_cast<long>(Left_Y_Coordinate_ID[piece]) & 0x7f;
	rightID = static_cast<long>(Right_Y_Coordinate_ID[piece]) & 0x7f;

	leftOverallShift = static_cast<long>(Left_Overall_Y_Shift[piece]);
	rightOverallShift = static_cast<long>(Right_Overall_Y_Shift[piece]);

	for (i = firstCoord, j = firstYIndex; i <= lastCoord; i++, j += direction)
		{
		// The legacy road-height lookup reads Track[].coords, but the FloatV2
		// physics goes back to the undecorated Piece_Y blocks (FV2_GetRawYCoord),
		// which nothing here used to touch - so FloatV2 saw the bridge frozen in
		// its loaded pose while the drawn bridge moved. Write both. This is the
		// same pre-shift value ConvertAmigaTrack() stores at load time, so the
		// two paths stay in step; the four Y blocks used by the bridge pieces
		// (IDs 95-98) belong to the DrawBridge track alone, so mutating them
		// cannot disturb any other track.
		y = draw_bridge_y_list[j];
		Piece_Y[leftID][i].y = y;
		Piece_Y[rightID][i].y = y;

		// top left y
		y = draw_bridge_y_list[j];
		y += leftOverallShift;
		Track[piece].coords[(i*4)].y = (y * PC_FACTOR);

		// top right y
		y = draw_bridge_y_list[j];
		y += rightOverallShift;
		Track[piece].coords[(i*4)+1].y = (y * PC_FACTOR);
		}

	return;
	}


void ResetDrawBridge( void )
	{
	on_draw_bridge_offset = 0;
	draw_bridge_frame_count = 0;

	// Set car's position to start piece, so that Draw Bridge will move
	player_current_piece = opponents_current_piece = PlayersStartPiece;

	MoveDrawBridge();
	}


/*	======================================================================================= */
/*	Function:		ReadAmigaTrackData														*/
/*																							*/
/*	Description:	Load all track resources when first called								*/
/*					Transfer data for required track into final locations					*/
/*	======================================================================================= */

#define	TRACK_DATA_SIZE	(804)


static long ReadAmigaTrackData( long track )
	{
	static WCHAR track_resource_names[NUM_TRACKS][32] =
												   {L"LittleRamp",
													L"SteppingStones",
													L"HumpBack",
													L"BigRamp",
													L"SkiJump",
													L"DrawBridge",
													L"HighJump",
													L"RollerCoaster"};
	static char *track_buffer_ptrs[NUM_TRACKS];

/*
// variables that were used to load from files
	char	track_filenames[][80] =
								   {"Tracks\\LittleRamp.bin",
									"Tracks\\SteppingStones.bin",
									"Tracks\\HumpBack.bin",
									"Tracks\\BigRamp.bin",
									"Tracks\\SkiJump.bin",
									"Tracks\\DrawBridge.bin",
									"Tracks\\HighJump.bin",
									"Tracks\\RollerCoaster.bin"};
	FILE	*in_file;
	char	buffer[TRACK_DATA_SIZE];
*/
	static long first_time = TRUE;
	char	*buffer;
	char	h, l;
	long	i, j;
	short	s;
	// read all tracks on first call
	if (first_time)
		{
		first_time = FALSE;

		for (i = 0; i < NUM_TRACKS; i++)
			{
			if ((buffer = (char *)GetTRACKResource(NULL, track_resource_names[i])) == NULL) {
				printf("Error loading Track %S\n", track_resource_names[i]);
				return(FALSE);
			}

			track_buffer_ptrs[i] = buffer;
			}
		}

	buffer = track_buffer_ptrs[track];

/*
// code that was previously used, to load from file
	memset(buffer, 0, sizeof(buffer));

	if ((in_file = fopen(track_filenames[track], "rb")) == NULL )		// read, binary
		{
		fprintf(out, "Can't open Amiga track data file\n");
		return(FALSE);
		}

	if ((i = fread(buffer, sizeof(char), TRACK_DATA_SIZE, in_file)) != TRACK_DATA_SIZE)
		{
		fclose(in_file);
		fprintf(out, "Can't read Amiga track data correctly (%d)\n", i);
		return(FALSE);
		}

	fclose(in_file);
*/

	// transfer track data into final locations
	i = 0;
	NumTrackPieces = static_cast<long>(buffer[i++]) & 0xff;
	PlayersStartPiece = static_cast<long>(buffer[i++]) & 0xff;
	StartLinePiece = PlayersStartPiece;
	HalfALapPiece = StartLinePiece + NumTrackPieces/2;
	if (HalfALapPiece > NumTrackPieces) HalfALapPiece -= NumTrackPieces;
//	VALUE2 = HalfALapPiece;

	for (j = 0; j < MAX_PIECES_PER_TRACK; i++, j++)
		Piece_X_Z_Position[j] = buffer[i];

	for (j = 0; j < MAX_PIECES_PER_TRACK; i++, j++)
		Piece_Angle_And_Template[j] = buffer[i];

	for (j = 0; j < MAX_PIECES_PER_TRACK; i++, j++)
		Left_Y_Coordinate_ID[j] = buffer[i];

	for (j = 0; j < MAX_PIECES_PER_TRACK; i++, j++)
		Right_Y_Coordinate_ID[j] = buffer[i];

	for (j = 0; j < MAX_PIECES_PER_TRACK; j++)
		{
		h = buffer[i++];
		l = buffer[i++];
		s = ((h & 0xff) << 8) | (l & 0xff);
		Left_Overall_Y_Shift[j] = s;
		}

	for (j = 0; j < MAX_PIECES_PER_TRACK; j++)
		{
		h = buffer[i++];
		l = buffer[i++];
		s = ((h & 0xff) << 8) | (l & 0xff);
		Right_Overall_Y_Shift[j] = s;
		}

	StandardBoost = static_cast<long>(buffer[i++]) & 0xff;
	SuperBoost = static_cast<long>(buffer[i++]) & 0xff;
	return(TRUE);
	}


/*	======================================================================================= */
/*	Function:		GetTRACKResource														*/
/*																							*/
/*	Description:	Locates and loads into global memory the requested track data file		*/
/*	======================================================================================= */

static void *GetTRACKResource( HMODULE hModule, LPCWSTR lpResName )
	{
	void		*pTRACKBytes;
#ifdef SCR_PORTABLE
const WCHAR* resname[] = {L"LITTLERAMP", L"STEPPINGSTONES", L"HUMPBACK", L"BIGRAMP", L"SKIJUMP", L"DRAWBRIDGE", L"HIGHJUMP", L"ROLLERCOASTER", 0};
const char* filename[] = {"Tracks/LittleRamp.bin", "Tracks/SteppingStones.bin", "Tracks/HumpBack.bin", "Tracks/BigRamp.bin", "Tracks/SkiJump.bin", "Tracks/DrawBridge.bin", "Tracks/HighJump.bin", "Tracks/RollerCoaster.bin"};
	int i = 0;
	while(resname[i] && wcscasecmp(resname[i], lpResName)) i++;
	if(!resname[i]) return NULL;
	// file found, get size, alloc size and read binary file
	FILE* f = fopen(filename[i], "rb");
	if(!f) return NULL;
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize<=0) { fclose(f); return NULL;}
	pTRACKBytes = malloc(fsize);
	fread(pTRACKBytes, 1, fsize, f);
	fclose(f);
#else
	HRSRC		hResInfo;
	HGLOBAL		hResData;

	if ((hResInfo = FindResource(hModule, lpResName, L"TRACK")) == NULL)
		return NULL;

	if ((hResData= LoadResource(hModule, hResInfo)) == NULL)
		return NULL;

	if	((pTRACKBytes = LockResource(hResData))==NULL)
		return NULL;
#endif
	return (void*)pTRACKBytes;
	}
