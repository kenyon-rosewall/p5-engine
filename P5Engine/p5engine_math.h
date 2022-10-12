#pragma once
#ifndef P5ENGINE_MATH_H
#define P5ENGINE_MATH_H

//
// Scalar operations
//


inline f32
Square(f32 A)
{
	f32 Result = A * A;

	return(Result);
}

inline f32
Lerp(f32 A, f32 t, f32 B)
{
	f32 Result = (1.0f - t) * A + t * B;

	return(Result);
}

inline f32
SafeRatioN(f32 Numerator, f32 Divisor, f32 N)
{
	f32 Result = N;

	if (Divisor != 0.0f)
	{
		Result = Numerator / Divisor;
	}

	return(Result);
}

inline f32
SafeRatio0(f32 Numerator, f32 Divisor)
{
	f32 Result = SafeRatioN(Numerator, Divisor, 0.0f);

	return(Result);
}

inline f32
SafeRatio1(f32 Numerator, f32 Divisor)
{
	f32 Result = SafeRatioN(Numerator, Divisor, 1.0f);

	return(Result);
}

inline f32
Clamp(f32 Min, f32 Value, f32 Max)
{
	f32 Result = Value;

	if (Result < Min)
	{
		Result = Min;
	}
	else if (Result > Max)
	{
		Result = Max;
	}

	return(Result);
}

inline f32
Clamp01(f32 Value)
{
	f32 Result = Clamp(0.0f, Value, 1.0f);

	return(Result);
}

inline f32
Clamp01MapToRange(f32 Min, f32 t, f32 Max)
{
	f32 Result = 0.0f;

	f32 Range = Max - Min;
	if (Range != 0.0f)
	{
		Result = Clamp01((t - Min) / Range);
	}

	return(Result);
}

//
// v2 operations
//

union v2
{
	f32 E[2];
	struct
	{
		f32 x, y;
	};
	struct
	{
		f32 u, v;
	};
};

inline v2
V2(f32 X, f32 Y)
{
	v2 Result = {};

	Result.x = X;
	Result.y = Y;

	return(Result);
}

inline v2
V2i(i32 X, i32 Y)
{
	v2 Result = V2((f32)X, (f32)Y);

	return(Result);
}

inline v2
V2i(u32 X, u32 Y)
{
	v2 Result = V2((f32)X, (f32)Y);

	return(Result);
}

inline v2
operator*(f32 A, v2 B)
{
	v2 Result = {};

	Result.x = A * B.x;
	Result.y = A * B.y;

	return(Result);
}

inline v2
operator*(v2 B, f32 A)
{
	v2 Result = A * B;

	return(Result);
}

inline v2&
operator*=(v2& B, f32 A)
{
	B = A * B;

	return(B);
}

inline v2
operator/(v2 A, f32 B)
{
	v2 Result = {};

	Result.x = A.x / B;
	Result.y = A.y / B;

	return(Result);
}

inline v2&
operator/=(v2& B, f32 A)
{
	B = B / A;

	return(B);
}

inline v2
operator-(v2 A)
{
	v2 Result = {};

	Result.x = -A.x;
	Result.y = -A.y;

	return (Result);
}

inline v2
operator+(v2 A, v2 B)
{
	v2 Result = {};

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;

	return(Result);
}

inline v2&
operator+=(v2& A, v2 B)
{
	A = A + B;

	return(A);
}

inline v2
operator-(v2 A, v2 B)
{
	v2 Result = {};

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;

	return(Result);
}

inline v2&
operator-=(v2& A, v2 B)
{
	A = A - B;

	return(A);
}

inline v2
Hadamard(v2 A, v2 B)
{
	v2 Result = V2(A.x * B.x, A.y * B.y);

	return(Result);
}

inline f32
Inner(v2 A, v2 B)
{
	f32 Result = A.x * B.x + A.y * B.y;

	return(Result);
}

inline f32
LengthSq(v2 A)
{
	f32 Result = Inner(A, A);

	return(Result);
}

