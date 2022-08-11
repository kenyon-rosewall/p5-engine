#pragma once

#ifdef P5ENGINE_WIN32
#define P5ENGINE_API __declspec(dllexport)
//#define P5ENGINE_API __declspec(dllimport)
#endif

#ifndef P5ENGINE_H
#define P5ENGINE_H

/*
  TODO: 

  - Rendering
    - Lighting
	- Straighten out all coordinate systems!
	  - Screen
	  - World
	  - Texture
	- Optimization

  ARCHITECTURE EXPLORATION
  - z
	- Need to make a solid concept of ground levels so the camera can
	  be freely placed in z and have multiple ground levels in one
	  sim region
	- Concept of ground in the collision loop so it can handle 
	  collisions coming onto _and off of_ stairwells, for example.
	- Make sure flying things can go over low walls
	- How is this rendered?

  - Collision detection?
    - Fix sword collisions!
	- Clean up predicate proliferation. Can we make a nice clean
	  set of flags/rules so that it's easy to understand how
	  things work in terms of special handling? This may involve
	  making the iteration handle everything instead of handling
	  overlap outside and so on.
    - Transient collision rules! Clear based on flag.
	  - Allow non-transient rules to override transient ones
	  - Entry/exit
	- What's the plan for robustness / shape definition
	- (Implement reprojection to handle interpenetration)
	- "Things pushing other things"

  - Implement multiple sim regions per frame
    - Per-entity clocking
	- Sim region merging? For multiple players?
	- Simple zoomed-out view for testing?

  - Debug code
    - Fonts
	- Logging
	- Diagramming
	- (A LITTLE GUI, but only a little!) Switchs / sliders / etc.
	- Draw tile chunks so we can verify that things are aligned /
	  in the chunks we want them to be in / 

  - Asset streaming

  - Audio
	- Sound effect triggers
	- Ambient sounds
	- Music

  - Metagame / save game
    - How do you enter "save slot"?
    - Persistent unlocks/etc.
    - Do we allow saved games? Probably yes, only for "pausing",
	- Continuous save for crash recovery
	
  - Rudimentary world gen (no quality, just "what sorts of things" we do)
    - Placement of background things
	- Connectivity?
	- Non-overlapping?
	- Map display

  - AI
	- Rudimentary monstar behavior example
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

#define Minimum(A, b) ((A < b) ? (A) : (b))
#define Maximum(A, b) ((A > b) ? (A) : (b))

//
//
//

struct memory_arena
{
	memory_index Size;
	uint8* Base;
	memory_index Used;

	uint32 TempCount;
};

struct temporary_memory
{
	memory_arena* Arena;
	memory_index Used;
};

inline void
InitializeArena(memory_arena* Arena, memory_index Size, void* Base)
{
	Arena->Size = Size;
	Arena->Base = (uint8*)Base;
	Arena->Used = 0;
	Arena->TempCount = 0;
}

#define PushStruct(Arena, type) (type*)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, PieceCount, type) (type*)PushSize_(Arena, (uint64_t)(PieceCount)*sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)
inline void*
PushSize_(memory_arena* Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	void* Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return(Result);
}

inline temporary_memory
BeginTemporaryMemory(memory_arena* Arena)
{
	temporary_memory Result = {};

	Result.Arena = Arena;
	Result.Used = Arena->Used;

	++Result.Arena->TempCount;

	return(Result);
}

inline void
EndTemporaryMemory(temporary_memory TempMemory)
{
	memory_arena* Arena = TempMemory.Arena;
	Assert(Arena->Used >= TempMemory.Used);
	Arena->Used = TempMemory.Used;
	Assert(Arena->TempCount > 0);
	--Arena->TempCount;
}

inline void
CheckArena(memory_arena* Arena)
{
	Assert(Arena->TempCount == 0);
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
#include "p5engine_render_group.h"

struct hero_bitmaps
{
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

struct ground_buffer
{
	world_position Pos;
	loaded_bitmap Bitmap;
};

struct game_state
{
	memory_arena WorldArena;
	world* World;

	real32 TypicalFloorHeight;

	// TODO: Should we allow split-screen?
	uint32 CameraFollowingEntityIndex;
	world_position CameraP;
	
	controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

	// TODO: Change the name to stored entity
	uint32 LowEntityCount;
	low_entity LowEntities[100000];

	loaded_bitmap Grass;
	loaded_bitmap Soil[3];
	loaded_bitmap Tuft[2];

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
	sim_entity_collision_volume_group* StandardRoomCollision;

	real32 Time;

	loaded_bitmap TestDiffuse;
	loaded_bitmap TestNormal;
};

struct transient_state
{
	bool32 IsInitialized;
	memory_arena TransientArena;
	uint32 GroundBufferCount;
	ground_buffer* GroundBuffers;

	uint32 EnvMapWidth;
	uint32 EnvMapHeight;
	// NOTE: 0 is bottom, 1 is middle, 2 is top
	environment_map EnvMaps[3];
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