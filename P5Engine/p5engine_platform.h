#pragma once

#ifndef P5ENGINE_PLATFORM_H
#define P5ENGINE_PLATFORM_H

#ifdef P5ENGINE_WIN32
#define P5ENGINE_API __declspec(dllexport)
//#define P5ENGINE_API __declspec(dllimport)
#endif

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

inline i32
TruncateReal32ToInt32(f32 Real32)
{
	i32 Result = (i32)Real32;
	return(Result);
}

inline u32
SafeTruncateUInt64(u64 Value)
{
	// TODO: Defines for maximum values
	Assert(Value <= 0xFFFFFFFF);
	u32 Result = (u32)Value;
	return(Result);
}

inline i16
SafeTruncateToUInt16(i32 Value)
{
	Assert(Value <= 65535);
	Assert(Value >= 0);

	i16 Result = (i16)Value;
	return(Result);
}

inline i16
SafeTruncateToInt16(i32 Value)
{
	Assert(Value <= 32767);
	Assert(Value >= -32768);

	i16 Result = (i16)Value;
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
	u32 ContentsSize;
	void* Contents;
}  debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(char* Filename, u32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

// TODO: actually start using this
extern struct game_memory* DebugGlobalMemory;

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
	void* Platform;
} platform_file_handle;
typedef struct platform_file_group
{
	u32 FileCount;
	void* Platform;
} platform_file_group;

typedef enum platform_file_type
{
	PlatformFileType_AssetFile,
	PlatformFileType_SavedGameFile,

	PlatformFileType_Count,
} platform_file_type;

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_FILE(name) platform_file_handle name(platform_file_group *FileGroup)
typedef PLATFORM_OPEN_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void* Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char* Message)
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

	u64 DebugStorageSize;
	void* DebugStorage; // NOTE: REQUIRED to be cleared to zero at startup

	platform_work_queue* HighPriorityQueue;
	platform_work_queue* LowPriorityQueue;

	platform_api PlatformAPI;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) P5ENGINE_API void name(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO: Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) P5ENGINE_API void name(game_memory* Memory, game_sound_output_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#if COMPILER_MSVC

#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()

inline u32 AtomicCompareExchangeU32(u32 volatile* Value, u32 New, u32 Expected)
{
	u32 Result = _InterlockedCompareExchange((long*)Value, New, Expected);

	return(Result);
}

inline u64 AtomicExchangeU64(u64 volatile* Value, u64 New)
{
	u64 Result = _InterlockedExchange64((__int64*)Value, New);

	return(Result);
}

inline u32 AtomicAddU32(u32 volatile* Value, u32 Addend)
{
	// NOTE: Returns the original value _prior_ to adding
	u32 Result = _InterlockedExchangeAdd((long*)Value, Addend);

	return(Result);
}

inline u64 AtomicAddU64(u64 volatile* Value, u64 Addend)
{
	// NOTE: Returns the original value _prior_ to adding
	u64 Result = _InterlockedExchangeAdd64((__int64*)Value, Addend);

	return(Result);
}

inline u32 GetThreadID()
{
	u8* ThreadLocalStorage = (u8*)__readgsqword(0x30);
	u32 ThreadID = *(u32*)(ThreadLocalStorage + 0x48);

	return(ThreadID);
}

#elif COMPILER_LLVM
// TODO: Does LLVM have read-specific barriers yet?
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile("" ::: "memory")
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile* Value, uint32 New, uint32 Expected)
{
	uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);

	return(Result);
}
#else
// TODO: Other compilers/platforms?
#endif

struct debug_table;
#define GAME_DEBUG_FRAME_END(name) P5ENGINE_API debug_table* name(game_memory* Memory)
typedef GAME_DEBUG_FRAME_END(game_debug_frame_end);

inline game_controller_input* GetController(game_input* Input, unsigned int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));

	game_controller_input* Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