inline f32
Length(v2 A)
{
	f32 Result = SquareRoot(LengthSq(A));

	return(Result);
}

inline v2
Normalize(v2 A)
{
	v2 Result = A / Length(A);

	return(Result);
}

inline v2
Clamp01(v2 Value)
{
	v2 Result = {};

	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);

	return(Result);
}

inline v2
Perp(v2 A)
{
	v2 Result = V2(-A.y, A.x);
	return(Result);
}

inline v2
Lerp(v2 A, f32 t, v2 B)
{
	v2 Result = (1.0f - t) * A + t * B;

	return(Result);
}

//
// v3 operations
//

union v3
{
	f32 E[3];
	struct
	{
		f32 x, y, z;
	};
	struct
	{
		f32 u, v, w;
	};
	struct
	{
		f32 r, g, b;
	};
	struct
	{
		v2 xy;
		f32 Ignored0;
	};
	struct
	{
		f32 Ignored1;
		v2 yz;
	};
	struct
	{
		v2 uv;
		f32 Ignored2;
	};
	struct
	{
		f32 Ignored3;
		v2 vw;
	};
};

inline v3
V3(f32 X, f32 Y, f32 Z)
{
	v3 Result = {};

	Result.x = X;
	Result.y = Y;
	Result.z = Z;

	return(Result);
}

inline v3
ToV3(v2 XY, f32 Z)
{
	v3 Result = {};

	Result.xy = XY;
	Result.z = Z;
	
	return(Result);
}

inline v3
operator*(f32 A, v3 B)
{
	v3 Result = {};

	Result.x = A * B.x;
	Result.y = A * B.y;
	Result.z = A * B.z;

	return(Result);
}

inline v3
operator*(v3 B, f32 A)
{
	v3 Result = A * B;

	return(Result);
}

inline v3&
operator*=(v3& B, f32 A)
{
	B = A * B;

	return(B);
}

inline v3
operator/(v3 A, f32 B)
{
	v3 Result = {};

	Result.x = A.x / B;
	Result.y = A.y / B;
	Result.z = A.z / B;

	return(Result);
}

inline v3&
operator/=(v3& B, f32 A)
{
	B = B / A;

	return(B);
}

inline v3
operator-(v3 A)
{
	v3 Result = {};

	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;

	return (Result);
}

inline v3
operator+(v3 A, v3 B)
{
	v3 Result = {};

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;

	return(Result);
}

inline v3&
operator+=(v3& A, v3 B)
{
	A = A + B;

	return(A);
}

inline v3
operator-(v3 A, v3 B)
{
	v3 Result = {};

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;

	return(Result);
}

inline v3&
operator-=(v3& A, v3 B)
{
	A = A - B;

	return(A);
}

inline v3
Hadamard(v3 A, v3 B)
{
	v3 Result = V3(A.x * B.x, A.y * B.y, A.z * B.z);

	return(Result);
}

inline f32
Inner(v3 A, v3 B)
{
	f32 Result = A.x * B.x + A.y * B.y + A.z * B.z;

	return(Result);
}

inline f32
LengthSq(v3 A)
{
	f32 Result = Inner(A, A);

	return(Result);
}

inline f32
Length(v3 A)
{
	f32 Result = SquareRoot(LengthSq(A));

	return(Result);
}

inline v3
Normalize(v3 A)
{
	v3 Result = A / Length(A);

	return(Result);
}

inline v3
Clamp01(v3 Value)
{
	v3 Result = {};

	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);
	Result.z = Clamp01(Value.z);

	return(Result);
}

inline v3
Lerp(v3 A, f32 t, v3 B)
{
	v3 Result = (1.0f - t) * A + t * B;

	return(Result);
}

//
// v4 operations
//

