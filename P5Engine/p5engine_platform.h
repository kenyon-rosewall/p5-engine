#pragma once

#ifndef P5ENGINE_PLATFORM_H
#define P5ENGINE_PLATFORM_H

/*
  NOTE:

  P5ENGINE_INTERNAL
   0 - Build for public release
   1 - Build for developer only

  P5ENGINE_SLOW
   0 - No slow code allowed!
   1 - Slow code welcome
*/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//
// NOTE: Compilers
//

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif // !COMPILER_MSVC

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif // !COMPILER_LLVM

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO: More Compilers
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif // _MSC_VER
#endif // !COMPILER_MSVC && !COMPILER_LLVM

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SSE/NEON optimizations are not available for this compiler yet
#endif

#include "p5engine_types.h"

typedef struct thread_context
{
	int Placeholder;
} thread_context;

/*
NOTE: Services tha the platform layer provides to the game.
*/

#if P5ENGINE_INTERNAL
/*
IMPORTANT:

These are NOT for doing anything in the shipping game - they are
blocking and the write doesn't protect against lost data!
*/
typedef struct debug_read_file_result
{
	u32 ContentsSize;
	void* Contents;
}  debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(char* Filename, u32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

enum class debug_cycle_counter_id
{
	/* 0 */ GameUpdateRender,
	/* 1 */ RenderGroupOutput,
	/* 2 */ DrawRectangleSlow,
	/* 3 */ ProcessPixel,
	/* 4 */ DrawRectangleQuick,
	Count,
};

typedef struct debug_cycle_counter
{
	u64 CycleCount;
	u32 HitCount;
} debug_cycle_counter;

extern struct game_memory* DebugGlobalMemory;

#if _MSC_VER

#define BEGIN_TIMED_BLOCK(ID) u64 StartCycleCount##ID = __rdtsc();
#define END_TIMED_BLOCK(ID) DebugGlobalMemory->Counters[(u32)debug_cycle_counter_id::##ID].CycleCount += __rdtsc() - StartCycleCount##ID; ++DebugGlobalMemory->Counters[(u32)debug_cycle_counter_id::##ID].HitCount;
// TODO: Clamp counters so that if the calc is wrong, it won't overflow
#define END_TIMED_BLOCK_COUNTED(ID, Count) DebugGlobalMemory->Counters[(u32)debug_cycle_counter_id::##ID].CycleCount += __rdtsc() - StartCycleCount##ID; DebugGlobalMemory->Counters[(u32)debug_cycle_counter_id::##ID].HitCount += Count;

#else

#define BEGIN_TIMED_BLOCK(ID)
#define END_TIMED_BLOCK(ID)

#endif

#endif

/*
NOTE: Services that the game provides to the platform layer.
(this may expand in the future - sound on separate thread, etc.)
*/

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use

// TODO: In the future, rendering _specifically_ will become a three-tiered abstraction
typedef struct game_offscreen_buffer
{
	// NOTE: Memory are always 32-bits wide, Memory Order BB GG RR XX
	void* Memory;
	int Width;
	int Height;
	int Pitch;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int  SampleCount;
	i16* Samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
	int HalfTransitionCount;
	b32 EndedDown;
} game_button_state;

typedef struct game_controller_input
{
	b32 IsConnected;
	b32 IsAnalogL;
	b32 IsAnalogR;

	f32 StickAverageLX;
	f32 StickAverageLY;
	f32 StickAverageRX;
	f32 StickAverageRY;

	union
	{
		game_button_state Buttons[12];
		struct
		{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;

			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Back;
			game_button_state Start;

			// NOTE: All buttons must be added above this line

			game_button_state Terminator;
		};
	};
} game_controller_input;

typedef struct game_input
{
	game_button_state MouseButtons[5];
	i32 MouseX, MouseY, MouseZ;

	b32 ExecutableReloaded;
	f32 dtForFrame;

	game_controller_input Controllers[5];
} game_input;

typedef struct platform_file_handle
{
	b32 NoErrors;
} platform_file_handle;
typedef struct platform_file_group
{
	u32 FileCount;
} platform_file_group;

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group* name(char* Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group* FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_FILE(name) platform_file_handle* name(platform_file_group* FileGroup)
typedef PLATFORM_OPEN_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle* Source, u64 Offset, u64 Size, void* Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle* Handle, char* Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue* Queue, void* FindData)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

#define PLATFORM_ALLOCATE_MEMORY(name) void* name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void* Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

typedef void platform_add_entry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* FindData);
typedef void platform_complete_all_work(platform_work_queue* Queue);

typedef struct platform_api
{
	platform_add_entry* AddEntry;
	platform_complete_all_work* CompleteAllWork;

	platform_get_all_files_of_type_begin* GetAllFilesOfTypeBegin;
	platform_get_all_files_of_type_end* GetAllFilesOfTypeEnd;
	platform_open_next_file* OpenNextFile;
	platform_read_data_from_file* ReadDataFromFile;
	platform_file_error* FileError;

	platform_allocate_memory* AllocateMemory;
	platform_deallocate_memory* DeallocateMemory;

	debug_platform_free_file_memory* DEBUGFreeFileMemory;
	debug_platform_read_entire_file* DEBUGReadEntireFile;
	debug_platform_write_entire_file* DEBUGWriteEntireFile;
} platform_api;

typedef struct game_memory
{
	u64 PermanentStorageSize;
	void* PermanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

	u64 TransientStorageSize;
	void* TransientStorage; // NOTE: REQUIRED to be cleared to zero at startup

	platform_work_queue* HighPriorityQueue;
	platform_work_queue* LowPriorityQueue;

	platform_api PlatformAPI;

#if P5ENGINE_INTERNAL
	debug_cycle_counter Counters[(u32)debug_cycle_counter_id::Count];
#endif
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) P5ENGINE_API void name(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO: Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) P5ENGINE_API void name(game_memory* Memory, game_sound_output_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

inline game_controller_input* GetController(game_input* Input, unsigned int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));

	game_controller_input* Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !P5ENGINE_PLATFORM_H
