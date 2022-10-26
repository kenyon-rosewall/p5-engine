#pragma once

#ifndef P5ENGINE_ASSET_H
#define P5ENGINE_ASSET_H

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

// TODO: Streamline this, by using header pointer as an indicator of 
// unloaded status?
enum asset_state
{
	AssetState_Unloaded,
	AssetState_Queued,
	AssetState_Loaded,
};

struct asset_memory_header
{
	asset_memory_header* Next;
	asset_memory_header* Prev;

	u32 AssetIndex;
	u32 TotalSize;
	u32 GenerationID;
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
	u32 NextGenerationID;

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

	u32 OperationLock;

	u32 InFlightGenerationCount;
	u32 InFlightGenerations[16];
};

inline void
BeginAssetLock(game_assets* Assets)
{
	for (;;)
	{
		if (AtomicCompareExchangeUInt32(&Assets->OperationLock, 1, 0) == 0)
		{
			break;
		}
	}
}

inline void
EndAssetLock(game_assets* Assets)
{
	CompletePreviousWritesBeforeFutureWrites;
	Assets->OperationLock = 0;
}

inline void
InsertAssetHeaderAtFront(game_assets* Assets, asset_memory_header* Header)
{
	asset_memory_header* Sentinel = &Assets->LoadedAssetSentinel;

	Header->Prev = Sentinel;
	Header->Next = Sentinel->Next;

	Header->Next->Prev = Header;
	Header->Prev->Next = Header;
}

internal void
RemoveAssetHeaderFromList(asset_memory_header* Header)
{
	Header->Prev->Next = Header->Next;
	Header->Next->Prev = Header->Prev;

	Header->Next = Header->Prev = 0;
}

inline asset_memory_header*
GetAsset(game_assets* Assets, u32 ID, u32 GenerationID)
{
	Assert(ID <= Assets->AssetCount);

	asset* Asset = Assets->Assets + ID;
	asset_memory_header* Result = 0;

	BeginAssetLock(Assets);

	if (Asset->State == AssetState_Loaded)
	{
		Result = Asset->Header;
		RemoveAssetHeaderFromList(Result);
		InsertAssetHeaderAtFront(Assets, Result);

		if (Asset->Header->GenerationID < GenerationID)
		{
			Asset->Header->GenerationID = GenerationID;
		}

		CompletePreviousWritesBeforeFutureWrites;
	}

	EndAssetLock(Assets);

	return(Result);
}

inline loaded_bitmap*
GetBitmap(game_assets* Assets, bitmap_id ID, u32 GenerationID)
{
	asset_memory_header* Header = GetAsset(Assets, ID.Value, GenerationID);

	loaded_bitmap* Result = Header ? &Header->Bitmap : 0;

	return(Result);
}

inline p5a_bitmap*
GetBitmapInfo(game_assets* Assets, bitmap_id ID)
{
	Assert(ID.Value <= Assets->AssetCount);
	p5a_bitmap* Result = &Assets->Assets[ID.Value].P5A.Bitmap;

	return(Result);
}

inline loaded_sound*
GetSound(game_assets* Assets, sound_id ID, u32 GenerationID)
{
	asset_memory_header* Header = GetAsset(Assets, ID.Value, GenerationID);

	loaded_sound* Result = Header ? &Header->Sound : 0;

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

internal void LoadBitmap(game_assets* Assets, bitmap_id ID, b32 Immediate);
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

inline u32
BeginGeneration(game_assets* Assets)
{
	BeginAssetLock(Assets);

	Assert(Assets->InFlightGenerationCount < ArrayCount(Assets->InFlightGenerations));
	u32 Result = Assets->NextGenerationID++;
	Assets->InFlightGenerations[Assets->InFlightGenerationCount++] = Result;

	EndAssetLock(Assets);

	return(Result);
}

inline void
EndGeneration(game_assets* Assets, u32 GenerationID)
{
	BeginAssetLock(Assets);

	for (u32 Index = 0;
		 Index < Assets->InFlightGenerationCount;
		 ++Index)
	{
		if (Assets->InFlightGenerations[Index] == GenerationID)
		{
			Assets->InFlightGenerations[Index] = Assets->InFlightGenerations[--Assets->InFlightGenerationCount];
			break;
		}
	}

	EndAssetLock(Assets);
}

#endif // !P5ENGINE_ASSET_H