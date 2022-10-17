

struct load_asset_work
{
	task_with_memory* Task;
	asset_slot* Slot;

	platform_file_handle* Handle;
	u64 Offset;
	u64 Size;
	void* Destination;

	u32 FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
	load_asset_work* Work = (load_asset_work*)FindData;


	Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

	CompletePreviousWritesBeforeFutureWrites;

	if (!PlatformNoFileErrors(Work->Handle))
	{
		// TODO: Should we actually fill in bogus data here and set to final state anyway?
		ZeroSize(Work->Size, Work->Destination);
	}

	Work->Slot->State = Work->FinalState;
	
	EndTaskWithMemory(Work->Task);
}

inline platform_file_handle*
GetFileHandleFor(game_assets* Assets, u32 FileIndex)
{
	Assert(FileIndex < Assets->FileCount);

	platform_file_handle* Result = Assets->Files[FileIndex].Handle;

	return(Result);
}

inline void*
AcquireAssetMemory(game_assets* Assets, memory_index Size)
{
	void* Result = Platform.AllocateMemory(Size);

	if (Result)
	{
		Assets->TotalMemoryUsed += Size;
	}

	return(Result);
}

inline void
ReleaseAssetMemory(game_assets* Assets, memory_index Size, void* Memory)
{
	if (Memory)
	{
		Assets->TotalMemoryUsed -= Size;
	}

	Platform.DeallocateMemory(Memory);
}

struct asset_memory_size
{
	u32 Total;
	u32 Data;
	u32 Section;
};

asset_memory_size
GetSizeOfAsset(game_assets* Assets, u32 Type, u32 SlotIndex)
{
	asset* Asset = Assets->Assets + SlotIndex;

	asset_memory_size Result = {};

	if (Type == AssetState_Sound)
	{
		p5a_sound* Info = &Asset->P5A.Sound;

		Result.Section = Info->SampleCount * sizeof(i16);
		Result.Data = Info->ChannelCount * Result.Section;
	}
	else
	{
		Assert(Type == AssetState_Bitmap);

		p5a_bitmap* Info = &Asset->P5A.Bitmap;

		u32 Width = SafeTruncateToUInt16(Info->Dim[0]);
		u32 Height = SafeTruncateToUInt16(Info->Dim[1]);
		Result.Section = 4 * Width;
		Result.Data = Result.Section * Height;
	}

	Result.Total = Result.Data + sizeof(asset_memory_header);

	return(Result);
}

