
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
DrawRectangle(loaded_bitmap* Buffer, v2 vMin, v2 vMax, v4 Color)
{
	real32 R = Color.r;
	real32 G = Color.g;
	real32 B = Color.b;
	real32 A = Color.a;

	int32 MinX = RoundReal32ToInt32(vMin.x);
	int32 MinY = RoundReal32ToInt32(vMin.y);
	int32 MaxX = RoundReal32ToInt32(vMax.x);
	int32 MaxY = RoundReal32ToInt32(vMax.y);

	if (MinX < 0)
	{
		MinX = 0;
	}

	if (MinY < 0)
	{
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

	uint32 Color32 = ((RoundReal32ToUInt32(A * 255.0f) << 24) |
					  (RoundReal32ToUInt32(R * 255.0f) << 16) | 
					  (RoundReal32ToUInt32(G * 255.0f) << 8) | 
					  (RoundReal32ToUInt32(B * 255.0f) << 0));

	uint8* Row = ((uint8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);

	for (int y = MinY; y < MaxY; ++y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int x = MinX; x < MaxX; ++x)
		{
			*Pixel++ = Color32;
		}

		Row += Buffer->Pitch;
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
	// TODO: 

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
					  (RoundReal32ToUInt32(Color.g * 255.0f) <<  8) |
					  (RoundReal32ToUInt32(Color.b * 255.0f) <<  0));

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

	for (int Y = YMin; Y < YMax; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int X = XMin; X < XMax; ++X)
		{
#if 1
			v2 PixelPos = V2i(X, Y);
			v2 d = PixelPos - Origin;

			real32 Edge0 = Inner(d, -Perp(XAxis));
			real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
			real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
			real32 Edge3 = Inner(d - YAxis, Perp(YAxis));

			if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0))
			{
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
						  ((uint32)(Blended255.g + 0.5f) <<  8) |
						  ((uint32)(Blended255.b + 0.5f) <<  0));
			}
#else
			*Pixel = Color32;
#endif
			++Pixel;
		}

		Row += Buffer->Pitch;
	}
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
RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget)
{
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

				DrawRectangle(OutputTarget, V2(0, 0), V2i(OutputTarget->Width, OutputTarget->Height), Entry->Color);

				BaseAddress += sizeof(*Entry);
			} break;
			
			case render_group_entry_type::render_entry_bitmap:
			{
				render_entry_bitmap* Entry = (render_entry_bitmap*)Data;

				entity_basis_p_result Basis = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenDim);
				Assert(Entry->Bitmap);
#if 0
				DrawBitmap(OutputTarget, Entry->Bitmap, Pos.x, Pos.y, 1);
#else
				DrawRectangleSlowly(
					OutputTarget, Basis.Pos, 
					Basis.Scale * V2(Entry->Size.x, 0), 
					Basis.Scale * V2(0, Entry->Size.y), 
					Entry->Color, Entry->Bitmap, 
					0, 0, 0, 0, PixelsToMeters
				);
#endif

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_rectangle:
			{
				render_entry_rectangle* Entry = (render_entry_rectangle*)Data;
				entity_basis_p_result Basis = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenDim);
				DrawRectangle(OutputTarget, Basis.Pos, Basis.Pos + Basis.Scale * Entry->Dim, Entry->Color);

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_coordinate_system:
			{
				render_entry_coordinate_system* Entry = (render_entry_coordinate_system*)Data;
				
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
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color);

				Pos = Entry->Origin + Entry->XAxis;
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color);

				Pos = Entry->Origin + Entry->YAxis;
				DrawRectangle(OutputTarget, Pos - Dim, Pos + Dim, Color);

				DrawRectangle(OutputTarget, vMax - Dim, vMax + Dim, Color);

#if 0
				for (uint32 PointIndex = 0; PointIndex < ArrayCount(Entry->Points); ++PointIndex)
				{
					v2 P = Entry->Points[PointIndex];
					P = Entry->Origin + P.x * Entry->XAxis + P.y * Entry->YAxis;
					DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
				}
#endif

				BaseAddress += sizeof(*Entry);
			} break;

			InvalidDefaultCase;
		}
	}
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
	Result->RenderCamera.DistanceAboveTarget = 30.0f;

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