/**************************************************************************

    Car.cpp - Functions for manipulating car (excluding player's car behaviour)

 **************************************************************************/

/*	============= */
/*	Include files */
/*	============= */
#include "dxstdafx.h"

#include "Car.h"
#include "StuntCarRacer.h"
#include "3D_Engine.h"
#include "Atlas.h"
#include "Opponent_Behaviour.h"
#include "Car_Behaviour.h"
/*	===== */
/*	Debug */
/*	===== */
extern FILE *out;

/*	========= */
/*	Constants */
/*	========= */
#define SCR_BASE_COLOUR	26

/*	The lofted body - six cross-sections, a cockpit tub and an axle beam - is under 90
	triangles, and the seven detail boxes another 84. Then four cylinder wheels: each is
	WHEEL_EDGES segments of four sidewall, two hub and two tread triangles, all stored
	both ways round - 192 at 12 edges, so 768 for the wheels alone. The wheels had all
	but filled the old 800.													*/
#define	MAX_VERTICES_PER_CAR	(1000*3)

/*	--- Visible suspension travel -------------------------------------------------------
	The wheel quads are VCAR_HEIGHT/4 tall and welded to the body, so half that is about
	as far as one can ride up before it parts company with the arch.

	Compression is measured in each car's own height units, and the two differ, so each
	needs its own three landmarks: where the wheel hangs free, where it rests under the
	car's own weight, and where it is properly loaded. All six come out of the physics.

	PLAYER - amount.below.road (front_left/front_right/rear_amount_below_road):
	  free    0		 ProcessWheel() zeroes it the instant the wheel leaves the road
			  (Physics_FloatV2.cpp) - the same as the Amiga's front.left.above.road.
	  rest    317	 static equilibrium. CalculateCarCollisionAcceleration() returns the
			  average amount below road as the spring force and CalculateGravity-
			  Acceleration() returns 317 per step, so the car settles where the two
			  cancel: average.amount.below.road == 317.
	  loaded  0x500	 the Amiga's own idea of "well compressed" - the threshold at which
			  set.road.position.values doubles the camera's lift (StuntCarRacer.s:13399,
			  quoted in full above CalcAmigaYPerspectiveShift()).
	  (it clamps at $11ff, well past loaded, so heavy landings peg the travel.)

	OPPONENT - road height minus actual height, out of CalculateWheelDifference():
	  rest    0		 there is no gravity term at all on this side. The height_adjust bias
			  is added going in and subtracted coming out, so the wheel acceleration is
			  zero exactly when the raw difference is zero.
	  free/loaded	 opponent_y is shifted by (LOG_PRECISION-3) against the player's
			  LOG_PRECISION, making its unit 4x coarser: 8 of them to a model unit
			  against the player's 32. So the player's two spans, scaled by 4.

	Below the rest point the wheel droops and above it compresses; the two sides are
	scaled separately because the physics ranges are not symmetric about rest. What the
	model is drawn at is the rest pose, which is why this cannot simply be the raw
	compression scaled down - doing that gave a car whose wheels stayed in the rest pose
	all the way through a jump and barely moved on landing.							*/
#define	SUSP_MAX_TRAVEL			(VCAR_HEIGHT/8)
#define	SUSP_PLAYER_REST		317
#define	SUSP_PLAYER_DROOP		(SUSP_PLAYER_REST - 0)		// rest down to free
#define	SUSP_PLAYER_LOAD		(0x500 - SUSP_PLAYER_REST)	// rest up to loaded
#define	SUSP_OPPONENT_REST		0
#define	SUSP_OPPONENT_DROOP		(SUSP_PLAYER_DROOP / 4)
#define	SUSP_OPPONENT_LOAD		(SUSP_PLAYER_LOAD / 4)

/*	The physics is a tripod but the car has four wheels, and the two cars share a
	different axle: the player averages its rear pair (rear_amount_below_road), the
	opponent its front (FRONT). Left alone, that axle would sit dead while the other
	worked. So split the shared value by the roll the free axle is showing, at half
	strength. The original had no such term - this is invention - but a dead axle reads
	worse on screen than a slightly overstated live one.							*/
#define	SUSP_ROLL_SHARE_NUM		1
#define	SUSP_ROLL_SHARE_DEN		2

/*	Wheel ride heights, in model units, positive meaning compressed - the body has sunk,
	so the wheel sits higher in its arch. Body roll and pitch are already in the world
	matrix, so these are purely what the suspension adds on top of it.				*/
typedef struct
{
	long rear_left, rear_right, front_left, front_right;
} CAR_SUSPENSION;

extern bool bSuperLeague;
extern int wideScreen;

/*	=========== */
/*	Static data */
/*	=========== */

/*	===================== */
/*	Function declarations */
/*	===================== */
/*
static void DrawHorizon( long viewpoint_y,
						 long viewpoint_x_angle,
						 long viewpoint_z_angle );
*/

#ifdef NOT_USED
/*	======================================================================================= */
/*	Functions:		DrawCar																	*/
/*					DrawCarTopSection														*/
/*					DrawCarBottomSection													*/
/*					DrawCarRightWheels														*/
/*					DrawCarLeftWheels														*/
/*					DrawCarRightWheelTread													*/
/*					DrawCarLeftWheelTread													*/
/*					MakeCarWheels															*/
/*																							*/
/*	Description:	Draw a 3D car into the required buffer									*/
/*	======================================================================================= */

static void MakeCarWheels( COORD_3D *cptr,
						   long num_edges,
						   long axle_length,
						   long axle_y,
						   long axle_spacing,
						   long wheel_radius,
						   long wheel_width )
	{
	// pointers to each wheel's co-ordinates
	COORD_3D *front_right_inner_ptr = cptr;
	COORD_3D *front_right_outer_ptr = front_right_inner_ptr + num_edges;
	COORD_3D *front_left_inner_ptr =  front_right_outer_ptr + num_edges;
	COORD_3D *front_left_outer_ptr =  front_left_inner_ptr + num_edges;
	COORD_3D *rear_right_inner_ptr =  front_left_outer_ptr + num_edges;
	COORD_3D *rear_right_outer_ptr =  rear_right_inner_ptr + num_edges;
	COORD_3D *rear_left_inner_ptr =  rear_right_outer_ptr + num_edges;
	COORD_3D *rear_left_outer_ptr =  rear_left_inner_ptr + num_edges;

	// wheel centre co-ordinates
	long front_right_inner_x = (axle_length/2);
	long front_right_outer_x = (axle_length/2) + wheel_width;
	long front_left_inner_x = -front_right_inner_x;
	long front_left_outer_x = -front_right_outer_x;
	long rear_right_inner_x = front_right_inner_x;
	long rear_right_outer_x = front_right_outer_x;
	long rear_left_inner_x = front_left_inner_x;
	long rear_left_outer_x = front_left_outer_x;

	long front_right_y = axle_y;
	long front_left_y = axle_y;
	long rear_right_y = axle_y;
	long rear_left_y = axle_y;

	long front_right_z = (axle_spacing/2);
	long front_left_z = front_right_z;
	long rear_right_z = -(axle_spacing/2);
	long rear_left_z = rear_right_z;

	long i, y, z;
	double angle, step;

	// start of code
	angle = PI/11;
	step = (static_cast<double>(2) * static_cast<double>(PI)) / static_cast<double>(num_edges);

	for ( i = 0; i < num_edges; i++ )
		{
		y = static_cast<long>(cos( angle ) * static_cast<double>(wheel_radius));
		z = static_cast<long>(sin( angle ) * static_cast<double>(wheel_radius));
		angle += step;

		if (angle > (2 * PI))
			angle -= (2 * PI);

		// store current co-ordinate for front right wheel
		front_right_inner_ptr->x = front_right_inner_x;
		front_right_inner_ptr->y = (y + front_right_y);
		front_right_inner_ptr->z = (z + front_right_z);
		front_right_inner_ptr++;

		front_right_outer_ptr->x = front_right_outer_x;
		front_right_outer_ptr->y = (y + front_right_y);
		front_right_outer_ptr->z = (z + front_right_z);
		front_right_outer_ptr++;

		// store current co-ordinate for front left wheel
		front_left_inner_ptr->x = front_left_inner_x;
		front_left_inner_ptr->y = (y + front_left_y);
		front_left_inner_ptr->z = (z + front_left_z);
		front_left_inner_ptr++;

		front_left_outer_ptr->x = front_left_outer_x;
		front_left_outer_ptr->y = (y + front_left_y);
		front_left_outer_ptr->z = (z + front_left_z);
		front_left_outer_ptr++;

		// store current co-ordinate for rear right wheel
		rear_right_inner_ptr->x = rear_right_inner_x;
		rear_right_inner_ptr->y = (y + rear_right_y);
		rear_right_inner_ptr->z = (z + rear_right_z);
		rear_right_inner_ptr++;

		rear_right_outer_ptr->x = rear_right_outer_x;
		rear_right_outer_ptr->y = (y + rear_right_y);
		rear_right_outer_ptr->z = (z + rear_right_z);
		rear_right_outer_ptr++;

		// store current co-ordinate for rear left wheel
		rear_left_inner_ptr->x = rear_left_inner_x;
		rear_left_inner_ptr->y = (y + rear_left_y);
		rear_left_inner_ptr->z = (z + rear_left_z);
		rear_left_inner_ptr++;

		rear_left_outer_ptr->x = rear_left_outer_x;
		rear_left_outer_ptr->y = (y + rear_left_y);
		rear_left_outer_ptr->z = (z + rear_left_z);
		rear_left_outer_ptr++;
		}
	}


