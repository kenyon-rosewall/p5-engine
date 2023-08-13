#pragma once

#ifndef P5ENGINE_DEBUG_H
#define P5ENGINE_DEBUG_H

struct debug_counter_snapshot
{
	u32 HitCount;
	u64 CycleCount;
};

struct debug_counter_state
{
	char* Filename;
	char* BlockName;

	u32 LineNumber;
};

struct debug_frame_region
{
	u32 LaneIndex;
	f32 MinT;
	f32 MaxT;
};

struct debug_frame
{
	u64 BeginClock;
	u64 EndClock;

	u32 RegionCount;
	debug_frame_region* Regions;
};

struct debug_state
{
	b32 Initialized;

	// NOTE: Collation
	memory_arena CollateArena;
	temporary_memory CollateTemp;

	u32 FrameBarLaneCount;
	u32 FrameCount;
	f32 FrameBarScale;

	debug_frame *Frames;
};

// TODO: Fix this for looped live code editing
global_variable render_group* DEBUGRenderGroup;

internal void DEBUGReset(game_assets* Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory* Memory);

#endif // P5ENGINE_DEBUG_H