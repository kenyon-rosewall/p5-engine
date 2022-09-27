
inline v4
Unpack4x8(u32 Packed)
{
	v4 Result = V4(
		(f32)((Packed >> 16) & 0xFF),
		(f32)((Packed >> 8) & 0xFF),
		(f32)((Packed >> 0) & 0xFF),
		(f32)((Packed >> 24) & 0xFF)
	);

	return(Result);
}

inline v4
UnscaleAndBiasNormal(v4 Normal)
{
	v4 Result = {};

	f32 Inv255 = 1.0f / 255.0f;

	Result.x = -1.0f + 2.0f * (Inv255 * Normal.x);
	Result.y = -1.0f + 2.0f * (Inv255 * Normal.y);
	Result.z = -1.0f + 2.0f * (Inv255 * Normal.z);
	Result.w = Inv255 * Normal.w;

	return(Result);
}

internal void
DrawRectangle(loaded_bitmap* Buffer, v2 vMin, v2 vMax, v4 Color, rectangle2i ClipRect, b32 Even)
{
	f32 R = Color.r;
	f32 G = Color.g;
	f32 B = Color.b;
	f32 A = Color.a;

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

	u32 Color32 = ((RoundReal32ToUInt32(A * 255.0f) << 24) |
					  (RoundReal32ToUInt32(R * 255.0f) << 16) | 
					  (RoundReal32ToUInt32(G * 255.0f) <<  8) | 
					  (RoundReal32ToUInt32(B * 255.0f) <<  0));

	u8* Row = ((u8*)Buffer->Memory + FillRect.MinX * BITMAP_BYTES_PER_PIXEL + FillRect.MinY * Buffer->Pitch);

	for (int y = FillRect.MinY; y < FillRect.MaxY; y += 2)
	{
		u32* Pixel = (u32*)Row;
		for (int x = FillRect.MinX; x < FillRect.MaxX; ++x)
		{
			*Pixel++ = Color32;
		}

		Row += 2 * Buffer->Pitch;
	}
}

struct bilinear_sample
{
	u32 A, B, C, D;
};

inline bilinear_sample
BilinearSample(loaded_bitmap* Texture, i32 X, i32 Y)
{
	bilinear_sample Result = {};

	u8* TexelPtr = ((u8*)Texture->Memory) + Y * Texture->Pitch + X * BITMAP_BYTES_PER_PIXEL;
	Result.A = *(u32*)(TexelPtr);
	Result.B = *(u32*)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
	Result.C = *(u32*)(TexelPtr + Texture->Pitch);
	Result.D = *(u32*)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);

	return(Result);
}

