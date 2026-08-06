/**************************************************************************

    Particles.h - World-space particle effects

    The player's sparks and dust are the Amiga's own effect: flat rectangles
    plotted straight into the 256x128 playfield in screen space, always from
    the bottom centre of the view where the player's own wheels are.  See
    DrawSceneParticles() in Car_Behaviour.cpp.  That cannot show anything
    happening to a car out in the world.

    This is the world-space counterpart, used for the opponent's landings.
    Particles are spawned at a world position, left behind as the car drives
    on, and depth tested against the road like any other scene geometry.

    RENDER ONLY.  Nothing here may touch the simulation, and everything here
    must draw from plain rand(), never SCR_Rand() - see Det_Rand.h.

 **************************************************************************/

#ifndef	_PARTICLES
#define	_PARTICLES

#ifdef SCR_PORTABLE
#include "dx_linux.h"
#endif

/*	Throw a burst of sparks from a world position.  'ferocity' runs 0..1 and sets both how
	many are thrown and how hard.  Slots are recycled oldest-first, so a burst never fails
	to appear - it just cuts an older one short.									*/
extern void EmitImpactSparks( const D3DXVECTOR3 *world_pos, float ferocity );

/*	Age the table and let gravity work on it.  Once per rendered frame.				*/
extern void UpdateWorldParticles( float fElapsedTime );

/*	Rebuild the vertex buffer and draw whatever is still in flight.  Expects the world
	transform to be identity - the particles are already in world space.				*/
extern void DrawWorldParticles( IDirect3DDevice9 *pd3dDevice );

/*	Park every slot as dead.  Called when a race starts, so a shower thrown on the last
	lap is not still hanging in the air on the next one.								*/
extern void ResetWorldParticles( void );

extern void FreeWorldParticleVertexBuffer( void );

#endif	/* _PARTICLES */
