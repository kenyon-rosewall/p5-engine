
inline v4
Unpack4x8(uint32 Packed)
{
	v4 Result = V4(
		(real32)((Packed >> 16) & 0xFF),
		(real32)((Packed >> 8) & 0xFF),
		(real32)((Packed >> 0) & 0xFF),
		(real32)((Packed >> 24) & 0xFF)
	);

	return(Result);
}

inline v4
SRGB255ToLinear1(v4 C)
{
	v4 Result = {};

	real32 Inv255 = 1.0f / 255.0f;

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

	real32 One255 = 255.0f;

	Result.r = One255 * SquareRoot(C.r);
	Result.g = One255 * SquareRoot(C.g);
	Result.b = One255 * SquareRoot(C.b);
	Result.a = One255 * C.a;

	return(Result);
}

inline v4
UnscaleAndBiasNormal(v4 Normal)
{
	v4 Result = {};

	real32 Inv255 = 1.0f / 255.0f;

	Result.x = -1.0f + 2.0f * (Inv255 * Normal.x);
	Result.y = -1.0f + 2.0f * (Inv255 * Normal.y);
	Result.z = -1.0f + 2.0f * (Inv255 * Normal.z);
	Result.w = Inv255 * Normal.w;

	return(Result);
}

internal void
DrawRectangle(loaded_bitmap* Buffer, v2 vMin, v2 vMax, v4 Color, rectangle2i ClipRect, bool32 Even)
{
	real32 R = Color.r;
	real32 G = Color.g;
	real32 B = Color.b;
	real32 A = Color.a;

	rectangle2i FillRect;
	FillRect.MinX = RoundReal32ToInt32(vMin.x);
	FillRect.MinY = RoundReal32ToInt32(vMin.y);
	FillRect.MaxX = RoundReal32ToInt32(vMax.x);
	FillRect.MaxY = RoundReal32ToInt32(vMax.y);

	FillRect = Intersect(FillRect, ClipRect);
	if (!Even == (FillRect.MinY & 1))
	{
		FillRect.MinY += 1;
	}

	uint32 Color32 = ((RoundReal32ToUInt32(A * 255.0f) << 24) |
					  (RoundReal32ToUInt32(R * 255.0f) << 16) | 
					  (RoundReal32ToUInt32(G * 255.0f) <<  8) | 
					  (RoundReal32ToUInt32(B * 255.0f) <<  0));

	uint8* Row = ((uint8*)Buffer->Memory + FillRect.MinX * BITMAP_BYTES_PER_PIXEL + FillRect.MinY * Buffer->Pitch);

	for (int y = FillRect.MinY; y < FillRect.MaxY; y += 2)
	{
		uint32* Pixel = (uint32*)Row;
		for (int x = FillRect.MinX; x < FillRect.MaxX; ++x)
		{
			*Pixel++ = Color32;
		}

		Row += 2 * Buffer->Pitch;
	}
}

struct bilinear_sample
{
	uint32 A, B, C, D;
};

inline bilinear_sample
BilinearSample(loaded_bitmap* Texture, int32 X, int32 Y)
{
	bilinear_sample Result = {};

	uint8* TexelPtr = ((uint8*)Texture->Memory) + Y * Texture->Pitch + X * BITMAP_BYTES_PER_PIXEL;
	Result.A = *(uint32*)(TexelPtr);
	Result.B = *(uint32*)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
	Result.C = *(uint32*)(TexelPtr + Texture->Pitch);
	Result.D = *(uint32*)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);

	return(Result);
}

inline v4
SRGBBilinearBlend(bilinear_sample Sample, real32 fX, real32 fY)
{
	v4 Result = {};

	v4 TexelA = Unpack4x8(Sample.A);
	v4 TexelB = Unpack4x8(Sample.B);
	v4 TexelC = Unpack4x8(Sample.C);
	v4 TexelD = Unpack4x8(Sample.D);

	TexelA = SRGB255ToLinear1(TexelA);
	TexelB = SRGB255ToLinear1(TexelB);
	TexelC = SRGB255ToLinear1(TexelC);
	TexelD = SRGB255ToLinear1(TexelD);

	Result = Lerp(Lerp(TexelA, fX, TexelB), fY, Lerp(TexelC, fX, TexelD));

	return(Result);
}

inline v3
SampleEnvironmentMap(v2 ScreenSpaceUV, v3 SampleDirection, real32 Roughness, environment_map* Map,
	real32 DistanceFromMapInZ)
{
	/* NOTE: ScreenSpaceUV tells us where the ray is being cast _from_
	   in normalized screen coordinates.

	   SampleDirection tells us what direction the cast is going - it
	   does not have to be normalized.

	   Roughness says which LODs of Map we sample from.
	*/

	// NOTE: Pick which LOD to sample from
	uint32 LODIndex = (uint32)(Roughness * (real32)(ArrayCount(Map->LOD) - 1) + 0.5f);
	Assert(LODIndex < ArrayCount(Map->LOD));

	loaded_bitmap* LOD = &Map->LOD[LODIndex];

	// NOTE: Compute the distance to the map and the scaling
	// factor for meters-to-UVs
	// TODO: Parameterize this, and should be different for X and Y,
	// based on map
	real32 UVsPerMeter = 0.1f;
	real32 C = (UVsPerMeter * DistanceFromMapInZ) / SampleDirection.y;
	// TODO: Make sure we know what direction Z should go in Y
	v2 Offset = C * V2(SampleDirection.x, SampleDirection.z);

	// NOTE: Find the intersection point
	v2 UV = ScreenSpaceUV + Offset;

	// NOTE: Clamp to the valid range
	UV.x = Clamp01(UV.x);
	UV.y = Clamp01(UV.y);

	// NOTE: Bilinear sample
	// TODO: Formalize texture boundaries!!!
	real32 tX = ((UV.x * (real32)(LOD->Width - 2)));
	real32 tY = ((UV.y * (real32)(LOD->Height - 2)));

	int32 X = (int32)tX;
	int32 Y = (int32)tY;

	real32 fX = tX - (real32)X;
	real32 fY = tY - (real32)Y;

	Assert((X >= 0) && (X < LOD->Width));
	Assert((Y >= 0) && (Y < LOD->Height));

#if 0
	// NOTE: Turn this on to see where in the map you're sampling
	uint8* TexelPtr = ((uint8*)LOD->Memory) + Y * LOD->Pitch + X * sizeof(uint32);
	*(uint32*)TexelPtr = 0xFFFFFFFF;
#endif

	bilinear_sample Sample = BilinearSample(LOD, X, Y);
	v3 Result = SRGBBilinearBlend(Sample, fX, fY).rgb;
		
	return(Result);
}

