/**************************************************************************

    Particles.cpp - World-space particle effects

 **************************************************************************/

/*	============= */
/*	Include files */
/*	============= */
#include "dxstdafx.h"

#include <math.h>
#include <stdlib.h>

#include "Particles.h"
#include "StuntCarRacer.h"
#include "Car.h"

/*	========= */
/*	Constants */
/*	========= */

/*	A hard landing throws up to SPARKS_PER_BURST, and three wheels can land within a frame
	or two of each other, so the table has to hold a few bursts at once without the newest
	one stealing slots from a shower that is still visibly in the air.				*/
#define	MAX_WORLD_PARTICLES		96
#define	SPARKS_PER_BURST		14

/*	Sparks are drawn as tiny solid octahedra rather than as camera-facing quads.  Billboards
	would need the camera's right and up vectors pulled back out of the view matrix, and
	D3DXMATRIX is a real DirectX matrix on Windows but a glm::mat4 on the portable path
	(dx_linux.h), with no portable way to read an element out of either.  A solid blob
	presents the same silhouette from every direction, which is all a flat, opaque,
	two-pixel Amiga spark ever did anyway.  Eight faces, no winding to get wrong.	*/
#define	PARTICLE_TRIANGLES		8
#define	PARTICLE_VERTICES		(PARTICLE_TRIANGLES * 3)
#define	MAX_PARTICLE_VERTICES	(MAX_WORLD_PARTICLES * PARTICLE_VERTICES)

/*	Motion, in world units per second.  A car is VCAR_HEIGHT (162) units tall, which is the
	only scale worth reasoning from - these are tuned by eye against it rather than derived
	from anything.  Gravity is stronger than life so the arc stays short and snappy at the
	speeds the cars land at.														*/
#define	SPARK_GRAVITY			2200.0f
#define	SPARK_SPEED_UP			420.0f		// upward throw at full ferocity
#define	SPARK_SPEED_SPREAD		260.0f		// sideways and fore/aft scatter
#define	SPARK_LIFE				0.55f		// seconds
#define	SPARK_LIFE_SPREAD		0.35f

/*	Apparent size.  The camera is equiangular at 0.17578 degrees per pixel of the 320-wide
	base space, so a pixel subtends 1/PARTICLE_FOCAL_PX of the distance to the eye.  Scaling
	the blob with distance holds it at a constant couple of base-space pixels - the chunky
	look the Amiga sparks have - instead of dwindling to nothing behind the car.  Clamped at
	both ends: a spark right under the camera must not fill the screen, and a far one must
	not grow big enough in world terms to poke through the road it is bouncing off.	*/
#define	PARTICLE_FOCAL_PX		326.0f
#define	PARTICLE_PIXELS			2.5f
#define	PARTICLE_MIN_SIZE		2.5f
#define	PARTICLE_MAX_SIZE		18.0f

/*	The Amiga spark is a two-by-two block of colour 3 (yellow) with a colour 15 (white)
	pixel at its top right - draw.spark, and DrawSpark() in Car_Behaviour.cpp.  With no
	room for a highlight at this size, the mix is made across the burst instead.		*/
#define	SPARK_COLOUR_YELLOW		3
#define	SPARK_COLOUR_WHITE		15


/*	===================== */
/*	Structure definitions */
/*	===================== */

typedef struct
{
	D3DXVECTOR3	pos;		// world space
	D3DXVECTOR3	vel;		// world units per second
	float		life;		// seconds remaining, <= 0 meaning the slot is free
	DWORD		colour;
} WORLD_PARTICLE;


/*	================= */
/*	Global variables */
/*	================= */

static WORLD_PARTICLE particles[MAX_WORLD_PARTICLES];
static long next_slot = 0;

static IDirect3DVertexBuffer9 *pParticleVB = NULL;
static long numParticleVertices = 0;


/*	================= */
/*	Local functions   */
/*	================= */

/*	A random float in 0..1.  Deliberately plain rand(), not SCR_Rand(): Det_Math/Det_Rand
	keep the simulation on a shared deterministic stream, and pulling a render-only effect
	onto it would make the sim depend on how often frames are drawn - which desyncs a
	lockstep netplay race immediately.  See the note at the top of Det_Rand.h.		*/
static inline float RandUnit( void )
{
	return static_cast<float>(rand() & 0x3fff) / static_cast<float>(0x3fff);
}

// A random float in -1..1
static inline float RandSigned( void )
{
	return (RandUnit() * 2.0f) - 1.0f;
}


/*	Store one octahedron, centred on the particle, into the vertex buffer.  Culling is off
	for the draw, so the face winding does not matter.								*/
