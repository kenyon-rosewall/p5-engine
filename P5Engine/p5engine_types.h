#pragma once

#ifndef P5ENGINE_TYPES_H
#define P5ENGIEN_TYPE_H

//
// NOTE: Types
//

// TODO: Implement sine ourselves
#include <stdint.h>
#include <limits.h>
#include <float.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float f32;
typedef double f64;

#define Real32Maximum FLT_MAX

#if !defined(internal)
#define internal static
#endif
#define global_variable static
#define local_persist static

#define BITMAP_BYTES_PER_PIXEL 4
#define Pi32 3.14159265359f
#define Tau32 6.28318530717958647692f

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

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

inline u32
SafeTruncateUInt64(u64 Value)
{
	// TODO: Defines for maximum values
	Assert(Value <= 0xFFFFFFFF);
	u32 Result = (u32)Value;
	return(Result);
}

#endif // !P5ENGINE_TYPES_H