
#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;
	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorzResolution;
	int32 VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;

	uint32 RedMask;
	uint32 GreenMask;
	uint32 BlueMask;
};

struct WAVE_header
{
	uint32 RIFFID;
	uint32 Size;
	uint32 WAVEID;
};

#define RIFF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))

enum
{
	WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
	WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
	WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
	WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};
struct WAVE_chunk
{
	uint32 ID;
	uint32 Size;
};

struct WAVE_fmt
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint32 nAvgBytesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
	uint16 cbSize;
	uint16 wValidBitsPerSample;
	uint32 dwChannelMask;
	uint8 SubFormat[16];
};
#pragma pack(pop)

internal v2
TopDownAlign(loaded_bitmap* Bitmap, v2 Align)
{
	Align.y = (real32)(Bitmap->Height - 1) - Align.y;

	Align.x = SafeRatio0(Align.x, (real32)Bitmap->Width);
	Align.y = SafeRatio0(Align.y, (real32)Bitmap->Height);

	return(Align);
}

internal void
SetTopDownAlign(hero_bitmaps* Bitmap, v2 Align)
{
	v2 AlignPercentage = TopDownAlign(&Bitmap->Character, Align);

	Bitmap->Character.AlignPercentage = AlignPercentage;
}

internal loaded_bitmap
DEBUGLoadBMP(char* Filename, v2 AlignPercentage = V2(0.5f, 0.5f))
{
	loaded_bitmap Result = {};

	debug_read_file_result ReadResult = PlatformReadEntireFile(Filename);
	if (ReadResult.ContentsSize)
	{
		bitmap_header* Header = (bitmap_header*)ReadResult.Contents;
		uint32* Memory = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Memory;
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		Result.AlignPercentage = AlignPercentage;
		Result.WidthOverHeight = SafeRatio0((real32)Header->Width, (real32)Header->Height);

		Assert(Result.Height > 0);
		Assert(Header->Compression == 3);

		// NOTE: If you are using this generically for some reason,
		// please remember that BMP files CAN GO IN EITHER DIRECTION and 
		// the height will be negative for top-down.
		// (Also, there can be compression, etc, etc... Don't think this
		// is complete BMP loading code, because it isn't!!

		// NOTE: Byte order in memory is determined by the Header itself, 
		// so we have to read out the masks and convert the pixels ourselves.
		int32 RedMask = Header->RedMask;
		int32 GreenMask = Header->GreenMask;
		int32 BlueMask = Header->BlueMask;
		int32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		int32 AlphaShiftDown = AlphaScan.Index;
		int32 RedShiftDown = RedScan.Index;
		int32 GreenShiftDown = GreenScan.Index;
		int32 BlueShiftDown = BlueScan.Index;

		uint32* SourceDest = Memory;
		for (int32 y = 0; y < Header->Height; ++y)
		{
			for (int32 x = 0; x < Header->Width; ++x)
			{
				uint32 C = *SourceDest;

				v4 Texel = V4(
					(real32)((C & RedMask) >> RedShiftDown),
					(real32)((C & GreenMask) >> GreenShiftDown),
					(real32)((C & BlueMask) >> BlueShiftDown),
					(real32)((C & AlphaMask) >> AlphaShiftDown)
				);

				Texel = SRGB255ToLinear1(Texel);
#if 1
				Texel.rgb *= Texel.a;
#endif
				Texel = Linear1ToSRGB255(Texel);


				*SourceDest++ = (((uint32)(Texel.a + 0.5f) << 24) |
					((uint32)(Texel.r + 0.5f) << 16) |
					((uint32)(Texel.g + 0.5f) << 8) |
					((uint32)(Texel.b + 0.5f) << 0));
			}
		}
	}
	
	Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;

#if 0
	Result.Memory = (uint32*)((uint8*)Result.Memory + Result.Pitch * (Result.Height - 1));
	Result.Pitch = -Result.Width;
#endif

	return(Result);
}

struct riff_iterator
{
	uint8* At;
	uint8* Stop;
};

inline riff_iterator
ParseChunkAt(void* At, void* Stop)
{
	riff_iterator Iter;

	Iter.At = (uint8*)At;
	Iter.Stop = (uint8*)Stop;

	return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	uint32 Size = (Chunk->Size + 1) & ~1;
	Iter.At += sizeof(WAVE_chunk) + Size;

	return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
	bool32 Result = (Iter.At < Iter.Stop);

	return(Result);
}

