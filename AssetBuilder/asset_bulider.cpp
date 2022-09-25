
#include "asset_builder.h"

#pragma pack(push, 1)
struct bitmap_header
{
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	i32 Width;
	i32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 SizeOfBitmap;
	i32 HorzResolution;
	i32 VertResolution;
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
	u8 SubFormat[16];
};
#pragma pack(pop)

struct loaded_bitmap
{
	i32 Width;
	i32 Height;
	i32 Pitch;
	void* Memory;

	void* Free;
};

struct entire_file
{
	u32 ContentsSize;
	void* Contents;
};

entire_file
ReadEntireFile(char* Filename)
{
	entire_file Result = {};

	FILE* In = fopen(Filename, "rb");
	if (In)
	{
		fseek(In, 0, SEEK_END);
		Result.ContentsSize = ftell(In);
		fseek(In, 0, SEEK_SET);

		Result.Contents = malloc(Result.ContentsSize);
		fread(Result.Contents, Result.ContentsSize, 1, In);
		fclose(In);
	}
	else
	{
		printf("ERROR: Cannot open file %s.\n", Filename);
	}

	return(Result);
}

internal loaded_bitmap
LoadBMP(char* Filename)
{
	loaded_bitmap Result = {};

	entire_file ReadResult = ReadEntireFile(Filename);
	if (ReadResult.ContentsSize != 0)
	{
		Result.Free = ReadResult.Contents;

		bitmap_header* Header = (bitmap_header*)ReadResult.Contents;
		u32* Memory = (u32*)((u8*)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Memory;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		Assert(Result.Height > 0);
		Assert(Header->Compression == 3);

		// NOTE: If you are using this generically for some reason,
		// please remember that BMP files CAN GO IN EITHER DIRECTION and 
		// the height will be negative for top-down.
		// (Also, there can be compression, etc, etc... Don't think this
		// is complete BMP loading code, because it isn't!!

		// NOTE: Byte order in memory is determined by the Header itself, 
		// so we have to read out the masks and convert the pixels ourselves.
		i32 RedMask = Header->RedMask;
		i32 GreenMask = Header->GreenMask;
		i32 BlueMask = Header->BlueMask;
		i32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		i32 AlphaShiftDown = AlphaScan.Index;
		i32 RedShiftDown = RedScan.Index;
		i32 GreenShiftDown = GreenScan.Index;
		i32 BlueShiftDown = BlueScan.Index;

		u32* SourceDest = Memory;
		for (i32 y = 0; y < Header->Height; ++y)
		{
			for (i32 x = 0; x < Header->Width; ++x)
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
								 ((u32)(Texel.g + 0.5f) <<  8) |
								 ((u32)(Texel.b + 0.5f) <<  0));
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
	u8* At;
	u8* Stop;
};

inline riff_iterator
ParseChunkAt(void* At, void* Stop)
{
	riff_iterator Iter;

	Iter.At = (u8*)At;
	Iter.Stop = (u8*)Stop;

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

struct loaded_sound
{
	u32 SampleCount; // NOTE: This is the sample count divided by 8
	u32 ChannelCount;
	i16* Samples[2];
	void* Free;
};

internal loaded_sound
LoadWAV(char* Filename, u32 SectionFirstSampleIndex, u32 SectionSampleCount)
{
	loaded_sound Result = {};

	entire_file ReadResult = ReadEntireFile(Filename);
	if (ReadResult.ContentsSize != 0)
	{
		Result.Free = ReadResult.Contents;

		WAVE_header* Header = (WAVE_header*)ReadResult.Contents;
		Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
		Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

		u32 ChannelCount = 0;
		u32 SampleDataSize = 0;
		i16* SampleData = 0;
		for (riff_iterator Iter = ParseChunkAt(Header + 1, (u8*)(Header + 1) + Header->Size - 4); IsValid(Iter); Iter = NextChunk(Iter))
		{
			switch (GetType(Iter))
			{
				case WAVE_ChunkID_fmt:
				{
					WAVE_fmt* fmt = (WAVE_fmt*)GetChunkData(Iter);
					Assert(fmt->wFormatTag == 1); // NOTE: Only support PCM
					Assert((fmt->nSamplesPerSec == 44100));
					Assert((fmt->wBitsPerSample == 16));
					Assert(fmt->nBlockAlign == (sizeof(i16) * fmt->nChannels));
					ChannelCount = fmt->nChannels;
				} break;

				case WAVE_ChunkID_data:
				{
					SampleData = (i16*)GetChunkData(Iter);
					SampleDataSize = GetChunkDataSize(Iter);
				} break;
			}
		}

		Assert(ChannelCount && SampleData);

		Result.ChannelCount = ChannelCount;
		u32 SampleCount = SampleDataSize / (ChannelCount * sizeof(i16));
		if (ChannelCount == 1)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = 0;
		}
		else if (ChannelCount == 2)
		{
			Result.Samples[0] = SampleData;
			Result.Samples[1] = SampleData + SampleCount;

#if 0
			for (uint32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				SampleData[2 * SampleIndex + 0] = (int16)SampleIndex;
				SampleData[2 * SampleIndex + 1] = (int16)SampleIndex;
			}
#endif

			for (u32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				i16 Source = SampleData[2 * SampleIndex];
				SampleData[2 * SampleIndex] = SampleData[SampleIndex];
				SampleData[SampleIndex] = Source;
			}
		}
		else
		{
			Assert(!"Invalid channel count in WAV file");
		}

		// TODO: Load right channels
		b32 AtEnd = true;
		Result.ChannelCount = 1;
		if (SectionSampleCount)
		{
			Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
			AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
			SampleCount = SectionSampleCount;
			for (u32 ChannelIndex = 0; ChannelIndex < Result.ChannelCount; ++ChannelIndex)
			{
				Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
			}
		}

		if (AtEnd)
		{
			u32 SampleCountAlign8 = Align8(SampleCount);
			for (u32 ChannelIndex = 0; ChannelIndex < Result.ChannelCount; ++ChannelIndex)
			{
				for (u32 SampleIndex = SampleCount; SampleIndex < (SampleCount + 8); ++SampleIndex)
				{
					Result.Samples[ChannelIndex][SampleIndex] += 0;
				}
			}
		}

		Result.SampleCount = SampleCount;
	}

	return(Result);
}

internal void
BeginAssetType(game_assets* Assets, asset_type_id TypeID)
{
	Assert(Assets->DEBUGAssetType == 0);

	Assets->DEBUGAssetType = Assets->AssetTypes + (u32)TypeID;
	Assets->DEBUGAssetType->TypeID = (u32)TypeID;
	Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
	Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets* Assets, char* Filename, f32 AlignPercentageX = 0.5f, f32 AlignPercentageY = 0.5f)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

	bitmap_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };

	asset_source* Source = Assets->AssetSources + Result.Value;
	p5a_asset* P5A = Assets->Assets + Result.Value;

	P5A->FirstTagIndex = Assets->TagCount;
	P5A->OnePastLastTagIndex = P5A->FirstTagIndex;
	P5A->Bitmap.AlignPercentage[0] = AlignPercentageX;
	P5A->Bitmap.AlignPercentage[1] = AlignPercentageY;

	Source->Type = asset_type::Bitmap;
	Source->Filename = Filename;

	Assets->AssetIndex = Result.Value;

	return(Result);
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

	sound_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };

	asset_source* Source = Assets->AssetSources + Result.Value;
	p5a_asset* P5A = Assets->Assets + Result.Value;

	P5A->FirstTagIndex = Assets->TagCount;
	P5A->OnePastLastTagIndex = P5A->FirstTagIndex;
	P5A->Sound.SampleCount = SampleCount;
	P5A->Sound.NextIDToPlay.Value = 0;

	Source->Type = asset_type::Sound;
	Source->Filename = Filename;
	Source->FirstSampleIndex = FirstSampleIndex;

	Assets->AssetIndex = Result.Value;

	return(Result);
}

