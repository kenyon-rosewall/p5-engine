#pragma once

#ifndef P5ENGINE_ASSET_H
#define P5ENGINE_ASSET_H

#include "p5engine_asset_type_id.h"

struct bitmap_id
{
	u32 Value;
};

struct sound_id
{
	u32 Value;
};

struct hero_bitmaps
{
	loaded_bitmap Character;
};

struct loaded_sound
{
	u32 SampleCount; // NOTE: This is the sample count divided by 8
	u32 ChannelCount;
	i16* Samples[2];
};

enum class asset_state
{
	Unloaded,
	Queued,
	Loaded,
	Locked
};

struct asset_bitmap_info
{
	char* Filename;
	v2 AlignPercentage;
};

struct asset_sound_info
{
	char* Filename;
	u32 FirstSampleIndex;
	u32 SampleCount;
	sound_id NextIDToPlay;
};

struct asset_tag
{
	asset_tag_id ID;
	f32 Value;
};

struct asset_slot
{
	asset_state State;
	union
	{
		loaded_bitmap* Bitmap;
		loaded_sound* Sound;
	};
};

struct asset
{
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;

	union
	{
		asset_bitmap_info Bitmap;
		asset_sound_info Sound;
	};
};

struct asset_vector
{
	f32 E[(u32)asset_tag_id::Count];
};

struct asset_type
{
	u32 FirstAssetIndex;
	u32 OnePastLastAssetIndex;
};

struct game_assets
{
	// TODO: Not crazy about this back pointer
	struct transient_state* TransientState;
	memory_arena Arena;

	f32 TagRange[(u32)asset_tag_id::Count];

	u32 TagCount;
	asset_tag* Tags;

	u32 AssetCount;
	asset* Assets;
	asset_slot* Slots;

	asset_type AssetTypes[(u32)asset_type_id::Count];

	// TODO: These should go away once we actually load an asset pack file
	u32 DEBUGUsedAssetCount;
	u32 DEBUGUsedTagCount;
	asset_type* DEBUGAssetType;
	asset* DEBUGAsset;
};

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	loaded_bitmap* Result = Assets->Slots[ID.Value].Bitmap;

	return(Result);
}

inline loaded_sound* GetSound(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	loaded_sound* Result = Assets->Slots[ID.Value].Sound;

	return(Result);
}

inline asset_sound_info* 
GetSoundInfo(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	asset_sound_info* Result = &Assets->Assets[ID.Value].Sound;

	return(Result);
}

inline b32
IsValid(bitmap_id ID)
{
	b32 Result = (ID.Value != 0);

	return(Result);
}

inline b32
IsValid(sound_id ID)
{
	b32 Result = (ID.Value != 0);

	return(Result);
}

internal void LoadBitmap(game_assets* Assets, bitmap_id ID);
internal void LoadSound(game_assets* Assets, sound_id ID);

#endif // !P5ENGINE_ASSET_H