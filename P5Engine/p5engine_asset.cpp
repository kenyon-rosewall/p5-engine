
#pragma pack(push, 1)
struct bitmap_header
{
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	s32 Width;
	s32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 SizeOfBitmap;
	s32 HorzResolution;
	s32 VertResolution;
	u32 ColorsUsed;
	u32 ColorsImportant;

	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
};

struct WAVE_header
{
	u32 RIFFID;
	u32 Size;
	u32 WAVEID;
};

#define RIFF_CODE(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

enum
{
	WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
	WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
	WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
	WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};
struct WAVE_chunk
{
	u32 ID;
	u32 Size;
};

struct WAVE_fmt
{
	u16 wFormatTag;
	u16 nChannels;
	u32 nSamplesPerSec;
	u32 nAvgBytesPerSec;
	u16 nBlockAlign;
	u16 wBitsPerSample;
	u16 cbSize;
	u16 wValidBitsPerSample;
	u32 dwChannelMask;
	u08 SubFormat[16];
};
#pragma pack(pop)

internal v2
TopDownAlign(loaded_bitmap* Bitmap, v2 Align)
{
	Align.y = (f32)(Bitmap->Height - 1) - Align.y;

	Align.x = SafeRatio0(Align.x, (f32)Bitmap->Width);
	Align.y = SafeRatio0(Align.y, (f32)Bitmap->Height);

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
		u32* Memory = (u32*)((u08*)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Memory;
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		Result.AlignPercentage = AlignPercentage;
		Result.WidthOverHeight = SafeRatio0((f32)Header->Width, (f32)Header->Height);

		Assert(Result.Height > 0);
		Assert(Header->Compression == 3);

		// NOTE: If you are using this generically for some reason,
		// please remember that BMP files CAN GO IN EITHER DIRECTION and 
		// the height will be negative for top-down.
		// (Also, there can be compression, etc, etc... Don't think this
		// is complete BMP loading code, because it isn't!!

		// NOTE: Byte order in memory is determined by the Header itself, 
		// so we have to read out the masks and convert the pixels ourselves.
		s32 RedMask = Header->RedMask;
		s32 GreenMask = Header->GreenMask;
		s32 BlueMask = Header->BlueMask;
		s32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		s32 AlphaShiftDown = AlphaScan.Index;
		s32 RedShiftDown = RedScan.Index;
		s32 GreenShiftDown = GreenScan.Index;
		s32 BlueShiftDown = BlueScan.Index;

		u32* SourceDest = Memory;
		for (s32 y = 0; y < Header->Height; ++y)
		{
			for (s32 x = 0; x < Header->Width; ++x)
			{
				u32 C = *SourceDest;

				v4 Texel = V4(
					(f32)((C & RedMask) >> RedShiftDown),
					(f32)((C & GreenMask) >> GreenShiftDown),
					(f32)((C & BlueMask) >> BlueShiftDown),
					(f32)((C & AlphaMask) >> AlphaShiftDown)
				);

				Texel = SRGB255ToLinear1(Texel);
#if 1
				Texel.rgb *= Texel.a;
#endif
				Texel = Linear1ToSRGB255(Texel);


				*SourceDest++ = (((u32)(Texel.a + 0.5f) << 24) |
					((u32)(Texel.r + 0.5f) << 16) |
					((u32)(Texel.g + 0.5f) << 8) |
					((u32)(Texel.b + 0.5f) << 0));
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
	u08* At;
	u08* Stop;
};

inline riff_iterator
ParseChunkAt(void* At, void* Stop)
{
	riff_iterator Iter;

	Iter.At = (u08*)At;
	Iter.Stop = (u08*)Stop;

	return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	u32 Size = (Chunk->Size + 1) & ~1;
	Iter.At += sizeof(WAVE_chunk) + Size;

	return(Iter);
}

inline b32
IsValid(riff_iterator Iter)
{
	b32 Result = (Iter.At < Iter.Stop);

	return(Result);
}

inline void*
GetChunkData(riff_iterator Iter)
{
	void* Result = (Iter.At + sizeof(WAVE_chunk));

	return(Result);
}

inline u32
GetType(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	u32 Result = Chunk->ID;

	return(Result);
}

inline u32
GetChunkDataSize(riff_iterator Iter)
{
	WAVE_chunk* Chunk = (WAVE_chunk*)Iter.At;
	u32 Result = Chunk->Size;

	return(Result);
}

internal loaded_sound
DEBUGLoadWAV(char* Filename, u32 SectionFirstSampleIndex, u32 SectionSampleCount)
{
	loaded_sound Result = {};
	                                                                                                                                                                                                                                                                                           
	debug_read_file_result ReadResult = PlatformReadEntireFile(Filename);
	if (ReadResult.ContentsSize != 0)
	{
		WAVE_header* Header = (WAVE_header*)ReadResult.Contents;
		Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
		Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

		u32 ChannelCount = 0;
		u32 SampleDataSize = 0;
		s16* SampleData = 0;
		for (riff_iterator Iter = ParseChunkAt(Header + 1, (u08*)(Header + 1) + Header->Size - 4); IsValid(Iter); Iter = NextChunk(Iter))
		{
			switch (GetType(Iter))
			{
				case WAVE_ChunkID_fmt:
				{
					WAVE_fmt* fmt = (WAVE_fmt*)GetChunkData(Iter);
					Assert(fmt->wFormatTag == 1); // NOTE: Only support PCM
					Assert((fmt->nSamplesPerSec == 44100));
					Assert((fmt->wBitsPerSample == 16));
					Assert(fmt->nBlockAlign == (sizeof(s16) * fmt->nChannels));
					ChannelCount = fmt->nChannels;
				} break;

				case WAVE_ChunkID_data:
				{
					SampleData = (s16*)GetChunkData(Iter);
					SampleDataSize = GetChunkDataSize(Iter);
				} break;
			}
		}
		
		Assert(ChannelCount && SampleData);

		Result.ChannelCount = ChannelCount;
		Result.SampleCount = SampleDataSize / (ChannelCount * sizeof(s16));
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

			for (u32 SampleIndex = 0; SampleIndex < Result.SampleCount; ++SampleIndex)
			{
				s16 Source = SampleData[2 * SampleIndex];
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
		if (SectionSampleCount)
		{
			Assert((SectionFirstSampleIndex + SectionSampleCount) <= Result.SampleCount);
			Result.SampleCount = SectionSampleCount;
			for (u32 ChannelIndex = 0; ChannelIndex < Result.ChannelCount; ++ChannelIndex)
			{
				Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
			}
		}
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
	if (ID.Value && (AtomicCompareExchangeUInt32((u32*)&Assets->Bitmaps[ID.Value].State, (u32)asset_state::Queued, (u32)asset_state::Unloaded) == (u32)asset_state::Unloaded))
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
	*Work->Sound = DEBUGLoadWAV(Info->Filename, Info->FirstSampleIndex, Info->SampleCount);

	CompletePreviousWritesBeforeFutureWrites;

	Work->Assets->Sounds[Work->ID.Value].Sound = Work->Sound;
	Work->Assets->Sounds[Work->ID.Value].State = Work->FinalState;

	EndTaskWithMemory(Work->Task);
}

internal void
LoadSound(game_assets* Assets, sound_id ID)
{
	if (ID.Value && (AtomicCompareExchangeUInt32((u32*)&Assets->Sounds[ID.Value].State, (u32)asset_state::Queued, (u32)asset_state::Unloaded) == (u32)asset_state::Unloaded))
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
		for (u32 TagIndex = Asset->FirstTagIndex; TagIndex < Asset->OnePastLastTagIndex; ++TagIndex)
		{
			asset_tag* Tag = Assets->Tags + TagIndex;
			
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
			Result = Asset->SlotID;
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

		asset* Asset = Assets->Assets + Type->FirstAssetIndex + Choice;
		Result = Asset->SlotID;
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
DEBUGAddSoundInfo(game_assets* Assets, char* Filename, u32 FirstSampleIndex, u32 SampleCount)
{
	Assert(Assets->DEBUGUsedSoundCount < Assets->SoundCount);
	sound_id ID = { Assets->DEBUGUsedSoundCount++ };

	asset_sound_info* Info = Assets->SoundInfos + ID.Value;
	Info->Filename = Filename;
	Info->FirstSampleIndex = FirstSampleIndex;
	Info->SampleCount = SampleCount;
	Info->NextIDToPlay.Value = 0;

	return(ID);
}

internal void
BeginAssetType(game_assets* Assets, asset_type_id TypeID)
{
	Assert(Assets->DEBUGAssetType == 0);
	Assets->DEBUGAssetType = Assets->AssetTypes + (u32)TypeID;
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

internal asset*
AddSoundAsset(game_assets* Assets, char* Filename, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

	asset* Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddSoundInfo(Assets, Filename, FirstSampleIndex, SampleCount).Value;

	Assets->DEBUGAsset = Asset;

	return(Asset);
}

internal void
AddTag(game_assets* Assets, asset_tag_id TagID, f32 Value)
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

	for (u32 TagType = 0; TagType < (u32)asset_tag_id::Count; ++TagType)
	{
		Assets->TagRange[TagType] = 1000000.0f;
	}
	Assets->TagRange[(u32)asset_tag_id::FacingDirection] = Tau32;

	Assets->BitmapCount = 256 * (u32)asset_type_id::Count;
	Assets->BitmapInfos = PushArray(Arena, Assets->BitmapCount, asset_bitmap_info);
	Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

	Assets->SoundCount = 256 * (u32)asset_type_id::Count;
	Assets->SoundInfos = PushArray(Arena, Assets->SoundCount, asset_sound_info);
	Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

	Assets->AssetCount = Assets->SoundCount + Assets->BitmapCount;
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

	Assets->TagCount = 1024 * (u32)asset_type_id::Count;
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

	f32 AngleRight = 0.0f * Pi32;
	f32 AngleBack = 0.5f * Pi32;
	f32 AngleLeft = 1.0f * Pi32;
	f32 AngleFront = 1.5f * Pi32;

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

	u32 OneMusicChunk = 3 * 44100;
	u32 TotalMusicSampleCount = 308532;
	BeginAssetType(Assets, asset_type_id::Music);
	asset* LastMusic = 0;
	for (u32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount = FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}

		asset* ThisMusic = AddSoundAsset(Assets, (char*)"../data/audio/music_test.wav", FirstSampleIndex, SampleCount);
		if (LastMusic)
		{
			Assets->SoundInfos[LastMusic->SlotID].NextIDToPlay.Value = ThisMusic->SlotID;
		}
		LastMusic = ThisMusic;
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Puhp);
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_00.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_01.wav");
	EndAssetType(Assets);

	return(Assets);
}