static void DrawCar( BYTE base_colour )
	{
	static long first_time = TRUE;

	// currently has same length as behavioural car, but is about 20% narrower
	// 24/04/1998 because co-ordinates are divided by 8

	// car co-ordinates		  x,   y,   z
	static COORD_3D car[] = {{416,-256,1024},	// floor
							 {-416,-256,1024},
							 {416,-256,-1024},
							 {-416,-256,-1024},
							 //
							 {416,-500,950},	// wing and door tops
							 {-416,-500,950},
							 {416,-600,400},
							 {-416,-600,400},
							 {416,-600,-1000},
							 {-416,-600,-1000},
							 {416,-600,-350},
							 {-416,-600,-350},
							 //
							 {364,-800,100},	// roof
							 {-364,-800,100},
							 {364,-800,-350},
							 {-364,-800,-350},
							 {364,-800,-800},
							 {-364,-800,-800},
							 //
							 {0,0,0},			// front right wheel inner
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// front right wheel outer
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// front left wheel inner
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// front left wheel outer
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// rear right wheel inner
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// rear right wheel outer
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// rear left wheel inner
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 //
							 {0,0,0},			// rear left wheel outer
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0},
							 {0,0,0}};

	// car roof surface
	long car_roof[4] = {12, 16, 17, 13};	// offsets into co-ordinates above

	// start of code
	if (first_time)
		{
		first_time = FALSE;
		MakeCarWheels(&car[18],
					  8,		  // number of edges per wheel
					  (1024-192), // axle length (i.e. distance between left and right wheels)
					  -240,		  // axle y position (i.e. ground clearance)
					  1500,		  // axle spacing (i.e. wheelbase)
					  240,		  // wheel radius (same as ground clearance)
					  192);		  // wheel width

		// temporarily reduce car size at runtime
		// eventually car size will be decided and this code can be removed
		long i, reduce = 8;
		for (i = 0; i < (sizeof(car) / sizeof(COORD_3D)); i++)
			{
			car[i].x /= reduce;
			car[i].y /= reduce;
			car[i].z /= reduce;
			}
		}

	if (TransformCoordinates(car, sizeof(car)) != TRUE)
		return;

	// now decide whether car top or bottom section should be drawn first
	// this decision is taken depending upon the visibility of the car roof
	if (PolygonVisible(car_roof) == TRUE)
		{
		DrawCarBottomSection(base_colour);
		DrawCarTopSection(base_colour);

		SetTextureColour(base_colour + 4);
		Polygon(car_roof, 4);		// ideally wouldn't do visibility check again here
		}
	else
		{
		DrawCarTopSection(base_colour);
		DrawCarBottomSection(base_colour);
		}
	}


static void DrawCarTopSection( BYTE base_colour )
	{
	// car top surfaces, e.g. windows and areas behind them
	long car_top_side1[4] = {10, 8, 16, 14};	// area behind side windows
	long car_top_side2[4] = {15, 17, 9, 11};

	long car_front_window[4] = {6, 12, 13, 7};
	long car_side_window1[4] = {6, 10, 14, 12};
	long car_side_window2[4] = {13, 15, 11, 7};
	long car_rear_window[4] = {9, 17, 16, 8};

	// start of code
	SetTextureColour(base_colour + 3);
	Polygon(car_top_side1, 4);
	Polygon(car_top_side2, 4);

	SetTextureColour(base_colour + 5);
	Polygon(car_front_window, 4);
	Polygon(car_side_window1, 4);
	Polygon(car_side_window2, 4);
	Polygon(car_rear_window, 4);
	}


static void DrawCarBottomSection( BYTE base_colour )
	{
	// car bottom surfaces, e.g. wings, bonnet, bottom of doors
	long car_side1[5] = {0, 2, 8, 6, 4};
	long car_side2[5] = {9, 3, 1, 5, 7};

	long car_floor[4] = {0, 1, 3, 2};

	long car_front[4] = {0, 4, 5, 1};
	long car_rear[4] = {3, 9, 8, 2};

	long car_bonnet[4] = {4, 6, 7, 5};

	// start of code
	// decide order in which to draw right wheels, floor and left wheels,
	// depending upon the visibility of one of the car sides (can use either)
	if (PolygonVisible(car_side1) == TRUE)
		{
		DrawCarLeftWheels(base_colour);

		SetTextureColour(base_colour + 1);
		Polygon(car_floor, 4);

		SetTextureColour(base_colour + 2);
		Polygon(car_side1, 5);		// ideally wouldn't do visibility check again here

		DrawCarRightWheels(base_colour);
		}
	else
		{
		DrawCarRightWheels(base_colour);

		SetTextureColour(base_colour + 1);
		Polygon(car_floor, 4);

		SetTextureColour(base_colour + 2);
		Polygon(car_side2, 5);

		DrawCarLeftWheels(base_colour);
		}

	SetTextureColour(base_colour + 3);
	Polygon(car_front, 4);
	Polygon(car_rear, 4);

	SetTextureColour(base_colour + 4);
	Polygon(car_bonnet, 4);
	}


