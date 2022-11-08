#define internal
#include "p5engine.h"

#if 0
#include "iaca-win64/iacaMarks.h"
#else
#define IACA_VC64_START
#define IACA_VC64_END
#endif

void
DrawRectangleQuickly(loaded_bitmap* Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap* Texture, f32 PixelsToMeters,
	rectangle2i ClipRect, b32 Even)
{
	TIMED_BLOCK();

	// NOTE: Premultiply color up front
	Color.rgb *= Color.a;

	f32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
	f32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);
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
			_mm_srli_si128(EndClipMask, 3 * 4),
			_mm_srli_si128(EndClipMask, 2 * 4),
			_mm_srli_si128(EndClipMask, 1 * 4),
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

		u8* Row = ((u8*)Buffer->Memory + FillRect.MinX * BITMAP_BYTES_PER_PIXEL + FillRect.MinY * Buffer->Pitch);
		i32 RowAdvance = 2 * Buffer->Pitch;
		i32 TexturePitch = Texture->Pitch;
		void* TextureMemory = Texture->Memory;

		__m128i TexturePitch_4x = _mm_set1_epi32(TexturePitch);
		__m128 Inv255_4x = _mm_set1_ps(1.0f / 255.0f);
		__m128 One = _mm_set1_ps(1.0f);
		__m128 Half = _mm_set1_ps(0.5f);
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
		__m128 WidthM2 = _mm_set1_ps((f32)Texture->Width - 2);
		__m128 HeightM2 = _mm_set1_ps((f32)Texture->Height - 2);

		int MinY = FillRect.MinY;
		int MaxY = FillRect.MaxY;
		int MinX = FillRect.MinX;
		int MaxX = FillRect.MaxX;

		TIMED_BLOCK((GetClampedRectArea(FillRect) / 2));

		for (int Y = MinY; Y < MaxY; Y += 2)
		{
			__m128 PixelPosY = _mm_set1_ps((f32)Y);
			PixelPosY = _mm_sub_ps(PixelPosY, Originy_4x);
			__m128 PynX = _mm_mul_ps(PixelPosY, nXAxisy_4x);
			__m128 PynY = _mm_mul_ps(PixelPosY, nYAxisy_4x);

			__m128 PixelPosX = _mm_set_ps((f32)(MinX + 3),
				(f32)(MinX + 2),
				(f32)(MinX + 1),
				(f32)(MinX + 0));
			PixelPosX = _mm_sub_ps(PixelPosX, Originx_4x);

			__m128i ClipMask = StartClipMask;

			u32* Pixel = (u32*)Row;
			for (int XI = MinX; XI < MaxX; XI += 4)
			{
#define mmSquare(a) _mm_mul_ps(a, a)
#define mmSquareRoot(a) _mm_rsqrt_ps(a)
#define M(a, i) ((float*)&(a))[i]
#define Mi(a, i) ((u32*)&(a))[i]

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

					// TODO: Bias texture coordinates to start 
					// on the boundary between the 0,0 and 1,1 pixels
					__m128 tX = _mm_add_ps(_mm_mul_ps(U, WidthM2), Half);
					__m128 tY = _mm_add_ps(_mm_mul_ps(V, HeightM2), Half);

					__m128i FetchX_4x = _mm_cvttps_epi32(tX);
					__m128i FetchY_4x = _mm_cvttps_epi32(tY);

					__m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(FetchX_4x));
					__m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(FetchY_4x));

					FetchX_4x = _mm_slli_epi32(FetchX_4x, 2);
					FetchY_4x = _mm_or_si128(_mm_mullo_epi16(FetchY_4x, TexturePitch_4x),
						_mm_slli_epi32(_mm_mulhi_epi16(FetchY_4x, TexturePitch_4x), 16));
					__m128i Fetch_4x = _mm_add_epi32(FetchX_4x, FetchY_4x);

					i32 Fetch0 = Mi(Fetch_4x, 0);
					i32 Fetch1 = Mi(Fetch_4x, 1);
					i32 Fetch2 = Mi(Fetch_4x, 2);
					i32 Fetch3 = Mi(Fetch_4x, 3);

					u8* TexelPtr0 = ((u8*)TextureMemory) + Fetch0;
					u8* TexelPtr1 = ((u8*)TextureMemory) + Fetch1;
					u8* TexelPtr2 = ((u8*)TextureMemory) + Fetch2;
					u8* TexelPtr3 = ((u8*)TextureMemory) + Fetch3;

					__m128i SampleA = _mm_setr_epi32(*(u32*)(TexelPtr0),
						*(u32*)(TexelPtr1),
						*(u32*)(TexelPtr2),
						*(u32*)(TexelPtr3));
					__m128i SampleB = _mm_setr_epi32(*(u32*)(TexelPtr0 + sizeof(u32)),
						*(u32*)(TexelPtr1 + sizeof(u32)),
						*(u32*)(TexelPtr2 + sizeof(u32)),
						*(u32*)(TexelPtr3 + sizeof(u32)));
					__m128i SampleC = _mm_setr_epi32(*(u32*)(TexelPtr0 + TexturePitch),
						*(u32*)(TexelPtr1 + TexturePitch),
						*(u32*)(TexelPtr2 + TexturePitch),
						*(u32*)(TexelPtr3 + TexturePitch));
					__m128i SampleD = _mm_setr_epi32(*(u32*)(TexelPtr0 + TexturePitch + sizeof(u32)),
						*(u32*)(TexelPtr1 + TexturePitch + sizeof(u32)),
						*(u32*)(TexelPtr2 + TexturePitch + sizeof(u32)),
						*(u32*)(TexelPtr3 + TexturePitch + sizeof(u32)));

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
	}
}

debug_record DebugRecordArray[__COUNTER__];