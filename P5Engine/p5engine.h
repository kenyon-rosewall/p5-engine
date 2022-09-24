#pragma once

#ifdef P5ENGINE_WIN32
#define P5ENGINE_API __declspec(dllexport)
//#define P5ENGINE_API __declspec(dllimport)
#endif

#ifndef P5ENGINE_H
#define P5ENGINE_H

/*
  TODO: 

  - Flush all thread queues before reloading DLL

  - Rendering
	- Straighten out all coordinate systems!
	  - Screen
	  - World
	  - Texture
	- Particle systems
    - Lighting
	- Final Optimization

  - Asset streaming
	- File format
	- Memory management

  - Debug code
    - Fonts
	- Logging
	- Diagramming
	- (A LITTLE GUI, but only a little!) Switchs / sliders / etc.
	- Draw tile chunks so we can verify that things are aligned /
	  in the chunks we want them to be in / 
	- Thread visualization

  - Audio
	- Sound effect triggers
	- Ambient sounds
	- Music

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
	u8* Base;
	memory_index Used;

	u32 TempCount;
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
	Arena->Base = (u8*)Base;
	Arena->Used = 0;
	Arena->TempCount = 0;
}

inline memory_index
GetAlignmentOffset(memory_arena* Arena, memory_index Alignment)
{
	memory_index AlignmentOffset = 0;
	memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
	memory_index AlignmentMask = Alignment - 1;
	if (ResultPointer & AlignmentMask)
	{
		AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
	}

	return(AlignmentOffset);
}

inline memory_index
GetArenaSizeRemaining(memory_arena* Arena, memory_index Alignment = 4)
{
	memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));

	return(Result);
}

#define PushStruct(Arena, type, ...) (type*)PushSize_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, PieceCount, type, ...) (type*)PushSize_(Arena, (uint64_t)(PieceCount)*sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ## __VA_ARGS__)
inline void*
PushSize_(memory_arena* Arena, memory_index SizeInit, memory_index Alignment = 4)
{
	memory_index Size = SizeInit;

	memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
	Size += AlignmentOffset;

	Assert((Arena->Used + Size) <= Arena->Size);
	void* Result = Arena->Base + Arena->Used + AlignmentOffset;
	Arena->Used += Size;

	Assert(Size >= SizeInit);

	return(Result);
}

// NOTE: This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char*
PushString(memory_arena* Arena, char* Source)
{
	u32 Size = 1;
	for (char* At = Source; *At; ++At)
	{
		++Size;
	}

	char* Dest = (char*)PushSize_(Arena, Size);
	for (u32 CharIndex = 0; CharIndex < Size; ++CharIndex)
	{
		Dest[CharIndex] = Source[CharIndex];
	}

	return(Dest);
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

inline void
SubArena(memory_arena* Result, memory_arena* Arena, memory_index Size, memory_index Alignment = 16)
{
	Result->Size = Size;
	Result->Base = (u8*)PushSize_(Arena, Size, Alignment);
	Result->Used = 0;
	Result->TempCount = 0;
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void
ZeroSize(memory_index Size, void* Ptr)
{
	// TODO: Check this guy for performance
	u8* Byte = (u8*)Ptr;
	while (Size--)
	{
		*Byte++ = 0;
	}
}

#include "p5engine_intrinsics.h"
#include "p5engine_math.h"
#include "p5engine_file_formats.h"
#include "p5engine_random.h"
#include "p5engine_world.h"
#include "p5engine_sim_region.h"
#include "p5engine_entity.h"
#include "p5engine_render_group.h"
#include "p5engine_asset.h"
#include "p5engine_audio.h"

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
	u32 EntityIndex;
	f32 SpeedMultiplier;

	// NOTE: These are the controller requests for simulation
	v3 ddP;
	v3 dSword;
	f32 dZ;
};

struct pairwise_collision_rule
{
	b32 CanCollide;
	u32 StorageIndexA;
	u32 StorageIndexB;

	pairwise_collision_rule* NextInHash;
};

struct ground_buffer
{
	world_position Pos;
	loaded_bitmap Bitmap;
};

struct hero_bitmap_ids
{
	bitmap_id Character;
};

struct game_state
{
	b32 IsInitialized;

	memory_arena MetaArena;
	memory_arena WorldArena;
	world* World;

	f32 TypicalFloorHeight;

	// TODO: Should we allow split-screen?
	u32 CameraFollowingEntityIndex;
	world_position CameraP;
	
	controlled_hero ControlledHeroes[ArrayCount(((game_input*)0)->Controllers)];

	// TODO: Change the name to stored entity
	u32 LowEntityCount;
	low_entity LowEntities[100000];

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

	f32 Time;

	// TODO: Re-fill this guy with gray
	loaded_bitmap TestDiffuse;
	loaded_bitmap TestNormal;

	random_series GeneralEntropy;
	f32 tSine;

	audio_state AudioState;
	playing_sound* Music;
};

struct task_with_memory
{
	b32 BeingUsed;
	memory_arena Arena;

	temporary_memory MemoryFlush;
};

struct transient_state
{
	b32 IsInitialized;
	memory_arena TransientArena;

	task_with_memory Tasks[4];

	game_assets* Assets;

	u32 GroundBufferCount;
	ground_buffer* GroundBuffers;

	platform_work_queue* HighPriorityQueue;
	platform_work_queue* LowPriorityQueue;

	u32 EnvMapWidth;
	u32 EnvMapHeight;
	// NOTE: 0 is bottom, 1 is middle, 2 is top

	environment_map EnvMaps[3];
};

inline low_entity*
GetLowEntity(game_state* GameState, u32 LowIndex)
{
	low_entity* Result = 0;

	if ((LowIndex > 0) && (LowIndex < GameState->LowEntityCount))
	{
		Result = GameState->LowEntities + LowIndex;
	}

	return(Result);
}

global_variable platform_add_entry* PlatformAddEntry;
global_variable platform_complete_all_work* PlatformCompleteAllWork;
global_variable debug_platform_read_entire_file* PlatformReadEntireFile;

internal void
AddCollisionRule(game_state* GameState, u32 StorageIndexA, u32 StorageIndexB, b32 ShouldCollide);
internal void
ClearCollisionRulesFor(game_state* GameState, u32 StorageIndex);

internal task_with_memory* BeginTaskWithMemory(transient_state* TransientState);
internal void EndTaskWithMemory(task_with_memory* Task);

#endif // !P5ENGINE_H