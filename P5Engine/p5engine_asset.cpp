

struct load_asset_work
{
	task_with_memory* Task;
	asset* Asset;

	platform_file_handle* Handle;
	u64 Offset;
	u64 Size;
	void* Destination;

	u32 FinalState;
};

internal void
LoadAssetWorkDirectly(load_asset_work* Work)
{
	Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

	CompletePreviousWritesBeforeFutureWrites;

	if (!PlatformNoFileErrors(Work->Handle))
	{
		ZeroSize(Work->Size, Work->Destination);
	}

	Work->Asset->State = Work->FinalState;
}

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
	load_asset_work* Work = (load_asset_work*)FindData;

	LoadAssetWorkDirectly(Work);
	
	EndTaskWithMemory(Work->Task);
}

inline asset_file*
GetFile(game_assets* Assets, u32 FileIndex)
{
	Assert(FileIndex < Assets->FileCount);

	asset_file* Result = Assets->Files + FileIndex;

	return(Result);
}

inline platform_file_handle*
GetFileHandleFor(game_assets* Assets, u32 FileIndex)
{
	Assert(FileIndex < Assets->FileCount);

	platform_file_handle* Result = &GetFile(Assets, FileIndex)->Handle;

	return(Result);
}

internal asset_memory_block*
InsertBlock(asset_memory_block* Prev, u64 Size, void* Memory)
{
	Assert(Size > sizeof(asset_memory_block));

	asset_memory_block* Block = (asset_memory_block*)Memory;
	Block->Flags = 0;
	Block->Size = Size - sizeof(asset_memory_block);

	Block->Prev = Prev;
	Block->Next = Prev->Next;

	Block->Prev->Next = Block;
	Block->Next->Prev = Block;

	return(Block);
}

internal asset_memory_block*
FindBlockForSize(game_assets* Assets, memory_index Size)
{
	asset_memory_block* Result = 0;

	// TODO: This probably will need to be accelerated in the 
	// future as the resident asset count grows.

	// TODO: Best match block
	for (asset_memory_block* Block = Assets->MemorySentinel.Next;
		 Block != &Assets->MemorySentinel;
		 Block = Block->Next)
	{
		if (!(Block->Flags & AssetMemory_Used))
		{
			if (Block->Size >= Size)
			{
				Result = Block;
				break;
			}
		}
	}

	return(Result);
}

internal b32
MergeIfPossible(game_assets* Assets, asset_memory_block* First, asset_memory_block* Second)
{
	b32 Result = false;

	if ((First != &Assets->MemorySentinel) &&
		(Second != &Assets->MemorySentinel))
	{
		if (!(First->Flags & AssetMemory_Used) &&
			!(Second->Flags & AssetMemory_Used))
		{
			u8* ExpectedSecond = (u8*)First + sizeof(asset_memory_block) + First->Size;
			if ((u8*)Second == ExpectedSecond)
			{
				Second->Next->Prev = Second->Prev;
				Second->Prev->Next = Second->Next;

				First->Size += sizeof(asset_memory_block) + Second->Size;

				Result = true;
			}
		}
	}

	return(Result);
}

internal b32
GenerationHasCompleted(game_assets* Assets, u32 CheckID)
{
	b32 Result = true;

	for (u32 Index = 0;
		 Index < Assets->InFlightGenerationCount;
		 ++Index)
	{
		if (Assets->InFlightGenerations[Index] == CheckID)
		{
			Result = false;
			break;
		}
	}

	return(Result);
}

