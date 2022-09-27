#pragma once

#ifndef P5ENGINE_ASSET_H
#define P5ENGINE_ASSET_H

#include "p5engine_asset_type_id.h"

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

struct asset_slot
{
	asset_state State;
	union
	{
		loaded_bitmap* Bitmap;
		loaded_sound* Sound;
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

struct asset_file
{
	platform_file_handle* Handle;

	// TODO: If we ever do thread stacks, AssetTypeArray doesn't
	// actually need to be kept here probably.
	p5a_header Header;
	p5a_asset_type* AssetTypeArray;

	u32 TagBase;
};

struct game_assets
{
	// TODO: Not crazy about this back pointer
	struct transient_state* TransientState;
	memory_arena Arena;

	f32 TagRange[(u32)asset_tag_id::Count];

	u32 FileCount;
	asset_file* Files;

	u32 TagCount;
	p5a_tag* Tags;

	u32 AssetCount;
	p5a_asset* Assets;
	asset_slot* Slots;

	asset_type AssetTypes[(u32)asset_type_id::Count];

	u8* P5AContents;
};

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset_slot* Slot = Assets->Slots + ID.Value;
	loaded_bitmap* Result = (Slot->State >= asset_state::Loaded) ? Slot->Bitmap : 0;

	return(Result);
}

inline loaded_sound* GetSound(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset_slot* Slot = Assets->Slots + ID.Value;
	loaded_sound* Result = (Slot->State >= asset_state::Loaded) ? Slot->Sound : 0;

	return(Result);
}

inline p5a_sound* 
GetSoundInfo(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	p5a_sound* Result = &Assets->Assets[ID.Value].Sound;

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