internal void
AddAssetHeaderToList(game_assets* Assets, u32 SlotIndex, void* Memory, asset_memory_size Size)
{
	asset_memory_header* Header = (asset_memory_header*)((u8*)Memory + Size.Data);

	asset_memory_header* Sentinel = &Assets->LoadedAssetSentinel;

	Header->SlotIndex = SlotIndex;
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

internal void
LoadBitmap(game_assets* Assets, bitmap_id ID, b32 Locked)
{
	asset_slot* Slot = Assets->Slots + ID.Value;
	if (ID.Value &&
		(AtomicCompareExchangeUInt32((u32*)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			asset* Asset = Assets->Assets + ID.Value;
			p5a_bitmap* Info = &Asset->P5A.Bitmap;
			loaded_bitmap* Bitmap = &Slot->Bitmap;

			Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
			Bitmap->WidthOverHeight = (f32)Info->Dim[0] / (f32)Info->Dim[1];
			Bitmap->Width = SafeTruncateToUInt16(Info->Dim[0]);
			Bitmap->Height = SafeTruncateToUInt16(Info->Dim[1]);

			asset_memory_size Size = GetSizeOfAsset(Assets, AssetState_Bitmap, ID.Value);
			Bitmap->Pitch = SafeTruncateToInt16(Size.Section);
			Bitmap->Memory = AcquireAssetMemory(Assets, Size.Total);

			load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->P5A.DataOffset;
			Work->Size = Size.Data;
			Work->Destination = Bitmap->Memory;
			Work->FinalState = (AssetState_Bitmap | (Locked ? AssetState_Locked : AssetState_Loaded));

			if (!Locked)
			{
				AddAssetHeaderToList(Assets, ID.Value, Bitmap->Memory, Size);
			}

			Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Slot->State = AssetState_Unloaded;
		}
	}
}

internal void
LoadSound(game_assets* Assets, sound_id ID)
{
	asset_slot* Slot = Assets->Slots + ID.Value;
	if (ID.Value &&
		(AtomicCompareExchangeUInt32((u32*)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			asset* Asset = Assets->Assets + ID.Value;
			p5a_sound* Info = &Asset->P5A.Sound;
			loaded_sound* Sound = &Slot->Sound;

			Sound->SampleCount = Info->SampleCount;
			Sound->ChannelCount = Info->ChannelCount;
			
			asset_memory_size Size = GetSizeOfAsset(Assets, AssetState_Sound, ID.Value);
			u32 ChannelSize = Size.Section;
			void* Memory = AcquireAssetMemory(Assets, Size.Total);

			i16* SoundAt = (i16*)Memory;
			for (u32 ChannelIndex = 0;
				 ChannelIndex < Sound->ChannelCount;
				 ++ChannelIndex)
			{
				Sound->Samples[ChannelIndex] = SoundAt;
				SoundAt += ChannelSize;
			}

			load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->P5A.DataOffset;
			Work->Size = Size.Data;
			Work->Destination = Memory;
			Work->FinalState = (AssetState_Sound | AssetState_Loaded);

			AddAssetHeaderToList(Assets, ID.Value, Memory, Size);

			Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Slot->State = AssetState_Unloaded;
		}
	}
}

inline void
PrefetchBitmap(game_assets* Assets, bitmap_id ID, b32 Locked)
{
	LoadBitmap(Assets, ID, Locked);
}

inline void
PrefetchSound(game_assets* Assets, sound_id ID)
{
	LoadSound(Assets, ID);
}

internal u32
GetBestMatchAssetFrom(game_assets* Assets, asset_type_id TypeID, asset_vector* MatchVector, asset_vector* WeightVector)
{
	u32 Result = 0;

	f32 BestDiff = Real32Maximum;
	asset_type* Type = Assets->AssetTypes + (u32)TypeID;
	for (u32 AssetIndex = Type->FirstAssetIndex; AssetIndex < Type->OnePastLastAssetIndex; ++AssetIndex)
	{
		asset* Asset = Assets->Assets + AssetIndex;

		f32 TotalWeightedDiff = 0.0f;
		for (u32 TagIndex = Asset->P5A.FirstTagIndex; TagIndex < Asset->P5A.OnePastLastTagIndex; ++TagIndex)
		{
			p5a_tag* Tag = Assets->Tags + TagIndex;
			
			f32 A = MatchVector->E[(u32)Tag->ID];
			f32 B = Tag->Value;
			f32 D0 = AbsoluteValue(A - B);
			f32 D1 = AbsoluteValue((A - Assets->TagRange[(u32)Tag->ID] * SignOf(A)) - B);
			f32 Difference = Minimum(D0, D1);

			f32 Weighted = WeightVector->E[(u32)Tag->ID] * AbsoluteValue(Difference);
			TotalWeightedDiff += Weighted;
		}

		if (BestDiff > TotalWeightedDiff)
		{
			BestDiff = TotalWeightedDiff;
			Result = AssetIndex;
		}
	}

	return(Result);
}

internal u32
GetRandomAssetFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	u32 Result = 0;

	asset_type* Type = Assets->AssetTypes + (u32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		u32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
		u32 Choice = RandomChoice(Series, Count);
		Result = Type->FirstAssetIndex + Choice;
	}

	return(Result);
}

internal u32
GetFirstAssetFrom(game_assets* Assets, asset_type_id TypeID)
{
	u32 Result = 0;
	
	asset_type* Type = Assets->AssetTypes + (u32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		Result = Type->FirstAssetIndex;
	}

	return(Result);
}

internal bitmap_id
GetBestMatchBitmapFrom(game_assets* Assets, asset_type_id TypeID, asset_vector* MatchVector, asset_vector* WeightVector)
{
	bitmap_id Result = { GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector) };

	return(Result);
}

internal bitmap_id
GetFirstBitmapFrom(game_assets* Assets, asset_type_id TypeID)
{
	bitmap_id Result = { GetFirstAssetFrom(Assets, TypeID) };

	return(Result);
}

internal bitmap_id
GetRandomBitmapFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	bitmap_id Result = { GetRandomAssetFrom(Assets, TypeID, Series) };

	return(Result);
}

