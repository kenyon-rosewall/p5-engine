
#include "asset_builder.h"

#define USE_FONTS_FROM_WINDOWS 1

#if USE_FONTS_FROM_WINDOWS
#include <Windows.h>
#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

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

internal loaded_bitmap
LoadGlyphBitmap(char* Filename, wchar_t* FontName, u32 Codepoint, p5a_asset* Asset)
{
	loaded_bitmap Result = {};

#if USE_FONTS_FROM_WINDOWS
	i32 MaxWidth = 1024;
	i32 MaxHeight = 1024;
	// TOOD: These won't work for extracting multiple fonts
	// The font has to be part of the spec when we call LoadGlyphBitmap
	static VOID* Bits = 0;
	static HDC DeviceContext = 0;
	static TEXTMETRIC TextMetric;
	if (!DeviceContext)
	{
		AddFontResourceExA(Filename, FR_PRIVATE, 0);
		i32 Height = 128; // TODO: Figure out how to specify pixels properly here
		HFONT Font = CreateFont(Height, 0, 0, 0,
			FW_NORMAL, // NOTE: Weight
			FALSE, // NOTE: Italic
			FALSE, // NOTE: Underline
			FALSE, // NOTE: Strikeout
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 
			DEFAULT_PITCH | FF_DONTCARE, FontName);

		DeviceContext = CreateCompatibleDC(GetDC(0));

		BITMAPINFO Info = {};
		Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
		Info.bmiHeader.biWidth = MaxWidth;
		Info.bmiHeader.biHeight = MaxHeight;
		Info.bmiHeader.biPlanes = 1;
		Info.bmiHeader.biBitCount = 32;
		Info.bmiHeader.biCompression = BI_RGB;
		Info.bmiHeader.biSizeImage = 0;
		Info.bmiHeader.biXPelsPerMeter = 0;
		Info.bmiHeader.biYPelsPerMeter= 0;
		Info.bmiHeader.biClrUsed = 0;
		Info.bmiHeader.biClrImportant = 0;
		HBITMAP Bitmap = CreateDIBSection(DeviceContext, &Info, DIB_RGB_COLORS, &Bits, 0, 0);

		SelectObject(DeviceContext, Bitmap);
		SelectObject(DeviceContext, Font);
		SetBkColor(DeviceContext, RGB(0, 0, 0));

		GetTextMetrics(DeviceContext, &TextMetric);
	}

	memset(Bits, 0xFF, MaxWidth * MaxHeight * sizeof(u32));
		
	wchar_t CheesePoint = (wchar_t)Codepoint;

	SIZE Size;
	GetTextExtentPoint32W(DeviceContext, &CheesePoint, 1, &Size);

	i32 BoundWidth = Size.cx;
	if (BoundWidth > MaxWidth)
	{
		BoundWidth = MaxWidth;
	}
	i32 BoundHeight = Size.cy;
	if (BoundHeight > MaxHeight)
	{
		BoundHeight = MaxHeight;
	}

	SetTextColor(DeviceContext, RGB(255, 255, 255));
	TextOutW(DeviceContext, 0, 0, &CheesePoint, 1);

	i32 MinX = 10000;
	i32 MinY = 10000;
	i32 MaxX = -10000;
	i32 MaxY = -10000;

	u32* Row = (u32*)Bits + (MaxHeight - 1) * MaxWidth;
	for (i32 Y = 0;
		 Y < BoundHeight;
		 ++Y)
	{
		u32* Pixel = Row;
		for (i32 X = 0;
			 X < BoundWidth;
			 ++X)
		{
#if 0
			COLORREF RefPixel = GetPixel(DeviceContext, X, Y);
			Assert(RefPixel == *Pixel);
#endif
			if (*Pixel != 0)
			{
				if (MinX > X)
				{
					MinX = X;
				}

				if (MinY > Y)
				{
					MinY = Y;
				}

				if (MaxX < X)
				{
					MaxX = X;
				}

				if (MaxY < Y)
				{
					MaxY = Y;
				}
			}

			++Pixel;
		}
		
		Row -= MaxWidth;
	}

	if (MinX <= MaxX)
	{
		i32 Width = (MaxX - MinX) + 1;
		i32 Height = (MaxY - MinY) + 1; 
		
		Result.Width = Width + 2;
		Result.Height = Height + 2;
		Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
		Result.Memory = malloc(Result.Height * Result.Pitch);
		Result.Free = Result.Memory;

		memset(Result.Memory, 0, Result.Height * Result.Pitch);

		u8* DestRow = (u8*)Result.Memory + (Result.Height - 1 - 1) * Result.Pitch;
		u32* SourceRow = (u32*)Bits + (MaxHeight - 1 - MinY) * MaxWidth;
		for (i32 Y = MinY;
			 Y <= MaxY;
			 ++Y)
		{
			u32* Source = (u32*)SourceRow + MinX;
			u32* Dest = (u32*)DestRow + 1;
			for (i32 X = MinX;
				 X <= MaxX;
				 ++X)
			{
#if 0
				COLORREF Pixel = GetPixel(DeviceContext, X, Y);
				Assert(Pixel == *Source);
#else
				u32 Pixel = *Source;
#endif
				f32 Gray = (f32)(Pixel & 0xFF);
				v4 Texel = V4(255, 255, 255, Gray);
				Texel = SRGB255ToLinear1(Texel);
				Texel.rgb *= Texel.a;
				Texel = Linear1ToSRGB255(Texel);

				*Dest++ = (((u32)(Texel.a + 0.5f) << 24) |
						   ((u32)(Texel.r + 0.5f) << 16) |
						   ((u32)(Texel.g + 0.5f) <<  8) |
						   ((u32)(Texel.b + 0.5f) <<  0));

				++Source;
			}

			DestRow -= Result.Pitch;
			SourceRow -= MaxWidth;
		}

		Asset->Bitmap.AlignPercentage[0] = 1.0f / (f32)Result.Width;
		Asset->Bitmap.AlignPercentage[1] = (1.0f + (MaxY - (BoundHeight - TextMetric.tmDescent))) / (f32)Result.Height;
	}
#else

	entire_file TTFFile = ReadEntireFile(Filename);
	if (TTFFile.ContentsSize != 0)
	{
		stbtt_fontinfo Font;
		stbtt_InitFont(&Font, (u8*)TTFFile.Contents, stbtt_GetFontOffsetForIndex((u8*)TTFFile.Contents, 0));

		i32 Width, Height, XOffset, YOffset;
		u8* MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, stbtt_ScaleForPixelHeight(&Font, 128.0f), Codepoint, &Width, &Height, &XOffset, &YOffset);

		Result.Width = Width;
		Result.Height = Height;
		Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
		Result.Memory = malloc(Height * Result.Pitch);
		Result.Free = Result.Memory;

		u8* Source = MonoBitmap;
		u8* DestRow = (u8*)Result.Memory + (Height - 1) * Result.Pitch;
		for (i32 Y = 0;
			 Y < Height;
			 ++Y)
		{
			u32* Dest = (u32*)DestRow;
			for (i32 X = 0;
				 X < Width;
				 ++X)
			{
				u8 Gray = *Source++;
				u8 Alpha = 0xFF;
				*Dest++ = ((Alpha << 24) |
						   (Gray  << 16) |
						   (Gray  << 8) |
						   (Gray  << 0));
			}

			DestRow -= Result.Pitch;
		}
		stbtt_FreeBitmap(MonoBitmap, 0);

		free(TTFFile.Contents);
	}
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

