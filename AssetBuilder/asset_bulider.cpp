
#include <stdio.h>
#include <stdlib.h>
#include "p5engine_platform.h"
#include "p5engine_asset_type_id.h"

FILE* Out = 0;

struct asset_bitmap_info
{
	char* Filename;
	f32 AlignPercentage[2];
};

struct asset_sound_info
{
	char* Filename;
	u32 FirstSampleIndex;
	u32 SampleCount;
	u32 NextIDToPlay;
};

struct asset_tag
{
	asset_tag_id ID;
	f32 Value;
};

struct asset
{
	u64 DataOffset;
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;
};

struct asset_type
{
	u32 FirstAssetIndex;
	u32 OnePastLastAssetIndex;
};

struct bitmap_asset
{
	char* Filename;
	f32 Alignment[2];
};

#define VERY_LARGE_NUMBER 4096

u32 BitmapCount;
u32 SoundCount;
u32 TagCount;
u32 AssetCount;

asset_bitmap_info* BitmapInfos[VERY_LARGE_NUMBER];
asset_sound_info* SoundInfos[VERY_LARGE_NUMBER];
asset_tag* Tags[VERY_LARGE_NUMBER];
asset Assets[VERY_LARGE_NUMBER];
asset_type AssetTypes[(u32)asset_type_id::Count];

u32 DEBUGUsedBitmapCount;
u32 DEBUGUsedSoundCount;
u32 DEBUGUsedAssetCount;
u32 DEBUGUsedTagCount;
asset_type* DEBUGAssetType;
asset* DEBUGAsset;

internal void
BeginAssetType(asset_type_id TypeID)
{
	Assert(DEBUGAssetType == 0);

	DEBUGAssetType = AssetTypes + (u32)TypeID;
	DEBUGAssetType->FirstAssetIndex = DEBUGUsedAssetCount;
	DEBUGAssetType->OnePastLastAssetIndex = DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(char* Filename, f32 AlignPercentageX = 0.5f, f32 AlignPercentageY = 0.5f)
{
	Assert(DEBUGAssetType);
	Assert(DEBUGAssetType->OnePastLastAssetIndex < AssetCount);

	asset* Asset = Assets + DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddBitmapInfo(Filename, AlignPercentageX, AlignPercentageY).Value;

	/*
	internal bitmap_id
		DEBUGAddBitmapInfo(char* Filename, v2 AlignPercentage)
	{
		Assert(DEBUGUsedBitmapCount < BitmapCount);
		bitmap_id ID = { DEBUGUsedBitmapCount++ };

		asset_bitmap_info* Info = BitmapInfos + ID.Value;
		Info->Filename = PushString(&Arena, Filename);
		Info->AlignPercentage = AlignPercentage;

		return(ID);
	}
	*/

	Assets->DEBUGAsset = Asset;
}

internal asset*
AddSoundAsset(char* Filename, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	Assert(DEBUGAssetType);
	Assert(DEBUGAssetType->OnePastLastAssetIndex < AssetCount);

	asset* Asset = Assets + DEBUGAssetType->OnePastLastAssetIndex++;
	Asset->FirstTagIndex = DEBUGUsedTagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->SlotID = DEBUGAddSoundInfo(Filename, FirstSampleIndex, SampleCount).Value;

	/*
	internal sound_id
		DEBUGAddSoundInfo(char* Filename, u32 FirstSampleIndex, u32 SampleCount)
	{
		Assert(DEBUGUsedSoundCount < SoundCount);
		sound_id ID = { DEBUGUsedSoundCount++ };

		asset_sound_info* Info = SoundInfos + ID.Value;
		Info->Filename = PushString(&Arena, Filename);
		Info->FirstSampleIndex = FirstSampleIndex;
		Info->SampleCount = SampleCount;
		Info->NextIDToPlay.Value = 0;

		return(ID);
	}
	*/

	DEBUGAsset = Asset;

	return(Asset);
}

internal void
AddTag(asset_tag_id TagID, f32 Value)
{
	Assert(DEBUGAsset);

	++DEBUGAsset->OnePastLastTagIndex;
	asset_tag* Tag = Tags + DEBUGUsedTagCount++;

	Tag->ID = TagID;
	Tag->Value = Value;
}

internal void
EndAssetType()
{
	Assert(DEBUGAssetType);

	DEBUGUsedAssetCount = DEBUGAssetType->OnePastLastAssetIndex;
	DEBUGAssetType = 0;
	DEBUGAsset = 0;
}


int
main(int ArgCount, char** Args)
{
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

	u32 OneMusicChunk = 10 * 44100;
	u32 TotalMusicSampleCount = 13582800;
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

	Out = fopen("test.p5a", "wb");
	if (Out)
	{


		fclose(Out);
	}
}