inline sound_id
GetBestMatchSoundFrom(game_assets* Assets, asset_type_id TypeID, asset_vector* MatchVector, asset_vector* WeightVector)
{
	sound_id Result = { GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector) };

	return(Result);
}

internal sound_id
GetFirstSoundFrom(game_assets* Assets, asset_type_id TypeID)
{
	sound_id Result = { GetFirstAssetFrom(Assets, TypeID) };

	return(Result);
}

internal sound_id
GetRandomSoundFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	sound_id Result = { GetRandomAssetFrom(Assets, TypeID, Series) };

	return(Result);
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TransientState)
{
	game_assets* Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TransientState = TransientState;
	Assets->TotalMemoryUsed = 0;
	Assets->TargetMemoryUsed = Size;

	Assets->LoadedAssetSentinel.Next = Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

	for (u32 TagType = 0; TagType < (u32)asset_tag_id::Count; ++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[(u32)asset_tag_id::FacingDirection] = Tau32;

	Assets->TagCount = 1;
	Assets->AssetCount = 1;

	{
		platform_file_group* FileGroup = Platform.GetAllFilesOfTypeBegin((char*)"p5a");
		Assets->FileCount = FileGroup->FileCount;
		Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
		for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex)
		{
			asset_file* File = Assets->Files + FileIndex;
			File->TagBase = Assets->TagCount;

			ZeroStruct(File->Header);
			File->Handle = Platform.OpenNextFile(FileGroup);
			Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);

			u32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(p5a_asset_type);
			File->AssetTypeArray = (p5a_asset_type*)PushSize(Arena, AssetTypeArraySize);
			Platform.ReadDataFromFile(File->Handle, File->Header.AssetTypes, AssetTypeArraySize, File->AssetTypeArray);

			if (File->Header.MagicValue != P5A_MAGIC_VALUE)
			{
				Platform.FileError(File->Handle, (char*)"P5A file has an invalid magic value.");
			}

			if (File->Header.Version > P5A_VERSION)
			{
				Platform.FileError(File->Handle, (char*)"P5A file is of a later version.");
			}

			if (PlatformNoFileErrors(File->Handle))
			{
				// NOTE: The first asset and tag slot in every 
				// P5A is a null asset (reserved) so we don't count
				// it as something we will need space for
				Assets->TagCount += (File->Header.TagCount - 1);
				Assets->AssetCount += (File->Header.AssetCount - 1);
			}
			else
			{
				// TODO: Eventually, have some way of notifying users of bogus files?
				InvalidCodePath;
			}
		}

		Platform.GetAllFilesOfTypeEnd(FileGroup);
	}

	// NOTE: Allocate all metadata space
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
	Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);
	Assets->Tags = PushArray(Arena, Assets->TagCount, p5a_tag);

	// NOTE: Reserve one null tag at the beginning
	ZeroStruct(Assets->Tags[0]);

	// NOTE: Load tags
	for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex)
	{
		asset_file* File = Assets->Files + FileIndex;
		if (PlatformNoFileErrors(File->Handle))
		{
			// NOTE: Skip the first tag, since it's null
			u32 TagArraySize = sizeof(p5a_tag) * (File->Header.TagCount - 1);
			Platform.ReadDataFromFile(File->Handle,
				File->Header.Tags + sizeof(p5a_tag),
				TagArraySize,
				Assets->Tags + File->TagBase
			);
		}
	}

	// NOTE: Reserve one null asset at the beginning
	u32 AssetCount = 0;
	ZeroStruct(*(Assets->Assets + AssetCount));
	++AssetCount;

	// TODO: Exercise for the reader - how would you do this in a way that scaled gracefully
	// to hundreds of asset pack files? (or more!)
	for (u32 DestTypeID = 0; DestTypeID < (u32)asset_type_id::Count; ++DestTypeID)
	{
		asset_type* DestType = Assets->AssetTypes + DestTypeID;
		DestType->FirstAssetIndex = AssetCount;

		for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex)
		{
			asset_file* File = Assets->Files + FileIndex;
			if (PlatformNoFileErrors(File->Handle))
			{
				for (u32 SourceIndex = 0; SourceIndex < File->Header.AssetTypeCount; ++SourceIndex)
				{
					p5a_asset_type* SourceType = File->AssetTypeArray + SourceIndex;

					if ((u32)SourceType->TypeID == DestTypeID)
					{
						u32 AssetCountForType = (SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex);

						temporary_memory TempMem = BeginTemporaryMemory(&TransientState->TransientArena);
						p5a_asset* P5AAssetArray = PushArray(&TransientState->TransientArena, AssetCountForType, p5a_asset);
						Platform.ReadDataFromFile(File->Handle,
							File->Header.Assets + SourceType->FirstAssetIndex * sizeof(p5a_asset),
							AssetCountForType * sizeof(p5a_asset),
							P5AAssetArray
						);
						for (u32 AssetIndex = 0; AssetIndex < AssetCountForType; ++AssetIndex)
						{
							p5a_asset* P5AAsset = P5AAssetArray + AssetIndex;
							Assert(AssetCount < Assets->AssetCount);

							asset* Asset = Assets->Assets + AssetCount++;
							Asset->FileIndex = FileIndex;
							Asset->P5A = *P5AAsset;
							if (Asset->P5A.FirstTagIndex == 0)
							{
								Asset->P5A.FirstTagIndex = Asset->P5A.OnePastLastTagIndex = 0;
							}
							else
							{
								Asset->P5A.FirstTagIndex += (File->TagBase - 1);
								Asset->P5A.OnePastLastTagIndex += (File->TagBase - 1);
							}
						}

						EndTemporaryMemory(TempMem);
					}
				}
			}
		}

		DestType->OnePastLastAssetIndex = AssetCount;
	}

	Assert(AssetCount == Assets->AssetCount);

	return(Assets);
}

