
internal void
DrawRectangle(loaded_bitmap* Buffer, v2 vMin, v2 vMax, real32 r, real32 g, real32 b)
{
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

	uint32 Color = ((RoundReal32ToUInt32(r * 255.0f) << 16) | (RoundReal32ToUInt32(g * 255.0f) << 8) | (RoundReal32ToUInt32(b * 255.0f) << 0));

	uint8* Row = ((uint8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);

	for (int y = MinY; y < MaxY; ++y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int x = MinX; x < MaxX; ++x)
		{
			*Pixel++ = Color;
		}

		Row += Buffer->Pitch;
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
			real32 A = 255.0f * (RSA + RDA - RSA * RDA);
			real32 r = InvRSA * DR + SR;
			real32 g = InvRSA * DG + SG;
			real32 b = InvRSA * DB + SB;

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
inline render_group_entry_header*
PushRenderElement_(render_group* Group, uint32 Size, render_group_entry_type Type)
{
	render_group_entry_header* Result = 0;

	if ((Group->PushBufferSize + Size) < Group->MaxPushBufferSize)
	{
		Result = (render_group_entry_header*)(Group->PushBufferBase + Group->PushBufferSize);
		Result->Type = Type;
		Group->PushBufferSize += Size;
	}
	else
	{
		InvalidCodePath;
	}

	return(Result);
}

inline void
PushEntry(render_group* Group, loaded_bitmap* Bitmap, v2 Offset, v2 Align,
	real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC)
{
	render_entry_bitmap* Entry = PushRenderElement(Group, render_entry_bitmap);
	if (Entry)
	{
		Entry->Bitmap = Bitmap;
		Entry->EntityBasis.Basis = Group->DefaultBasis;
		Entry->EntityBasis.Offset = Group->MetersToPixels * V2(Offset.x, -Offset.y) - Align;
		Entry->EntityBasis.OffsetZ = OffsetZ;
		Entry->EntityBasis.EntityZC = EntityZC;
		Entry->r = Color.r;
		Entry->g = Color.g;
		Entry->b = Color.b;
		Entry->a = Color.a;
	}
}

inline void
PushBitmap(render_group* Group, loaded_bitmap* Bitmap, v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f, real32 EntityZC = 1.0f)
{
	v2 Dim = V2i(Bitmap->Width, Bitmap->Height);
	v4 Color = V4(1.0f, 1.0f, 1.0f, Alpha);
	PushEntry(Group, Bitmap, Offset, Align, OffsetZ, Dim, Color, EntityZC);
}

inline void
PushRect(render_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	render_entry_rectangle* Entry = PushRenderElement(Group, render_entry_rectangle);
	if (Entry)
	{
		v2 HalfDim = 0.5f * Group->MetersToPixels * Dim;
		
		Entry->EntityBasis.Basis = Group->DefaultBasis;
		Entry->EntityBasis.Offset = Group->MetersToPixels * V2(Offset.x, -Offset.y) - HalfDim;
		Entry->EntityBasis.OffsetZ = OffsetZ;
		Entry->EntityBasis.EntityZC = EntityZC;
		Entry->Dim = Group->MetersToPixels * Dim;
		Entry->r = Color.r;
		Entry->g = Color.g;
		Entry->b = Color.b;
		Entry->a = Color.a;
	}
}

inline void
PushRectOutline(render_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	real32 Thickness = 0.1f;

	// NOTE: Top and bottom
	PushEntry(Group, 0, Offset - V2(0, 0.5f * Dim.y), V2(0, 0), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);
	PushEntry(Group, 0, Offset + V2(0, 0.5f * Dim.y), V2(0, 0), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);

	// NOTE: Left and right
	PushEntry(Group, 0, Offset - V2(0.5f * Dim.x, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
	PushEntry(Group, 0, Offset + V2(0.5f * Dim.x, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
}

inline v2
GetRenderEntityBasisPos(render_group* RenderGroup, render_entity_basis* EntityBasis, v2 ScreenCenter)
{
	v3 EntityBasePos = EntityBasis->Basis->Pos;
	real32 ZFudge = (1.0f + 0.1f * (EntityBasePos.z + EntityBasis->OffsetZ));

	real32 EntityGroundPointX = ScreenCenter.x + RenderGroup->MetersToPixels * ZFudge * EntityBasePos.x;
	real32 EntityGroundPointY = ScreenCenter.y - RenderGroup->MetersToPixels * ZFudge * EntityBasePos.y;
	real32 EntityZ = -RenderGroup->MetersToPixels * EntityBasePos.z;

	v2 Center = V2(
		EntityGroundPointX + EntityBasis->Offset.x,
		EntityGroundPointY + EntityBasis->Offset.y + EntityBasis->EntityZC * EntityZ
	);

	return(Center);
}

internal void
RenderGroupToOutput(render_group* RenderGroup, loaded_bitmap* OutputTarget)
{
	for (uint32 BaseAddress = 0; BaseAddress < RenderGroup->PushBufferSize;)
	{
		v2 ScreenCenter = V2(
			0.5f * (real32)OutputTarget->Width,
			0.5f * (real32)OutputTarget->Height
		);
		render_group_entry_header* Header = (render_group_entry_header*)(RenderGroup->PushBufferBase + BaseAddress);

		switch (Header->Type)
		{
			case render_group_entry_type::render_entry_clear:
			{
				render_entry_clear* Entry = (render_entry_clear*)Header;

				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_bitmap:
			{
				render_entry_bitmap* Entry = (render_entry_bitmap*)Header;
				v2 Pos = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenCenter);
				Assert(Entry->Bitmap);
				DrawBitmap(OutputTarget, Entry->Bitmap, Pos.x, Pos.y, Entry->a);
				
				BaseAddress += sizeof(*Entry);
			} break;

			case render_group_entry_type::render_entry_rectangle:
			{
				render_entry_rectangle* Entry = (render_entry_rectangle*)Header;
				v2 Pos = GetRenderEntityBasisPos(RenderGroup, &Entry->EntityBasis, ScreenCenter);
				DrawRectangle(OutputTarget, Pos, Pos + Entry->Dim, Entry->r, Entry->g, Entry->b);

				BaseAddress += sizeof(*Entry);
			} break;

			InvalidDefaultCase;
		}
	}
}

internal render_group*
AllocateRenderGroup(memory_arena* Arena, uint32 MaxPushBufferSize, real32 MetersToPixels)
{
	render_group* Result = PushStruct(Arena, render_group);
	Result->PushBufferBase = (uint8*)PushSize(Arena, MaxPushBufferSize);

	Result->DefaultBasis = PushStruct(Arena, render_basis);
	Result->DefaultBasis->Pos = V3(0, 0, 0);
	Result->MetersToPixels = MetersToPixels;

	Result->MaxPushBufferSize = MaxPushBufferSize;
	Result->PushBufferSize = 0;

	return(Result);
}