internal void
AddTag(game_assets* Assets, asset_tag_id TagID, f32 Value)
{
	Assert(Assets->AssetIndex);

	p5a_asset* P5A = Assets->Assets + Assets->AssetIndex;
	++P5A->OnePastLastTagIndex;
	p5a_tag* Tag = Assets->Tags + Assets->TagCount++;

	Tag->ID = (u32)TagID;
	Tag->Value = Value;
}

internal void
EndAssetType(game_assets* Assets)
{
	Assert(Assets->DEBUGAssetType);

	Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
	Assets->DEBUGAssetType = 0;
	Assets->AssetIndex = 0;
}

int
main(int ArgCount, char** Args)
{
	game_assets Assets_;
	game_assets* Assets = &Assets_;

	Assets->TagCount = 1;
	Assets->AssetCount = 1;
	Assets->DEBUGAssetType = 0;
	Assets->AssetIndex = 0;

	BeginAssetType(Assets, asset_type_id::Shadow);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/shadow.bmp", 0.5f, 1.09090912f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tree);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/tree.bmp", 0.5f, 0.340425521f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Monstar);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/enemy.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Familiar);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/orb.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Grass);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/grass1.bmp", 0.5f, 0.5f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Soil);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/soil1.bmp");
	AddBitmapAsset(Assets, (char*)"data/bitmaps/soil2.bmp");
	AddBitmapAsset(Assets, (char*)"data/bitmaps/soil4.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tuft);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/tuft1.bmp");
	AddBitmapAsset(Assets, (char*)"data/bitmaps/tuft2.bmp");
	EndAssetType(Assets);

	f32 AngleRight = 0.0f * Pi32;
	f32 AngleBack = 0.5f * Pi32;
	f32 AngleLeft = 1.0f * Pi32;
	f32 AngleFront = 1.5f * Pi32;

	f32 HeroAlignX = 0.5f;
	f32 HeroAlignY = 0.109375f;

	BeginAssetType(Assets, asset_type_id::Character);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/char-right-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/char-back-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/char-left-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/char-front-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	f32 SwordAlignX = 0.5f;
	f32 SwordAlignY = 0.828125f;

	BeginAssetType(Assets, asset_type_id::Sword);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/sword-right.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/sword-back.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/sword-left.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"data/bitmaps/sword-front.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	//
	// SOUNDS
	//

	BeginAssetType(Assets, asset_type_id::Bloop);
	AddSoundAsset(Assets, (char*)"data/audio/bloop_00.wav");
	AddSoundAsset(Assets, (char*)"data/audio/bloop_01.wav");
	AddSoundAsset(Assets, (char*)"data/audio/bloop_02.wav");
	AddSoundAsset(Assets, (char*)"data/audio/bloop_03.wav");
	AddSoundAsset(Assets, (char*)"data/audio/bloop_04.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Crack);
	AddSoundAsset(Assets, (char*)"data/audio/crack_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Drop);
	AddSoundAsset(Assets, (char*)"data/audio/drop_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Glide);
	AddSoundAsset(Assets, (char*)"data/audio/glide_00.wav");
	EndAssetType(Assets);

	u32 OneMusicChunk = 10 * 44100;
	u32 TotalMusicSampleCount = 13582800;
	BeginAssetType(Assets, asset_type_id::Music);
	sound_id LastMusic = { 0 };
	for (u32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount = FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}

		sound_id ThisMusic = AddSoundAsset(Assets, (char*)"data/audio/music_test.wav", FirstSampleIndex, SampleCount);
		if (LastMusic.Value)
		{
			Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
		}
		LastMusic = ThisMusic;
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Puhp);
	AddSoundAsset(Assets, (char*)"data/audio/puhp_00.wav");
	AddSoundAsset(Assets, (char*)"data/audio/puhp_01.wav");
	EndAssetType(Assets);

	FILE* Out = fopen("data/assets.p5a", "wb");
	if (Out)
	{
		p5a_header Header = {};
		Header.MagicValue = P5A_MAGIC_VALUE;
		Header.Version = P5A_VERSION;
		Header.TagCount = Assets->TagCount;
		Header.AssetTypeCount = (u32)asset_type_id::Count; // TODO: Do we really want to do this? Sparseness.
		Header.AssetCount = Assets->AssetCount;

		u32 TagArraySize = Header.TagCount * sizeof(p5a_tag);
		u32 AssetTypeArraySize = Header.AssetTypeCount * sizeof(p5a_asset_type);
		u32 AssetArraySize = Header.AssetCount * sizeof(p5a_asset);

		Header.Tags = sizeof(Header);
		Header.AssetTypes = Header.Tags + TagArraySize;
		Header.Assets = Header.AssetTypes + AssetTypeArraySize;

		fwrite(&Header, sizeof(Header), 1, Out);
		fwrite(Assets->Tags, TagArraySize, 1, Out);
		fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
		fseek(Out, AssetArraySize, SEEK_CUR);
		for (u32 AssetIndex = 1; AssetIndex < Assets->AssetCount; ++AssetIndex)
		{
			asset_source* Source = Assets->AssetSources + AssetIndex;
			p5a_asset* Dest = Assets->Assets + AssetIndex;

			Dest->DataOffset = ftell(Out);

			if (Source->Type == asset_type::Sound)
			{
				loaded_sound WAV = LoadWAV(Source->Filename, Source->FirstSampleIndex, Dest->Sound.SampleCount);

				Dest->Sound.SampleCount = WAV.SampleCount;
				Dest->Sound.ChannelCount = WAV.ChannelCount;
				for (u32 ChannelIndex = 0; ChannelIndex < WAV.ChannelCount; ++ChannelIndex)
				{
					fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount * sizeof(i16), 1, Out);
				}

				free(WAV.Free);
			}
			else
			{
				Assert(Source->Type == asset_type::Bitmap);

				loaded_bitmap BMP = LoadBMP(Source->Filename);

				Dest->Bitmap.Dim[0] = BMP.Width;
				Dest->Bitmap.Dim[1] = BMP.Height;

				Assert((BMP.Width * 4) == BMP.Pitch);
				fwrite(BMP.Memory, BMP.Width * BMP.Height * 4, 1, Out);

				free(BMP.Free);
			}
		}
		fseek(Out, (u32)Header.Assets, SEEK_SET);
		fwrite(Assets->Assets, AssetArraySize, 1, Out);

		fclose(Out);
	}
	else
	{
		printf("ERROR: Couldn't open file :(\n");
	}
}