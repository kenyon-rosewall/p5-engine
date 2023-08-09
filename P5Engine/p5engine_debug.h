#pragma once

#ifndef P5ENGINE_DEBUG_H
#define P5ENGINE_DEBUG_H

struct debug_counter_snapshot
{
	u32 HitCount;
	u64 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 120
struct debug_counter_state
{
	char* Filename;
	char* BlockName;

	u32 LineNumber;

	debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state
{
	u32 SnapshotIndex;
	u32 CounterCount;
	debug_counter_state CounterStates[512];
};

// TODO: Fix this for looped live code editing
global_variable render_group* DEBUGRenderGroup;

internal void DEBUGReset(game_assets* Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory* Memory);

#endif // P5ENGINE_DEBUG_H