inline void*
GetChunkData(riff_iterator Iter)
{
	void* Result = (Iter.At + sizeof(WAVE_chunk));

	return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	uint32 Result = Chunk->ID;

	return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	uint32 Result = Chunk->Size;

	return(Result);
}

internal loaded_sound
DEBUGLoadWAV(char* Filename)
{
	loaded_sound Result = {};
	                                                                                                                                                                                                                                                                                           
	debug_read_file_result ReadResult = PlatformReadEntireFile(Filename);
	if (ReadResult.ContentsSize != 0)
	{
		WAVE_header* Header = (WAVE_header*)ReadResult.Contents;
		Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
		Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

		uint32 ChannelCount = 0;
		uint32 SampleDataSize = 0;
		int16* SampleData = 0;
		for (riff_iterator Iter = ParseChunkAt(Header + 1, (uint8*)(Header + 1) + Header->Size - 4); IsValid(Iter); Iter = NextChunk(Iter))
		{
			switch (GetType(Iter))
			{
				case WAVE_ChunkID_fmt:
				{
					WAVE_fmt* fmt = (WAVE_fmt*)GetChunkData(Iter);
					Assert(fmt->wFormatTag == 1); // NOTE: Only support PCM
					Assert((fmt->nSamplesPerSec == 48000) || (fmt->nSamplesPerSec == 44100));
					Assert((fmt->wBitsPerSample == 16));
					Assert(fmt->nBlockAlign == (sizeof(int16) * fmt->nChannels));
					ChannelCount = fmt->nChannels;
				} break;

				case WAVE_ChunkID_data:
				{
					SampleData = (int16*)GetChunkData(Iter);
					SampleDataSize = GetChunkDataSize(Iter);
				} break;
			}
		}
		
		Assert(ChannelCount && SampleData);

		Result.ChannelCount = ChannelCount;
		Result.SampleCount = SampleDataSize / (ChannelCount * sizeof(int16));
		if (ChannelCount == 1)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = 0;
		}
		else if (ChannelCount == 2)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = SampleData + Result.SampleCount;

#if 0
			for (uint32 SampleIndex = 0; SampleIndex < Result.SampleCount; ++SampleIndex)
			{
				SampleData[2 * SampleIndex + 0] = (int16)SampleIndex;
				SampleData[2 * SampleIndex + 1] = (int16)SampleIndex;
			}
#endif

			for (uint32 SampleIndex = 0; SampleIndex < Result.SampleCount; ++SampleIndex)
			{
				int16 Source = SampleData[2 * SampleIndex];
				SampleData[2 * SampleIndex] = SampleData[SampleIndex];
				SampleData[SampleIndex] = Source;
			}
		}
		else
		{
			Assert(!"Invalid channel count in WAV file");
		}

		// TODO: Load right channels
		Result.ChannelCount = 1;
	}

	return(Result);
}

struct load_bitmap_work
{
	game_assets* Assets;
	bitmap_id ID;
	task_with_memory* Task;
	loaded_bitmap* Bitmap;
	asset_state FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
	load_bitmap_work* Work = (load_bitmap_work*)Data;

	asset_bitmap_info* Info = Work->Assets->BitmapInfos + Work->ID.Value;
	*Work->Bitmap = DEBUGLoadBMP(Info->Filename, Info->AlignPercentage);

	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Bitmaps[Work->ID.Value].Bitmap = Work->Bitmap;
	Work->Assets->Bitmaps[Work->ID.Value].State = Work->FinalState;

	EndTaskWithMemory(Work->Task);
}

internal void
LoadBitmap(game_assets* Assets, bitmap_id ID)
{
	if (ID.Value && (AtomicCompareExchangeUInt32((uint32*)&Assets->Bitmaps[ID.Value].State, (uint32)asset_state::Queued, (uint32)asset_state::Unloaded) == (uint32)asset_state::Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			load_bitmap_work* Work = PushStruct(&Task->Arena, load_bitmap_work);

			Work->Assets = Assets;
			Work->ID = ID;
			Work->Task = Task;
			Work->Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);
			Work->FinalState = asset_state::Loaded;

			PlatformAddEntry(Assets->TransientState->LowPriorityQueue, LoadBitmapWork, Work);
		}
		else
		{
			Assets->Bitmaps[ID.Value].State = asset_state::Unloaded;
		}
	}
}

