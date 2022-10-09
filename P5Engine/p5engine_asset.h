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
	i16* Samples[2];
	u32 SampleCount; // NOTE: This is the sample count divided by 8
	u32 ChannelCount;
};

enum asset_state
{
	AssetState_Unloaded,
	AssetState_Queued,
	AssetState_Loaded,
	AssetState_StateMask = 0xFFF,

	AssetState_Lock = 0x1000,
};

struct asset_memory_header
{
	asset_memory_header* Next;
	asset_memory_header* Prev;

	u32 AssetIndex;
	u32 TotalSize;
	union
	{
		loaded_bitmap Bitmap;
		loaded_sound Sound;
	};
};

struct asset
{
	u32 State;
	asset_memory_header* Header;

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

struct asset_file
{
	platform_file_handle Handle;

	// TODO: If we ever do thread stacks, AssetTypeArray doesn't
	// actually need to be kept here probably.
	p5a_header Header;
	p5a_asset_type* AssetTypeArray;

	u32 TagBase;
};

enum asset_memory_block_flags
{
	AssetMemory_Used = 0x1,
};

struct asset_memory_block
{
	asset_memory_block* Prev;
	asset_memory_block* Next;
	u64 Flags;
	memory_index Size;
};

struct game_assets
{
	// TODO: Not crazy about this back pointer
	struct transient_state* TransientState;

	asset_memory_block MemorySentinel;
	
	asset_memory_header LoadedAssetSentinel;

	f32 TagRange[(u32)asset_tag_id::Count];

	u32 FileCount;
	asset_file* Files;

	u32 TagCount;
	p5a_tag* Tags;

	u32 AssetCount;
	asset* Assets;

	asset_type AssetTypes[(u32)asset_type_id::Count];
};

inline b32
IsLocked(asset* Asset)
{
	b32 Result = (Asset->State & AssetState_Lock);
	return(Result);
}

inline u32
GetState(asset* Asset)
{
	u32 Result = Asset->State & AssetState_StateMask;
	return(Result);
}

internal void MoveHeaderToFront(game_assets* Assets, asset* Asset);

inline loaded_bitmap* GetBitmap(game_assets* Assets, bitmap_id ID, b32 MustBeLocked)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset* Asset = Assets->Assets + ID.Value;
	loaded_bitmap* Result = 0;
	if (GetState(Asset) >= AssetState_Loaded)
	{
		Assert(!MustBeLocked || IsLocked(Asset));
		CompletePreviousReadsBeforeFutureReads;
		Result = &Asset->Header->Bitmap;
		MoveHeaderToFront(Assets, Asset);
	}

	return(Result);
}

inline loaded_sound* GetSound(game_assets* Assets, sound_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);

	asset* Asset = Assets->Assets + ID.Value;
	loaded_sound* Result = 0;
	if (GetState(Asset) >= AssetState_Loaded)
	{
		CompletePreviousReadsBeforeFutureReads;
		Result = &Asset->Header->Sound;
		MoveHeaderToFront(Assets, Asset);
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