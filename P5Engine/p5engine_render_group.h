#pragma once

#ifndef P5ENGINE_RENDER_GROUP_H
#define P5ENGINE_RENDER_GROUP_H

struct environment_map
{
	// NOTE: LOD[0] is 2^WidthPow2 x 2^HeightPow2
	int32 WidthPow2;
	int32 HeightPow2;
	loaded_bitmap* LOD[4];
};

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
	render_entry_rectangle,
	render_entry_coordinate_system
};

struct render_group_entry_header
{
	render_group_entry_type Type;
};

struct render_entry_clear
{
	v4 Color;
};

struct render_entry_coordinate_system
{
	v2 Origin;
	v2 XAxis;
	v2 YAxis;
	v4 Color;
	loaded_bitmap* Texture;
	loaded_bitmap* NormalMap;

	environment_map* Top;
	environment_map* Middle;
	environment_map* Bottom;
};

struct render_entry_bitmap
{
	render_entity_basis EntityBasis;
	loaded_bitmap* Bitmap;
	real32 r, g, b, a;
};

struct render_entry_rectangle
{
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