

struct load_asset_work
{
	task_with_memory* Task;
	asset_slot* Slot;

	platform_file_handle* Handle;
	u64 Offset;
	u64 Size;
	void* Destination;

	asset_state FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
	load_asset_work* Work = (load_asset_work*)Data;

#if 0
	Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
#endif

	CompletePreviousWritesBeforeFutureWrites;

	// TODO: Should we actually fill in bogus data here and set to final state anyway?
#if 0
	if (PlatformNoFileErrors(Work->Handle))
#endif
	{
		Work->Slot->State = Work->FinalState;
	}

	EndTaskWithMemory(Work->Task);
}

inline platform_file_handle*
GetFileHandleFor(game_assets* Assets, u32 FileIndex)
{
	Assert(FileIndex < Assets->FileCount);

	platform_file_handle* Result = Assets->Files[FileIndex].Handle;

	return(Result);
}

internal void
LoadBitmap(game_assets* Assets, bitmap_id ID)
{
	if (ID.Value && (AtomicCompareExchangeUInt32((u32*)&Assets->Slots[ID.Value].State, (u32)asset_state::Queued, (u32)asset_state::Unloaded) == (u32)asset_state::Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			asset* Asset = Assets->Assets + ID.Value;
			p5a_bitmap* Info = &Asset->P5A.Bitmap;
			loaded_bitmap* Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);

			Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
			Bitmap->WidthOverHeight = (f32)Info->Dim[0] / (f32)Info->Dim[1];
			Bitmap->Width = Info->Dim[0];
			Bitmap->Height = Info->Dim[1];
			Bitmap->Pitch = 4 * Info->Dim[0];
			u32 MemorySize = Bitmap->Pitch * Bitmap->Height;
			Bitmap->Memory = PushSize(&Assets->Arena, MemorySize);

			load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->P5A.DataOffset;
			Work->Size = MemorySize;
			Work->Destination = Bitmap->Memory;
			Work->FinalState = asset_state::Loaded;
			Work->Slot->Bitmap = Bitmap;

			Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Assets->Slots[ID.Value].State = asset_state::Unloaded;
		}
	}
}

internal void
LoadSound(game_assets* Assets, sound_id ID)
{
	if (ID.Value && (AtomicCompareExchangeUInt32((u32*)&Assets->Slots[ID.Value].State, (u32)asset_state::Queued, (u32)asset_state::Unloaded) == (u32)asset_state::Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			asset* Asset = Assets->Assets + ID.Value;
			p5a_sound* Info = &Asset->P5A.Sound;
			loaded_sound* Sound = PushStruct(&Assets->Arena, loaded_sound);

			Sound->ChannelCount = Info->ChannelCount;
			Sound->SampleCount = Info->SampleCount;
			u32 ChannelSize = Sound->SampleCount * sizeof(i16);
			u32 MemorySize = Sound->ChannelCount * ChannelSize;

			void* Memory = PushSize(&Assets->Arena, MemorySize);
			i16* SoundAt = (i16*)Memory;
			for (u32 ChannelIndex = 0; ChannelIndex < Sound->ChannelCount; ++ChannelIndex)
			{
				Sound->Samples[ChannelIndex] = SoundAt;
				SoundAt += ChannelSize;
			}

			load_asset_work* Work = PushStruct(&Task->Arena, load_asset_work);
			Work->Task = Task;
			Work->Slot = Assets->Slots + ID.Value;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->P5A.DataOffset;
			Work->Size = MemorySize;
			Work->Destination = Memory;
			Work->FinalState = asset_state::Loaded;
			Work->Slot->Sound = Sound;

			Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Assets->Slots[ID.Value].State = asset_state::Unloaded;
		}
	}
}

inline void
PrefetchBitmap(game_assets* Assets, bitmap_id ID)
{
	LoadBitmap(Assets, ID);
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

	for (u32 TagType = 0; TagType < (u32)asset_tag_id::Count; ++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[(u32)asset_tag_id::FacingDirection] = Tau32;

	Assets->TagCount = 0;
	Assets->AssetCount = 0;

	{
		platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin((char*)"p5a");
		Assets->FileCount = FileGroup.FileCount;
		Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
		for (u32 FileIndex = 0; FileIndex < FileGroup.FileCount; ++FileIndex)
		{
			asset_file* File = Assets->Files + FileIndex;
			File->TagBase = Assets->TagCount;

			ZeroStruct(File->Header);
			File->Handle = Platform.OpenFile(FileGroup, FileIndex);
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
				Assets->TagCount += File->Header.TagCount;
				Assets->AssetCount += File->Header.AssetCount;
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

	// NOTE: Load tags
	for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex)
	{
		asset_file* File = Assets->Files + FileIndex;
		if (PlatformNoFileErrors(File->Handle))
		{
			u32 TagArraySize = sizeof(p5a_tag) * File->Header.TagCount;
			Platform.ReadDataFromFile(File->Handle,
				File->Header.Tags,
				TagArraySize,
				Assets->Tags + File->TagBase
			);
		}
	}

	// TODO: Exersize for the reader - how would you do this in a way thot scaled gracefully
	// to hundreds of asset pack files? (or more!)
	u32 AssetCount = 0;
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
							p5a_asset* P5AAsset = P5AAssetArray + AssetIndex;;
							Assert(AssetCount <= Assets->AssetCount);

							asset* Asset = Assets->Assets + AssetCount++;
							Asset->P5A = *P5AAsset;
							Asset->P5A.FirstTagIndex += File->TagBase;
							Asset->P5A.OnePastLastTagIndex += File->TagBase;

							Asset->FileIndex = FileIndex;
						}

						EndTemporaryMemory(TempMem);
					}
				}
			}
		}

		DestType->OnePastLastAssetIndex = AssetCount;
	}

	// Assert(AssetCount == Assets->AssetCount);

#if 0
	debug_read_file_result ReadResult = Platform.DEBUGReadEntireFile((char*)"../data/assets.p5a");
	if (ReadResult.ContentsSize != 0)
	{
		p5a_header* Header = (p5a_header*)ReadResult.Contents;

		Assets->AssetCount = Header->AssetCount;
		Assets->Assets = (asset*)((u8*)ReadResult.Contents + Header->Assets);
		Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);

		Assets->TagCount = Header->TagCount;
		Assets->Tags = (p5a_tag*)((u8*)ReadResult.Contents + Header->Tags);

		p5a_asset_type* P5AAssetTypes = (p5a_asset_type*)((u8*)ReadResult.Contents + Header->AssetTypes);

		for (u32 AssetTypeIndex = 0; AssetTypeIndex < Header->AssetTypeCount; ++AssetTypeIndex)
		{
			p5a_asset_type* Source = P5AAssetTypes + AssetTypeIndex;

			if (Source->TypeID < (u32)asset_type_id::Count)
			{
				asset_type* Dest = Assets->AssetTypes + Source->TypeID;
				// TODO: Support merging
				Assert(Dest->FirstAssetIndex == 0);
				Assert(Dest->OnePastLastAssetIndex == 0);

				Dest->FirstAssetIndex = Source->FirstAssetIndex;
				Dest->OnePastLastAssetIndex = Source->OnePastLastAssetIndex;
			}
		}

		Assets->P5AContents = (u8*)ReadResult.Contents;
	}
#endif

	return(Assets);
}