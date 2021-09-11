#pragma once

#ifdef P5ENGINE_WIN32
#define P5ENGINE_API __declspec(dllexport)
//#define P5ENGINE_API __declspec(dllimport)
#endif

#ifndef P5ENGINE_H
#define P5ENGINE_H

/*
  TODO: 

  ARCHITECTURE EXPLORATION
  - Collision detection?
    - Transient collision rules! Clear based on flag.
	- Entry/exit
	- What's the plan for robustness / shape definition
	- (Implement reprojection to handle interpenetration)
  - Z
	- Figure out how you go "up" and "down", and how is this rendered?
  - Implement multiple sim regions per frame
    - Per-entity clocking
	- Sim region merging? For multiple players?

  - Debug code
	- Logging
	- Diagramming
	- (A LITTLE GUI, but only a little!) Switchs / sliders / etc.

  - Audio
	- Sound effect triggers
	- Ambient sounds
	- Music
  - Asset streaming

  - Metagame / save game
    - Persistent unlocks/etc.
    - Do we allow saved games? Probably yes, only for "pausing",
	- Continuous save for crash recovery
  - Rudimentary world genreratione(no quality, just "what sorts of things" we do)
    - Placement of background things
	- Connectivity?
	- Non-overlapping?
  - AI
	- Cudimentary monstar behavior example
	- Pathfinding
	- AI "storage"

  - Animation, should probably lead into rendering
    - Skeletal animation
	- Particle systems

  PRODUCTION
  - Rendering
  -> GAME
    - Entity system
    - World generation

*/

#include "p5engine_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

//
//
//

struct memory_arena
{
	memory_index Size;
	uint8* Base;
	memory_index Used;
};

inline void
InitializeArena(memory_arena* Arena, memory_index Size, void* Base)
{
	Arena->Size = Size;
	Arena->Base = (uint8*)Base;
	Arena->Used = 0;
}

#define PushStruct(Arena, type) (type*)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, PieceCount, type) (type*)PushSize_(Arena, (uint64_t)(PieceCount)*sizeof(type))
inline void*
PushSize_(memory_arena* Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	void* Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return(Result);
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void
ZeroSize(memory_index Size, void* Ptr)
{
	// TODO: Check this guy for performance
	uint8* Byte = (uint8*)Ptr;
	while (Size--)
	{
		*Byte++ = 0;
	}
}

#include "p5engine_intrinsics.h"
#include "p5engine_math.h"
#include "p5engine_world.h"
#include "p5engine_sim_region.h"
#include "p5engine_entity.h"

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	uint32* Pixels;
};

struct hero_bitmaps
{
	v2 Align;

	loaded_bitmap Character;
};

struct low_entity
{
	// TODO: It's kind of busted that Pos's can be invalid here, 
	// AND we store whether they would be invalid in the flags field...
	// Can we do something better here?
	world_position Pos;
	sim_entity Sim;
};

struct entity_visible_piece
{
	loaded_bitmap* Bitmap;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;

	real32 R, G, B, A;
	v2 Dim;
};

struct controlled_hero
{
	uint32 EntityIndex;
	real32 SpeedMultiplier;

	// NOTE: These are the controller requests for simulation
	v3 ddP;
	v3 dSword;
	real32 dZ;
};

struct pairwise_collision_rule
{
	bool32 CanCollide;
	uint32 StorageIndexA;
	uint32 StorageIndexB;

	pairwise_collision_rule* NextInHash;
};

struct game_state
{
	memory_arena WorldArena;
	world* World;
	real32 MetersToPixels;

	// TODO: Should we allow split-screen?
	uint32 CameraFollowingEntityIndex;
	world_position CameraP;
	
	controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

	// TODO: Change the name to stored entity
	uint32 LowEntityCount;
	low_entity LowEntities[100000];

	loaded_bitmap Backdrop;
	loaded_bitmap Shadow;
	hero_bitmaps Hero[4];

	loaded_bitmap Tree;
	loaded_bitmap Stairs;
	loaded_bitmap Monstar;
	loaded_bitmap Familiar;
	loaded_bitmap Sword[4];

	// TODO: Must be power of two
	pairwise_collision_rule* CollisionRuleHash[256];
	pairwise_collision_rule* FirstFreeCollisionRule;

	sim_entity_collision_volume_group* NullCollision;
	sim_entity_collision_volume_group* SwordCollision;
	sim_entity_collision_volume_group* StairsCollision;
	sim_entity_collision_volume_group* HeroCollision;
	sim_entity_collision_volume_group* MonstarCollision;
	sim_entity_collision_volume_group* FamiliarCollision;
	sim_entity_collision_volume_group* WallCollision;
};

// TODO: This is dumb, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct entity_visible_piece_group
{
	uint32 PieceCount;
	entity_visible_piece Pieces[8];

	game_state* GameState;
};

inline low_entity*
GetLowEntity(game_state* GameState, uint32 LowIndex)
{
	low_entity* Result = 0;

	if ((LowIndex > 0) && (LowIndex < GameState->LowEntityCount))
	{
		Result = GameState->LowEntities + LowIndex;
	}

	return(Result);
}

internal void
AddCollisionRule(game_state* GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide);
internal void
ClearCollisionRulesFor(game_state* GameState, uint32 StorageIndex);
#endif // !P5ENGINE_H