internal void
EvictAsset(game_assets* Assets, asset_memory_header* Header)
{
	u32 SlotIndex = Header->SlotIndex;
	asset_slot* Slot = Assets->Slots + Header->SlotIndex;

	Assert(GetState(Slot) == AssetState_Loaded);

	asset_memory_size Size = GetSizeOfAsset(Assets, GetType(Slot), SlotIndex);

	void* Memory = 0;
	if (GetType(Slot) == AssetState_Sound)
	{
		Memory = Slot->Sound.Samples[0];
	}
	else
	{
		Assert(GetType(Slot) == AssetState_Bitmap);

		Memory = Slot->Bitmap.Memory;
	}

	RemoveAssetHeaderFromList(Header);
	ReleaseAssetMemory(Assets, Size.Total, Memory);

	Slot->State = AssetState_Unloaded;
}

internal void
EvictAssetsAsNecessary(game_assets* Assets)
{
	while (Assets->TotalMemoryUsed > Assets->TargetMemoryUsed)
	{
		asset_memory_header* Header = Assets->LoadedAssetSentinel.Prev;
		if (Header != &Assets->LoadedAssetSentinel)
		{
			asset_slot* Slot = Assets->Slots + Header->SlotIndex;
			if (GetState(Slot) >= AssetState_Loaded)
			{
				EvictAsset(Assets, Header);
			}
		}
		else
		{
			InvalidCodePath;
			break;
		}
	}
}