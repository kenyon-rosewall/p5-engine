
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
#pragma pack(pop)

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
	if (ID.Value && (AtomicCompareExchangeUInt32((uint32*)&Assets->Bitmaps[ID.Value].State, (uint32)asset_state::Unloaded, (uint32)asset_state::Queued) == (uint32)asset_state::Unloaded))
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
	}
}

internal void
LoadSound()
{
	
}

internal bitmap_id
RandomAssetFrom(game_assets* Assets, asset_type_id TypeID, random_series* Series)
{
	bitmap_id Result = {};

	asset_type* Type = Assets->AssetTypes + (uint32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
		uint32 Choice = RandomChoice(Series, Count);

		asset* Asset = Assets->Assets + Type->FirstAssetIndex + Choice;
		Result.Value = Asset->SlotID;
	}

	return(Result);
}

internal bitmap_id
GetFirstBitmapID(game_assets* Assets, asset_type_id TypeID)
{
	bitmap_id Result = {};
	
	asset_type* Type = Assets->AssetTypes + (uint32)TypeID;
	if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
	{
		asset* Asset = Assets->Assets + Type->FirstAssetIndex;
		Result.Value = Asset->SlotID;
	}

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
	asset* Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = 0;
	Asset->OnePastLastTagIndex = 0;
	Asset->SlotID = DEBUGAddBitmapInfo(Assets, Filename, AlignPercentage).Value;
}

internal void
EndAssetType(game_assets* Assets)
{
	Assert(Assets->DEBUGAssetType);
	Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
	Assets->DEBUGAssetType = 0;
}

internal game_assets*
AllocateGameAssets(memory_arena* Arena, memory_index Size, transient_state* TransientState)
{
	game_assets* Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TransientState = TransientState;

	Assets->BitmapCount = 256 * (uint32)asset_type_id::Count;
	Assets->DEBUGUsedBitmapCount = 1;
	Assets->BitmapInfos = PushArray(Arena, Assets->BitmapCount, asset_bitmap_info);
	Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

	Assets->SoundCount = 1;
	Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

	Assets->AssetCount = Assets->SoundCount + Assets->BitmapCount;
	Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

	Assets->DEBUGUsedBitmapCount = 1;
	Assets->DEBUGUsedBitmapCount = 1;

	BeginAssetType(Assets, asset_type_id::Shadow);
	AddBitmapAsset(Assets, (char*)"../data/shadow.bmp", V2(0.5f, 1.09090912f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tree);
	AddBitmapAsset(Assets, (char*)"../data/tree.bmp", V2(0.5f, 0.340425521f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Monstar);
	AddBitmapAsset(Assets, (char*)"../data/enemy.bmp", V2(0.5f, 0.0f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Familiar);
	AddBitmapAsset(Assets, (char*)"../data/orb.bmp", V2(0.5f, 0.0f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Grass);
	AddBitmapAsset(Assets, (char*)"../data/grass1.bmp", V2(0.5f, 0.5f));
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Soil);
	AddBitmapAsset(Assets, (char*)"../data/soil1.bmp");
	AddBitmapAsset(Assets, (char*)"../data/soil2.bmp");
	AddBitmapAsset(Assets, (char*)"../data/soil4.bmp");
	EndAssetType(Assets);
	
	BeginAssetType(Assets, asset_type_id::Tuft);
	AddBitmapAsset(Assets, (char*)"../data/tuft1.bmp");
	AddBitmapAsset(Assets, (char*)"../data/tuft2.bmp");
	EndAssetType(Assets);

	hero_bitmaps* HeroBitmap;

	HeroBitmap = Assets->Hero;
	HeroBitmap->Character = DEBUGLoadBMP((char*)"../data/char-right-0.bmp");
	SetTopDownAlign(HeroBitmap, V2(32, 56));
	++HeroBitmap;
	HeroBitmap->Character = DEBUGLoadBMP((char*)"../data/char-back-0.bmp");
	SetTopDownAlign(HeroBitmap, V2(32, 56));
	++HeroBitmap;
	HeroBitmap->Character = DEBUGLoadBMP((char*)"../data/char-left-0.bmp");
	SetTopDownAlign(HeroBitmap, V2(32, 56));
	++HeroBitmap;
	HeroBitmap->Character = DEBUGLoadBMP((char*)"../data/char-front-0.bmp");
	SetTopDownAlign(HeroBitmap, V2(32, 56));
	++HeroBitmap;

	loaded_bitmap* SwordBitmap;

	SwordBitmap = Assets->Sword;
	*SwordBitmap = DEBUGLoadBMP((char*)"../data/sword-right.bmp");
	TopDownAlign(SwordBitmap, V2(32, 10));
	++SwordBitmap;
	*SwordBitmap = DEBUGLoadBMP((char*)"../data/sword-back.bmp");
	TopDownAlign(SwordBitmap, V2(32, 10));
	++SwordBitmap;
	*SwordBitmap = DEBUGLoadBMP((char*)"../data/sword-left.bmp");
	TopDownAlign(SwordBitmap, V2(32, 10));
	++SwordBitmap;
	*SwordBitmap = DEBUGLoadBMP((char*)"../data/sword-front.bmp");
	TopDownAlign(SwordBitmap, V2(32, 10));
	++SwordBitmap;

	return(Assets);
}