static void StoreParticle( UTVERTEX *pVertices, const WORLD_PARTICLE *p, float size )
{
	// +x, -x, +y, -y, +z, -z
	static const float axis[6][3] = {{ 1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
									 { 0.0f, 1.0f, 0.0f}, { 0.0f,-1.0f, 0.0f},
									 { 0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f,-1.0f}};

	// Each face takes one x pole, one y pole and one z pole
	static const long face[PARTICLE_TRIANGLES][3] = {{0,2,4}, {0,2,5}, {0,3,4}, {0,3,5},
													 {1,2,4}, {1,2,5}, {1,3,4}, {1,3,5}};

	long v = 0;
	for (long f = 0; f < PARTICLE_TRIANGLES; f++)
		for (long c = 0; c < 3; c++)
		{
			const float *a = axis[face[f][c]];

			pVertices[v].pos.x = p->pos.x + (a[0] * size);
			pVertices[v].pos.y = p->pos.y + (a[1] * size);
			pVertices[v].pos.z = p->pos.z + (a[2] * size);
			pVertices[v].color = p->colour;
			pVertices[v].tu = 0.0f;
			pVertices[v].tv = 0.0f;
			v++;
		}
}


/*	================= */
/*	Global functions  */
/*	================= */

void ResetWorldParticles( void )
{
	for (long i = 0; i < MAX_WORLD_PARTICLES; i++)
		particles[i].life = 0.0f;

	next_slot = 0;
	numParticleVertices = 0;
}


void EmitImpactSparks( const D3DXVECTOR3 *world_pos, float ferocity )
{
	if (ferocity <= 0.0f) return;
	if (ferocity > 1.0f) ferocity = 1.0f;

	// A gentle landing gets a couple of sparks, a hard one a full shower
	long count = 2 + static_cast<long>(ferocity * (SPARKS_PER_BURST - 2));

	for (long n = 0; n < count; n++)
	{
		/*	Round-robin rather than a free-slot search: with the table sized for several
			bursts the slot being reused is the oldest one, which is either already dead
			or the tail of a shower thrown some time ago.						*/
		WORLD_PARTICLE *p = &particles[next_slot];
		next_slot = (next_slot + 1) % MAX_WORLD_PARTICLES;

		p->pos = *world_pos;

		/*	Thrown up and out.  The horizontal scatter is not scaled by ferocity as hard
			as the lift is - a light landing should still spit its few sparks sideways,
			or they just bob straight up and down and read as a fountain.		*/
		const float lift = SPARK_SPEED_UP * (0.35f + (0.65f * ferocity));

		p->vel.x = RandSigned() * SPARK_SPEED_SPREAD * (0.5f + (0.5f * ferocity));
		p->vel.y = lift * (0.5f + (0.5f * RandUnit()));
		p->vel.z = RandSigned() * SPARK_SPEED_SPREAD * (0.5f + (0.5f * ferocity));

		p->life = SPARK_LIFE + (RandUnit() * SPARK_LIFE_SPREAD);

		// Mostly yellow with a scatter of white, as the Amiga's two-by-two block is
		p->colour = SCRGB(((rand() & 3) == 0) ? SPARK_COLOUR_WHITE : SPARK_COLOUR_YELLOW);
	}
}


void UpdateWorldParticles( float fElapsedTime )
{
	/*	Clamped for the same reason UpdateSparks() clamps its step: coming back from a
		menu, or any other stall, must not fling the whole table off into the distance
		in a single frame.														*/
	float dt = fElapsedTime;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.1f) dt = 0.1f;

	for (long i = 0; i < MAX_WORLD_PARTICLES; i++)
	{
		WORLD_PARTICLE *p = &particles[i];
		if (p->life <= 0.0f)
			continue;

		p->life -= dt;
		if (p->life <= 0.0f)
			continue;

		p->vel.y -= SPARK_GRAVITY * dt;

		p->pos.x += p->vel.x * dt;
		p->pos.y += p->vel.y * dt;
		p->pos.z += p->vel.z * dt;
	}
}


void DrawWorldParticles( IDirect3DDevice9 *pd3dDevice )
{
	numParticleVertices = 0;

	// Nothing in flight - skip the lock entirely, which is the usual case
	long live = 0;
	for (long i = 0; i < MAX_WORLD_PARTICLES; i++)
		if (particles[i].life > 0.0f)
			live++;
	if (live == 0)
		return;

	if (pParticleVB == NULL)
	{
		if( FAILED( pd3dDevice->CreateVertexBuffer( MAX_PARTICLE_VERTICES*sizeof(UTVERTEX),
				D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC, D3DFVF_UTVERTEX,
				D3DPOOL_DEFAULT, &pParticleVB, NULL ) ) )
		{
			OutputDebugStringW(L"ERROR: Failed to create particle vertex buffer\n");
			return;
		}
	}

	UTVERTEX *pVertices;
	if( FAILED( pParticleVB->Lock( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD ) ) )
	{
		OutputDebugStringW(L"ERROR: Failed to lock particle vertex buffer\n");
		return;
	}

	D3DXVECTOR3 eye;
	GetEyeWorldPosition(&eye);

	for (long i = 0; i < MAX_WORLD_PARTICLES; i++)
	{
		const WORLD_PARTICLE *p = &particles[i];
		if (p->life <= 0.0f)
			continue;

		const float dx = p->pos.x - eye.x;
		const float dy = p->pos.y - eye.y;
		const float dz = p->pos.z - eye.z;
		const float distance = sqrtf((dx*dx) + (dy*dy) + (dz*dz));

		float size = (distance * PARTICLE_PIXELS) / PARTICLE_FOCAL_PX;
		if (size < PARTICLE_MIN_SIZE) size = PARTICLE_MIN_SIZE;
		if (size > PARTICLE_MAX_SIZE) size = PARTICLE_MAX_SIZE;

		StoreParticle(&pVertices[numParticleVertices], p, size);
		numParticleVertices += PARTICLE_VERTICES;
	}

	pParticleVB->Unlock();

	if (numParticleVertices == 0)
		return;

	pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

	pd3dDevice->SetStreamSource( 0, pParticleVB, 0, sizeof(UTVERTEX) );
	pd3dDevice->SetFVF( D3DFVF_UTVERTEX );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, numParticleVertices/3 );

	// Put the cull mode back the way the rest of the world pass expects to find it
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
}


void FreeWorldParticleVertexBuffer( void )
{
	if (pParticleVB) pParticleVB->Release(), pParticleVB = NULL;
	numParticleVertices = 0;
}
