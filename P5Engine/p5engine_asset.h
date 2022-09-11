#pragma once

#ifndef P5ENGINE_ASSET_H
#define P5ENGINE_ASSET_H

struct hero_bitmaps
{
	loaded_bitmap Character;
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
	loaded_bitmap* Bitmap;
};

enum class asset_tag_id
{
	Smoothness,
	Flatness,

	Count,
};

enum class asset_type_id
{
	None,

	Backdrop,
	Shadow,
	Tree,
	Stairs,
	Monstar,
	Familiar,

	Count,
};

struct asset_tag
{
	asset_tag_id ID;
	real32 Value;
};

struct asset
{
	uint32 FirstTagIndex;
	uint32 OnePastLastTagIndex;
	uint32 SlotID;
};

struct asset_type
{
	uint32 FirstAssetIndex;
	uint32 OnePastLastAssetIndex;
};

struct asset_bitmap_info
{
	v2 AlignPercentage;
	real32 WidthOverHeight;
	int32 Width;
	int32 Height;
};

struct asset_group
{
	uint32 FirstTagIndex;
	uint32 OnePastLastTagIndex;
};

struct game_assets
{
	// TODO: Not crazy about this back pointer
	struct transient_state* TransientState;
	memory_arena Arena;

	uint32 BitmapCount;
	asset_slot* Bitmaps;

	uint32 SoundCount;
	asset_slot* Sounds;

	uint32 TagCount;
	asset_tag* Tags;

	uint32 AssetCount;
	asset* Assets;

	asset_type AssetTypes[(uint32)asset_type_id::Count];

	// NOTE: Structured assets
	hero_bitmaps Hero[4];
	loaded_bitmap Sword[4];

	// NOTE: Arrayed assets
	loaded_bitmap Grass;
	loaded_bitmap Soil[3];
	loaded_bitmap Tuft[2];
};

struct bitmap_id
{
	uint32 Value;
};

struct audio_id
{
	uint32 Value;
};

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID)
{
	loaded_bitmap* Result = Assets->Bitmaps[ID.Value].Bitmap;

	return(Result);
}

internal void LoadBitmap(game_assets* Assets, bitmap_id ID);
internal void LoadSound(game_assets* Assets, audio_id ID);

#endif // !P5ENGINE_ASSET_H