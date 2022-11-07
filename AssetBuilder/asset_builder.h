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

#define USE_FONTS_FROM_WINDOWS 1

#define ONE_PAST_MAX_FONT_CODEPOINT (0x10ffff + 1)

#if USE_FONTS_FROM_WINDOWS
#include <Windows.h>

#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024

global_variable VOID* GlobalFontBits = 0;
global_variable HDC GlobalFontDeviceContext = 0;

#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

struct loaded_bitmap
{
	i32 Width;
	i32 Height;
	i32 Pitch;
	void* Memory;

	void* Free;
};

struct loaded_font
{
	HFONT Win32Handle;
	TEXTMETRIC TextMetric;
	f32 LineAdvance;

	p5a_font_glyph* Glyphs;
	f32* HorizontalAdvance;

	u32 MinCodepoint;
	u32 MaxCodepoint;

	u32 MaxGlyphCount;
	u32 GlyphCount;

	u32 OnePastHighestCodepoint;
	u32 *GlyphIndexFromCodepoint;
};

enum class asset_type
{
	Sound,
	Bitmap,
	Font,
	FontGlyph,
};

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