union v4
{
	f32 E[4];
	struct
	{
		f32 x, y, z, w;
	};
	struct
	{
		f32 r, g, b, a;
	};
	struct
	{
		v3 rgb;
		f32 Ignored0;
	};
	struct
	{
		v3 xyz;
		f32 Ignored1;
	};
	struct
	{
		v2 xy;
		f32 Ignored2;
		f32 Ignored3;
	};
	struct
	{
		f32 Ignored4;
		v2 yz;
		f32 Ignored5;
	};
	struct
	{
		f32 Ignored6;
		f32 Ignored7;
		v2 zw;
	};
};

inline v4
V4(f32 X, f32 Y, f32 Z, f32 W)
{
	v4 Result = {};

	Result.x = X;
	Result.y = Y;
	Result.z = Z;
	Result.w = W;

	return(Result);
}

inline v4
ToV4(v3 XYZ, f32 W)
{
	v4 Result;

	Result.xyz = XYZ;
	Result.w = W;

	return(Result);
}

inline v4
operator*(f32 A, v4 B)
{
	v4 Result = {};

	Result.x = A * B.x;
	Result.y = A * B.y;
	Result.z = A * B.z;
	Result.w = A * B.w;

	return(Result);
}

inline v4
operator*(v4 B, f32 A)
{
	v4 Result = A * B;

	return(Result);
}

inline v4&
operator*=(v4& B, f32 A)
{
	B = A * B;

	return(B);
}

inline v4
operator/(v4 A, f32 B)
{
	v4 Result = {};

	Result.x = A.x / B;
	Result.y = A.y / B;
	Result.z = A.z / B;
	Result.w = A.w / B;

	return(Result);
}

inline v4&
operator/=(v4& B, f32 A)
{
	B = B / A;

	return(B);
}

inline v4
operator-(v4 A)
{
	v4 Result = {};

	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;
	Result.w = -A.w;

	return (Result);
}

inline v4
operator+(v4 A, v4 B)
{
	v4 Result = {};

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;
	Result.w = A.w + B.w;

	return(Result);
}

inline v4&
operator+=(v4& A, v4 B)
{
	A = A + B;

	return(A);
}

inline v4
operator-(v4 A, v4 B)
{
	v4 Result = {};

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;
	Result.w = A.w - B.w;

	return(Result);
}

inline v4&
operator-=(v4& A, v4 B)
{
	A = A - B;

	return(A);
}

inline v4
Hadamard(v4 A, v4 B)
{
	v4 Result = V4(A.x * B.x, A.y * B.y, A.z * B.z, A.w * B.w);

	return(Result);
}

inline f32
Inner(v4 A, v4 B)
{
	f32 Result = A.x * B.x + A.y * B.y + A.z * B.z + A.w * B.w;

	return(Result);
}

inline f32
LengthSq(v4 A)
{
	f32 Result = Inner(A, A);

	return(Result);
}

inline f32
Length(v4 A)
{
	f32 Result = SquareRoot(LengthSq(A));

	return(Result);
}

inline v4
Normalize(v4 A)
{
	v4 Result = A / Length(A);

	return(Result);
}

inline v4
Clamp01(v4 Value)
{
	v4 Result = {};

	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);
	Result.z = Clamp01(Value.z);
	Result.w = Clamp01(Value.w);

	return(Result);
}

inline v4
Lerp(v4 A, f32 t, v4 B)
{
	v4 Result = (1.0f - t) * A + t * B;

	return(Result);
}

inline v4
SRGB255ToLinear1(v4 C)
{
	v4 Result = {};

	f32 Inv255 = 1.0f / 255.0f;

	Result.r = Square(Inv255 * C.r);
	Result.g = Square(Inv255 * C.g);
	Result.b = Square(Inv255 * C.b);
	Result.a = Inv255 * C.a;

	return(Result);
}

inline v4
Linear1ToSRGB255(v4 C)
{
	v4 Result = {};

	f32 One255 = 255.0f;

	Result.r = One255 * SquareRoot(C.r);
	Result.g = One255 * SquareRoot(C.g);
	Result.b = One255 * SquareRoot(C.b);
	Result.a = One255 * C.a;

	return(Result);
}

