#pragma once
#ifndef P5ENGINE_INTRINSICS_H
#define P5ENGINE_INTRINSICS_H

// 
// TODO: Convert all of these to platform-efficient 
// versions and remove <math.h>

#include <math.h>

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline u32 AtomicCompareExchangeUInt32(u32 volatile* Value, u32 New, u32 Expected)
{
	u32 Result = _InterlockedCompareExchange((long*)Value, New, Expected);

	return(Result);
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

inline i32
SignOf(i32 Value)
{
	i32 Result = (Value >= 0) ? 1 : -1;

	return(Result);
}

inline f32
SignOf(f32 Value)
{
	f32 Result = (Value >= 0) ? 1.0f : -1.0f;

	return(Result);
}

inline f32
SquareRoot(f32 Real32)
{
	f32 Result = sqrtf(Real32);

	return(Result);
}

inline f32
AbsoluteValue(f32 Real32)
{
	f32 Result = (f32)fabs(Real32);

	return(Result);
}

inline u32
RotateLeft(u32 Value, i32 Amount)
{
#if COMPILER_MSVC
	u32 Result = _rotl(Value, Amount);
#else
	// TODO: Actually port this to other compiler platforms
	u32 Result = ((Amount > 0) ? 
		((Value << Amount) | (Value >> (32 - Amount))) : 
		((Value >> -Amount) | (Value << (32 + Amount))));
#endif

	return(Result);
}

inline u32
RotateRight(u32 Value, i32 Amount)
{
#if COMPILER_MSVC
	u32 Result = _rotr(Value, Amount);
#else
	// TODO: Actually port this to other compiler platforms
	u32 Result = ((Amount > 0) ? 
		((Value >> Amount) | (Value << (32 - Amount))) : 
		((Value << -Amount) | (Value >> (32 + Amount))));
#endif

	return(Result);
}

inline i32
RoundReal32ToInt32(f32 Real32)
{
	i32 Result = (i32)roundf(Real32);
	return(Result);
}

inline u32
RoundReal32ToUInt32(f32 Real32)
{
	u32 Result = (u32)roundf(Real32);
	return(Result);
}

inline i32
FloorReal32ToInt32(f32 Real32)
{
	i32 Result = (i32)floorf(Real32);
	return(Result);
}

inline i32
CeilReal32ToInt32(f32 Real32)
{
	i32 Result = (i32)ceilf(Real32);

	return(Result);
}

inline i32
TruncateReal32ToInt32(f32 Real32)
{
	i32 Result = (i32)Real32;
	return(Result);
}

inline f32
Sin(f32 Angle)
{
	f32 Result = sinf(Angle);
	return(Result);
}

inline f32
Cos(f32 Angle)
{
	f32 Result = cosf(Angle);
	return(Result);
}

inline f32
ATan2(f32 y, f32 x)
{
	f32 Result = (f32)atan2(y, x);
	return(Result);
}

struct bit_scan_result
{
	b32 Found;
	i32 Index;
};
inline bit_scan_result
FindLeastSignificantSetBit(u32 Value)
{
	bit_scan_result Result = {};

#if COMPILER_MSVC
	Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
	for (u32 Test = 0; Test < 32; ++Test)
	{
		if (Value & (1 << Test))
		{
			Result.Index = Test;
			Result.Found = true;
			break;
		}
	}
#endif

	return(Result);
}


#endif // !P5ENGINE_INTRINSICS_H