struct debug_record
{
	char* Filename;
	char* BlockName;

	u32 LineNumber;
	u32 Reserved;
};

enum class debug_event_type
{
	FrameMaker,
	BeginBlock,
	EndBlock,
};

struct debug_event
{
	u64 Clock;
	u16 ThreadID;
	u16 CoreIndex;
	u16 DebugRecordIndex;
	u8 TranslationUnit;
	u8 Type;
};

#define MAX_DEBUG_THREAD_COUNT 256
#define MAX_DEBUG_EVENT_ARRAY_COUNT 64
#define MAX_DEBUG_TRANSLATION_UNITS 3
#define MAX_DEBUG_EVENT_COUNT (16 * 65536)
#define MAX_DEBUG_RECORD_COUNT (65536)
struct debug_table
{
	// TODO: No attempt is currently made to ensure that the final
	// debug records being written to the event array actually complete
	// their output prior to the swap of the event array index.

	u32 CurrentEventArrayIndex;
	u64 volatile EventArrayIndex_EventIndex;
	u32 EventCount[MAX_DEBUG_EVENT_ARRAY_COUNT];
	debug_event Events[MAX_DEBUG_EVENT_ARRAY_COUNT][MAX_DEBUG_EVENT_COUNT];
	
	u32 RecordCount[MAX_DEBUG_TRANSLATION_UNITS];
	debug_record Records[MAX_DEBUG_TRANSLATION_UNITS][MAX_DEBUG_RECORD_COUNT];
};

extern debug_table *GlobalDebugTable;

inline void RecordDebugEvent(i32 RecordIndex, debug_event_type EventType)
{
	u64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1);
	u32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;
	Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);
	debug_event* Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex;
	Event->Clock = __rdtsc();
	Event->ThreadID = (u16)GetThreadID();
	Event->CoreIndex = 0;
	Event->DebugRecordIndex = (u16)RecordIndex;
	Event->TranslationUnit = TRANSLATION_UNIT_INDEX;
	Event->Type = (u8)EventType;
}

#define FRAME_MARKER() \
	{i32 Counter = __COUNTER__; \
	RecordDebugEvent(Counter, debug_event_type::FrameMaker); \
	debug_record* Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
	Record->Filename = __FILE__; \
	Record->LineNumber = __LINE__; \
	Record->BlockName = "Frame Marker";}

#define TIMED_BLOCK__(BlockName, Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, BlockName, ## __VA_ARGS__)
#define TIMED_BLOCK_(BlockName, Number, ...) TIMED_BLOCK__(BlockName, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(BlockName, ...) TIMED_BLOCK_(#BlockName, __LINE__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(__FUNCTION__, __LINE__, ## __VA_ARGS__)

#define BEGIN_BLOCK_(Counter, FilenameInit, LineNumberInit, BlockNameInit) \
	{debug_record* Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
	Record->Filename = FilenameInit; \
	Record->LineNumber = LineNumberInit; \
	Record->BlockName = BlockNameInit; \
	RecordDebugEvent(Counter, debug_event_type::BeginBlock);}
#define END_BLOCK_(Counter) \
	RecordDebugEvent(Counter, debug_event_type::EndBlock);

#define BEGIN_BLOCK(Name) \
	i32 Counter_##Name = __COUNTER__; \
	BEGIN_BLOCK_(Counter_##Name, __FILE__, __LINE__, #Name)

#define END_BLOCK(Name) \
	END_BLOCK_(Counter_##Name)

struct timed_block
{
	i32 Counter;

	timed_block(i32 CounterInit, char* Filename, i32 LineNumber, char* BlockName, i32 HitCountInit = 1)
	{
		// TODO: Record the hit count value here?
		Counter = CounterInit;
		BEGIN_BLOCK_(Counter, Filename, LineNumber, BlockName);
	}

	~timed_block()
	{
		END_BLOCK_(Counter);
	}
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !P5ENGINE_PLATFORM_H