//
// Rectangle2 operations
//

struct rectangle2
{
	v2 Min, Max;
};

inline v2
GetMinCorner(rectangle2 Rect)
{
	v2 Result = Rect.Min;
	return(Result);
}

inline v2
GetMaxCorner(rectangle2 Rect)
{
	v2 Result = Rect.Max;
	return(Result);
}

inline v2
GetDim(rectangle2 Rect)
{
	v2 Result = Rect.Max - Rect.Min;
	return(Result);
}

inline v2
GetCenter(rectangle2 Rect)
{
	v2 Result = 0.5f * (Rect.Min + Rect.Max);
	return(Result);
}

inline rectangle2
RectMinMax(v2 Min, v2 Max)
{
	rectangle2 Result = {};

	Result.Min = Min;
	Result.Max = Max;

	return(Result);
}

inline rectangle2
RectMinDim(v2 Min, v2 Dim)
{
	rectangle2 Result = {};

	Result.Min = Min;
	Result.Max = Min + Dim;

	return(Result);
}

inline rectangle2
RectCenterHalfDim(v2 Center, v2 HalfDim)
{
	rectangle2 Result = {};

	Result.Min = Center - HalfDim;
	Result.Max = Center + HalfDim;

	return(Result);
}

inline rectangle2
AddRadiusTo(rectangle2 A, v2 Radius)
{
	rectangle2 Result = {};
	
	Result.Min = A.Min - Radius;
	Result.Max = A.Max + Radius;

	return(Result);
}

inline rectangle2
Offset(rectangle2 A, v2 Offset)
{
	rectangle2 Result = {};

	Result.Min = A.Min + Offset;
	Result.Max = A.Max + Offset;

	return(Result);
}

inline rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
	rectangle2 Result = RectCenterHalfDim(Center, 0.5f * Dim);

	return(Result);
}

inline b32
IsInRectangle(rectangle2 Rectangle, v2 Test)
{
	b32 Result = ((Test.x >= Rectangle.Min.x) &&
					 (Test.y >= Rectangle.Min.y) &&
					 (Test.x <  Rectangle.Max.x) &&
					 (Test.y <  Rectangle.Max.y));

	return(Result);
}

inline b32
RectanglesIntersect(rectangle2 A, rectangle2 B)
{
	b32 Result = !((B.Max.x < A.Min.x) || (B.Min.x > A.Max.x) ||
					  (B.Max.y < A.Min.y) || (B.Min.y > A.Max.y));

	return(Result);
}

inline v2
GetBarycentric(rectangle2 A, v2 P)
{
	v2 Result = {};

	Result.x = (P.x - A.Min.x) / (A.Max.x - A.Min.x);
	Result.y = (P.y - A.Min.y) / (A.Max.y - A.Min.y);

	return(Result);
}

//
// Rectangle2i operations
//

struct rectangle2i
{
	i32 MinX, MinY;
	i32 MaxX, MaxY;
};

inline rectangle2i
Intersect(rectangle2i A, rectangle2i B)
{
	rectangle2i Result;

	Result.MinX = (A.MinX < B.MinX) ? B.MinX : A.MinX;
	Result.MinY = (A.MinY < B.MinY) ? B.MinY : A.MinY;
	Result.MaxX = (A.MaxX > B.MaxX) ? B.MaxX : A.MaxX;
	Result.MaxY = (A.MaxY > B.MaxY) ? B.MaxY : A.MaxY;

	return(Result);
}

inline rectangle2i
Union(rectangle2i A, rectangle2i B)
{
	rectangle2i Result;

	Result.MinX = (A.MinX < B.MinX) ? A.MinX : B.MinX;
	Result.MinY = (A.MinY < B.MinY) ? A.MinY : B.MinY;
	Result.MaxX = (A.MaxX > B.MaxX) ? A.MaxX : B.MaxX;
	Result.MaxY = (A.MaxY > B.MaxY) ? A.MaxY : B.MaxY;

	return(Result);
}