internal asset_memory_header*
AcquireAssetMemory(game_assets* Assets, u32 Size, u32 AssetIndex)
{
	asset_memory_header* Result = 0;

	BeginAssetLock(Assets);

	asset_memory_block* Block = FindBlockForSize(Assets, Size);
	for (;;)
	{
		if (Block && (Size <= Block->Size))
		{
			Block->Flags |= AssetMemory_Used;

			Result = (asset_memory_header*)(Block + 1);

			memory_index RemainingSize = Block->Size - Size;
			memory_index BlockSplitThreshold = 4096;
			if (RemainingSize > BlockSplitThreshold)
			{
				Block->Size -= RemainingSize;
				InsertBlock(Block, RemainingSize, (u8*)Result + Size);
			}

			break;
		}
		else
		{
			for (asset_memory_header* Header = Assets->LoadedAssetSentinel.Prev;
				Header != &Assets->LoadedAssetSentinel;
				Header = Header->Prev)
			{
				if (Header != &Assets->LoadedAssetSentinel)
				{
					asset* Asset = Assets->Assets + Header->AssetIndex;
					if ((Asset->State == AssetState_Loaded) &&
						(GenerationHasCompleted(Assets, Asset->Header->GenerationID)))
					{
						AssetIndex = Header->AssetIndex; // u32
						asset* Asset = Assets->Assets + Header->AssetIndex;

						Assert(Asset->State == AssetState_Loaded);

						RemoveAssetHeaderFromList(Header);

						Block = (asset_memory_block*)Asset->Header - 1;
						Block->Flags &= ~AssetMemory_Used;
						if (MergeIfPossible(Assets, Block->Prev, Block))
						{
							Block = Block->Prev;
						}

						MergeIfPossible(Assets, Block, Block->Next);

						Asset->State = AssetState_Unloaded;
						Asset->Header = 0;

						break;
					}
				}
			}
		}
	}

	if (Result)
	{
		Result->AssetIndex = AssetIndex;
		Result->TotalSize = Size;
		InsertAssetHeaderAtFront(Assets, Result);
	}

	EndAssetLock(Assets);

	return(Result);
}

struct asset_memory_size
{
	u32 Total;
	u32 Data;
	u32 Section;
};

internal void
LoadBitmap(game_assets* Assets, bitmap_id ID, b32 Immediate)
{
	asset* Asset = Assets->Assets + ID.Value;
	if (ID.Value)
	{
		if (AtomicCompareExchangeUInt32((u32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded)
		{
			task_with_memory* Task = 0;

			if (!Immediate)
			{
				Task = BeginTaskWithMemory(Assets->TransientState);
			}

			if (Immediate || Task)
			{
				p5a_bitmap* Info = &Asset->P5A.Bitmap;

				asset_memory_size Size = {};
				u32 Width = Info->Dim[0];
				u32 Height = Info->Dim[1];
				Size.Section = 4 * Width;
				Size.Data = Size.Section * Height;
				Size.Total = Size.Data + sizeof(asset_memory_header);

				Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value);

				loaded_bitmap* Bitmap = &Asset->Header->Bitmap;
				Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
				Bitmap->WidthOverHeight = (f32)Info->Dim[0] / (f32)Info->Dim[1];
				Bitmap->Width = Info->Dim[0];
				Bitmap->Height = Info->Dim[1];
				Bitmap->Pitch = Size.Section;
				Bitmap->Memory = (Asset->Header + 1);

				load_asset_work Work;
				Work.Task = Task;
				Work.Asset = Asset;
				Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
				Work.Offset = Asset->P5A.DataOffset;
				Work.Size = Size.Data;
				Work.Destination = Bitmap->Memory;
				Work.FinalState = AssetState_Loaded;

				if (Task)
				{
					load_asset_work* TaskWork = PushStruct(&Task->Arena, load_asset_work);
					*TaskWork = Work;
					Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, TaskWork);
				}
				else
				{
					LoadAssetWorkDirectly(&Work);
				}
			}
			else
			{
				Asset->State = AssetState_Unloaded;
			}
		}
		else if (Immediate)
		{
			// TODO: Do we want to have a more coherent story here
			// for what happens when two force-load people hit the load
			//  at the same time?
			asset_state volatile* State = (asset_state volatile*)&Asset->State;
			while (*State == AssetState_Queued)
			{
				// Just wait
			}
		}
	}
}

