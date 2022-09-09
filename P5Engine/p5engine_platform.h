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

//
// NOTE: Types
//

// TODO: Implement sine ourselves
#include <stdint.h>
#include <limits.h>
#include <float.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#define Real32Maximum FLT_MAX

#define global_variable static
#define local_persist static
#define internal static

#define Pi32 3.14159265359f

#if P5ENGINE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int *)0  = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO: swap, min, max ... macros???

#define Align16(Value) ((Value + 15) & ~15)

inline uint32
SafeTruncateUInt64(uint64 Value)
{
	// TODO: Defines for maximum values
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

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
	uint32 ContentsSize;
	void* Contents;
}  debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Context, void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context* Context, char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context* Context, char* Filename, uint32 MemorySize, void* Memory)
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
	uint64 CycleCount;
	uint32 HitCount;
} debug_cycle_counter;

extern struct game_memory* DebugGlobalMemory;

#if _MSC_VER

#define BEGIN_TIMED_BLOCK(ID) uint64 StartCycleCount##ID = __rdtsc();
#define END_TIMED_BLOCK(ID) DebugGlobalMemory->Counters[(uint32)debug_cycle_counter_id::##ID].CycleCount += __rdtsc() - StartCycleCount##ID; ++DebugGlobalMemory->Counters[(uint32)debug_cycle_counter_id::##ID].HitCount;
// TODO: Clamp counters so that if the calc is wrong, it won't overflow
#define END_TIMED_BLOCK_COUNTED(ID, Count) DebugGlobalMemory->Counters[(uint32)debug_cycle_counter_id::##ID].CycleCount += __rdtsc() - StartCycleCount##ID; DebugGlobalMemory->Counters[(uint32)debug_cycle_counter_id::##ID].HitCount += Count;

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

// TODO: In the future, rendering _specifically_ will become a three-tiered abstraction!!!
#define BITMAP_BYTES_PER_PIXEL 4
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
	int16* Samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input
{
	bool32 IsConnected;
	bool32 IsAnalogL;
	bool32 IsAnalogR;

	real32 StickAverageLX;
	real32 StickAverageLY;
	real32 StickAverageRX;
	real32 StickAverageRY;

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
	int32 MouseX, MouseY, MouseZ;

	bool32 ExecutableReloaded;
	real32 dtForFrame;

	game_controller_input Controllers[5];
} game_input;

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue* Queue, void* Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data);
typedef void platform_complete_all_work(platform_work_queue* Queue);

typedef struct game_memory
{
	bool32 IsInitialized;

	uint64 PermanentStorageSize;
	void* PermanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

	uint64 TransientStorageSize;
	void* TransientStorage; // NOTE: REQUIRED to be cleared to zero at startup

	platform_work_queue* HighPriorityQueue;
	platform_work_queue* LowPriorityQueue;

	platform_add_entry* PlatformAddEntry;
	platform_complete_all_work* PlatformCompleteAllWork;

	debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;

#if P5ENGINE_INTERNAL
	debug_cycle_counter Counters[(uint32)debug_cycle_counter_id::Count];
#endif
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) P5ENGINE_API void name(thread_context* Context, game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO: Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) P5ENGINE_API void name(thread_context* Context, game_memory* Memory, game_sound_output_buffer* SoundBuffer)
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
