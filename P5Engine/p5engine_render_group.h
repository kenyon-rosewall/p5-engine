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

// TODO: Remove the header
enum class render_group_entry_type
{
	render_entry_clear,
	render_entry_bitmap,
	render_entry_rectangle,
	render_entry_coordinate_system
};

struct render_group_entry_header
{
	render_group_entry_type Type;
};

struct render_entry_clear
{
	render_group_entry_header Header;
	v4 Color;
};

struct render_entry_coordinate_system
{
	render_group_entry_header Header;
	v2 Origin;
	v2 XAxis;
	v2 YAxis;
	v4 Color;
	loaded_bitmap* Texture;

	v2 Points[16];
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