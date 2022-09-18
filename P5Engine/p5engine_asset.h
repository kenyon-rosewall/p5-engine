#pragma once

#ifndef P5ENGINE_ASSET_H
#define P5ENGINE_ASSET_H

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
	u32 SampleCount;
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

enum class asset_tag_id
{
	Smoothness,
	Flatness,
	FacingDirection,

	Count,
};

enum class asset_type_id
{
	None,

	//
	// NOTE: Bitmaps
	//

	Shadow,
	Tree,
	Monstar,
	Familiar,
	Grass,

	Soil,
	Tuft,

	Character,
	Sword,

	//
	// NOTE: Sounds
	//

	Bloop,
	Crack,
	Drop,
	Glide,
	Music,
	Puhp,

	Count,
};

struct asset_tag
{
	asset_tag_id ID;
	f32 Value;
};

struct asset
{
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;
	u32 SlotID;
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

struct game_assets
{
	// TODO: Not crazy about this back pointer
	struct transient_state* TransientState;
	memory_arena Arena;

	f32 TagRange[(u32)asset_tag_id::Count];

	u32 BitmapCount;
	asset_bitmap_info* BitmapInfos;
	asset_slot* Bitmaps;

	u32 SoundCount;
	asset_sound_info* SoundInfos;
	asset_slot* Sounds;

	u32 TagCount;
	asset_tag* Tags;

	u32 AssetCount;
	asset* Assets;

	asset_type AssetTypes[(u32)asset_type_id::Count];
	
	// NOTE: Structured assets
	hero_bitmaps Hero[4];
	loaded_bitmap Sword[4];

	// TODO: These should go away once we actually load an asset pack file
	u32 DEBUGUsedBitmapCount;
	u32 DEBUGUsedSoundCount;
	u32 DEBUGUsedAssetCount;
	u32 DEBUGUsedTagCount;
	asset_type* DEBUGAssetType;
	asset* DEBUGAsset;
};

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID)
{
	Assert(ID.Value <= Assets->BitmapCount);
	loaded_bitmap* Result = Assets->Bitmaps[ID.Value].Bitmap;

	return(Result);
}

inline loaded_sound* GetSound(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->SoundCount);
	loaded_sound* Result = Assets->Sounds[ID.Value].Sound;

	return(Result);
}

inline asset_sound_info* 
GetSoundInfo(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->SoundCount);
	asset_sound_info* Result = Assets->SoundInfos + ID.Value;

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