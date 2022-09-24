#pragma once

#ifndef ASSET_BUILDER_H
#define ASSET_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include "p5engine_types.h"
#include "p5engine_asset_type_id.h"
#include "p5engine_file_formats.h"

struct bitmap_id
{
	u32 Value;
};

struct sound_id
{
	u32 Value;
};

struct asset_bitmap_info
{
	char* Filename;
	f32 AlignPercentage[2];
};

struct asset_sound_info
{
	char* Filename;
	u32 FirstSampleIndex;
	u32 SampleCount;
	sound_id NextIDToPlay;
};

struct asset
{
	u64 DataOffset;
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;

	union
	{
		asset_bitmap_info Bitmap;
		asset_sound_info Sound;
	};
};

struct asset_type
{
	u32 FirstAssetIndex;
	u32 OnePastLastAssetIndex;
};

struct bitmap_asset
{
	char* Filename;
	f32 Alignment[2];
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
	u32 TagCount;
	p5a_tag Tags[VERY_LARGE_NUMBER];

	u32 AssetTypeCount;
	p5a_asset_type AssetTypes[(u32)asset_type_id::Count];

	u32 AssetCount;
	asset Assets[VERY_LARGE_NUMBER];

	p5a_asset_type* DEBUGAssetType;
	asset* DEBUGAsset;
};


#endif // !ASSET_BUILDER_H