internal void
DrawRectangleSlowly(loaded_bitmap* Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap* Texture, loaded_bitmap* NormalMap, environment_map* Top, environment_map* Middle, environment_map* Bottom,
	real32 PixelsToMeters)
{
	BEGIN_TIMED_BLOCK(DrawRectangleSlow);

	// NOTE: Premultiply color up front
	Color.rgb *= Color.a;

	real32 XAxisLength = Length(XAxis);
	real32 YAxisLength = Length(YAxis);

	v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
	v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;

	// NOTE: NzScale could be a parameter if we want people to have
	// control over the amount of scaling in the Z direction that the
	// normals appear to have.
	real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

	real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
	real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

	uint32 Color32 = ((RoundReal32ToUInt32(Color.a * 255.0f) << 24) |
		(RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
		(RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
		(RoundReal32ToUInt32(Color.b * 255.0f) << 0));

	int WidthMax = (Buffer->Width - 1);
	int HeightMax = (Buffer->Height - 1);

	real32 InvWidthMax = 1.0f / (real32)WidthMax;
	real32 InvHeightMax = 1.0f / (real32)HeightMax;

	// TODO: This will need to be specified separately!
	real32 OriginZ = 0.0f;
	real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
	real32 FixedCastY = InvHeightMax * OriginY;

	int32 XMin = WidthMax;
	int32 XMax = 0;
	int32 YMin = HeightMax;
	int32 YMax = 0;

	v2 P[4] = { Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis };
	for (int PointIndex = 0; PointIndex < ArrayCount(P); ++PointIndex)
	{
		v2 TestP = P[PointIndex];
		int FloorX = FloorReal32ToInt32(TestP.x);
		int CeilX = CeilReal32ToInt32(TestP.x);
		int FloorY = FloorReal32ToInt32(TestP.y);
		int CeilY = CeilReal32ToInt32(TestP.y);

		if (XMin > FloorX) { XMin = FloorX; }
		if (YMin > FloorY) { YMin = FloorY; }
		if (XMax < CeilX) { XMax = CeilX; }
		if (YMax < CeilY) { YMax = CeilY; }
	}

	if (XMin < 0) { XMin = 0; }
	if (YMin < 0) { YMin = 0; }
	if (XMax > WidthMax) { XMax = WidthMax; }
	if (YMax > HeightMax) { YMax = HeightMax; }

	uint8* Row = ((uint8*)Buffer->Memory + XMin * BITMAP_BYTES_PER_PIXEL + YMin * Buffer->Pitch);

	BEGIN_TIMED_BLOCK(ProcessPixel);
	for (int Y = YMin; Y < YMax; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int X = XMin; X < XMax; ++X)
		{
			BEGIN_TIMED_BLOCK(TestPixel);

#if 1
			v2 PixelPos = V2i(X, Y);
			v2 d = PixelPos - Origin;

			real32 Edge0 = Inner(d, -Perp(XAxis));
			real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
			real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
			real32 Edge3 = Inner(d - YAxis, Perp(YAxis));

			if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0))
			{
				BEGIN_TIMED_BLOCK(FillPixel);

#if 1
				v2 ScreenSpaceUV = V2(InvWidthMax * (real32)X, FixedCastY);
				real32 ZDiff = PixelsToMeters * ((real32)Y - OriginY);
#else
				v2 ScreenSpaceUV = V2(InvWidthMax * (real32)X, InvHeightMax * (real32)Y);
				real32 ZDiff = 0.0f;
#endif

				real32 U = InvXAxisLengthSq * Inner(d, XAxis);
				real32 V = InvYAxisLengthSq * Inner(d, YAxis);

#if 0
				Assert((U >= 0.0f) && (U <= 1.0f));
				Assert((V >= 0.0f) && (V <= 1.0f));
#endif

				real32 tX = ((U * (real32)(Texture->Width - 2)));
				real32 tY = ((V * (real32)(Texture->Height - 2)));

				int32 X = (int32)tX;
				int32 Y = (int32)tY;

				real32 fX = tX - (real32)X;
				real32 fY = tY - (real32)Y;

				Assert((X >= 0) && (X < Texture->Width));
				Assert((Y >= 0) && (Y < Texture->Height));


				bilinear_sample TexelSample = BilinearSample(Texture, X, Y);
				v4 Texel = SRGBBilinearBlend(TexelSample, fX, fY);

				if (NormalMap)
				{
					bilinear_sample NormalSample = BilinearSample(NormalMap, X, Y);

					v4 NormalA = Unpack4x8(NormalSample.A);
					v4 NormalB = Unpack4x8(NormalSample.B);
					v4 NormalC = Unpack4x8(NormalSample.C);
					v4 NormalD = Unpack4x8(NormalSample.D);

					v4 Normal = Lerp(Lerp(NormalA, fX, NormalB), fY, Lerp(NormalC, fX, NormalD));

					Normal = UnscaleAndBiasNormal(Normal);

					Normal.xy = Normal.x * NxAxis + Normal.y * NyAxis;
					Normal.z *= NzScale;
					Normal.xyz = Normalize(Normal.xyz);

					// NOTE: The eye vector is always assumed to be [0, 0, 1]
					// This is just the simplified version of the reflection -e + 2^T N N 
					v3 BounceDirection = 2.0f * Normal.z * Normal.xyz;
					BounceDirection.z -= 1.0f;

					// TODO: Eventually we need to support two mappings.
					// one for top-down view (which we don't do now) and
					// one for sideways, which is what's happening here.
					BounceDirection.z = -BounceDirection.z;

					environment_map* FarMap = 0;
					real32 Pz = OriginZ + ZDiff;
					real32 MapZ = 2.0f;
					real32 tEnvMap = BounceDirection.y;
					real32 tFarMap = 0.0f;
					if (tEnvMap < -0.5f)
					{
						FarMap = Bottom;
						tFarMap = -1.0f - 2.0f * tEnvMap;
					}
					else if (tEnvMap > 0.5f)
					{
						FarMap = Top;
						tFarMap = 2.0f * (tEnvMap - 0.5f);
					}

					tFarMap *= tFarMap;
					tFarMap *= tFarMap;

					// TODO: How do we sample from the middle map?
					v3 LightColor = V3(0, 0, 0);
					if (FarMap)
					{
						real32 DistanceFromMapInZ = FarMap->Pz - Pz;
						v3 FarMapColor = SampleEnvironmentMap(ScreenSpaceUV, BounceDirection, Normal.w, FarMap,
							DistanceFromMapInZ);
						LightColor = Lerp(LightColor, tFarMap, FarMapColor);
					}

					// TODO: Actually do a sighting model computation here
					Texel.rgb = Texel.rgb + Texel.a * LightColor;

#if 0
					// NOTE: Draws the bounce direction 
					Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f * BounceDirection;
					Texel.rgb *= Texel.a;
#endif
				}

				Texel = Hadamard(Texel, Color);
				Texel.rgb = Clamp01(Texel.rgb);

				v4 Dest = Unpack4x8(*Pixel);

				// NOTE: Go from sRGB to "linear" brightness space
				Dest = SRGB255ToLinear1(Dest);

				v4 Blended = (1.0f - Texel.a) * Dest + Texel;
				v4 Blended255 = Linear1ToSRGB255(Blended);

				*Pixel = (((uint32)(Blended255.a + 0.5f) << 24) |
					((uint32)(Blended255.r + 0.5f) << 16) |
					((uint32)(Blended255.g + 0.5f) << 8) |
					((uint32)(Blended255.b + 0.5f) << 0));
			}
#else
			* Pixel = Color32;
#endif
			++Pixel;
		}

		Row += Buffer->Pitch;
	}

	END_TIMED_BLOCK_COUNTED(ProcessPixel, (XMax - XMin - 1) * (YMax - YMin - 1));

	END_TIMED_BLOCK(DrawRectangleSlow);
}

