#pragma once

#ifndef P5ENGINE_DEBUG_H
#define P5ENGINE_DEBUG_H

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

struct debug_record
{
	char* Filename;
	char* FunctionName;

	u32 LineNumber;
	u32 Reserved;

	u64 HitCount_CycleCount;
};

debug_record DebugRecordArray[];

struct timed_block
{
	debug_record* Record;
	u64 StartCycles;
	u32 HitCount;

	timed_block(i32 Counter, char* Filename, i32 LineNumber, char* FunctionName, i32 HitCountInit = 1)
	{
		HitCount = HitCountInit;
		Record = DebugRecordArray + Counter;
		Record->Filename = Filename;
		Record->LineNumber = LineNumber;
		Record->FunctionName = FunctionName;

		StartCycles = __rdtsc();
	}

	~timed_block()
	{
		u64 Delta = (__rdtsc() - StartCycles) | ((u64)HitCount << 32);
		AtomicAddU64(&Record->HitCount_CycleCount, Delta);
	}
};

struct debug_counter_snapshot
{
	u32 HitCount;
	u32 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 120
struct debug_counter_state
{
	char* Filename;
	char* FunctionName;

	u32 LineNumber;

	debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state
{
	u32 SnapshotIndex;
	u32 CounterCount;
	debug_counter_state CounterStates[512];
	debug_frame_end_info FrameEndInfos[DEBUG_SNAPSHOT_COUNT];
};

// TODO: Fix this for looped live code editing
global_variable render_group* DEBUGRenderGroup;

internal void DEBUGReset(game_assets* Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory* Memory);

#endif // P5ENGINE_DEBUG_H