inline i32
GetClampedRectArea(rectangle2i A)
{
	i32 Width = (A.MaxX - A.MinX);
	i32 Height = (A.MaxY - A.MinY);

	i32 Result = 0;

	if ((Width > 0) && (Height > 0))
	{
		Result = Width * Height;
	}

	return(Result);
}

inline b32
HasArea(rectangle2i A)
{
	b32 Result = ((A.MinX < A.MaxX) && (A.MinY < A.MaxY));

	return(Result);
}

inline rectangle2i
InvertedInfinityRectangle(void)
{
	rectangle2i Result;

	Result.MinX = Result.MinY = INT_MAX;
	Result.MaxX = Result.MaxY = -INT_MAX;

	return(Result);
}

//
// Rectangle3 operations
//

struct rectangle3
{
	v3 Min, Max;
};

inline v3
GetMinCorner(rectangle3 Rect)
{
	v3 Result = Rect.Min;
	return(Result);
}

inline v3
GetMaxCorner(rectangle3 Rect)
{
	v3 Result = Rect.Max;
	return(Result);
}

inline v3
GetDim(rectangle3 Rect)
{
	v3 Result = Rect.Max - Rect.Min;
	return(Result);
}

inline v3
GetCenter(rectangle3 Rect)
{
	v3 Result = 0.5f * (Rect.Min + Rect.Max);
	return(Result);
}

inline rectangle3
RectMinMax(v3 Min, v3 Max)
{
	rectangle3 Result = {};

	Result.Min = Min;
	Result.Max = Max;

	return(Result);
}

inline rectangle3
RectMinDim(v3 Min, v3 Dim)
{
	rectangle3 Result = {};

	Result.Min = Min;
	Result.Max = Min + Dim;

	return(Result);
}

inline rectangle3
RectCenterHalfDim(v3 Center, v3 HalfDim)
{
	rectangle3 Result = {};

	Result.Min = Center - HalfDim;
	Result.Max = Center + HalfDim;

	return(Result);
}

inline rectangle3
AddRadiusTo(rectangle3 A, v3 Radius)
{
	rectangle3 Result = {};

	Result.Min = A.Min - Radius;
	Result.Max = A.Max + Radius;

	return(Result);
}

inline rectangle3
Offset(rectangle3 A, v3 Offset)
{
	rectangle3 Result = {};

	Result.Min = A.Min + Offset;
	Result.Max = A.Max + Offset;

	return(Result);
}

inline rectangle3
RectCenterDim(v3 Center, v3 Dim)
{
	rectangle3 Result = RectCenterHalfDim(Center, 0.5f * Dim);

	return(Result);
}

inline b32
IsInRectangle(rectangle3 Rectangle, v3 Test)
{
	b32 Result = ((Test.x >= Rectangle.Min.x) &&
					 (Test.y >= Rectangle.Min.y) &&
					 (Test.z >= Rectangle.Min.z) &&
					 (Test.x <  Rectangle.Max.x) &&
					 (Test.y <  Rectangle.Max.y) &&
					 (Test.z <  Rectangle.Max.z));

	return(Result);
}

inline b32
RectanglesIntersect(rectangle3 A, rectangle3 B)
{
	b32 Result = !((B.Max.x <= A.Min.x) || (B.Min.x >= A.Max.x) ||
					  (B.Max.y <= A.Min.y) || (B.Min.y >= A.Max.y) ||
					  (B.Max.z <= A.Min.z) || (B.Min.z >= A.Max.z));

	return(Result);
}

inline v3
GetBarycentric(rectangle3 A, v3 P)
{
	v3 Result = {};

	Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
	Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);
	Result.z = SafeRatio0(P.z - A.Min.z, A.Max.z - A.Min.z);

	return(Result);
}

#endif // !P5ENGINE_MATH_H