#if 0
#include "..\iaca-win64\iacaMarks.h"
#else
#define IACA_VC64_START
#define IACA_VC64_END
#endif

internal void
DrawRectangleQuickly(loaded_bitmap* Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
					 loaded_bitmap* Texture, real32 PixelsToMeters, 
					 rectangle2i ClipRect, bool32 Even)
{
	BEGIN_TIMED_BLOCK(DrawRectangleQuick);

	// NOTE: Premultiply color up front
	Color.rgb *= Color.a;

	real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
	real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);
	v2 nXAxis = InvXAxisLengthSq * XAxis;
	v2 nYAxis = InvYAxisLengthSq * YAxis;

	rectangle2i FillRect = InvertedInfinityRectangle();

	v2 P[4] = { Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis };
	for (int PointIndex = 0; PointIndex < ArrayCount(P); ++PointIndex)
	{
		v2 TestP = P[PointIndex];
		int FloorX = FloorReal32ToInt32(TestP.x);
		int CeilX = CeilReal32ToInt32(TestP.x) + 1;
		int FloorY = FloorReal32ToInt32(TestP.y);
		int CeilY = CeilReal32ToInt32(TestP.y) + 1;

		if (FillRect.MinX > FloorX) { FillRect.MinX = FloorX; }
		if (FillRect.MinY > FloorY) { FillRect.MinY = FloorY; }
		if (FillRect.MaxX < CeilX) { FillRect.MaxX = CeilX; }
		if (FillRect.MaxY < CeilY) { FillRect.MaxY = CeilY; }
	}

	FillRect = Intersect(ClipRect, FillRect);

	if (!Even == (FillRect.MinY & 1))
	{
		FillRect.MinY += 1;
	}

	if (HasArea(FillRect))
	{
		__m128i StartClipMask = _mm_set1_epi8(-1);
		__m128i EndClipMask = _mm_set1_epi8(-1);

		__m128i StartClipMasks[] =
		{
			_mm_slli_si128(StartClipMask, 0 * 4),
			_mm_slli_si128(StartClipMask, 1 * 4),
			_mm_slli_si128(StartClipMask, 2 * 4),
			_mm_slli_si128(StartClipMask, 3 * 4),
		};

		__m128i EndClipMasks[] =
		{
			_mm_srli_si128(EndClipMask, 0 * 4),
			_mm_srli_si128(EndClipMask, 1 * 4),
			_mm_srli_si128(EndClipMask, 2 * 4),
			_mm_srli_si128(EndClipMask, 3 * 4),
		};

		if (FillRect.MinX & 3)
		{
			StartClipMask = StartClipMasks[FillRect.MinX & 3];
			FillRect.MinX = FillRect.MinX & ~3;
		}

		if (FillRect.MaxX & 3)
		{
			EndClipMask = EndClipMasks[FillRect.MaxX & 3];
			FillRect.MaxX = (FillRect.MaxX & ~3) + 4;
		}

		uint8* Row = ((uint8*)Buffer->Memory + FillRect.MinX * BITMAP_BYTES_PER_PIXEL + FillRect.MinY * Buffer->Pitch);
		int32 RowAdvance = 2 * Buffer->Pitch;
		int32 TexturePitch = Texture->Pitch;
		void* TextureMemory = Texture->Memory;

		__m128i TexturePitch_4x = _mm_set1_epi32(TexturePitch);
		__m128 Inv255_4x = _mm_set1_ps(1.0f / 255.0f);
		__m128 One = _mm_set1_ps(1.0f);
		__m128 Zero = _mm_set1_ps(0.0f);
		__m128 Four = _mm_set1_ps(4.0f);
		__m128i MaskFF = _mm_set1_epi32(0xFF);
		__m128i MaskFFFF = _mm_set1_epi32(0xFFFF);
		__m128i MaskFF00FF = _mm_set1_epi32(0x00FF00FF);
		__m128 Colorr_4x = _mm_set1_ps(Color.r);
		__m128 Colorg_4x = _mm_set1_ps(Color.g);
		__m128 Colorb_4x = _mm_set1_ps(Color.b);
		__m128 Colora_4x = _mm_set1_ps(Color.a);
		__m128 nXAxisx_4x = _mm_set1_ps(nXAxis.x);
		__m128 nXAxisy_4x = _mm_set1_ps(nXAxis.y);
		__m128 nYAxisx_4x = _mm_set1_ps(nYAxis.x);
		__m128 nYAxisy_4x = _mm_set1_ps(nYAxis.y);
		__m128 Originx_4x = _mm_set1_ps(Origin.x);
		__m128 Originy_4x = _mm_set1_ps(Origin.y);
		__m128 MaxColorValue = _mm_set1_ps(255.0f * 255.0f);
		__m128 WidthM2 = _mm_set1_ps((real32)Texture->Width - 2);
		__m128 HeightM2 = _mm_set1_ps((real32)Texture->Height - 2);

		int MinY = FillRect.MinY;
		int MaxY = FillRect.MaxY;
		int MinX = FillRect.MinX;
		int MaxX = FillRect.MaxX;

		BEGIN_TIMED_BLOCK(ProcessPixel);

		for (int Y = MinY; Y < MaxY; Y += 2)
		{
			__m128 PixelPosY = _mm_set1_ps((real32)Y);
			PixelPosY = _mm_sub_ps(PixelPosY, Originy_4x);
			__m128 PynX = _mm_mul_ps(PixelPosY, nXAxisy_4x);
			__m128 PynY = _mm_mul_ps(PixelPosY, nYAxisy_4x);

			__m128 PixelPosX = _mm_set_ps((real32)(MinX + 3),
				(real32)(MinX + 2),
				(real32)(MinX + 1),
				(real32)(MinX + 0));
			PixelPosX = _mm_sub_ps(PixelPosX, Originx_4x);

			__m128i ClipMask = StartClipMask;

			uint32* Pixel = (uint32*)Row;
			for (int XI = MinX; XI < MaxX; XI += 4)
			{
#define mmSquare(a) _mm_mul_ps(a, a)
#define mmSquareRoot(a) _mm_rsqrt_ps(a)
#define M(a, i) ((float*)&(a))[i]
#define Mi(a, i) ((uint32*)&(a))[i]

				IACA_VC64_START;
				__m128 U = _mm_add_ps(_mm_mul_ps(PixelPosX, nXAxisx_4x), PynX);
				__m128 V = _mm_add_ps(_mm_mul_ps(PixelPosX, nYAxisx_4x), PynY);

				__m128i WriteMask = _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(U, Zero),
					_mm_cmple_ps(U, One)),
					_mm_and_ps(_mm_cmpge_ps(V, Zero),
						_mm_cmple_ps(V, One))));
				WriteMask = _mm_and_si128(WriteMask, ClipMask);
				// TODO: Later, re-check if this helps
				// if (_mm_movemask_epi8(WriteMask))
				{
					__m128i OriginalDest = _mm_load_si128((__m128i*)Pixel);

					U = _mm_min_ps(_mm_max_ps(U, Zero), One);
					V = _mm_min_ps(_mm_max_ps(V, Zero), One);

					// TODO: Formalize texture boundaries
					__m128 tX = _mm_mul_ps(U, WidthM2);
					__m128 tY = _mm_mul_ps(V, HeightM2);

					__m128i FetchX_4x = _mm_cvttps_epi32(tX);
					__m128i FetchY_4x = _mm_cvttps_epi32(tY);

					__m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(FetchX_4x));
					__m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(FetchY_4x));

					FetchX_4x = _mm_slli_epi32(FetchX_4x, 2);
					FetchY_4x = _mm_or_si128(_mm_mullo_epi16(FetchY_4x, TexturePitch_4x),
											 _mm_slli_epi32(_mm_mulhi_epi16(FetchY_4x, TexturePitch_4x), 16));
					__m128i Fetch_4x = _mm_add_epi32(FetchX_4x, FetchY_4x);

					int32 Fetch0 = Mi(Fetch_4x, 0);
					int32 Fetch1 = Mi(Fetch_4x, 1);
					int32 Fetch2 = Mi(Fetch_4x, 2);
					int32 Fetch3 = Mi(Fetch_4x, 3);

					uint8* TexelPtr0 = ((uint8*)TextureMemory) + Fetch0;
					uint8* TexelPtr1 = ((uint8*)TextureMemory) + Fetch1;
					uint8* TexelPtr2 = ((uint8*)TextureMemory) + Fetch2;
					uint8* TexelPtr3 = ((uint8*)TextureMemory) + Fetch3;

					__m128i SampleA = _mm_setr_epi32(*(uint32*)(TexelPtr0),
						*(uint32*)(TexelPtr1),
						*(uint32*)(TexelPtr2),
						*(uint32*)(TexelPtr3));
					__m128i SampleB = _mm_setr_epi32(*(uint32*)(TexelPtr0 + sizeof(uint32)),
						*(uint32*)(TexelPtr1 + sizeof(uint32)),
						*(uint32*)(TexelPtr2 + sizeof(uint32)),
						*(uint32*)(TexelPtr3 + sizeof(uint32)));
					__m128i SampleC = _mm_setr_epi32(*(uint32*)(TexelPtr0 + TexturePitch),
						*(uint32*)(TexelPtr1 + TexturePitch),
						*(uint32*)(TexelPtr2 + TexturePitch),
						*(uint32*)(TexelPtr3 + TexturePitch));
					__m128i SampleD = _mm_setr_epi32(*(uint32*)(TexelPtr0 + TexturePitch + sizeof(uint32)),
						*(uint32*)(TexelPtr1 + TexturePitch + sizeof(uint32)),
						*(uint32*)(TexelPtr2 + TexturePitch + sizeof(uint32)),
						*(uint32*)(TexelPtr3 + TexturePitch + sizeof(uint32)));

					// NOTE: Unpack bilinear samples
					__m128i TexelArb = _mm_and_si128(SampleA, MaskFF00FF);
					__m128i TexelAag = _mm_and_si128(_mm_srli_epi32(SampleA, 8), MaskFF00FF);
					TexelArb = _mm_mullo_epi16(TexelArb, TexelArb);
					__m128 TexelAa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelAag, 16));
					TexelAag = _mm_mullo_epi16(TexelAag, TexelAag);

					__m128i TexelBrb = _mm_and_si128(SampleB, MaskFF00FF);
					__m128i TexelBag = _mm_and_si128(_mm_srli_epi32(SampleB, 8), MaskFF00FF);
					TexelBrb = _mm_mullo_epi16(TexelBrb, TexelBrb);
					__m128 TexelBa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBag, 16));
					TexelBag = _mm_mullo_epi16(TexelBag, TexelBag);

					__m128i TexelCrb = _mm_and_si128(SampleC, MaskFF00FF);
					__m128i TexelCag = _mm_and_si128(_mm_srli_epi32(SampleC, 8), MaskFF00FF);
					TexelCrb = _mm_mullo_epi16(TexelCrb, TexelCrb);
					__m128 TexelCa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCag, 16));
					TexelCag = _mm_mullo_epi16(TexelCag, TexelCag);

					__m128i TexelDrb = _mm_and_si128(SampleD, MaskFF00FF);
					__m128i TexelDag = _mm_and_si128(_mm_srli_epi32(SampleD, 8), MaskFF00FF);
					TexelDrb = _mm_mullo_epi16(TexelDrb, TexelDrb);
					__m128 TexelDa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDag, 16));
					TexelDag = _mm_mullo_epi16(TexelDag, TexelDag);

					// NOTE: Load destination
					__m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskFF));
					__m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskFF));
					__m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskFF));
					__m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskFF));

					// NOTE: Convert texture from sRGB 0-255 to "linear" 0-1 brightness space
					__m128 TexelAr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelArb, 16));
					__m128 TexelAg = _mm_cvtepi32_ps(_mm_and_si128(TexelAag, MaskFFFF));
					__m128 TexelAb = _mm_cvtepi32_ps(_mm_and_si128(TexelArb, MaskFFFF));

					__m128 TexelBr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBrb, 16));
					__m128 TexelBg = _mm_cvtepi32_ps(_mm_and_si128(TexelBag, MaskFFFF));
					__m128 TexelBb = _mm_cvtepi32_ps(_mm_and_si128(TexelBrb, MaskFFFF));

					__m128 TexelCr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCrb, 16));
					__m128 TexelCg = _mm_cvtepi32_ps(_mm_and_si128(TexelCag, MaskFFFF));
					__m128 TexelCb = _mm_cvtepi32_ps(_mm_and_si128(TexelCrb, MaskFFFF));

					__m128 TexelDr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDrb, 16));
					__m128 TexelDg = _mm_cvtepi32_ps(_mm_and_si128(TexelDag, MaskFFFF));
					__m128 TexelDb = _mm_cvtepi32_ps(_mm_and_si128(TexelDrb, MaskFFFF));

					// NOTE: Bilinear texture blend
					__m128 ifX = _mm_sub_ps(One, fX);
					__m128 ifY = _mm_sub_ps(One, fY);

					__m128 l0 = _mm_mul_ps(ifY, ifX);
					__m128 l1 = _mm_mul_ps(ifY, fX);
					__m128 l2 = _mm_mul_ps(fY, ifX);
					__m128 l3 = _mm_mul_ps(fY, fX);

					__m128 Texelr = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAr), _mm_mul_ps(l1, TexelBr)),
						_mm_add_ps(_mm_mul_ps(l2, TexelCr), _mm_mul_ps(l3, TexelDr)));
					__m128 Texelg = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAg), _mm_mul_ps(l1, TexelBg)),
						_mm_add_ps(_mm_mul_ps(l2, TexelCg), _mm_mul_ps(l3, TexelDg)));
					__m128 Texelb = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAb), _mm_mul_ps(l1, TexelBb)),
						_mm_add_ps(_mm_mul_ps(l2, TexelCb), _mm_mul_ps(l3, TexelDb)));
					__m128 Texela = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAa), _mm_mul_ps(l1, TexelBa)),
						_mm_add_ps(_mm_mul_ps(l2, TexelCa), _mm_mul_ps(l3, TexelDa)));

					// NOTE: Modulate by incoming color
					Texelr = _mm_mul_ps(Texelr, Colorr_4x);
					Texelg = _mm_mul_ps(Texelg, Colorg_4x);
					Texelb = _mm_mul_ps(Texelb, Colorb_4x);
					Texela = _mm_mul_ps(Texela, Colora_4x);

					// NOTE: Clamp colors to valid range
					Texelr = _mm_min_ps(_mm_max_ps(Texelr, Zero), MaxColorValue);
					Texelg = _mm_min_ps(_mm_max_ps(Texelg, Zero), MaxColorValue);
					Texelb = _mm_min_ps(_mm_max_ps(Texelb, Zero), MaxColorValue);

					// NOTE: Go from sRGB to "linear" brightness space
					Destr = mmSquare(Destr);
					Destg = mmSquare(Destg);
					Destb = mmSquare(Destb);
					Desta = _mm_mul_ps(Inv255_4x, Desta);

					// NOTE: Destination blend
					__m128 InvTexelA = _mm_sub_ps(One, _mm_mul_ps(Inv255_4x, Texela));
					__m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA, Destr), Texelr);
					__m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA, Destg), Texelg);
					__m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA, Destb), Texelb);
					__m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA, Desta), Texela);

					// NOTE: Go from "linear" 0-1 brightness space to sRGB 0-255