internal void
LoadSound(game_assets* Assets, sound_id ID)
{
	asset* Asset = Assets->Assets + ID.Value;
	if (ID.Value &&
		(AtomicCompareExchangeUInt32((u32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			p5a_sound* Info = &Asset->P5A.Sound;

			asset_memory_size Size = {};
			Size.Section = Info->SampleCount * sizeof(i16);
			Size.Data = Info->ChannelCount * Size.Section;
			Size.Total = Size.Data + sizeof(asset_memory_header);

			Asset->Header = (asset_memory_header*)AcquireAssetMemory(Assets, Size.Total, ID.Value);

			loaded_sound* Sound = &Asset->Header->Sound;
			Sound->SampleCount = Info->SampleCount;
			Sound->ChannelCount = Info->ChannelCount;
			u32 ChannelSize = Size.Section;

			void* Memory = (Asset->Header + 1);
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
			Work->Asset = Asset;
			Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
			Work->Offset = Asset->P5A.DataOffset;
			Work->Size = Size.Data;
			Work->Destination = Memory;
			Work->FinalState = AssetState_Loaded;

			Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, Work);
		}
		else
		{
			Asset->State = AssetState_Unloaded;
		}
	}
}

internal void
LoadFont(game_assets* Assets, font_id ID, b32 Immediate)
{
	// TODO: Merge this boilerplate. Same between LoadBitmap, LoadSound and LoadFont
	asset* Asset = Assets->Assets + ID.Value;
	if (ID.Value)
	{
		if (AtomicCompareExchangeUInt32((u32*)&Asset->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded)
		{
			task_with_memory* Task = 0;

			if (!Immediate)
			{
				Task = BeginTaskWithMemory(Assets->TransientState);
			}

			if (Immediate || Task)
			{
				p5a_font* Info = &Asset->P5A.Font;

				u32 HorizontalAdvanceSize = sizeof(f32) * Info->CodepointCount * Info->CodepointCount;
				u32 CodepointsSize = Info->CodepointCount * sizeof(bitmap_id);
				u32 SizeData = CodepointsSize + HorizontalAdvanceSize;
				u32 SizeTotal = SizeData + sizeof(asset_memory_header);

				Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value);

				loaded_font* Font = &Asset->Header->Font;
				Font->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->FontBitmapIDOffset;
				Font->Codepoints = (bitmap_id*)(Asset->Header + 1);
				Font->HorizontalAdvance = (f32*)((u8*)Font->Codepoints + CodepointsSize);

				load_asset_work Work;
				Work.Task = Task;
				Work.Asset = Asset;
				Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
				Work.Offset = Asset->P5A.DataOffset;
				Work.Size = SizeData;
				Work.Destination = Font->Codepoints;
				Work.FinalState = AssetState_Loaded;

				if (Task)
				{
					load_asset_work* TaskWork = PushStruct(&Task->Arena, load_asset_work);
					*TaskWork = Work;
					Platform.AddEntry(Assets->TransientState->LowPriorityQueue, LoadAssetWork, TaskWork);
				}
				else
				{
					LoadAssetWorkDirectly(&Work);
				}
			}
			else
			{
				Asset->State = AssetState_Unloaded;
			}
		}
		else if (Immediate)
		{
			// TODO: Do we want to have a more coherent story here
			// for what happens when two force-load people hit the load
			//  at the same time?
			asset_state volatile* State = (asset_state volatile*)&Asset->State;
			while (*State == AssetState_Queued)
			{
				// Just wait
			}
		}
	}
}

inline void
PrefetchBitmap(game_assets* Assets, bitmap_id ID)
{
	LoadBitmap(Assets, ID, false);
}

inline void
PrefetchSound(game_assets* Assets, sound_id ID)
{
	LoadSound(Assets, ID);
}

inline void
PrefetchFont(game_assets* Assets, font_id ID)
{
	LoadFont(Assets, ID, false);
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

internal font_id
GetBestMatchFontFrom(game_assets* Assets, asset_type_id TypeID, asset_vector* MatchVector, asset_vector* WeightVector)
{
	font_id Result = { GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector) };

	return(Result);
}

internal font_id
GetFirstFontFrom(game_assets* Assets, asset_type_id TypeID)
{
	font_id Result = { GetFirstAssetFrom(Assets, TypeID) };

	return(Result);
}

internal font_id
GetRandomFontFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	font_id Result = { GetRandomAssetFrom(Assets, TypeID, Series) };

	return(Result);
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TransientState)
{
	game_assets* Assets = PushStruct(Arena, game_assets);

	Assets->NextGenerationID = 0;
	Assets->InFlightGenerationCount = 0;

	Assets->MemorySentinel.Flags = 0;
	Assets->MemorySentinel.Size = 0;
	Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
	Assets->MemorySentinel.Next = &Assets->MemorySentinel;

	InsertBlock(&Assets->MemorySentinel, Size, (asset_memory_block*)PushSize(Arena, Size));
	
	Assets->TransientState = TransientState;
	
	Assets->LoadedAssetSentinel.Next = Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

	for (u32 TagType = 0;
		 TagType < (u32)asset_tag_id::Count;
		 ++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[(u32)asset_tag_id::FacingDirection] = Tau32;

	Assets->TagCount = 1;
	Assets->AssetCount = 1;

	{
		platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_AssetFile);
		Assets->FileCount = FileGroup.FileCount;
		Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
		for (u32 FileIndex = 0;
			 FileIndex < Assets->FileCount;
			 ++FileIndex)
		{
			asset_file* File = Assets->Files + FileIndex;
			File->FontBitmapIDOffset = 0;
			File->TagBase = Assets->TagCount;

			ZeroStruct(File->Header);
			File->Handle = Platform.OpenNextFile(&FileGroup);
			Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);

			u32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(p5a_asset_type);
			File->AssetTypeArray = (p5a_asset_type*)PushSize(Arena, AssetTypeArraySize);
			Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypes, AssetTypeArraySize, File->AssetTypeArray);

			if (File->Header.MagicValue != P5A_MAGIC_VALUE)
			{
				Platform.FileError(&File->Handle, (char*)"P5A file has an invalid magic value.");
			}

			if (File->Header.Version > P5A_VERSION)
			{
				Platform.FileError(&File->Handle, (char*)"P5A file is of a later version.");
			}

			if (PlatformNoFileErrors(&File->Handle))
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

		Platform.GetAllFilesOfTypeEnd(&FileGroup);
	}

	// NOTE: Allocate all metadata space
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
	Assets->Tags = PushArray(Arena, Assets->TagCount, p5a_tag);

	// NOTE: Reserve one null tag at the beginning
	ZeroStruct(Assets->Tags[0]);

	// NOTE: Load tags
	for (u32 FileIndex = 0;
		 FileIndex < Assets->FileCount;
		 ++FileIndex)
	{
		asset_file* File = Assets->Files + FileIndex;
		if (PlatformNoFileErrors(&File->Handle))
		{
			// NOTE: Skip the first tag, since it's null
			u32 TagArraySize = sizeof(p5a_tag) * (File->Header.TagCount - 1);
			Platform.ReadDataFromFile(&File->Handle,
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
	for (u32 DestTypeID = 0;
		 DestTypeID < (u32)asset_type_id::Count;
		 ++DestTypeID)
	{
		asset_type* DestType = Assets->AssetTypes + DestTypeID;
		DestType->FirstAssetIndex = AssetCount;

		for (u32 FileIndex = 0;
			 FileIndex < Assets->FileCount;
			 ++FileIndex)
		{
			asset_file* File = Assets->Files + FileIndex;
			if (PlatformNoFileErrors(&File->Handle))
			{
				for (u32 SourceIndex = 0;
					 SourceIndex < File->Header.AssetTypeCount;
					 ++SourceIndex)
				{
					p5a_asset_type* SourceType = File->AssetTypeArray + SourceIndex;

					if ((u32)SourceType->TypeID == DestTypeID)
					{
						if (SourceType->TypeID == (u32)asset_type_id::FontGlyph)
						{
							File->FontBitmapIDOffset = AssetCount - SourceType->FirstAssetIndex;
						}

						u32 AssetCountForType = (SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex);

						temporary_memory TempMem = BeginTemporaryMemory(&TransientState->TransientArena);
						p5a_asset* P5AAssetArray = PushArray(&TransientState->TransientArena, AssetCountForType, p5a_asset);
						Platform.ReadDataFromFile(&File->Handle,
							File->Header.Assets + SourceType->FirstAssetIndex * sizeof(p5a_asset),
							AssetCountForType * sizeof(p5a_asset),
							P5AAssetArray
						);

						for (u32 AssetIndex = 0;
							 AssetIndex < AssetCountForType;
							 ++AssetIndex)
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

inline u32
GetClampedCodepoint(p5a_font* Info, u32 Codepoint)
{
	u32 Result = 0;
	if (Codepoint < Info->CodepointCount)
	{
		Result = Codepoint;
	}

	return(Result);
}

internal f32
GetHorizontalAdvanceForPair(p5a_font* Info, loaded_font* Font, u32 DesiredPrevCodepoint, u32 DesiredCodepoint)
{
	u32 PrevCodepoint = GetClampedCodepoint(Info, DesiredPrevCodepoint);
	u32 Codepoint = GetClampedCodepoint(Info, DesiredCodepoint);

	f32 Result = Font->HorizontalAdvance[PrevCodepoint * Info->CodepointCount + Codepoint];

	return(Result);
}

internal bitmap_id
GetBitmapForGlyph(game_assets* Assets, p5a_font* Info, loaded_font* Font, u32 DesiredCodepoint)
{
	u32 Codepoint = GetClampedCodepoint(Info, DesiredCodepoint);

	bitmap_id Result = Font->Codepoints[Codepoint];
	Result.Value += Font->BitmapIDOffset;
	
	return(Result);
}

internal f32
GetLineAdvanceFor(p5a_font* Info)
{
	f32 Result = Info->LineAdvance;

	return(Result);
}