#pragma once

#ifndef ASSET_BUILDER_H
#define ASSET_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "p5engine_types.h"
#include "p5engine_file_formats.h"
#include "p5engine_intrinsics.h"
#include "p5engine_math.h"

enum class asset_type
{
	Sound,
	Bitmap,
	Font,
	FontGlyph,
};

struct loaded_font;
struct asset_source_font
{
	loaded_font* Font;
};

struct asset_source_font_glyph
{
	loaded_font* Font;
	u32 Codepoint;
};

struct asset_source_bitmap
{
	char* Filename;
};

struct asset_source_sound
{
	char* Filename;
	u32 FirstSampleIndex;
};

struct asset_source
{
	asset_type Type;
	union
	{
		asset_source_bitmap Bitmap;
		asset_source_sound Sound;
		asset_source_font Font;
		asset_source_font_glyph Glyph;
	};
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
	u32 TagCount;
	p5a_tag Tags[VERY_LARGE_NUMBER];

	u32 AssetTypeCount;
	p5a_asset_type AssetTypes[(u32)asset_type_id::Count];

	u32 AssetCount;
	asset_source AssetSources[VERY_LARGE_NUMBER];
	p5a_asset Assets[VERY_LARGE_NUMBER];

	p5a_asset_type* DEBUGAssetType;
	u32 AssetIndex;
};


#endif // !ASSET_BUILDER_H