#if 1
					Blendedr = _mm_mul_ps(Blendedr, mmSquareRoot(Blendedr));
					Blendedg = _mm_mul_ps(Blendedg, mmSquareRoot(Blendedg));
					Blendedb = _mm_mul_ps(Blendedb, mmSquareRoot(Blendedb));
#else
					Blendedr = _mm_sqrt_ps(Blendedr);
					Blendedg = _mm_sqrt_ps(Blendedg);
					Blendedb = _mm_sqrt_ps(Blendedb);
#endif

					__m128i Intr = _mm_cvtps_epi32(Blendedr);
					__m128i Intg = _mm_cvtps_epi32(Blendedg);
					__m128i Intb = _mm_cvtps_epi32(Blendedb);
					__m128i Inta = _mm_cvtps_epi32(Blendeda);

					__m128i Sb = Intb;
					__m128i Sg = _mm_slli_epi32(Intg, 8);
					__m128i Sr = _mm_slli_epi32(Intr, 16);
					__m128i Sa = _mm_slli_epi32(Inta, 24);

					__m128i Out = _mm_or_si128(_mm_or_si128(Sr, Sg), _mm_or_si128(Sb, Sa));

					__m128i MaskedOut = _mm_or_si128(_mm_and_si128(WriteMask, Out), _mm_andnot_si128(WriteMask, OriginalDest));

					_mm_store_si128((__m128i*)Pixel, MaskedOut);
				}

				PixelPosX = _mm_add_ps(Four, PixelPosX);
				Pixel += 4;

				if ((XI + 8) < MaxX)
				{
					ClipMask = _mm_set1_epi8(-1);
				}
				else
				{
					ClipMask = EndClipMask;
				}

				IACA_VC64_END;
			}

			Row += RowAdvance;
		}

		END_TIMED_BLOCK_COUNTED(ProcessPixel, GetClampedRectArea(FillRect) / 2);
	}

	END_TIMED_BLOCK(DrawRectangleQuick);     
}

