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
	// TODO: This could be shrunk to 12 bytes if the loaded_bitmap
	// ever got down that small
	i16* Samples[2];
	u32 SampleCount; // NOTE: This is the sample count divided by 8
	u32 ChannelCount;
};

enum asset_state
{
	AssetState_Unloaded,
	AssetState_Queued,
	AssetState_Loaded,
	AssetState_Locked,
	AssetState_StateMask = 0xFFF,

	AssetState_Sound = 0x1000,
	AssetState_Bitmap = 0x2000,
	AssetState_TypeMask = 0xF000,
};

struct asset_slot
{
	u32 State;
	union
	{
		loaded_bitmap Bitmap;
		loaded_sound Sound;
	};
};

struct asset
{
	p5a_asset P5A;
	u32 FileIndex;
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

struct asset_memory_header
{
	asset_memory_header* Next;
	asset_memory_header* Prev;
	u32 SlotIndex;
	u32 Reserved;
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

	u64 TargetMemoryUsed;
	u64 TotalMemoryUsed;
	asset_memory_header LoadedAssetSentinel;

	f32 TagRange[(u32)asset_tag_id::Count];

	u32 FileCount;
	asset_file* Files;

	u32 TagCount;
	p5a_tag* Tags;

	u32 AssetCount;
	asset* Assets;
	asset_slot* Slots;

	asset_type AssetTypes[(u32)asset_type_id::Count];
};

inline u32
GetState(asset_slot* Slot)
{
	u32 Result = Slot->State & AssetState_StateMask;

	return(Result);
}

inline u32
GetType(asset_slot* Slot)
{
	u32 Result = Slot->State & AssetState_TypeMask;

	return(Result);
}

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID, b32 MustBeLocked)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset_slot* Slot = Assets->Slots + ID.Value;
	loaded_bitmap* Result = 0;
	if (GetState(Slot) >= AssetState_Loaded)
	{
		Assert(!MustBeLocked || (GetState(Slot) == AssetState_Locked));
		CompletePreviousReadsBeforeFutureReads;
		Result = &Slot->Bitmap;
	}

	return(Result);
}

inline loaded_sound* GetSound(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset_slot* Slot = Assets->Slots + ID.Value;
	loaded_sound* Result = 0;
	if (GetState(Slot) >= AssetState_Loaded)
	{
		CompletePreviousReadsBeforeFutureReads;
		Result = &Slot->Sound;
	}

	return(Result);
}

inline p5a_sound* 
GetSoundInfo(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	p5a_sound* Result = &Assets->Assets[ID.Value].P5A.Sound;

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

internal void LoadBitmap(game_assets* Assets, bitmap_id ID, b32 Locked);
internal void LoadSound(game_assets* Assets, sound_id ID);

inline sound_id
GetNextSoundInChain(game_assets* Assets, sound_id ID)
{
	sound_id Result = {};

	p5a_sound* Info = GetSoundInfo(Assets, ID);
	switch (Info->Chain)
	{
		case p5a_sound_chain::None:
		{
			// NOTE: Nothing to do
		} break;

		case p5a_sound_chain::Loop:
		{
			Result = ID;
		} break;

		case p5a_sound_chain::Advance:
		{
			Result.Value = ID.Value + 1;
		} break;

		InvalidDefaultCase;
	}

	return(Result);
}

#endif // !P5ENGINE_ASSET_H