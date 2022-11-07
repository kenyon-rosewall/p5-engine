#pragma once

#ifndef P5ENGINE_DEBUG_H
#define P5ENGINE_DEBUG_H

#define TIMED_BLOCK(ID) timed_block TimedBlock##ID((u32)debug_cycle_counter_id::##ID)

struct timed_block
{
	i64 StartCycleCount;
	u32 ID;

	timed_block(u32 IDInit)
	{
		ID = IDInit;
		BEGIN_TIMED_BLOCK_(StartCycleCount);
	}

	~timed_block()
	{
		END_TIMED_BLOCK_(StartCycleCount, ID);
	}
};

#endif // P5ENGINE_DEBUG_H