internal void
ChangeSaturation(loaded_bitmap* Buffer, real32 Level)
{
	uint8* DestRow = (uint8*)Buffer->Memory;
	for (int32 y = 0; y < Buffer->Height; ++y)
	{
		uint32* Dest = (uint32*)DestRow;
		for (int32 x = 0; x < Buffer->Width; ++x)
		{
			v4 D = Unpack4x8(*Dest);
			D = SRGB255ToLinear1(D);

			real32 Avg = (1.0f / 3.0f) * (D.r + D.g + D.b);
			v3 Delta = V3(D.r - Avg, D.g - Avg, D.b - Avg);

			v4 Result = ToV4(V3(Avg, Avg, Avg) + Level * Delta, D.a);

			Result = Linear1ToSRGB255(Result);

			*Dest = (((uint32)(Result.a + 0.5f) << 24) |
				     ((uint32)(Result.r + 0.5f) << 16) |
				     ((uint32)(Result.g + 0.5f) << 8)  |
				     ((uint32)(Result.b + 0.5f) << 0));

			++Dest;
		}

		DestRow += Buffer->Pitch;
	}
}

internal void
DrawBitmap(loaded_bitmap* Buffer, loaded_bitmap* Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
{
	int32 MinX = RoundReal32ToInt32(RealX);
	int32 MinY = RoundReal32ToInt32(RealY);
	int32 MaxX = MinX + Bitmap->Width;
	int32 MaxY = MinY + Bitmap->Height;

	int32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	int32 SourceOffsetY = 0;
	if (MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	uint8* SourceRow = (uint8*)Bitmap->Memory + Bitmap->Pitch * SourceOffsetY + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
	uint8* DestRow = ((uint8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);
	for (int32 y = MinY; y < MaxY; ++y)
	{
		uint32* Dest = (uint32*)DestRow;
		uint32* Source = (uint32*)SourceRow;
		for (int32 x = MinX; x < MaxX; ++x)
		{
			v4 Texel = Unpack4x8(*Source);
			Texel = SRGB255ToLinear1(Texel);
			Texel *= CAlpha;

			v4 D = Unpack4x8(*Dest);
			D = SRGB255ToLinear1(D);

			v4 Result = (1.0f - Texel.a) * D + Texel;
			Result = Linear1ToSRGB255(Result);

			*Dest = (((uint32)(Result.a + 0.5f) << 24) |
					 ((uint32)(Result.r + 0.5f) << 16) |
					 ((uint32)(Result.g + 0.5f) <<  8) |
					 ((uint32)(Result.b + 0.5f) <<  0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	}
}

internal void
DrawMatte(loaded_bitmap* Buffer, loaded_bitmap* Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
{
	int32 MinX = RoundReal32ToInt32(RealX);
	int32 MinY = RoundReal32ToInt32(RealY);
	int32 MaxX = MinX + Bitmap->Width;
	int32 MaxY = MinY + Bitmap->Height;

	int32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	int32 SourceOffsetY = 0;
	if (MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	uint8* SourceRow = (uint8*)Bitmap->Memory + Bitmap->Pitch * SourceOffsetY + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
	uint8* DestRow = ((uint8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);
	for (int32 y = MinY; y < MaxY; ++y)
	{
		uint32* Dest = (uint32*)DestRow;
		uint32* Source = (uint32*)SourceRow;
		for (int32 x = MinX; x < MaxX; ++x)
		{
			real32 SA = (real32)((*Source >> 24) & 0xFF);
			real32 RSA = (SA / 255.0f) * CAlpha;
			real32 SR = CAlpha * (real32)((*Source >> 16) & 0xFF);
			real32 SG = CAlpha * (real32)((*Source >> 8) & 0xFF);
			real32 SB = CAlpha * (real32)((*Source >> 0) & 0xFF);

			real32 DA = (real32)((*Dest >> 24) & 0xFF);
			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0) & 0xFF);
			real32 RDA = (DA / 255.0f);

			real32 InvRSA = (1.0f - RSA);
			// real32 A = 255.0f * (RSA + RDA - RSA * RDA);
			real32 A = InvRSA * DA;
			real32 r = InvRSA * DR;
			real32 g = InvRSA * DG;
			real32 b = InvRSA * DB;

			*Dest = (((uint32)(A + 0.5f) << 24) |
				((uint32)(r + 0.5f) << 16) |
				((uint32)(g + 0.5f) << 8) |
				((uint32)(b + 0.5f) << 0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	}
}

#define PushRenderElement(Group, type) (type*)PushRenderElement_(Group, sizeof(type), render_group_entry_type::##type)
inline void*
PushRenderElement_(render_group* Group, uint32 Size, render_group_entry_type Type)
{
	void* Result = 0;

	Size += sizeof(render_group_entry_header);

	if ((Group->PushBufferSize + Size) < Group->MaxPushBufferSize)
	{
		render_group_entry_header* Header = (render_group_entry_header*)(Group->PushBufferBase + Group->PushBufferSize);
		Header->Type = Type;
		Result = (uint8*)Header + sizeof(*Header);
		Group->PushBufferSize += Size;
	}
	else
	{
		InvalidCodePath;
	}

	return(Result);
}

inline void
PushBitmap(render_group* Group, loaded_bitmap* Bitmap, v3 Offset, real32 Height, v4 Color = V4(1, 1, 1, 1))
{
	render_entry_bitmap* Entry = PushRenderElement(Group, render_entry_bitmap);
	if (Entry)
	{
		Entry->Bitmap = Bitmap;
		Entry->EntityBasis.Basis = Group->DefaultBasis;
		v2 Size = V2(Height * Bitmap->WidthOverHeight, Height);
		v2 Align = Hadamard(Bitmap->AlignPercentage, Size);
		Entry->EntityBasis.Offset = Offset - ToV3(Align, 0);
		Entry->Color = Group->GlobalAlpha * Color;
		Entry->Size = Size;
	}
}

inline void
PushRect(render_group* Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
	render_entry_rectangle* Entry = PushRenderElement(Group, render_entry_rectangle);
	if (Entry)
	{
		Entry->EntityBasis.Basis = Group->DefaultBasis;
		Entry->EntityBasis.Offset = (Offset - ToV3(0.5f * Dim, 0));
		Entry->Dim = Dim;
		Entry->Color = Color;
	}
}

inline void
PushRectOutline(render_group* Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
	real32 Thickness = 0.1f;

	// NOTE: Top and bottom
	PushRect(Group, Offset - V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);
	PushRect(Group, Offset + V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);

	// NOTE: Left and right
	PushRect(Group, Offset - V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
	PushRect(Group, Offset + V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
}

inline void
Clear(render_group* Group, v4 Color)
{
	render_entry_clear* Entry = PushRenderElement(Group, render_entry_clear);
	if (Entry)
	{
		Entry->Color = Color;
	}
}

inline render_entry_coordinate_system*
CoordinateSystem(render_group* Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap* Texture, 
	loaded_bitmap* NormalMap, environment_map* Top, environment_map* Middle, environment_map* Bottom)
{
	render_entry_coordinate_system* Entry = PushRenderElement(Group, render_entry_coordinate_system);
	if (Entry)
	{
		Entry->Origin = Origin;
		Entry->XAxis = XAxis;
		Entry->YAxis = YAxis;
		Entry->Color = Color;
		Entry->Texture = Texture;
		Entry->NormalMap = NormalMap;
		Entry->Top = Top;
		Entry->Middle = Middle;
		Entry->Bottom = Bottom;
	}

	return(Entry);
}

struct entity_basis_p_result
{
	v2 Pos;
	real32 Scale;
	bool32 Valid;
};
inline entity_basis_p_result
GetRenderEntityBasisPos(render_group* RenderGroup, render_entity_basis* EntityBasis, v2 ScreenDim)
{
	v2 ScreenCenter = 0.5f * ScreenDim;

	entity_basis_p_result Result = {};

	v3 EntityPos = EntityBasis->Basis->Pos;

	real32 DistanceToPosZ = RenderGroup->RenderCamera.DistanceAboveTarget - EntityPos.z;
	real32 NearClipPlane = 0.2f;

	v3 RawXY = ToV3(EntityPos.xy + EntityBasis->Offset.xy, 1.0f);

	if (DistanceToPosZ > NearClipPlane)
	{
		v3 ProjectedXY = (1.0f / DistanceToPosZ) * RenderGroup->RenderCamera.FocalLength * RawXY;
		Result.Pos = ScreenCenter + RenderGroup->MetersToPixels * ProjectedXY.xy;
		Result.Scale = RenderGroup->MetersToPixels * ProjectedXY.z;
		Result.Valid = true;
	}

	return(Result);
}

internal void
RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget, rectangle2i ClipRect, bool32 Even)
{
	BEGIN_TIMED_BLOCK(RenderGroupOutput);

	v2 ScreenDim = V2(
		(real32)OutputTarget->Width,
		(real32)OutputTarget->Height
	);

	real32 PixelsToMeters = 1.0f / RenderGroup->MetersToPixels;

	for (uint32 BaseAddress = 0; BaseAddress < RenderGroup->PushBufferSize;)
	{
		render_group_entry_header* Header = (render_group_entry_header*)(RenderGroup->PushBufferBase + BaseAddress);
		BaseAddress += sizeof(*Header);
		void* Data = (uint8*)Header + sizeof(*Header);

		switch (Header->Type)
		{
			case render_group_entry_type::render_entry_clear:
			{
				render_entry_clear* Entry = (render_entry_clear*)Data;

				DrawRectangle(OutputTarget, V2(0, 0), 
							  V2i(OutputTarget->Width, OutputTarget->Height), 
							  Entry->Color, ClipRect, Even);

				BaseAddress += sizeof(*Entry);
			} break;
			
			case render_group_entry_type::render_entry_bitmap:
			{
				render_entry_bitmap* Entry = (render_entry_bitmap*)Data;

				entity_basis_p_result Basis = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenDim);
				Assert(Entry->Bitmap);
#if 0
				// DrawBitmap(OutputTarget, Entry->Bitmap, Pos.x, Pos.y, 1);
				DrawRectangleSlowly(
					OutputTarget, Basis.Pos,
					Basis.Scale * V2(Entry->Size.x, 0),
					Basis.Scale * V2(0, Entry->Size.y),
					Entry->Color, Entry->Bitmap, 0, 0, 0, 0, PixelsToMeters
				);
#else
				DrawRectangleQuickly(
					OutputTarget, Basis.Pos, 
					Basis.Scale * V2(Entry->Size.x, 0), 
					Basis.Scale * V2(0, Entry->Size.y), 
					Entry->Color, Entry->Bitmap, PixelsToMeters, ClipRect, Even
				);
#endif

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_rectangle:
			{
				render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
				entity_basis_p_result Basis = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenDim);
				DrawRectangle(OutputTarget, Basis.Pos, 
							  Basis.Pos + Basis.Scale * Entry->Dim, 
							  Entry->Color, ClipRect, Even);

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_coordinate_system:
			{
				render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)Data;
#if 0
				
				v2 vMax = (Entry->Origin + Entry->XAxis + Entry->YAxis);
				DrawRectangleSlowly(
					OutputTarget, 
					Entry->Origin, 
					Entry->XAxis, 
					Entry->YAxis, 
					Entry->Color, 
					Entry->Texture, 
					Entry->NormalMap,
					Entry->Top,
					Entry->Middle,
					Entry->Bottom,
					PixelsToMeters
				);

				v4 Color = V4(1, 1, 0, 1);
				v2 Dim = V2(2, 2);
				v2 Pos = Entry->Origin;
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color, ClipRect, Even);

				Pos = Entry->Origin + Entry->XAxis;
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color, ClipRect, Even);

				Pos = Entry->Origin + Entry->YAxis;
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color, ClipRect, Even);

				DrawRectangle(OutputTarget, vMax - Dim, vMax + Dim, Color, ClipRect, Even);

#if 0
				for (uint32 PointIndex = 0; PointIndex < ArrayCount(Entry->Points); ++PointIndex)
				{
					v2 P = Entry->Points[PointIndex];
					P = Entry->Origin + P.x * Entry->XAxis + P.y * Entry->YAxis;
					DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
				}
#endif
#endif
				BaseAddress += sizeof(*Entry);
			} break;

			InvalidDefaultCase;
		}
	}

	END_TIMED_BLOCK(RenderGroupOutput);
}

struct tile_render_work
{
	render_group* RenderGroup;
	loaded_bitmap* OutputTarget;
	rectangle2i ClipRect;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork)
{
	tile_render_work* Work = (tile_render_work*)Data;

	RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, true);
	RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, false);
}

internal void
TiledRenderGroupToOutput(platform_work_queue* RenderQueue, render_group* RenderGroup, loaded_bitmap* OutputTarget)
{
	int32 const TileCountX = 4;
	int32 const TileCountY = 4;
	tile_render_work WorkArray[TileCountX * TileCountY];

	Assert(((uintptr)OutputTarget->Memory & 15) == 0);

	int32 TileWidth = OutputTarget->Width / TileCountX;
	int32 TileHeight = OutputTarget->Height / TileCountY;

	TileWidth = ((TileWidth + 3) / 4) * 4;

	int WorkCount = 0;
	for (int TileY = 0; TileY < TileCountY; ++TileY)
	{
		for (int TileX = 0; TileX < TileCountX; ++TileX)
		{
			tile_render_work* Work = WorkArray + WorkCount++;

			rectangle2i ClipRect = {};
			ClipRect.MinX = TileX * TileWidth;
			ClipRect.MaxX = ClipRect.MinX + TileWidth;
			ClipRect.MinY = TileY * TileHeight;
			ClipRect.MaxY = ClipRect.MinY + TileHeight;

			if (ClipRect.MaxX > OutputTarget->Width)
			{
				ClipRect.MaxX = OutputTarget->Width;
			}

			Work->RenderGroup = RenderGroup;
			Work->OutputTarget = OutputTarget;
			Work->ClipRect = ClipRect;

#if 1
			// Multithread
			PlatformAddEntry(RenderQueue, DoTiledRenderWork, Work);
#else
			// Single Thread
			DoTiledRenderWork(RenderQueue, Work);
#endif
		}
	}

	PlatformCompleteAllWork(RenderQueue);
}

internal render_group*
AllocateRenderGroup(memory_arena* Arena, uint32 MaxPushBufferSize, uint32 ResolutionPixelsX, uint32 ResolutionPixelsY)
{
	render_group* Result = PushStruct(Arena, render_group);
	Result->PushBufferBase = (uint8*)PushSize(Arena, MaxPushBufferSize);

	Result->DefaultBasis = PushStruct(Arena, render_basis);
	Result->DefaultBasis->Pos = V3(0, 0, 0);

	Result->MaxPushBufferSize = MaxPushBufferSize;
	Result->PushBufferSize = 0;

	// NOTE: Meters the person is sitting from their monitor
	Result->GameCamera.FocalLength = 0.6f;
	Result->GameCamera.DistanceAboveTarget = 9.0f;
	Result->RenderCamera = Result->GameCamera;
	// Result->RenderCamera.DistanceAboveTarget = 50.0f;

	Result->GlobalAlpha = 1.0f;

	// TODO: Need to adjust this based on buffer size
	// NOTE: Horizontal measurement of monitor in meters
	real32 WidthOfMonitor = 0.635f;
	Result->MetersToPixels = real32(ResolutionPixelsX) * WidthOfMonitor;

	real32 PixelsToMeters = 1.0f / Result->MetersToPixels;
	Result->MonitorHalfDimInMeters = 0.5 * PixelsToMeters * V2(ResolutionPixelsX, ResolutionPixelsY);

	return(Result);
}

inline v2
Unproject(render_group* Group, v2 ProjectedXY, real32 AtDistanceFromCamera)
{
	v2 WorldXY = (AtDistanceFromCamera / Group->GameCamera.FocalLength) * ProjectedXY;
	return(WorldXY);
}

inline rectangle2
GetCameraRectangleAtDistance(render_group* Group, real32 DistanceFromCamera)
{
	v2 RawXY = Unproject(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);

	rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);

	return(Result);
}

inline rectangle2
GetCameraRectangleAtTarget(render_group* Group)
{
	rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->GameCamera.DistanceAboveTarget);

	return(Result);
}