struct load_sound_work
{
	game_assets* Assets;
	sound_id ID;
	task_with_memory* Task;
	loaded_sound* Sound;
	asset_state FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadSoundWork)
{
	load_sound_work* Work = (load_sound_work*)Data;

	asset_sound_info* Info = Work->Assets->SoundInfos + Work->ID.Value;
	*Work->Sound = DEBUGLoadWAV(Info->Filename);

	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Sounds[Work->ID.Value].Sound = Work->Sound;
	Work->Assets->Sounds[Work->ID.Value].State = Work->FinalState;

	EndTaskWithMemory(Work->Task);
}

internal void
LoadSound(game_assets* Assets, sound_id ID)
{
	if (ID.Value && (AtomicCompareExchangeUInt32((uint32*)&Assets->Sounds[ID.Value].State, (uint32)asset_state::Queued, (uint32)asset_state::Unloaded) == (uint32)asset_state::Unloaded))
	{
		task_with_memory* Task = BeginTaskWithMemory(Assets->TransientState);
		if (Task)
		{
			load_sound_work* Work = PushStruct(&Task->Arena, load_sound_work);

			Work->Assets = Assets;
			Work->ID = ID;
			Work->Task = Task;
			Work->Sound = PushStruct(&Assets->Arena, loaded_sound);
			Work->FinalState = asset_state::Loaded;

			PlatformAddEntry(Assets->TransientState->LowPriorityQueue, LoadSoundWork, Work);
		}
		else
		{
			Assets->Sounds[ID.Value].State = asset_state::Unloaded;
		}
	}
}

internal uint32
GetBestMatchAssetFrom(game_assets* Assets, asset_type_id TypeID, asset_vector* MatchVector, asset_vector* WeightVector)
{
	uint32 Result = 0;

	real32 BestDiff = Real32Maximum;
	asset_type* Type = Assets->AssetTypes + (uint32)TypeID;
	for (uint32 AssetIndex = Type->FirstAssetIndex; AssetIndex < Type->OnePastLastAssetIndex; ++AssetIndex)
	{
		asset* Asset = Assets->Assets + AssetIndex;

		real32 TotalWeightedDiff = 0.0f;
		for (uint32 TagIndex = Asset->FirstTagIndex; TagIndex < Asset->OnePastLastTagIndex; ++TagIndex)
		{
			asset_tag* Tag = Assets->Tags + TagIndex;
			
			real32 A = MatchVector->E[(uint32)Tag->ID];
			real32 B = Tag->Value;
			real32 D0 = AbsoluteValue(A - B);
			real32 D1 = AbsoluteValue((A - Assets->TagRange[(uint32)Tag->ID] * SignOf(A)) - B);
			real32 Difference = Minimum(D0, D1);

			real32 Weighted = WeightVector->E[(uint32)Tag->ID] * AbsoluteValue(Difference);
			TotalWeightedDiff += Weighted;
		}

		if (BestDiff > TotalWeightedDiff)
		{
			BestDiff = TotalWeightedDiff;
			Result = Asset->SlotID;
		}
	}

	return(Result);
}

internal uint32
GetRandomAssetFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	uint32 Result = 0;

	asset_type* Type = Assets->AssetTypes + (uint32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
		uint32 Choice = RandomChoice(Series, Count);

		asset* Asset = Assets->Assets + Type->FirstAssetIndex + Choice;
		Result = Asset->SlotID;
	}

	return(Result);
}

