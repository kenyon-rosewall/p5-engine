#pragma once

#ifndef P5ENGINE_RENDER_GROUP_H
#define P5ENGINE_RENDER_GROUP_H

struct render_basis
{
	v3 Pos;
};

struct entity_visible_piece
{
	render_basis* Basis;
	loaded_bitmap* Bitmap;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;

	real32 R, G, B, A;
	v2 Dim;
};

struct render_group
{
	render_basis* DefaultBasis;
	real32 MetersToPixels;
	uint32 PieceCount;

	uint32 MaxPushBufferSize;
	uint32 PushBufferSize;
	uint8* PushBufferBase;
};

inline void*
PushRenderElement(render_group* Group, uint32 Size)
{
	void* Result = 0;

	if ((Group->PushBufferSize + Size) < Group->MaxPushBufferSize)
	{
		Result = (Group->PushBufferBase + Group->PushBufferSize);
		Group->PushBufferSize += Size;
	}
	else
	{
		InvalidCodePath;
	}

	return(Result);
}

inline void
PushPiece(render_group* Group, loaded_bitmap* Bitmap, v2 Offset, v2 Align,
	real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC)
{
	entity_visible_piece* Piece = (entity_visible_piece*)PushRenderElement(Group, sizeof(entity_visible_piece));
	Piece->Basis = Group->DefaultBasis;
	Piece->Bitmap = Bitmap;
	Piece->Offset = Group->MetersToPixels * V2(Offset.X, -Offset.Y) - Align;
	Piece->OffsetZ = OffsetZ;
	Piece->Dim = Dim;
	Piece->R = Color.R;
	Piece->G = Color.G;
	Piece->B = Color.B;
	Piece->A = Color.A;
	Piece->EntityZC = EntityZC;
}

inline void
PushBitmap(render_group* Group, loaded_bitmap* Bitmap, v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f, real32 EntityZC = 1.0f)
{
	v2 Dim = V2i(Bitmap->Width, Bitmap->Height);
	v4 Color = V4(1.0f, 1.0f, 1.0f, Alpha);
	PushPiece(Group, Bitmap, Offset, Align, OffsetZ, Dim, Color, EntityZC);
}

inline void
PushRect(render_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	PushPiece(Group, 0, Offset, V2(0, 0), OffsetZ, Dim, Color, EntityZC);
}

inline void
PushRectOutline(render_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	real32 Thickness = 0.1f;

	// NOTE: Top and bottom
	PushPiece(Group, 0, Offset - V2(0, 0.5f * Dim.Y), V2(0, 0), OffsetZ, V2(Dim.X, Thickness), Color, EntityZC);
	PushPiece(Group, 0, Offset + V2(0, 0.5f * Dim.Y), V2(0, 0), OffsetZ, V2(Dim.X, Thickness), Color, EntityZC);

	// NOTE: Left and right
	PushPiece(Group, 0, Offset - V2(0.5f * Dim.X, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.Y), Color, EntityZC);
	PushPiece(Group, 0, Offset + V2(0.5f * Dim.X, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.Y), Color, EntityZC);
}

#endif // !P5ENGINE_RENDER_GROUP_H