internal bitmap_id
AddCharacterAsset(game_assets* Assets, char* FontFile, wchar_t* FontName, u32 Codepoint, f32 AlignPercentageX = 0.5f, f32 AlignPercentageY = 0.5f)
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

	Source->Type = asset_type::Font;
	Source->Filename = FontFile;
	Source->FontName = FontName;
	Source->Codepoint = Codepoint;

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
	P5A->Sound.Chain = p5a_sound_chain::None;

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

internal void
WriteP5A(game_assets* Assets, char* Filename)
{
	FILE* Out = fopen(Filename, "wb");
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
				loaded_bitmap Bitmap;
				if (Source->Type == asset_type::Font)
				{
					Bitmap = LoadGlyphBitmap(Source->Filename, Source->FontName, Source->Codepoint, Dest);
				}
				else
				{
					Assert(Source->Type == asset_type::Bitmap);

					Bitmap = LoadBMP(Source->Filename);
				}

				Dest->Bitmap.Dim[0] = Bitmap.Width;
				Dest->Bitmap.Dim[1] = Bitmap.Height;

				Assert((Bitmap.Width * 4) == Bitmap.Pitch);

				fwrite(Bitmap.Memory, Bitmap.Width * Bitmap.Height * 4, 1, Out);
				free(Bitmap.Free);
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

internal void
Initialize(game_assets* Assets)
{
	Assets->TagCount = 1;
	Assets->AssetCount = 1;
	Assets->DEBUGAssetType = 0;
	Assets->AssetIndex = 0;

	Assets->AssetTypeCount = (u32)asset_type_id::Count;
	memset(Assets->AssetTypes, 0, sizeof(Assets->AssetTypes));
}

internal void
WriteHero(void)
{
	game_assets Assets_;
	game_assets* Assets = &Assets_;
	Initialize(Assets);

	f32 AngleRight = 0.0f * Pi32;
	f32 AngleBack = 0.5f * Pi32;
	f32 AngleLeft = 1.0f * Pi32;
	f32 AngleFront = 1.5f * Pi32;

	f32 HeroAlignX = 0.5f;
	f32 HeroAlignY = 0.109375f;

	BeginAssetType(Assets, asset_type_id::Character);
	AddBitmapAsset(Assets, "data/bitmaps/char-right-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "data/bitmaps/char-back-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "data/bitmaps/char-left-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "data/bitmaps/char-front-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	f32 SwordAlignX = 0.5f;
	f32 SwordAlignY = 0.828125f;

	BeginAssetType(Assets, asset_type_id::Sword);
	AddBitmapAsset(Assets, "data/bitmaps/sword-right.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, "data/bitmaps/sword-back.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, "data/bitmaps/sword-left.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, "data/bitmaps/sword-front.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	WriteP5A(Assets, "data/assets1.p5a");
}

internal void
WriteNonHero(void)
{
	game_assets Assets_;
	game_assets* Assets = &Assets_;
	Initialize(Assets);

	BeginAssetType(Assets, asset_type_id::Shadow);
	AddBitmapAsset(Assets, "data/bitmaps/shadow.bmp", 0.5f, 1.09090912f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tree);
	AddBitmapAsset(Assets, "data/bitmaps/tree.bmp", 0.5f, 0.340425521f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Monstar);
	AddBitmapAsset(Assets, "data/bitmaps/enemy.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Familiar);
	AddBitmapAsset(Assets, "data/bitmaps/orb.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Grass);
	AddBitmapAsset(Assets, "data/bitmaps/grass1.bmp", 0.5f, 0.5f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Soil);
	AddBitmapAsset(Assets, "data/bitmaps/soil1.bmp");
	AddBitmapAsset(Assets, "data/bitmaps/soil2.bmp");
	AddBitmapAsset(Assets, "data/bitmaps/soil4.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tuft);
	AddBitmapAsset(Assets, "data/bitmaps/tuft1.bmp");
	AddBitmapAsset(Assets, "data/bitmaps/tuft2.bmp");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Font);
	for (u32 Character = '!';
		 Character <= '~';
		 ++Character)
	{
		// AddCharacterAsset(Assets, "c:/Windows/Fonts/arial.ttf", L"Arial", Character);
		AddCharacterAsset(Assets, "c:/Windows/Fonts/cour.ttf", L"Courier New", Character);
		AddTag(Assets, asset_tag_id::UnicodeCodepoint, (f32)Character);
	}
	EndAssetType(Assets);

	WriteP5A(Assets, "data/assets2.p5a");
}

internal void
WriteSounds(void)
{
	game_assets Assets_;
	game_assets* Assets = &Assets_;
	Initialize(Assets);

	BeginAssetType(Assets, asset_type_id::Bloop);
	AddSoundAsset(Assets, "data/audio/bloop_00.wav");
	AddSoundAsset(Assets, "data/audio/bloop_01.wav");
	AddSoundAsset(Assets, "data/audio/bloop_02.wav");
	AddSoundAsset(Assets, "data/audio/bloop_03.wav");
	AddSoundAsset(Assets, "data/audio/bloop_04.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Crack);
	AddSoundAsset(Assets, "data/audio/crack_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Drop);
	AddSoundAsset(Assets, "data/audio/drop_00.wav");
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Glide);
	AddSoundAsset(Assets, "data/audio/glide_00.wav");
	EndAssetType(Assets);

	u32 OneMusicChunk = 10 * 44100;
	u32 TotalMusicSampleCount = 13582800;
	BeginAssetType(Assets, asset_type_id::Music);
	for (u32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount = FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}

		sound_id ThisMusic = AddSoundAsset(Assets, "data/audio/music_test.wav", FirstSampleIndex, SampleCount);
		if ((FirstSampleIndex + OneMusicChunk) < TotalMusicSampleCount)
		{
			Assets->Assets[ThisMusic.Value].Sound.Chain = p5a_sound_chain::Advance;
		}
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Puhp);
	AddSoundAsset(Assets, "data/audio/puhp_00.wav");
	AddSoundAsset(Assets, "data/audio/puhp_01.wav");
	EndAssetType(Assets);

	WriteP5A(Assets, "data/assets3.p5a");
}

int
main(int ArgCount, char** Args)
{
	WriteHero();
	WriteNonHero();
	WriteSounds();
}