inline v4
SRGBBilinearBlend(bilinear_sample Sample, f32 fX, f32 fY)
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
SampleEnvironmentMap(v2 ScreenSpaceUV, v3 SampleDirection, f32 Roughness, environment_map* Map,
	f32 DistanceFromMapInZ)
{
	/* NOTE: ScreenSpaceUV tells us where the ray is being cast _from_
	   in normalized screen coordinates.

	   SampleDirection tells us what direction the cast is going - it
	   does not have to be normalized.

	   Roughness says which LODs of Map we sample from.
	*/

	// NOTE: Pick which LOD to sample from
	u32 LODIndex = (u32)(Roughness * (f32)(ArrayCount(Map->LOD) - 1) + 0.5f);
	Assert(LODIndex < ArrayCount(Map->LOD));

	loaded_bitmap* LOD = &Map->LOD[LODIndex];

	// NOTE: Compute the distance to the map and the scaling
	// factor for meters-to-UVs
	// TODO: Parameterize this, and should be different for X and Y,
	// based on map
	f32 UVsPerMeter = 0.1f;
	f32 C = (UVsPerMeter * DistanceFromMapInZ) / SampleDirection.y;
	// TODO: Make sure we know what direction Z should go in Y
	v2 Offset = C * V2(SampleDirection.x, SampleDirection.z);

	// NOTE: Find the intersection point
	v2 UV = ScreenSpaceUV + Offset;

	// NOTE: Clamp to the valid range
	UV.x = Clamp01(UV.x);
	UV.y = Clamp01(UV.y);

	// NOTE: Bilinear sample
	// TODO: Formalize texture boundaries!!!
	f32 tX = ((UV.x * (f32)(LOD->Width - 2)));
	f32 tY = ((UV.y * (f32)(LOD->Height - 2)));

	i32 X = (i32)tX;
	i32 Y = (i32)tY;

	f32 fX = tX - (f32)X;
	f32 fY = tY - (f32)Y;

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
	f32 PixelsToMeters)
{
	BEGIN_TIMED_BLOCK(DrawRectangleSlow);

	// NOTE: Premultiply color up front
	Color.rgb *= Color.a;

	f32 XAxisLength = Length(XAxis);
	f32 YAxisLength = Length(YAxis);

	v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
	v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;

	// NOTE: NzScale could be a parameter if we want people to have
	// control over the amount of scaling in the Z direction that the
	// normals appear to have.
	f32 NzScale = 0.5f * (XAxisLength + YAxisLength);

	f32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
	f32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

	u32 Color32 = ((RoundReal32ToUInt32(Color.a * 255.0f) << 24) |
		(RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
		(RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
		(RoundReal32ToUInt32(Color.b * 255.0f) << 0));

	int WidthMax = (Buffer->Width - 1);
	int HeightMax = (Buffer->Height - 1);

	f32 InvWidthMax = 1.0f / (f32)WidthMax;
	f32 InvHeightMax = 1.0f / (f32)HeightMax;

	// TODO: This will need to be specified separately!
	f32 OriginZ = 0.0f;
	f32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
	f32 FixedCastY = InvHeightMax * OriginY;

	i32 XMin = WidthMax;
	i32 XMax = 0;
	i32 YMin = HeightMax;
	i32 YMax = 0;

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

	u8* Row = ((u8*)Buffer->Memory + XMin * BITMAP_BYTES_PER_PIXEL + YMin * Buffer->Pitch);

	BEGIN_TIMED_BLOCK(ProcessPixel);
	for (int Y = YMin; Y < YMax; ++Y)
	{
		u32* Pixel = (u32*)Row;
		for (int X = XMin; X < XMax; ++X)
		{
			BEGIN_TIMED_BLOCK(TestPixel);

#if 1
			v2 PixelPos = V2i(X, Y);
			v2 d = PixelPos - Origin;

			f32 Edge0 = Inner(d, -Perp(XAxis));
			f32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
			f32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
			f32 Edge3 = Inner(d - YAxis, Perp(YAxis));

			if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0))
			{
				BEGIN_TIMED_BLOCK(FillPixel);

#if 1
				v2 ScreenSpaceUV = V2(InvWidthMax * (f32)X, FixedCastY);
				f32 ZDiff = PixelsToMeters * ((f32)Y - OriginY);
#else
				v2 ScreenSpaceUV = V2(InvWidthMax * (f32)X, InvHeightMax * (f32)Y);
				f32 ZDiff = 0.0f;
#endif

				f32 U = InvXAxisLengthSq * Inner(d, XAxis);
				f32 V = InvYAxisLengthSq * Inner(d, YAxis);

#if 0
				Assert((U >= 0.0f) && (U <= 1.0f));
				Assert((V >= 0.0f) && (V <= 1.0f));
#endif

				f32 tX = ((U * (f32)(Texture->Width - 2)));
				f32 tY = ((V * (f32)(Texture->Height - 2)));

				i32 X = (i32)tX;
				i32 Y = (i32)tY;

				f32 fX = tX - (f32)X;
				f32 fY = tY - (f32)Y;

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
					f32 Pz = OriginZ + ZDiff;
					f32 MapZ = 2.0f;
					f32 tEnvMap = BounceDirection.y;
					f32 tFarMap = 0.0f;
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
						f32 DistanceFromMapInZ = FarMap->Pz - Pz;
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

				*Pixel = (((u32)(Blended255.a + 0.5f) << 24) |
					((u32)(Blended255.r + 0.5f) << 16) |
					((u32)(Blended255.g + 0.5f) << 8) |
					((u32)(Blended255.b + 0.5f) << 0));
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

internal void
ChangeSaturation(loaded_bitmap* Buffer, f32 Level)
{
	u8* DestRow = (u8*)Buffer->Memory;
	for (i32 y = 0; y < Buffer->Height; ++y)
	{
		u32* Dest = (u32*)DestRow;
		for (i32 x = 0; x < Buffer->Width; ++x)
		{
			v4 D = Unpack4x8(*Dest);
			D = SRGB255ToLinear1(D);

			f32 Avg = (1.0f / 3.0f) * (D.r + D.g + D.b);
			v3 Delta = V3(D.r - Avg, D.g - Avg, D.b - Avg);

			v4 Result = ToV4(V3(Avg, Avg, Avg) + Level * Delta, D.a);

			Result = Linear1ToSRGB255(Result);

			*Dest = (((u32)(Result.a + 0.5f) << 24) |
				     ((u32)(Result.r + 0.5f) << 16) |
				     ((u32)(Result.g + 0.5f) << 8)  |
				     ((u32)(Result.b + 0.5f) << 0));

			++Dest;
		}

		DestRow += Buffer->Pitch;
	}
}

internal void
DrawBitmap(loaded_bitmap* Buffer, loaded_bitmap* Bitmap, f32 RealX, f32 RealY, f32 CAlpha = 1.0f)
{
	i32 MinX = RoundReal32ToInt32(RealX);
	i32 MinY = RoundReal32ToInt32(RealY);
	i32 MaxX = MinX + Bitmap->Width;
	i32 MaxY = MinY + Bitmap->Height;

	i32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	i32 SourceOffsetY = 0;
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

	u8* SourceRow = (u8*)Bitmap->Memory + Bitmap->Pitch * SourceOffsetY + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
	u8* DestRow = ((u8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);
	for (i32 y = MinY; y < MaxY; ++y)
	{
		u32* Dest = (u32*)DestRow;
		u32* Source = (u32*)SourceRow;
		for (i32 x = MinX; x < MaxX; ++x)
		{
			v4 Texel = Unpack4x8(*Source);
			Texel = SRGB255ToLinear1(Texel);
			Texel *= CAlpha;

			v4 D = Unpack4x8(*Dest);
			D = SRGB255ToLinear1(D);

			v4 Result = (1.0f - Texel.a) * D + Texel;
			Result = Linear1ToSRGB255(Result);

			*Dest = (((u32)(Result.a + 0.5f) << 24) |
					 ((u32)(Result.r + 0.5f) << 16) |
					 ((u32)(Result.g + 0.5f) <<  8) |
					 ((u32)(Result.b + 0.5f) <<  0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	}
}

internal void
DrawMatte(loaded_bitmap* Buffer, loaded_bitmap* Bitmap, f32 RealX, f32 RealY, f32 CAlpha = 1.0f)
{
	i32 MinX = RoundReal32ToInt32(RealX);
	i32 MinY = RoundReal32ToInt32(RealY);
	i32 MaxX = MinX + Bitmap->Width;
	i32 MaxY = MinY + Bitmap->Height;

	i32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	i32 SourceOffsetY = 0;
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

	u8* SourceRow = (u8*)Bitmap->Memory + Bitmap->Pitch * SourceOffsetY + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
	u8* DestRow = ((u8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);
	for (i32 y = MinY; y < MaxY; ++y)
	{
		u32* Dest = (u32*)DestRow;
		u32* Source = (u32*)SourceRow;
		for (i32 x = MinX; x < MaxX; ++x)
		{
			f32 SA = (f32)((*Source >> 24) & 0xFF);
			f32 RSA = (SA / 255.0f) * CAlpha;
			f32 SR = CAlpha * (f32)((*Source >> 16) & 0xFF);
			f32 SG = CAlpha * (f32)((*Source >> 8) & 0xFF);
			f32 SB = CAlpha * (f32)((*Source >> 0) & 0xFF);

			f32 DA = (f32)((*Dest >> 24) & 0xFF);
			f32 DR = (f32)((*Dest >> 16) & 0xFF);
			f32 DG = (f32)((*Dest >> 8) & 0xFF);
			f32 DB = (f32)((*Dest >> 0) & 0xFF);
			f32 RDA = (DA / 255.0f);

			f32 InvRSA = (1.0f - RSA);
			// real32 A = 255.0f * (RSA + RDA - RSA * RDA);
			f32 A = InvRSA * DA;
			f32 r = InvRSA * DR;
			f32 g = InvRSA * DG;
			f32 b = InvRSA * DB;

			*Dest = (((u32)(A + 0.5f) << 24) |
				((u32)(r + 0.5f) << 16) |
				((u32)(g + 0.5f) << 8) |
				((u32)(b + 0.5f) << 0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	}
}

struct entity_basis_p_result
{
	v2 Pos;
	f32 Scale;
	b32 Valid;
};
inline entity_basis_p_result
GetRenderEntityBasisPos(render_transform* Transform, v3 OriginalPos)
{
	entity_basis_p_result Result = {};
	v3 Pos = OriginalPos + Transform->OffsetPos;

	if (Transform->Orthographic)
	{
		Result.Pos = Transform->ScreenCenter + Transform->MetersToPixels * Pos.xy;
		Result.Scale = Transform->MetersToPixels;
		Result.Valid = true;
	}
	else
	{
		f32 DistanceAboveTarget = Transform->DistanceAboveTarget;

#if 0
		// TODO: How do we want to control the debug camera?
		if (1)
		{
			DistanceAboveTarget += 50.0f;
		}
#endif

		f32 DistanceToPosZ = DistanceAboveTarget - Pos.z;
		f32 NearClipPlane = 0.2f;

		v3 RawXY = ToV3(Pos.xy, 1.0f);

		if (DistanceToPosZ > NearClipPlane)
		{
			v3 ProjectedXY = (1.0f / DistanceToPosZ) * Transform->FocalLength * RawXY;
			Result.Scale = Transform->MetersToPixels * ProjectedXY.z;
			Result.Pos = Transform->ScreenCenter + Transform->MetersToPixels * ProjectedXY.xy;
			Result.Valid = true;
		}
	}

	return(Result);
}

#define PushRenderElement(Group, type) (type*)PushRenderElement_(Group, sizeof(type), render_group_entry_type::##type)
inline void*
PushRenderElement_(render_group* Group, u32 Size, render_group_entry_type Type)
{
	void* Result = 0;

	Size += sizeof(render_group_entry_header);

	if ((Group->PushBufferSize + Size) < Group->MaxPushBufferSize)
	{
		render_group_entry_header* Header = (render_group_entry_header*)(Group->PushBufferBase + Group->PushBufferSize);
		Header->Type = Type;
		Result = (u8*)Header + sizeof(*Header);
		Group->PushBufferSize += Size;
	}
	else
	{
		InvalidCodePath;
	}

	return(Result);
}

inline void
PushBitmap(render_group* Group, loaded_bitmap* Bitmap, v3 Offset, f32 Height, v4 Color = V4(1, 1, 1, 1))
{
	v2 Size = V2(Height * Bitmap->WidthOverHeight, Height);
	v2 Align = Hadamard(Bitmap->AlignPercentage, Size);
	v3 Pos = Offset - ToV3(Align, 0);

	entity_basis_p_result Basis = GetRenderEntityBasisPos(&Group->Transform, Pos);
	if (Basis.Valid)
	{
		render_entry_bitmap* Entry = PushRenderElement(Group, render_entry_bitmap);
		if (Entry)
		{
			Entry->Bitmap = Bitmap;
			Entry->Pos = Basis.Pos;
			Entry->Color = Group->GlobalAlpha * Color;
			Entry->Size = Basis.Scale * Size;
		}
	}
}

inline void
PushBitmap(render_group* Group, bitmap_id ID, v3 Offset, f32 Height, v4 Color = V4(1, 1, 1, 1))
{
	loaded_bitmap* Bitmap = GetBitmap(Group->Assets, ID);
	if (Bitmap)
	{
		PushBitmap(Group, Bitmap, Offset, Height, Color);
	}
	else
	{
		LoadBitmap(Group->Assets, ID);
		++Group->MissingResourceCount;
	}
}

inline void
PushRect(render_group* Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
	v3 Pos = (Offset - ToV3(0.5f * Dim, 0));

	entity_basis_p_result Basis = GetRenderEntityBasisPos(&Group->Transform, Pos);
	if (Basis.Valid)
	{
		render_entry_rectangle* Entry = PushRenderElement(Group, render_entry_rectangle);
		if (Entry)
		{
			Entry->Pos = Basis.Pos;
			Entry->Color = Color;
			Entry->Dim = Basis.Scale * Dim;
		}
	}
}

inline void
PushRectOutline(render_group* Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
	f32 Thickness = 0.1f;

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

inline void
CoordinateSystem(render_group* Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap* Texture, 
	loaded_bitmap* NormalMap, environment_map* Top, environment_map* Middle, environment_map* Bottom)
{
#if 0
	entity_basis_p_result Basis = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenDim);

	if (Basis.Valid)
	{
		Entry = PushRenderElement(Group, render_entry_coordinate_system);
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
	}
#endif
}

internal void
RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget, rectangle2i ClipRect, b32 Even)
{
	BEGIN_TIMED_BLOCK(RenderGroupOutput);

	f32 NullPixelsToMeters = 1.0f;

	for (u32 BaseAddress = 0; BaseAddress < RenderGroup->PushBufferSize;)
	{
		render_group_entry_header* Header = (render_group_entry_header*)(RenderGroup->PushBufferBase + BaseAddress);
		BaseAddress += sizeof(*Header);
		void* Data = (u8*)Header + sizeof(*Header);

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
				Assert(Entry->Bitmap);

#if 0
				// DrawBitmap(OutputTarget, Entry->Bitmap, Pos.x, Pos.y, 1);
				DrawRectangleSlowly(
					OutputTarget, Entry->Pos,
					V2(Entry->Size.x, 0),
					V2(0, Entry->Size.y), Entry->Color, 
					Entry->Bitmap, 0, 0, 0, 0, NullPixelsToMeters
				);
#else
				DrawRectangleQuickly(
					OutputTarget, Entry->Pos, 
					V2(Entry->Size.x, 0), 
					V2(0, Entry->Size.y), Entry->Color, 
					Entry->Bitmap, NullPixelsToMeters, ClipRect, Even
				);
#endif

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_rectangle:
			{
				render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
				DrawRectangle(OutputTarget, Entry->Pos, 
							  Entry->Pos + Entry->Dim, 
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
RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget)
{
	Assert(((uintptr)OutputTarget->Memory & 15) == 0);

	rectangle2i ClipRect = {};
	ClipRect.MinX = 0;
	ClipRect.MaxX = OutputTarget->Width;
	ClipRect.MinY = 0;
	ClipRect.MaxY = OutputTarget->Height;

	tile_render_work Work;
	Work.RenderGroup = RenderGroup;
	Work.OutputTarget = OutputTarget;
	Work.ClipRect = ClipRect;

	DoTiledRenderWork(0, &Work);
}

internal void
TiledRenderGroupToOutput(platform_work_queue* RenderQueue, render_group* RenderGroup, loaded_bitmap* OutputTarget)
{
	/*
		TODO: 

		- Make sure that tiles are all cache-aligned
		- Can we get hyperthreads synced so they do interleaved lines
		- How big should the tiles be for performance?
		- Actually ballpark the memory bandwidth for our DrawRectangleQuickly
		- Re-test some of our instruction choices

	*/

	i32 const TileCountX = 4;
	i32 const TileCountY = 4;
	tile_render_work WorkArray[TileCountX * TileCountY];

	Assert(((uintptr)OutputTarget->Memory & 15) == 0);

	i32 TileWidth = OutputTarget->Width / TileCountX;
	i32 TileHeight = OutputTarget->Height / TileCountY;

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

			if (TileX == (TileCountX - 1))
			{
				ClipRect.MaxX = OutputTarget->Width;
			}

			if (TileY == (TileCountY - 1))
			{
				ClipRect.MaxY = OutputTarget->Height;
			}

			Work->RenderGroup = RenderGroup;
			Work->OutputTarget = OutputTarget;
			Work->ClipRect = ClipRect;

#if 1
			// Multithread
			Platform.AddEntry(RenderQueue, DoTiledRenderWork, Work);
#else
			// Single Thread
			DoTiledRenderWork(RenderQueue, Work);
#endif
		}
	}

	Platform.CompleteAllWork(RenderQueue);
}

internal render_group*
AllocateRenderGroup(game_assets* Assets, memory_arena* Arena, u32 MaxPushBufferSize)
{
	render_group* Result = PushStruct(Arena, render_group);
	
	if (MaxPushBufferSize == 0)
	{
		// TODO: Safe cast from memory_uint to uint32?
		MaxPushBufferSize = (u32)GetArenaSizeRemaining(Arena);
	}
	Result->PushBufferBase = (u8*)PushSize(Arena, MaxPushBufferSize);

	Result->MaxPushBufferSize = MaxPushBufferSize;
	Result->PushBufferSize = 0;

	Result->Assets = Assets;
	Result->GlobalAlpha = 1.0f;

	Result->Transform.OffsetPos = V3(0.0f, 0.0f, 0.0f);
	Result->Transform.Scale = 1.0f;

	Result->MissingResourceCount = 0;

	return(Result);
}

inline void
Perspective(render_group* RenderGroup, i32 PixelWidth, i32 PixelHeight, f32 MetersToPixels, f32 FocalLength, f32 DistanceAboveTarget)
{
	v2 Dim = V2(PixelWidth, PixelHeight);

	// TODO: Need to adjust this based on buffer size
	f32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
	RenderGroup->Transform.ScreenCenter = 0.5f * Dim;
	RenderGroup->MonitorHalfDimInMeters = PixelsToMeters * RenderGroup->Transform.ScreenCenter;

	RenderGroup->Transform.MetersToPixels = MetersToPixels;
	RenderGroup->Transform.FocalLength = FocalLength; // NOTE: Meters the person is sitting from their monitor
	RenderGroup->Transform.DistanceAboveTarget = DistanceAboveTarget;

	RenderGroup->Transform.Orthographic = false;
}

inline void
Orthographic(render_group* RenderGroup, i32 PixelWidth, i32 PixelHeight, f32 MetersToPixels)
{
	v2 Dim = V2(PixelWidth, PixelHeight);

	// TODO: Need to adjust this based on buffer size
	f32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
	RenderGroup->Transform.ScreenCenter = 0.5f * Dim;
	RenderGroup->MonitorHalfDimInMeters = PixelsToMeters * RenderGroup->Transform.ScreenCenter;

	RenderGroup->Transform.MetersToPixels = MetersToPixels;
	RenderGroup->Transform.FocalLength = 1.0f; // NOTE: Meters the person is sitting from their monitor
	RenderGroup->Transform.DistanceAboveTarget = 1.0f;

	RenderGroup->Transform.Orthographic = true;
}

inline v2
Unproject(render_group* Group, v2 ProjectedXY, f32 AtDistanceFromCamera)
{
	v2 WorldXY = (AtDistanceFromCamera / Group->Transform.FocalLength) * ProjectedXY;
	return(WorldXY);
}

inline rectangle2
GetCameraRectangleAtDistance(render_group* Group, f32 DistanceFromCamera)
{
	v2 RawXY = Unproject(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);

	rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);

	return(Result);
}

inline rectangle2
GetCameraRectangleAtTarget(render_group* Group)
{
	rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->Transform.DistanceAboveTarget);

	return(Result);
}

inline b32
AllResourcesPresent(render_group* Group)
{
	b32 Result = (Group->MissingResourceCount == 0);

	return(Result);
}