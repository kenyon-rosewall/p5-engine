#pragma once

#ifndef P5ENGINE_RENDER_GROUP_H
#define P5ENGINE_RENDER_GROUP_H

struct render_basis
{
	v3 Pos;
};

struct render_entity_basis
{
	render_basis* Basis;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;
};

enum class render_group_entry_type
{
	render_entry_clear,
	render_entry_bitmap,
	render_entry_rectangle
};

struct render_group_entry_header
{
	render_group_entry_type Type;
};

struct render_entry_clear
{
	render_group_entry_header Header;
	real32 r, g, b, A;
};

struct render_entry_bitmap
{
	render_group_entry_header Header;
	render_entity_basis EntityBasis;
	loaded_bitmap* Bitmap;
	real32 r, g, b, a;
};

struct render_entry_rectangle
{
	render_group_entry_header Header;
	render_entity_basis EntityBasis;
	v2 Dim;
	real32 r, g, b, a;
};

struct render_group
{
	render_basis* DefaultBasis;
	real32 MetersToPixels;

	uint32 MaxPushBufferSize;
	uint32 PushBufferSize;
	uint8* PushBufferBase;
};

#endif // !P5ENGINE_RENDER_GROUP_H