internal uint32
GetFirstAssetFrom(game_assets* Assets, asset_type_id TypeID)
{
	uint32 Result = 0;
	
	asset_type* Type = Assets->AssetTypes + (uint32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		asset* Asset = Assets->Assets + Type->FirstAssetIndex;
		Result = Asset->SlotID;
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

internal bitmap_id
DEBUGAddBitmapInfo(game_assets* Assets, char* Filename, v2 AlignPercentage)
{
	Assert(Assets->DEBUGUsedBitmapCount < Assets->BitmapCount);
	bitmap_id ID = { Assets->DEBUGUsedBitmapCount++ };

	asset_bitmap_info* Info = Assets->BitmapInfos + ID.Value;
	Info->Filename = Filename;
	Info->AlignPercentage = AlignPercentage;

	return(ID);
}

internal sound_id
DEBUGAddSoundInfo(game_assets* Assets, char* Filename)
{
	Assert(Assets->DEBUGUsedSoundCount < Assets->SoundCount);
	sound_id ID = { Assets->DEBUGUsedSoundCount++ };

	asset_sound_info* Info = Assets->SoundInfos + ID.Value;
	Info->Filename = Filename;

	return(ID);
}

internal void
BeginAssetType(game_assets* Assets, asset_type_id TypeID)
{
	Assert(Assets->DEBUGAssetType == 0);
	Assets->DEBUGAssetType = Assets->AssetTypes + (uint32)TypeID;
	Assets->DEBUGAssetType->FirstAssetIndex = Assets->DEBUGUsedAssetCount;
	Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(game_assets* Assets, char* Filename, v2 AlignPercentage = V2(0.5f, 0.5f))
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

	asset* Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddBitmapInfo(Assets, Filename, AlignPercentage).Value;

	Assets->DEBUGAsset = Asset;
}

internal void
AddSoundAsset(game_assets* Assets, char* Filename)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

	asset* Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddSoundInfo(Assets, Filename).Value;

	Assets->DEBUGAsset = Asset;
}

internal void
AddTag(game_assets* Assets, asset_tag_id TagID, real32 Value)
{
	Assert(Assets->DEBUGAsset);

	++Assets->DEBUGAsset->OnePastLastTagIndex;
	asset_tag* Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;

	Tag->ID = TagID;
	Tag->Value = Value;
}

internal void
EndAssetType(game_assets* Assets)
{
	Assert(Assets->DEBUGAssetType);

	Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
	Assets->DEBUGAssetType = 0;
	Assets->DEBUGAsset = 0;
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TransientState)
{
	game_assets* Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TransientState = TransientState;

	for (uint32 TagType = 0; TagType < (uint32)asset_tag_id::Count; ++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[(uint32)asset_tag_id::FacingDirection] = Tau32;

	Assets->BitmapCount = 256 * (uint32)asset_type_id::Count;
	Assets->BitmapInfos = PushArray(Arena, Assets->BitmapCount, asset_bitmap_info);
	Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

	Assets->SoundCount = 256 * (uint32)asset_type_id::Count;
	Assets->SoundInfos = PushArray(Arena, Assets->SoundCount, asset_sound_info);
	Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

	Assets->AssetCount = Assets->SoundCount + Assets->BitmapCount;
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

	Assets->TagCount = 1024 * (uint32)asset_type_id::Count;
	Assets->Tags = PushArray(Arena, Assets->TagCount, asset_tag);

	Assets->DEBUGUsedBitmapCount = 1;
	Assets->DEBUGUsedSoundCount = 1;
	Assets->DEBUGUsedAssetCount = 1;

	//
	// BITMAPS
	//

	BeginAssetType(Assets, asset_type_id::Shadow);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/shadow.bmp", V2(0.5f, 1.09090912f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tree);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/tree.bmp", V2(0.5f, 0.340425521f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Monstar);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/enemy.bmp", V2(0.5f, 0.0f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Familiar);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/orb.bmp", V2(0.5f, 0.0f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Grass);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/grass1.bmp", V2(0.5f, 0.5f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Soil);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/soil1.bmp");
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/soil2.bmp");
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/soil4.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tuft);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/tuft1.bmp");
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/tuft2.bmp");
	EndAssetType(Assets);

	real32 AngleRight = 0.0f * Pi32;
	real32 AngleBack = 0.5f * Pi32;
	real32 AngleLeft = 1.0f * Pi32;
	real32 AngleFront = 1.5f * Pi32;

	v2 HeroAlign = V2(0.5f, 0.109375f);

	BeginAssetType(Assets, asset_type_id::Character);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-right-0.bmp", HeroAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-back-0.bmp", HeroAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-left-0.bmp", HeroAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-front-0.bmp", HeroAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	v2 SwordAlign = V2(0.5f, 0.828125f);

	BeginAssetType(Assets, asset_type_id::Sword);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-right.bmp", SwordAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-back.bmp", SwordAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-left.bmp", SwordAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-front.bmp", SwordAlign);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	//
	// SOUNDS
	//

	BeginAssetType(Assets, asset_type_id::Bloop);
	AddSoundAsset(Assets, (char*)"../data/audio/bloop_00.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/bloop_01.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/bloop_02.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/bloop_03.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/bloop_04.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Crack);
	AddSoundAsset(Assets, (char*)"../data/audio/crack_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Drop);
	AddSoundAsset(Assets, (char*)"../data/audio/drop_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Glide);
	AddSoundAsset(Assets, (char*)"../data/audio/glide_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Music);
	AddSoundAsset(Assets, (char*)"../data/audio/music_test.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Puhp);
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_00.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_01.wav");
	EndAssetType(Assets);

	return(Assets);
}