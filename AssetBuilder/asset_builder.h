#pragma once

#ifndef ASSET_BUILDER_H
#define ASSET_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include "p5engine_types.h"
#include "p5engine_asset_type_id.h"
#include "p5engine_file_formats.h"
#include "p5engine_intrinsics.h"
#include "p5engine_math.h"

enum class asset_type
{
	Sound,
	Bitmap,
};

struct asset_source
{
	asset_type Type;
	char* Filename;
	u32 FirstSampleIndex;
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