static void DrawCarRightWheels( BYTE base_colour )
	{
	long front_right_inner_wheel[8] = {18, 19, 20, 21, 22, 23, 24, 25};
	long front_right_outer_wheel[8] = {33, 32, 31, 30, 29, 28, 27, 26};
	long rear_right_inner_wheel[8] = {50, 51, 52, 53, 54, 55, 56, 57};
	long rear_right_outer_wheel[8] = {65, 64, 63, 62, 61, 60, 59, 58};

	long wheel_orientation[3] = {0, 3, 6};

	// start of code
	if (TransformedZ(18) < TransformedZ(50))
		{
		// front wheel is infront of rear wheel
		SetTextureColour(base_colour + 6);
		PolygonEx(rear_right_inner_wheel, 8, wheel_orientation);
		PolygonEx(rear_right_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarRightWheelTread(18+32);

		SetTextureColour(base_colour + 6);
		PolygonEx(front_right_inner_wheel, 8, wheel_orientation);
		PolygonEx(front_right_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarRightWheelTread(18);
		}
	else
		{
		// front wheel is behind rear wheel
		SetTextureColour(base_colour + 6);
		PolygonEx(front_right_inner_wheel, 8, wheel_orientation);
		PolygonEx(front_right_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarRightWheelTread(18);

		SetTextureColour(base_colour + 6);
		PolygonEx(rear_right_inner_wheel, 8, wheel_orientation);
		PolygonEx(rear_right_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarRightWheelTread(18+32);
		}
	}


static void DrawCarLeftWheels( BYTE base_colour )
	{
	long front_left_inner_wheel[8] = {41, 40, 39, 38, 37, 36, 35, 34};
	long front_left_outer_wheel[8] = {42, 43, 44, 45, 46, 47, 48, 49};
	long rear_left_inner_wheel[8] = {73, 72, 71, 70, 69, 68, 67, 66};
	long rear_left_outer_wheel[8] = {74, 75, 76, 77, 78, 79, 80, 81};

	long wheel_orientation[3] = {0, 3, 6};

	// start of code
	if (TransformedZ(34) < TransformedZ(66))
		{
		// front wheel is infront of rear wheel
		SetTextureColour(base_colour + 6);
		PolygonEx(rear_left_inner_wheel, 8, wheel_orientation);
		PolygonEx(rear_left_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarLeftWheelTread(18+48);

		SetTextureColour(base_colour + 6);
		PolygonEx(front_left_inner_wheel, 8, wheel_orientation);
		PolygonEx(front_left_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarLeftWheelTread(18+16);
		}
	else
		{
		// front wheel is behind rear wheel
		SetTextureColour(base_colour + 6);
		PolygonEx(front_left_inner_wheel, 8, wheel_orientation);
		PolygonEx(front_left_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarLeftWheelTread(18+16);

		SetTextureColour(base_colour + 6);
		PolygonEx(rear_left_inner_wheel, 8, wheel_orientation);
		PolygonEx(rear_left_outer_wheel, 8, wheel_orientation);

		SetTextureColour(base_colour + 7);
		DrawCarLeftWheelTread(18+48);
		}
	}


// following two functions are hard-coded for eight tread surfaces

static void DrawCarRightWheelTread( long offset )	// offset into co-ordinates
	{
	long tread1[4] = {1 + offset, 0 + offset, 8 + offset, 9 + offset};
	long tread2[4] = {2 + offset, 1 + offset, 9 + offset, 10 + offset};
	long tread3[4] = {3 + offset, 2 + offset, 10 + offset, 11 + offset};
	long tread4[4] = {4 + offset, 3 + offset, 11 + offset, 12 + offset};
	long tread5[4] = {5 + offset, 4 + offset, 12 + offset, 13 + offset};
	long tread6[4] = {6 + offset, 5 + offset, 13 + offset, 14 + offset};
	long tread7[4] = {7 + offset, 6 + offset, 14 + offset, 15 + offset};
	long tread8[4] = {0 + offset, 7 + offset, 15 + offset, 8 + offset};

	// start of code
	Polygon(tread1, 4);
	Polygon(tread2, 4);
	Polygon(tread3, 4);
	Polygon(tread4, 4);
	Polygon(tread5, 4);
	Polygon(tread6, 4);
	Polygon(tread7, 4);
	Polygon(tread8, 4);
	}


static void DrawCarLeftWheelTread( long offset )	// offset into co-ordinates
	{
	long tread1[4] = {0 + offset, 1 + offset, 9 + offset, 8 + offset};
	long tread2[4] = {1 + offset, 2 + offset, 10 + offset, 9 + offset};
	long tread3[4] = {2 + offset, 3 + offset, 11 + offset, 10 + offset};
	long tread4[4] = {3 + offset, 4 + offset, 12 + offset, 11 + offset};
	long tread5[4] = {4 + offset, 5 + offset, 13 + offset, 12 + offset};
	long tread6[4] = {5 + offset, 6 + offset, 14 + offset, 13 + offset};
	long tread7[4] = {6 + offset, 7 + offset, 15 + offset, 14 + offset};
	long tread8[4] = {7 + offset, 0 + offset, 8 + offset, 15 + offset};

	// start of code
	Polygon(tread1, 4);
	Polygon(tread2, 4);
	Polygon(tread3, 4);
	Polygon(tread4, 4);
	Polygon(tread5, 4);
	Polygon(tread6, 4);
	Polygon(tread7, 4);
	Polygon(tread8, 4);
	}
#endif

/*	======================================================================================= */
/*	Function:		DrawCar																	*/
/*																							*/
/*	Description:	Draw the car using the supplied viewpoint								*/
/*	======================================================================================= */
static IDirect3DVertexBuffer9 *pCarVB = NULL;
static IDirect3DVertexBuffer9 *pOpponentCarVB = NULL;
static long numCarVertices = 0;			// the mesh being built, by StoreCarTriangle()
static long numPlayerCarVertices = 0;	// what each buffer ended up holding - the two
static long numOpponentCarVertices = 0;	// cars are different models, so different counts

// Per-wheel suspension compression, written by both the legacy and FloatV2 physics paths
extern long front_left_amount_below_road, front_right_amount_below_road, rear_amount_below_road;

static void StoreCarTriangle( COORD_3D *c1, COORD_3D *c2, COORD_3D *c3, UTVERTEX *pVertices, DWORD colour )
{
D3DXVECTOR3 v1, v2, v3;//, edge1, edge2, surface_normal;

	if ((numCarVertices+3) > MAX_VERTICES_PER_CAR)
	{
		MessageBox(NULL, L"Exceeded numCarVertices", L"StoreCarTriangle", MB_OK);
		return;
	}

	v1 = D3DXVECTOR3( static_cast<float>(c1->x), static_cast<float>(c1->y), static_cast<float>(c1->z) );
	v2 = D3DXVECTOR3( static_cast<float>(c2->x), static_cast<float>(c2->y), static_cast<float>(c2->z) );
	v3 = D3DXVECTOR3( static_cast<float>(c3->x), static_cast<float>(c3->y), static_cast<float>(c3->z) );

	/*
	// Calculate surface normal
	edge1 = v2-v1; edge2 = v3-v2;
	D3DXVec3Cross( &surface_normal, &edge1, &edge2 );
	D3DXVec3Normalize( &surface_normal, &surface_normal );
	*/

	pVertices[numCarVertices].pos = v1;
//	pVertices[numCarVertices].normal = surface_normal;
	pVertices[numCarVertices].color = colour;
	++numCarVertices;

	pVertices[numCarVertices].pos = v2;
//	pVertices[numCarVertices].normal = surface_normal;
	pVertices[numCarVertices].color = colour;
	++numCarVertices;

	pVertices[numCarVertices].pos = v3;
//	pVertices[numCarVertices].normal = surface_normal;
	pVertices[numCarVertices].color = colour;
	++numCarVertices;
}


/*	Wheel as a cylinder on the axle rather than the Amiga's flat quad.

	The quad the original drew lies in the x/y plane: its two x values are the wheel's
	width across the axle, its two y values the wheel's height, and every corner shares
	one z. So it was never a disc seen side-on - it was the tread band, drawn flat.
	That is what made it read as a square block.

	The four corners still define the wheel, so suspension travel and the car's footprint
	are unchanged. From them: the axle runs along x from quad[0].x to quad[2].x, the wheel
	radius is half the y span, and the centre sits on the quad's z. Round in the y/z plane
	means the tyre now bulges half a radius past the car's nose and tail, as a real wheel
	at the extremity of the wheelbase does.

	Each side is a tyre sidewall annulus around a hub disc of alternating light and dark
	wedges, which reads as spokes; between the two sides runs the tread band. The tyre is
	flat-shaded off a fixed overhead light, quantised, so the barrel reads as round without
	gouraud, while the hub stays unlit so it holds its contrast at any angle. Both windings
	are stored for every triangle, as the quads did - the car is drawn with backface
	culling on and the left and right wheels mirror.

	The car's own base colour is palette entry 0, which is black, so the wheels take their
	colours from the greys in the car block instead - shading black gets you black.	*/
#define	WHEEL_EDGES			12		// segments round the tyre (also 6 spokes)
#define	WHEEL_HUB_FRACTION	0.55	// hub disc radius, as a fraction of the tyre's
#define	WHEEL_SIDE_SHADE	0.72f	// sidewall, flat - it points along the axle
#define	WHEEL_TREAD_MIN		0.55f	// tread band, underside
#define	WHEEL_TREAD_MAX		1.30f	// tread band, top

#define	WHEEL_ROLL_PER_SPEED	0.00055		// radians per second per unit of z speed
#define	WHEEL_ROLL_MAX			0.80		// radians per frame, below the aliasing point
#define	WHEEL_ROLL_REAR_RATE	1.0
#define	WHEEL_ROLL_FRONT_RATE	2.0			// smaller, lighter wheel - spins up well past it

#define	WHEEL_TYRE_COLOUR	17					// {0x33,0x33,0x33} rubber
#define	WHEEL_HUB_COLOUR	(SCR_BASE_COLOUR+14)	// {0xbb,0xbb,0xbb} bright rim
#define	WHEEL_SPOKE_COLOUR	16					// {0x44,0x44,0x44} gap between spokes

/*	Both facings of one triangle. */
static void StoreCarTriangle2( const COORD_3D *c1, const COORD_3D *c2, const COORD_3D *c3,
							   UTVERTEX *pVertices, DWORD colour )
{
	StoreCarTriangle(const_cast<COORD_3D*>(c1), const_cast<COORD_3D*>(c2),
					 const_cast<COORD_3D*>(c3), pVertices, colour);
	StoreCarTriangle(const_cast<COORD_3D*>(c3), const_cast<COORD_3D*>(c2),
					 const_cast<COORD_3D*>(c1), pVertices, colour);
}

static void StoreCarWheel( const COORD_3D *quad, UTVERTEX *pVertices, double roll )
{
	// corners run (x0,ylow) (x0,yhigh) (x1,yhigh) (x1,ylow), all at the same z
	long   x0 = quad[0].x, x1 = quad[2].x;
	double cy = (static_cast<double>(quad[0].y) + static_cast<double>(quad[1].y)) / 2.0;
	double r  = (static_cast<double>(quad[1].y) - static_cast<double>(quad[0].y)) / 2.0;
	double cz = static_cast<double>(quad[0].z);
	double rh = r * WHEEL_HUB_FRACTION;

	COORD_3D axle0 = { x0, static_cast<long>(cy), static_cast<long>(cz) };
	COORD_3D axle1 = { x1, static_cast<long>(cy), static_cast<long>(cz) };

	DWORD side_colour  = SCRGBShaded(WHEEL_TYRE_COLOUR, WHEEL_SIDE_SHADE);
	DWORD hub_colour   = SCRGB(WHEEL_HUB_COLOUR);
	DWORD spoke_colour = SCRGB(WHEEL_SPOKE_COLOUR);

	COORD_3D tyre0[WHEEL_EDGES], tyre1[WHEEL_EDGES];	// tyre rim, inner and outer side
	COORD_3D hub0[WHEEL_EDGES],  hub1[WHEEL_EDGES];		// hub disc, same
	double   ny[WHEEL_EDGES];		// y of the outward normal, for the tread shade

	for (long i = 0; i < WHEEL_EDGES; i++)
	{
		double a = roll + (2.0 * PI * static_cast<double>(i)) / static_cast<double>(WHEEL_EDGES);
		double c = cos(a), s = sin(a);

		tyre0[i].x = x0;	tyre1[i].x = x1;
		tyre0[i].y = tyre1[i].y = static_cast<long>(cy + r * c);
		tyre0[i].z = tyre1[i].z = static_cast<long>(cz + r * s);

		hub0[i].x = x0;		hub1[i].x = x1;
		hub0[i].y = hub1[i].y = static_cast<long>(cy + rh * c);
		hub0[i].z = hub1[i].z = static_cast<long>(cz + rh * s);

		ny[i] = c;
	}

	for (long i = 0; i < WHEEL_EDGES; i++)
	{
		long j = (i + 1) % WHEEL_EDGES;

		// sidewalls - the ring of rubber between hub and tread, one each side
		StoreCarTriangle2(&hub0[i], &tyre0[i], &tyre0[j], pVertices, side_colour);
		StoreCarTriangle2(&hub0[i], &tyre0[j], &hub0[j], pVertices, side_colour);
		StoreCarTriangle2(&hub1[i], &tyre1[i], &tyre1[j], pVertices, side_colour);
		StoreCarTriangle2(&hub1[i], &tyre1[j], &hub1[j], pVertices, side_colour);

		// hub, alternating wedges so it reads as spokes
		DWORD wedge = (i & 1) ? spoke_colour : hub_colour;
		StoreCarTriangle2(&axle0, &hub0[i], &hub0[j], pVertices, wedge);
		StoreCarTriangle2(&axle1, &hub1[i], &hub1[j], pVertices, wedge);

		/*	Tread band. Light straight down, so the facing is the segment normal's y;
			quantised to keep the flat-shaded look of everything else.			*/
		double lit   = (ny[i] + ny[j]) / 2.0;					// -1 down, +1 up
		float  shade = WHEEL_TREAD_MIN + (WHEEL_TREAD_MAX - WHEEL_TREAD_MIN)
										  * static_cast<float>((lit + 1.0) / 2.0);
		shade = static_cast<float>(static_cast<long>(shade * 8.0f + 0.5f)) / 8.0f;
		DWORD tread_colour = SCRGBShaded(WHEEL_TYRE_COLOUR, shade);

		StoreCarTriangle2(&tyre0[i], &tyre1[i], &tyre1[j], pVertices, tread_colour);
		StoreCarTriangle2(&tyre0[i], &tyre1[j], &tyre0[j], pVertices, tread_colour);
	}
}


/*	Wheel sizes. Fat rears, narrow tucked-in fronts.

	Every wheel's bottom edge stays on -VCAR_HEIGHT/4: that is the ground, the height the
	car is translated by when it is drawn (see StuntCarRacer.cpp), so a wheel that grows
	downward puts the car in the road and one that shrinks leaves it hovering. The rears
	therefore get their extra diameter upward, past the top of the body sides - which is
	what a raised rear tyre does anyway.

	Radius applies in z as well as y, so a bigger rear also stands further out past the
	tail. The rears keep their inner face hard against the body side at VCAR_WIDTH/4,
	which is as far inboard as anything can sit back there without disappearing into the
	bodywork; their width is half what it was, which is still fat next to the fronts
	without the barrel dominating the car from behind.

	The fronts used to line up their inner face with the rears', for want of anywhere
	else to put them - with a full width nose, inboard of VCAR_WIDTH/4 was inside the
	body. The lofted nose is a tenth of the car's width, so that no longer applies and
	the fronts pull in to half the rear track. A narrower front track is what the artwork
	shows, it shortens the axle beam reaching out to them, and the fronts stay narrow
	across the axle - they were never meant to match the rears for width.

	WHEEL_REAR_OUTER and WHEEL_FRONT_OUTER live in Car.h, because the opponent's shadow is
	scaled by them - it is built from the road footprint the car no longer fills.	*/
#define	WHEEL_REAR_TOP		(VCAR_HEIGHT/16)		// above the axle line, so d = 50 not 40
#define	WHEEL_REAR_INNER	(VCAR_WIDTH/4)			// unchanged - rear track stays wide
#define	WHEEL_FRONT_TOP		0						// fronts keep the original 40 diameter
#define	WHEEL_FRONT_INNER	(VCAR_WIDTH/8)			// pulled inboard of the rears' line

/*	The car at rest. Wheels first, four vertices each, in the order rear left, rear right,
	front left, front right - CreateCarInVB() leans on that layout to apply ride height,
	so keep the four groups where they are. Within a group the corners run (inner, bottom)
	(inner, top) (outer, top) (outer, bottom); StoreCarWheel() reads the axle and the
	radius back out of that.													*/
static const COORD_3D car_rest[16] = {
//x,					y,					z
{-WHEEL_REAR_OUTER,		-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},		// rear left wheel
{-WHEEL_REAR_OUTER,		WHEEL_REAR_TOP,		-VCAR_LENGTH/2},
{-WHEEL_REAR_INNER,		WHEEL_REAR_TOP,		-VCAR_LENGTH/2},
{-WHEEL_REAR_INNER,		-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},

{WHEEL_REAR_INNER,		-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},		// rear right wheel
{WHEEL_REAR_INNER,		WHEEL_REAR_TOP,		-VCAR_LENGTH/2},
{WHEEL_REAR_OUTER,		WHEEL_REAR_TOP,		-VCAR_LENGTH/2},
{WHEEL_REAR_OUTER,		-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},

{-WHEEL_FRONT_OUTER,	-VCAR_HEIGHT/4,		VCAR_LENGTH/2},		// front left wheel
{-WHEEL_FRONT_OUTER,	WHEEL_FRONT_TOP,	VCAR_LENGTH/2},
{-WHEEL_FRONT_INNER,	WHEEL_FRONT_TOP,	VCAR_LENGTH/2},
{-WHEEL_FRONT_INNER,	-VCAR_HEIGHT/4,		VCAR_LENGTH/2},

{WHEEL_FRONT_INNER,		-VCAR_HEIGHT/4,		VCAR_LENGTH/2},		// front right wheel
{WHEEL_FRONT_INNER,		WHEEL_FRONT_TOP,	VCAR_LENGTH/2},
{WHEEL_FRONT_OUTER,		WHEEL_FRONT_TOP,	VCAR_LENGTH/2},
{WHEEL_FRONT_OUTER,		-VCAR_HEIGHT/4,		VCAR_LENGTH/2}};


/*	The body.

	It used to be a single wedge: one cross-section at the nose, one at the tail, lofted
	between them. Six faces. That is why it read as a box on wheels rather than as the
	buggy on the loading screen - a shape with no waist, no bonnet line and a roof over
	a cockpit that should be open to the sky.

	So the body is now a run of cross-sections down the z axis, each with its own width
	at the floor and at the deck, lofted section to section: the nose narrows to a point
	well inboard of the front wheels, the middle swells to full width for the cockpit,
	and the tail stands up into the engine block. The old wedge's extremes are kept - the
	body still spans the full VCAR_LENGTH and stops at VCAR_WIDTH/4 where the wheels
	begin - so the car occupies exactly the space it always did. This is all cosmetic;
	the physics has never used these vertices.

	Sections run nose first. Front and rear are capped, and one segment is flagged as the
	cockpit: instead of a deck it gets an open tub, which is what makes it a racing car
	seen from above rather than a lid.											*/
typedef struct
{
	long	z;
	long	half_width_floor;	// the body's widest, at the underside
	long	y_floor;
	long	half_width_deck;	// tucked in above, so the sides slope
	long	y_deck;
} CAR_SECTION;

#define	BODY_FLOOR	(-VCAR_HEIGHT/8)		// underside, as the wedge had it
#define	BODY_SIDE	(VCAR_WIDTH/4)			// hard against the wheels' inner faces

/*	Height is the thing to keep an eye on here. The body can never be wider than
	2*BODY_SIDE, because that is where the wheels start, so every unit the deck gains
	makes the car squarer in cross-section and squarer is what reads as clunky. The
	engine block used to stand VCAR_HEIGHT/4 above the floor, which put its roof as far
	above the ground as the body was wide - a cube. Halving that keeps the block clearly
	the tallest thing on the car while leaving it wider than it is high, and the cockpit
	deck comes down with it so the step up to the block is still worth seeing.	*/
static const CAR_SECTION car_body[] = {
//	z						floor half-width	floor y			deck half-width		deck y
{	VCAR_LENGTH/2,			VCAR_WIDTH/10,		BODY_FLOOR,		VCAR_WIDTH/10,		-VCAR_HEIGHT/16		},	// nose tip
{	(3*VCAR_LENGTH)/8,		VCAR_WIDTH/7,		BODY_FLOOR,		VCAR_WIDTH/8,		-VCAR_HEIGHT/24		},	// nose
{	VCAR_LENGTH/10,			BODY_SIDE,			BODY_FLOOR,		(9*BODY_SIDE)/10,	VCAR_HEIGHT/40		},	// scuttle
{	-VCAR_LENGTH/6,			BODY_SIDE,			BODY_FLOOR,		(9*BODY_SIDE)/10,	VCAR_HEIGHT/40		},	// back of the cockpit
{	-VCAR_LENGTH/4,			BODY_SIDE,			BODY_FLOOR,		(17*BODY_SIDE)/20,	VCAR_HEIGHT/8		},	// engine bulkhead
{	-VCAR_LENGTH/2,			BODY_SIDE,			BODY_FLOOR,		(3*BODY_SIDE)/4,	VCAR_HEIGHT/8		}};	// tail

#define	BODY_SECTIONS		(sizeof(car_body) / sizeof(CAR_SECTION))
#define	COCKPIT_SEGMENT		2			// the tub lies between section 2 and section 3
#define	COCKPIT_RIM			(BODY_SIDE/4)	// bodywork left either side of the opening
#define	COCKPIT_FLOOR		(-VCAR_HEIGHT/20)	// how deep the tub is cut into the deck

/*	The front wheels sit a long way outboard of a nose this narrow, so without something
	spanning them they hang in the air. A beam on the axle line is what the real thing
	would have and what the artwork shows.										*/
/*	Halfway between the front wheel's bottom (-VCAR_HEIGHT/4, the ground) and its top
	(WHEEL_FRONT_TOP) is the axle line, and it lands on the body floor.			*/
#define	AXLE_BEAM_Y			((-VCAR_HEIGHT/4 + WHEEL_FRONT_TOP) / 2)
#define	AXLE_BEAM_THICK		(VCAR_HEIGHT/40)
#define	AXLE_BEAM_LONG		(VCAR_LENGTH/40)

/*	A quad, wound so that (v2-v1) x (v3-v2) points out of the car - the winding the wedge
	used, and the one D3DCULL_CCW wants.										*/
static void StoreCarQuad( const COORD_3D *a, const COORD_3D *b, const COORD_3D *c,
						  const COORD_3D *d, UTVERTEX *pVertices, DWORD colour )
{
	StoreCarTriangle(const_cast<COORD_3D*>(a), const_cast<COORD_3D*>(b),
					 const_cast<COORD_3D*>(c), pVertices, colour);
	StoreCarTriangle(const_cast<COORD_3D*>(a), const_cast<COORD_3D*>(c),
					 const_cast<COORD_3D*>(d), pVertices, colour);
}

/*	An axis-aligned box, all six faces outward. */
static void StoreCarBox( long x0, long x1, long y0, long y1, long z0, long z1,
						 UTVERTEX *pVertices, DWORD side, DWORD top, DWORD end )
{
	COORD_3D p[8];
	for (long i = 0; i < 8; i++)
	{
		p[i].x = (i & 1) ? x1 : x0;
		p[i].y = (i & 2) ? y1 : y0;
		p[i].z = (i & 4) ? z1 : z0;
	}
	#define	P(xb,yb,zb)	(&p[(xb) | ((yb)<<1) | ((zb)<<2)])

	StoreCarQuad(P(0,1,0), P(0,1,1), P(1,1,1), P(1,1,0), pVertices, top);	// top
	StoreCarQuad(P(1,0,0), P(1,0,1), P(0,0,1), P(0,0,0), pVertices, top);	// bottom
	StoreCarQuad(P(0,0,1), P(0,1,1), P(0,1,0), P(0,0,0), pVertices, side);	// left
	StoreCarQuad(P(1,0,0), P(1,1,0), P(1,1,1), P(1,0,1), pVertices, side);	// right
	StoreCarQuad(P(1,0,1), P(1,1,1), P(0,1,1), P(0,0,1), pVertices, end);	// front
	StoreCarQuad(P(0,0,0), P(0,1,0), P(1,1,0), P(1,0,0), pVertices, end);	// rear
	#undef P
}

/*	The same box on both flanks, given the right-hand one's x range. */
static void StoreCarBoxPair( long x0, long x1, long y0, long y1, long z0, long z1,
							 UTVERTEX *pVertices, DWORD side, DWORD top, DWORD end )
{
	StoreCarBox( x0,  x1, y0, y1, z0, z1, pVertices, side, top, end);
	StoreCarBox(-x1, -x0, y0, y1, z0, z1, pVertices, side, top, end);
}

/*	Detail.

	The lofted body is the right shape but every one of its faces is a big flat sheet,
	and at this triangle count the eye has nothing to catch on - which is the other half
	of looking clunky. These are the fittings the artwork hangs off that shape: a roll
	hoop behind the driver's head, a pipe down each flank, a blade across the nose and an
	intake standing on the engine deck. All are boxes, so all are 12 triangles, and all
	sit proud of the bodywork rather than being cut into it - nothing here has to agree
	with the section table, which leaves the body free to be retuned without breaking
	them.

	The pipes stop short of the rear wheel: the tyre reaches VCAR_LENGTH/2 minus its own
	radius up the flank, and a pipe run into that is a pipe through the tyre.	*/
#define	HOOP_Z_FRONT		(-(11*VCAR_LENGTH)/64)		// just behind the cockpit opening
#define	HOOP_Z_REAR			(-(13*VCAR_LENGTH)/64)
#define	HOOP_TOP			(VCAR_HEIGHT/5)				// stands above the engine deck
#define	HOOP_OUTER			((4*BODY_SIDE)/5)
#define	HOOP_INNER			((13*BODY_SIDE)/20)
#define	HOOP_BAR_BOTTOM		((13*VCAR_HEIGHT)/80)

#define	PIPE_Z_FRONT		(-VCAR_LENGTH/8)
#define	PIPE_Z_REAR			(-(25*VCAR_LENGTH)/64)		// clear of the rear tyre
#define	PIPE_INNER			((19*BODY_SIDE)/20)			// starts inside the bodywork
#define	PIPE_OUTER			((23*BODY_SIDE)/20)			// and stands proud of it
#define	PIPE_TOP			(-(3*VCAR_HEIGHT)/80)
#define	PIPE_BOTTOM			(-(7*VCAR_HEIGHT)/80)

#define	BLADE_HALF_WIDTH	((3*VCAR_WIDTH)/20)			// wider than the nose it caps
#define	BLADE_TOP			(-VCAR_HEIGHT/20)
#define	BLADE_BOTTOM		(-(7*VCAR_HEIGHT)/80)
#define	BLADE_Z_FRONT		(VCAR_LENGTH/2 + VCAR_LENGTH/64)
#define	BLADE_Z_REAR		(VCAR_LENGTH/2 - VCAR_LENGTH/64)

#define	INTAKE_HALF_WIDTH	(BODY_SIDE/2)
#define	INTAKE_TOP			((7*VCAR_HEIGHT)/40)
#define	INTAKE_Z_FRONT		(-(19*VCAR_LENGTH)/64)
#define	INTAKE_Z_REAR		(-(29*VCAR_LENGTH)/64)

static void StoreCarDetail( UTVERTEX *pVertices, DWORD side_colour, DWORD deck_colour )
{
	DWORD metal = SCRGB(SCR_BASE_COLOUR+14);			// bright, as the wheel rims are
	DWORD shade = SCRGBShaded(SCR_BASE_COLOUR+14, 0.7f);	// its sides, to give the boxes an edge

	// roll hoop: an upright each side of the cockpit and a bar across the top
	StoreCarBoxPair(HOOP_INNER, HOOP_OUTER, car_body[COCKPIT_SEGMENT+1].y_deck, HOOP_TOP,
					HOOP_Z_REAR, HOOP_Z_FRONT, pVertices, shade, metal, shade);
	StoreCarBox(-HOOP_OUTER, HOOP_OUTER, HOOP_BAR_BOTTOM, HOOP_TOP,
				HOOP_Z_REAR, HOOP_Z_FRONT, pVertices, shade, metal, shade);

	// exhaust down each flank
	StoreCarBoxPair(PIPE_INNER, PIPE_OUTER, PIPE_BOTTOM, PIPE_TOP,
					PIPE_Z_REAR, PIPE_Z_FRONT, pVertices, metal, shade, shade);

	// blade across the nose
	StoreCarBox(-BLADE_HALF_WIDTH, BLADE_HALF_WIDTH, BLADE_BOTTOM, BLADE_TOP,
				BLADE_Z_REAR, BLADE_Z_FRONT, pVertices, shade, shade, metal);

	// intake standing on the engine deck
	StoreCarBox(-INTAKE_HALF_WIDTH, INTAKE_HALF_WIDTH,
				car_body[BODY_SECTIONS-1].y_deck, INTAKE_TOP,
				INTAKE_Z_REAR, INTAKE_Z_FRONT, pVertices, side_colour, deck_colour, side_colour);
}

static void StoreCarBody( UTVERTEX *pVertices )
{
	DWORD side_colour, end_colour, floor_colour;
	DWORD deck_colour = SCRGB(SCR_BASE_COLOUR+15);
	DWORD tub_colour  = SCRGBShaded(WHEEL_TYRE_COLOUR, 0.9f);	// shadowed cockpit

	if (bSuperLeague)
	{
		side_colour  = SCRGB(SCR_BASE_COLOUR+21);
		end_colour   = SCRGB(SCR_BASE_COLOUR+20);
		floor_colour = SCRGB(SCR_BASE_COLOUR+19);
	}
	else
	{
		side_colour  = SCRGB(SCR_BASE_COLOUR+12);
		end_colour   = SCRGB(SCR_BASE_COLOUR+10);
		floor_colour = SCRGB(SCR_BASE_COLOUR+9);
	}

	/*	Each section's four corners: left and right, at the floor and at the deck. */
	COORD_3D lf[BODY_SECTIONS], rf[BODY_SECTIONS], ld[BODY_SECTIONS], rd[BODY_SECTIONS];

	for (long i = 0; i < static_cast<long>(BODY_SECTIONS); i++)
	{
		const CAR_SECTION *s = &car_body[i];

		lf[i].x = -s->half_width_floor;	lf[i].y = s->y_floor;	lf[i].z = s->z;
		rf[i].x =  s->half_width_floor;	rf[i].y = s->y_floor;	rf[i].z = s->z;
		ld[i].x = -s->half_width_deck;	ld[i].y = s->y_deck;	ld[i].z = s->z;
		rd[i].x =  s->half_width_deck;	rd[i].y = s->y_deck;	rd[i].z = s->z;
	}

	// nose and tail caps
	StoreCarQuad(&rf[0], &rd[0], &ld[0], &lf[0], pVertices, end_colour);
	long t = static_cast<long>(BODY_SECTIONS) - 1;
	StoreCarQuad(&lf[t], &ld[t], &rd[t], &rf[t], pVertices, end_colour);

	for (long f = 0; f < t; f++)		// f is the front section of the pair, r the rear
	{
		long r = f + 1;

		StoreCarQuad(&lf[f], &ld[f], &ld[r], &lf[r], pVertices, side_colour);	// left
		StoreCarQuad(&rf[r], &rd[r], &rd[f], &rf[f], pVertices, side_colour);	// right
		StoreCarQuad(&rf[r], &rf[f], &lf[f], &lf[r], pVertices, floor_colour);	// underside

		if (f != COCKPIT_SEGMENT)
		{
			StoreCarQuad(&ld[r], &ld[f], &rd[f], &rd[r], pVertices, deck_colour);
			continue;
		}

		/*	The cockpit. A rim of deck is left down each side and across each end, and
			the opening between them drops to a tub floor. The tub's walls face inward
			so they are stored both ways round - a face you are meant to see the back
			of is the one case backface culling gets wrong.						*/
		COORD_3D il[2], ir[2], tl[2], tr[2];	// opening edge, then tub floor, front/rear

		for (long e = 0; e < 2; e++)
		{
			long s = e ? r : f;
			long inner_x = car_body[s].half_width_deck - COCKPIT_RIM;
			long inner_z = car_body[s].z - (e ? -COCKPIT_RIM : COCKPIT_RIM);

			il[e].x = -inner_x;	il[e].y = car_body[s].y_deck;	il[e].z = inner_z;
			ir[e].x =  inner_x;	ir[e].y = car_body[s].y_deck;	ir[e].z = inner_z;
			tl[e] = il[e];	tl[e].y = COCKPIT_FLOOR;
			tr[e] = ir[e];	tr[e].y = COCKPIT_FLOOR;
		}

		// deck left around the opening: down each side, then across the nose and tail ends
		StoreCarQuad(&ld[r], &ld[f], &il[0], &il[1], pVertices, deck_colour);
		StoreCarQuad(&ir[1], &ir[0], &rd[f], &rd[r], pVertices, deck_colour);
		StoreCarQuad(&il[0], &ld[f], &rd[f], &ir[0], pVertices, deck_colour);
		StoreCarQuad(&ld[r], &il[1], &ir[1], &rd[r], pVertices, deck_colour);

		// the tub: four walls and a floor
		StoreCarTriangle2(&il[0], &il[1], &tl[1], pVertices, tub_colour);
		StoreCarTriangle2(&il[0], &tl[1], &tl[0], pVertices, tub_colour);
		StoreCarTriangle2(&ir[0], &ir[1], &tr[1], pVertices, tub_colour);
		StoreCarTriangle2(&ir[0], &tr[1], &tr[0], pVertices, tub_colour);
		StoreCarTriangle2(&il[0], &ir[0], &tr[0], pVertices, tub_colour);
		StoreCarTriangle2(&il[0], &tr[0], &tl[0], pVertices, tub_colour);
		StoreCarTriangle2(&il[1], &ir[1], &tr[1], pVertices, tub_colour);
		StoreCarTriangle2(&il[1], &tr[1], &tl[1], pVertices, tub_colour);
		StoreCarTriangle2(&tl[0], &tl[1], &tr[1], pVertices, tub_colour);
		StoreCarTriangle2(&tl[0], &tr[1], &tr[0], pVertices, tub_colour);
	}

	// front axle beam, spanning the gap the narrowed nose leaves out to the wheels
	StoreCarBox(-WHEEL_FRONT_INNER, WHEEL_FRONT_INNER,
				AXLE_BEAM_Y - AXLE_BEAM_THICK, AXLE_BEAM_Y + AXLE_BEAM_THICK,
				VCAR_LENGTH/2 - AXLE_BEAM_LONG, VCAR_LENGTH/2 + AXLE_BEAM_LONG,
				pVertices, floor_colour, side_colour, floor_colour);

	StoreCarDetail(pVertices, side_colour, deck_colour);
}

/*	The original car, kept for the opponent.

	This is the model as it was before the body was lofted and the wheels turned into
	cylinders: a six sided wedge with a flat quad at each corner for a wheel. Seen from
	another car that is what Stunt Car Racer's opponent has always looked like, and the
	new model - a cockpit tub, a roll hoop, spoked wheels - is detail you only ever get
	close enough to read on your own car anyway.

	The one thing it keeps from the rewrite is the suspension: the four wheel quads still
	ride up into their arches by the current compression, exactly as the cylinders do, so
	the opponent's wheels work over bumps rather than sitting welded to the body.

	Layout matches car_rest - four wheels of four vertices each in the order rear left,
	rear right, front left, front right - and then eight body points, rear four first. */
static const COORD_3D legacy_car_rest[16+8] = {
//x,					y,					z
{-VCAR_WIDTH/2,			-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},		// rear left wheel
{-VCAR_WIDTH/2,			0,					-VCAR_LENGTH/2},
{-VCAR_WIDTH/4,			0,					-VCAR_LENGTH/2},
{-VCAR_WIDTH/4,			-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},

{VCAR_WIDTH/4,			-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},		// rear right wheel
{VCAR_WIDTH/4,			0,					-VCAR_LENGTH/2},
{VCAR_WIDTH/2,			0,					-VCAR_LENGTH/2},
{VCAR_WIDTH/2,			-VCAR_HEIGHT/4,		-VCAR_LENGTH/2},

{-VCAR_WIDTH/2,			-VCAR_HEIGHT/4,		VCAR_LENGTH/2},			// front left wheel
{-VCAR_WIDTH/2,			0,					VCAR_LENGTH/2},
{-VCAR_WIDTH/4,			0,					VCAR_LENGTH/2},
{-VCAR_WIDTH/4,			-VCAR_HEIGHT/4,		VCAR_LENGTH/2},

{VCAR_WIDTH/4,			-VCAR_HEIGHT/4,		VCAR_LENGTH/2},			// front right wheel
{VCAR_WIDTH/4,			0,					VCAR_LENGTH/2},
{VCAR_WIDTH/2,			0,					VCAR_LENGTH/2},
{VCAR_WIDTH/2,			-VCAR_HEIGHT/4,		VCAR_LENGTH/2},

{-VCAR_WIDTH/4,			-VCAR_HEIGHT/8,		-VCAR_LENGTH/2},		// car rear points
{-(3*VCAR_WIDTH)/16,	VCAR_HEIGHT/4,		-VCAR_LENGTH/2},
{(3*VCAR_WIDTH)/16,		VCAR_HEIGHT/4,		-VCAR_LENGTH/2},
{VCAR_WIDTH/4,			-VCAR_HEIGHT/8,		-VCAR_LENGTH/2},

{-VCAR_WIDTH/4,			-VCAR_HEIGHT/8,		VCAR_LENGTH/2},			// car front points
{-VCAR_WIDTH/4,			0,					VCAR_LENGTH/2},
{VCAR_WIDTH/4,			0,					VCAR_LENGTH/2},
{VCAR_WIDTH/4,			-VCAR_HEIGHT/8,		VCAR_LENGTH/2}};

/*	One wheel quad, stored both ways round - the car is drawn with backface culling on
	and a flat quad has to be visible from either flank.							*/
static void StoreLegacyWheel( const COORD_3D *quad, UTVERTEX *pVertices, DWORD colour )
{
	StoreCarQuad(&quad[0], &quad[1], &quad[2], &quad[3], pVertices, colour);
	StoreCarQuad(&quad[3], &quad[2], &quad[1], &quad[0], pVertices, colour);
}

static void CreateLegacyCarInVB( UTVERTEX *pVertices, const CAR_SUSPENSION *susp )
{
COORD_3D car[16+8];

	memcpy(car, legacy_car_rest, sizeof(car));

	// Ride the four wheel groups up into their arches by the current compression
	for (long i = 0; i < 4; i++)
	{
		car[ 0+i].y += susp->rear_left;
		car[ 4+i].y += susp->rear_right;
		car[ 8+i].y += susp->front_left;
		car[12+i].y += susp->front_right;
	}

	DWORD wheel_colour = SCRGB(SCR_BASE_COLOUR+0);
	StoreLegacyWheel(&car[0],  pVertices, wheel_colour);	// rear left
	StoreLegacyWheel(&car[4],  pVertices, wheel_colour);	// rear right
	StoreLegacyWheel(&car[8],  pVertices, wheel_colour);	// front left
	StoreLegacyWheel(&car[12], pVertices, wheel_colour);	// front right

	const COORD_3D *b = &car[16];			// body: rear four points, then front four

	DWORD side_colour, end_colour, floor_colour;
	if (bSuperLeague)
	{
		side_colour  = SCRGB(SCR_BASE_COLOUR+21);
		end_colour   = SCRGB(SCR_BASE_COLOUR+20);
		floor_colour = SCRGB(SCR_BASE_COLOUR+19);
	}
	else
	{
		side_colour  = SCRGB(SCR_BASE_COLOUR+12);
		end_colour   = SCRGB(SCR_BASE_COLOUR+10);
		floor_colour = SCRGB(SCR_BASE_COLOUR+9);
	}

	StoreCarQuad(&b[4], &b[5], &b[1], &b[0], pVertices, side_colour);	// left
	StoreCarQuad(&b[3], &b[2], &b[6], &b[7], pVertices, side_colour);	// right
	StoreCarQuad(&b[0], &b[1], &b[2], &b[3], pVertices, end_colour);	// back
	StoreCarQuad(&b[7], &b[6], &b[5], &b[4], pVertices, end_colour);	// front
	StoreCarQuad(&b[1], &b[5], &b[6], &b[2], pVertices, SCRGB(SCR_BASE_COLOUR+15));	// top
	StoreCarQuad(&b[3], &b[7], &b[4], &b[0], pVertices, floor_colour);	// bottom
}


static void CreateCarInVB( UTVERTEX *pVertices, const CAR_SUSPENSION *susp, double roll )
{
COORD_3D car[16];

	memcpy(car, car_rest, sizeof(car));

	// Ride the four wheel groups up into their arches by the current compression
	for (long i = 0; i < 4; i++)
	{
		car[ 0+i].y += susp->rear_left;
		car[ 4+i].y += susp->rear_right;
		car[ 8+i].y += susp->front_left;
		car[12+i].y += susp->front_right;
	}

	/*	Fronts and rears are different sizes now, so the same road speed turns them at
		different rates - the smaller front wheel spins faster, as it should.	*/
	double rear_roll  = roll * WHEEL_ROLL_REAR_RATE;
	double front_roll = roll * WHEEL_ROLL_FRONT_RATE;

	StoreCarWheel(&car[0],  pVertices, rear_roll);		// rear left
	StoreCarWheel(&car[4],  pVertices, rear_roll);		// rear right
	StoreCarWheel(&car[8],  pVertices, front_roll);		// front left
	StoreCarWheel(&car[12], pVertices, front_roll);		// front right
/**/

	StoreCarBody(pVertices);
}

/*	Rebuild one car into its buffer. The two cars are drawn in the same frame at different
	ride heights and wheel angles, so they cannot share a buffer - hence the pair. The mesh
	is 780 triangles at most, so refilling both every frame is nothing.

	'legacy' picks the original wedge-and-quads model, which is what the opponent gets; it
	has its own triangle count, so the count is handed back for the draw call.		*/
static HRESULT RebuildCarVB( IDirect3DDevice9 *pd3dDevice, IDirect3DVertexBuffer9 **ppVB,
							 const CAR_SUSPENSION *susp, double roll,
							 bool legacy, long *pnumVertices )
{
	if (*ppVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( MAX_VERTICES_PER_CAR*sizeof(UTVERTEX),
				D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC, D3DFVF_UTVERTEX, D3DPOOL_DEFAULT, ppVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create car vertex buffer\n");
			return E_FAIL;
		}
	}

	UTVERTEX *pVertices;
	if( FAILED( (*ppVB)->Lock( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock car vertex buffer\n");
		return E_FAIL;
	}
	numCarVertices = 0;
	if (legacy)
		CreateLegacyCarInVB(pVertices, susp);
	else
		CreateCarInVB(pVertices, susp, roll);
	*pnumVertices = numCarVertices;
	(*ppVB)->Unlock();
	return S_OK;
}


HRESULT CreateCarVertexBuffer (IDirect3DDevice9 *pd3dDevice)
{
	// Both cars start at rest; UpdateCarSuspension() takes over from the first frame
	static const CAR_SUSPENSION rest = {0, 0, 0, 0};

	if (RebuildCarVB(pd3dDevice, &pCarVB, &rest, 0.0, false, &numPlayerCarVertices) != S_OK)
		return E_FAIL;
	if (RebuildCarVB(pd3dDevice, &pOpponentCarVB, &rest, 0.0, true, &numOpponentCarVertices) != S_OK)
		return E_FAIL;
	return S_OK;
}


/*	One wheel's physics compression turned into model units of travel about the rest pose:
	negative drooping, positive compressed. 'rest' is where the car sits under its own
	weight, 'droop' the span from there down to a free-hanging wheel, 'load' the span from
	there up to properly loaded. Anything past either end pegs at the travel limit, which
	is what a wheel at the end of its stroke does anyway.							*/
static long SuspensionTravel( long below, long rest, long droop, long load )
{
	long d = below - rest;
	long travel = (d < 0) ? (d * SUSP_MAX_TRAVEL / droop)
						  : (d * SUSP_MAX_TRAVEL / load);

	if (travel >  SUSP_MAX_TRAVEL) travel =  SUSP_MAX_TRAVEL;
	if (travel < -SUSP_MAX_TRAVEL) travel = -SUSP_MAX_TRAVEL;
	return travel;
}


/*	Clamp to the travel limit and split the shared axle by the roll the free axle shows.
	'shared' is the single compression both wheels of that axle run on; 'free_left' and
	'free_right' are the independent pair at the other end.							*/
static void BuildSuspension( CAR_SUSPENSION *susp, long free_left, long free_right,
							 long shared, bool shared_axle_is_rear )
{
	long roll = ((free_left - free_right) / 2) * SUSP_ROLL_SHARE_NUM / SUSP_ROLL_SHARE_DEN;

	if (shared_axle_is_rear)
	{
		susp->front_left  = free_left;
		susp->front_right = free_right;
		susp->rear_left   = shared + roll;
		susp->rear_right  = shared - roll;
	}
	else
	{
		susp->rear_left   = free_left;
		susp->rear_right  = free_right;
		susp->front_left  = shared + roll;
		susp->front_right = shared - roll;
	}

	long *wheel[4] = { &susp->rear_left, &susp->rear_right, &susp->front_left, &susp->front_right };
	for (long i = 0; i < 4; i++)
	{
		if (*wheel[i] >  SUSP_MAX_TRAVEL) *wheel[i] =  SUSP_MAX_TRAVEL;
		if (*wheel[i] < -SUSP_MAX_TRAVEL) *wheel[i] = -SUSP_MAX_TRAVEL;
	}
}


/*	Turn a car's forward speed into how far its wheels advance this frame.

	A truly correct rate - road distance over wheel radius - is far too fast to draw: at
	racing speed the wheel turns most of a revolution per frame, and a 12 sided rim with
	spokes on it under a 50Hz sample just strobes, or appears to run backwards. So the
	rate is proportional to speed but scaled down and then capped below the point where
	the pattern starts to alias, which is a segment or so per frame. Wagon wheels in films
	have the same problem and no fix; the eye reads "spinning fast" long before the rate
	is right, so this is tuned by look rather than by arithmetic.

	The rate is per second and scaled by the frame time, because this runs off the render
	loop rather than the 50Hz physics clock - otherwise the wheels would spin faster on a
	faster machine. The cap, though, is per frame: aliasing is a property of how far the
	pattern jumps between two drawn images, not of how long that took. It is divided by the
	front rate because the fronts are the fastest thing on the car: cap the shared angle at
	the limit and the fronts would jump twice it.

	The angle is kept as a double and never wrapped by the caller - it is fed to cos/sin,
	which is happy with any magnitude, and a long race is nowhere near losing precision.	*/
static double AdvanceWheelRoll( double angle, long z_speed, float fElapsedTime )
{
	double step = static_cast<double>(z_speed) * WHEEL_ROLL_PER_SPEED
											   * static_cast<double>(fElapsedTime);

	double limit = WHEEL_ROLL_MAX / WHEEL_ROLL_FRONT_RATE;

	if (step >  limit) step =  limit;
	if (step < -limit) step = -limit;

	return angle + step;
}


void UpdateCarSuspension (IDirect3DDevice9 *pd3dDevice, float fElapsedTime)
{
CAR_SUSPENSION susp;
long rear_left, rear_right, front;
static double player_roll = 0.0;

#define	PLAYER_TRAVEL(v)	SuspensionTravel((v), SUSP_PLAYER_REST, \
											 SUSP_PLAYER_DROOP, SUSP_PLAYER_LOAD)
#define	OPPONENT_TRAVEL(v)	SuspensionTravel((v), SUSP_OPPONENT_REST, \
											 SUSP_OPPONENT_DROOP, SUSP_OPPONENT_LOAD)

	// Player: the free pair is at the front, the rear pair share rear_amount_below_road
	BuildSuspension(&susp,
					PLAYER_TRAVEL(front_left_amount_below_road),
					PLAYER_TRAVEL(front_right_amount_below_road),
					PLAYER_TRAVEL(rear_amount_below_road),
					true);
	player_roll = AdvanceWheelRoll(player_roll, player_z_speed, fElapsedTime);
	RebuildCarVB(pd3dDevice, &pCarVB, &susp, player_roll, false, &numPlayerCarVertices);

	// Opponent: the other way round - the rear pair are free, the front wheels share
	GetOpponentWheelCompression(&rear_left, &rear_right, &front);
	BuildSuspension(&susp,
					OPPONENT_TRAVEL(rear_left),
					OPPONENT_TRAVEL(rear_right),
					OPPONENT_TRAVEL(front),
					false);
	// The opponent's model is the original one - flat wheel quads, with nothing to roll
	RebuildCarVB(pd3dDevice, &pOpponentCarVB, &susp, 0.0, true, &numOpponentCarVertices);
}


void FreeCarVertexBuffer (void)
{
	if (pCarVB) pCarVB->Release(), pCarVB = NULL;
	if (pOpponentCarVB) pOpponentCarVB->Release(), pOpponentCarVB = NULL;
}


static void DrawCarVB (IDirect3DDevice9 *pd3dDevice, IDirect3DVertexBuffer9 *pVB,
					   long numVertices)
{
	pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

	pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof(UTVERTEX) );
	pd3dDevice->SetFVF( D3DFVF_UTVERTEX );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, numVertices/3 );	// 3 points per triangle
}


void DrawCar (IDirect3DDevice9 *pd3dDevice)
{
	DrawCarVB(pd3dDevice, pCarVB, numPlayerCarVertices);
}


void DrawOpponentCar (IDirect3DDevice9 *pd3dDevice)
{
	DrawCarVB(pd3dDevice, pOpponentCarVB, numOpponentCarVertices);
}

struct TRANSFORMEDTEXVERTEX
{
    FLOAT x, y, z, rhw; // The transformed position for the vertex.
	FLOAT u, v;			// Texture
};
#define D3DFVF_TRANSFORMEDTEXVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

struct TRANSFORMEDCOLVERTEX
{
    FLOAT x, y, z, rhw;	// The transformed position for the vertex.
	DWORD color;		// Color
};
#define D3DFVF_TRANSFORMEDCOLVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

static IDirect3DVertexBuffer9 *pCockpitVB = NULL, *pSpeedBarCB = NULL;
#define MAX_COCKIPTVB 512
static int old_speedbar = -1;

extern IDirect3DTexture9 *g_pAtlas;
extern long front_left_height_difference, front_right_height_difference;
extern long leftwheel_angle, rightwheel_angle;
extern long boost_activated;
extern long new_damage;
extern long nholes;

/*	--- Cockpit wheel travel ------------------------------------------------------------
	How far up its stroke each front wheel sprite is drawn, straight out of the Amiga's
	update.wheel.positions ("Reference only/StuntCarRacer.s":11623):

		uwp1	move.l	#new.front.left.difference,a0
			move.w	(a0,d1.w),d0
			addi.w	#256,d0
			bpl	uwp2
			move.w	#0,d0
		uwp2	cmpi.w	#2048,d0
			bcs	uwp3
			move.w	#2047,d0
		uwp3	lsr.w	#3,d0
			not.b	d0
			asl.w	#1,d0
			move.l	#sin.table,a1
			move.w	(a1,d0.w),d0
			rol.w	#5,d0
			andi.b	#$1f,d0
			not.b	d0
			add.b	B.1bbdd,d0		( = $ba normally, $92 once wrecked )
		...	cmpi.b	#$b9,d0 / cmpi.b	#$97,d0		( clamp )

	Three things matter here, and the port had all three wrong.

	FIRST, the input is new.front.left.difference - road height minus actual height,
	clamped to [-$300, $1400] by car.collision.detection - and NOT amount.below.road.
	The two agree while the wheel is loaded, but the moment it leaves the road
	front.left.above.road zeroes amount.below.road, whereas the difference keeps going
	negative all the way to -$300. The difference is what knows about droop; the port
	was reading amount.below.road, so an airborne wheel was pinned at its ground pose
	and the sprite never dropped at all.

	SECOND, the curve. sin.table is the quarter-wave get.sin.cos reads: 257 entries (the
	516 perspective.table sits after covers 514 bytes plus padding), indexed there by
	(angle >> 5) & $3fe, and decreasing - entry 0 is the peak, so entry k is the cosine
	of k * PI/512. Here the index is 255 - i and only the top five bits of the entry
	survive rol #5 / and $1f. Working that back, the travel is 32 * sin((i+1) * PI/512):
	quick off the droop stop, flattening as the wheel packs up.

	THIRD, the range - and this is what the mask width settles. The entries are unsigned
	0.16, peaking at $ffff, so those five bits give a full 0..31. Had the table been the
	signed 1.15 one would assume, bit 15 would never be set, the step could not exceed
	15, and Crammond would have written and $0f. So the stroke is 32 lines of the Amiga's
	200-line screen - 32 * 2.4 = 76.8 in the 480-line space the cockpit art is scaled
	into here. The port's old amount.below.road >> 6 spanned 0..71, close in total but
	with every pixel of it on the compression side and none on droop.

	The wrecked case (B.1bbdd = $92) is a separate lowered ride height, not travel, and
	the port handles the wreck with its own artwork - so only the $ba value is used.	*/
#define	COCKPIT_WHEEL_STEPS		32			// the five bits left after rol #5 / and $1f
#define	COCKPIT_WHEEL_Y_SCALE	2.4f		// Amiga 200-line art into the 480-line base

/*	The Amiga's step at the static ride height, which is where the cockpit artwork is
	drawn. amount.below.road settles where the suspension force cancels gravity - see the
	travel note at the top of this file - so the difference rests at 317 too, giving
	i = (317 + 256) >> 3 = 71 and a step of 13. Offsets are taken from there, so the wheel
	sits where it always has when parked and now has stroke either side of it.		*/
#define	COCKPIT_WHEEL_REST_STEP	13

static int CockpitWheelStep( long height_difference )
{
	// new.front.left.difference: car.collision.detection's clamp, StuntCarRacer.s:15917
	if (height_difference >  0x1400) height_difference =  0x1400;
	if (height_difference < -0x300)  height_difference = -0x300;

	long i = height_difference + 256;
	if (i < 0) i = 0;
	if (i > 2047) i = 2047;
	i >>= 3;						// 0..255

	// sin.table[255 - i], keeping the top five bits. Equivalent to the quarter-wave
	// lookup, with none of the table: the entries are a plain unsigned 0.16 cosine.
	double entry = 65535.0 * cos((255 - i) * (M_PI / 512.0));
	int step = static_cast<int>(entry) >> 11;
	if (step < 0) step = 0;
	if (step > COCKPIT_WHEEL_STEPS - 1) step = COCKPIT_WHEEL_STEPS - 1;
	return step;
}

/*	Base-space pixels to raise the wheel sprite by: positive compressed, negative drooping. */
static float CockpitWheelOffset( long height_difference )
{
	return (CockpitWheelStep(height_difference) - COCKPIT_WHEEL_REST_STEP)
		   * COCKPIT_WHEEL_Y_SCALE;
}


HRESULT CreateCockpitVertexBuffer (IDirect3DDevice9 *pd3dDevice)
{
	if (pCockpitVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( MAX_COCKIPTVB*sizeof(TRANSFORMEDTEXVERTEX),
				D3DUSAGE_WRITEONLY, D3DFVF_TRANSFORMEDTEXVERTEX, D3DPOOL_DEFAULT, &pCockpitVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create cockpit vertex buffer\n");
			return E_FAIL;
		}
	}
	if (pSpeedBarCB == NULL)
	{
		if ( FAILED( pd3dDevice->CreateVertexBuffer( 4*sizeof(TRANSFORMEDCOLVERTEX),
				D3DUSAGE_WRITEONLY, D3DFVF_TRANSFORMEDCOLVERTEX, D3DPOOL_DEFAULT, &pSpeedBarCB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create speed bar vertex buffer\n");
			return E_FAIL;
		}
	}
	return S_OK;
}


void FreeCockpitVertexBuffer (void)
{
	if (pCockpitVB) pCockpitVB->Release(), pCockpitVB = NULL;
	if (pSpeedBarCB) pSpeedBarCB->Release(), pSpeedBarCB = NULL;
	/*if (pLeftwheelVB) pLeftwheelVB->Release(), pLeftwheelVB = NULL;
	if (pRightwheelVB) pRightwheelVB->Release(), pRightwheelVB = NULL;*/
}

extern long CalculateDisplaySpeed (void);

static int cockpit_vtx = 0;
static void AddQuad(TRANSFORMEDTEXVERTEX *pVertices, float x1, float y1, float x2, float y2, float z, int idx, int revX, float w) {
	float u1 = (revX)?atlas_tx2[idx]:atlas_tx1[idx], v1 = atlas_ty1[idx];
	float u2 = (revX)?atlas_tx1[idx]:atlas_tx2[idx], v2 = atlas_ty2[idx];
	if(w!=1.0f) {
		u2 = u1 + (u2-u1)*w;
	}
	pVertices+=cockpit_vtx;
	pVertices[0].x = x1; pVertices[0].y = y1; pVertices[0].z = z; pVertices[0].rhw = 1.0f;
	pVertices[1].x = x2; pVertices[1].y = y1; pVertices[1].z = z; pVertices[1].rhw = 1.0f;
	pVertices[2].x = x2; pVertices[2].y = y2; pVertices[2].z = z; pVertices[2].rhw = 1.0f;
	pVertices[0].u = u1; pVertices[0].v = v1;
	pVertices[1].u = u2; pVertices[1].v = v1;
	pVertices[2].u = u2; pVertices[2].v = v2;
	cockpit_vtx += 3;
	pVertices += 3;
	pVertices[0].x = x1; pVertices[0].y = y1; pVertices[0].z = z; pVertices[0].rhw = 1.0f;
	pVertices[1].x = x2; pVertices[1].y = y2; pVertices[1].z = z; pVertices[1].rhw = 1.0f;
	pVertices[2].x = x1; pVertices[2].y = y2; pVertices[2].z = z; pVertices[2].rhw = 1.0f;
	pVertices[0].u = u1; pVertices[0].v = v1;
	pVertices[1].u = u2; pVertices[1].v = v2;
	pVertices[2].u = u1; pVertices[2].v = v2;
	cockpit_vtx += 3;
}

#ifdef SCR_PORTABLE
extern int GL_MSAA;
#endif

void DrawCockpit (IDirect3DDevice9 *pd3dDevice)
{
#ifdef SCR_PORTABLE
	if(GL_MSAA)
		glDisable(GL_MULTISAMPLE);
#endif
	// Get current screen dimensions and calculate scale factors
	long current_width, current_height;
	GetScreenDimensions(&current_width, &current_height);
	float base_width = wideScreen ? static_cast<float>(BASE_WIDTH_WIDESCREEN) : static_cast<float>(BASE_WIDTH_STANDARD);
	float base_height = static_cast<float>(BASE_HEIGHT);
	float scaleX = static_cast<float>(current_width) / base_width;
	float scaleY = static_cast<float>(current_height) / base_height;
	
	// Prepare Cockpit drawing
	TRANSFORMEDTEXVERTEX *pVertices;
	cockpit_vtx = 0;
	if( FAILED( pCockpitVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock cockpit vertex buffer\n");
		return;
	}
	float leftwheel_y = CockpitWheelOffset(front_left_height_difference);
	float Wide = wideScreen ? COCKPIT_WIDESCREEN_OFFSET : 0.0f;
	float X1 = (Wide+COCKPIT_WHEEL_LEFT_OFFSET)*2*scaleX, X2 = ((Wide+COCKPIT_WHEEL_LEFT_OFFSET)*2+2*COCKPIT_WHEEL_WIDTH)*scaleX;
	float Y1 = (480.0f-COCKPIT_WHEEL_HEIGHT*2.4f-COCKPIT_WHEEL_BOTTOM_GAP*2.4f)*scaleY, Y2 = (480.0f-COCKPIT_WHEEL_BOTTOM_GAP*2.4f)*scaleY;
	Y1-=leftwheel_y*scaleY;
	Y2-=leftwheel_y*scaleY;
	AddQuad(pVertices, X1, Y1, X2, Y2, 0.8f, eWheel0+(leftwheel_angle>>16)%6, 0,1);
	float rightwheel_y = CockpitWheelOffset(front_right_height_difference);
	X1 = (Wide*2.f+640.f-COCKPIT_WHEEL_LEFT_OFFSET*2.f - COCKPIT_WHEEL_WIDTH*2)*scaleX, X2 = (Wide*2.f+640.f-COCKPIT_WHEEL_LEFT_OFFSET*2.f)*scaleX;
	Y1 = (480.0f-COCKPIT_WHEEL_HEIGHT*2.4f-COCKPIT_WHEEL_BOTTOM_GAP*2.4f)*scaleY, Y2 = (480.0f-COCKPIT_WHEEL_BOTTOM_GAP*2.4f)*scaleY;
	Y1-=rightwheel_y*scaleY;
	Y2-=rightwheel_y*scaleY;
	AddQuad(pVertices, X1, Y1, X2, Y2, 0.8f, eWheel0+(rightwheel_angle>>16)%6, 1,1);

	int engineFrame = eEngine;
	if(boost_activated) {
		static int frame = 0;
		frame = (frame+1)%16;
		const int engineframes[8] = {0,0,0,1,2,2,2,1};
		engineFrame = eEngineFlames0 + engineframes[frame>>1];
	}
	if(wideScreen) {
		AddQuad(pVertices, 0.0f, COCKPIT_WLEFT_Y_OFFSET*2.4f*scaleY, COCKPIT_WLEFT_X_OFFSET*2.f*scaleX, 480.0f*scaleY, 0.9f, (bSuperLeague)?eCockpitWL2:eCockpitWL, 0,1);
		AddQuad(pVertices, (800.f-COCKPIT_WRIGHT_X_OFFSET)*scaleX, COCKPIT_WRIGHT_Y_OFFSET*2.4f*scaleY, 800.f*scaleX, 480.0f*scaleY, 0.9f, (bSuperLeague)?eCockpitWR2:eCockpitWR, 0,1);
	}
	AddQuad(pVertices, (Wide+COCKPIT_ENGINE_X_OFFSET)*2.0f*scaleX, COCKPIT_ENGINE_Y_OFFSET*2.4f*scaleY, (Wide+COCKPIT_ENGINE_X_OFFSET+COCKPIT_ENGINE_WIDTH)*2.0f*scaleX, (COCKPIT_ENGINE_Y_OFFSET+COCKPIT_ENGINE_HEIGHT)*2.4f*scaleY, 0.89f, engineFrame, 0,1);
	AddQuad(pVertices, (Wide+COCKPIT_TOP_X_OFFSET)*2.f*scaleX, 0.0f, (Wide+COCKPIT_TOP_X_OFFSET+COCKPIT_TOP_WIDTH)*2.f*scaleX, COCKPIT_TOP_HEIGHT*2.4f*scaleY, 0.9f, (bSuperLeague)?eCockpitTop2:eCockpitTop, 0,1);
	AddQuad(pVertices, Wide*2.f*scaleX+0.0f, 0.0f, (Wide+COCKPIT_TOP_X_OFFSET)*2.f*scaleX, COCKPIT_SIDE_HEIGHT*2.4f*scaleY, 0.9f, (bSuperLeague)?eCockpitLeft2:eCockpitLeft, 0,1);
	AddQuad(pVertices, (Wide+COCKPIT_RIGHT_X_OFFSET)*2.f*scaleX, 0.0f, (640.0f+Wide*2.f)*scaleX, COCKPIT_SIDE_HEIGHT*2.4f*scaleY, 0.9f, (bSuperLeague)?eCockpitRight2:eCockpitRight, 0,1);
	AddQuad(pVertices, Wide*2*scaleX+0.0f, COCKPIT_SIDE_HEIGHT*2.4f*scaleY, (640.0f+Wide*2.f)*scaleX, 480.0f*scaleY, 0.9f, (bSuperLeague)?eCockpitBottom2:eCockpitBottom, 0,1);
	if (new_damage) {
		// cracking... width is 238, offset is 41 (in 320x200 screen space)
		float dam = static_cast<float>(new_damage); if (dam>COCKPIT_TOP_WIDTH) dam=COCKPIT_TOP_WIDTH;
		float damX1 = (Wide+COCKPIT_TOP_X_OFFSET)*2.0f*scaleX, damX2 = (Wide+COCKPIT_TOP_X_OFFSET+dam)*2.0f*scaleX;
		float damY1 = 0.0f, damY2 = 0.0f+COCKPIT_DAMAGE_HEIGHT*2.4f*scaleY;
		AddQuad(pVertices, damX1, damY1, damX2, damY2, 0.91f, (bSuperLeague)?eCracking2:eCracking, 0, dam/COCKPIT_TOP_WIDTH);
	}
	for (int i=0; i<nholes; i++) {
		float holeX1 = (Wide+COCKPIT_HOLE_X_OFFSET+COCKPIT_HOLE_SPACING*i)*2*scaleX, holeX2 = holeX1 + COCKPIT_HOLE_WIDTH*2.0f*scaleX;
		float holeY1 = 0.0f, holeY2 = 0.0f+COCKPIT_DAMAGE_HEIGHT*2.4f*scaleY;
		AddQuad(pVertices, holeX1, holeY1, holeX2, holeY2, 0.95f, (bSuperLeague)?eHole2:eHole, 0,1);
	}

	pCockpitVB->Unlock();

	// Prepare speedbar
	if (old_speedbar != CalculateDisplaySpeed()) {
		old_speedbar = CalculateDisplaySpeed();
		TRANSFORMEDCOLVERTEX *pSpeedVertices;
		if( FAILED( pSpeedBarCB->Lock( 0, 0, (void**)&pSpeedVertices, 0 ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to lock speed bar vertex buffer\n");
			return;
		}
		float speedX1 = (Wide*2.f+COCKPIT_SPEEDBAR_X_OFFSET)*scaleX, speedX2 = (Wide*2.f+COCKPIT_SPEEDBAR_X_OFFSET + ((old_speedbar > COCKPIT_SPEEDBAR_MAX) ? (old_speedbar-COCKPIT_SPEEDBAR_MAX) : old_speedbar)/static_cast<float>(COCKPIT_SPEEDBAR_MAX)*COCKPIT_SPEEDBAR_WIDTH)*scaleX;
		float speedY1 = (480.0f-COCKPIT_SPEEDBAR_Y_OFFSET)*scaleY, speedY2=(480.0f-COCKPIT_SPEEDBAR_Y_OFFSET+COCKPIT_SPEEDBAR_HEIGHT)*scaleY;
#ifdef SCR_PORTABLE
#define SPEEDCOL1 0xff00ffff	// ABGR
#define SPEEDCOL2 0xff00ccff	// ABGR
#else
#define SPEEDCOL1 0xffffff00	// ARGB
#define SPEEDCOL2 0xffffcc00	// ARGB
#endif
		pSpeedVertices[0].x = speedX1; pSpeedVertices[0].y = speedY1; pSpeedVertices[0].z = 1.0f; pSpeedVertices[0].rhw = 1.0f; pSpeedVertices[0].color = (old_speedbar > COCKPIT_SPEEDBAR_MAX)?SPEEDCOL2:SPEEDCOL1;
		pSpeedVertices[1].x = speedX2; pSpeedVertices[1].y = speedY1; pSpeedVertices[1].z = 1.0f; pSpeedVertices[1].rhw = 1.0f; pSpeedVertices[1].color = (old_speedbar > COCKPIT_SPEEDBAR_MAX)?SPEEDCOL2:SPEEDCOL1;
		pSpeedVertices[2].x = speedX2; pSpeedVertices[2].y = speedY2; pSpeedVertices[2].z = 1.0f; pSpeedVertices[2].rhw = 1.0f; pSpeedVertices[2].color = (old_speedbar > COCKPIT_SPEEDBAR_MAX)?SPEEDCOL2:SPEEDCOL1;
		pSpeedVertices[3].x = speedX1; pSpeedVertices[3].y = speedY2; pSpeedVertices[3].z = 1.0f; pSpeedVertices[3].rhw = 1.0f; pSpeedVertices[3].color = (old_speedbar > COCKPIT_SPEEDBAR_MAX)?SPEEDCOL2:SPEEDCOL1;
		pSpeedBarCB->Unlock();
	}

	pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_BLENDDIFFUSEALPHA);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTSS_COLORARG1);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
#ifndef SCR_PORTABLE
	// DirectX build only - the shim has no SetSamplerState. This used to say
	// "#ifdef WIN32", which MinGW predefines, so it broke the Windows SDL build.
	pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
#endif
	// Draw Cockpit
	pd3dDevice->SetTexture( 0, g_pAtlas );
	pd3dDevice->SetStreamSource( 0, pCockpitVB, 0, sizeof(TRANSFORMEDTEXVERTEX) );

	pd3dDevice->SetFVF( D3DFVF_TRANSFORMEDTEXVERTEX );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, cockpit_vtx/3 );	// 3 points per triangle

	// Draw Speed bar
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
	pd3dDevice->SetStreamSource( 0, pSpeedBarCB, 0, sizeof(TRANSFORMEDCOLVERTEX) );

	pd3dDevice->SetFVF( D3DFVF_TRANSFORMEDCOLVERTEX );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );	// 3 points per triangle

	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	//pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
#ifdef SCR_PORTABLE
	if(GL_MSAA)
		glEnable(GL_MULTISAMPLE);
#endif
}