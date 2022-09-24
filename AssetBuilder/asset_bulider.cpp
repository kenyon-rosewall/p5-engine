
#include "asset_builder.h"

FILE* Out = 0;

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

	asset* Asset = Assets->Assets + Result.Value;
	Asset->FirstTagIndex = Assets->TagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->Bitmap.Filename = Filename;
	Asset->Bitmap.AlignPercentage[0] = AlignPercentageX;
	Asset->Bitmap.AlignPercentage[1] = AlignPercentageY;

	Assets->DEBUGAsset = Asset;

	return(Result);
}

internal sound_id
AddSoundAsset(game_assets* Assets, char* Filename, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
	Assert(Assets->DEBUGAssetType);
	Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

	sound_id Result = { Assets->DEBUGAssetType->OnePastLastAssetIndex++ };

	asset* Asset = Assets->Assets + Result.Value;
	Asset->FirstTagIndex = Assets->TagCount;
	Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
	Asset->Sound.Filename = Filename;
	Asset->Sound.FirstSampleIndex = FirstSampleIndex;
	Asset->Sound.SampleCount = SampleCount;
	Asset->Sound.NextIDToPlay.Value = 0;

	Assets->DEBUGAsset = Asset;

	return(Result);
}

internal void
AddTag(game_assets* Assets, asset_tag_id TagID, f32 Value)
{
	Assert(Assets->DEBUGAsset);

	++Assets->DEBUGAsset->OnePastLastTagIndex;
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
	Assets->DEBUGAsset = 0;
}

int
main(int ArgCount, char** Args)
{
	game_assets Assets_;
	game_assets* Assets = &Assets_;

	Assets->TagCount = 1;
	Assets->AssetCount = 1;
	Assets->DEBUGAssetType = 0;
	Assets->DEBUGAsset = 0;

	BeginAssetType(Assets, asset_type_id::Shadow);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/shadow.bmp", 0.5f, 1.09090912f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Tree);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/tree.bmp", 0.5f, 0.340425521f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Monstar);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/enemy.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Familiar);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/orb.bmp", 0.5f, 0.0f);
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Grass);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/grass1.bmp", 0.5f, 0.5f);
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

	f32 HeroAlignX = 0.5f;
	f32 HeroAlignY = 0.109375f;

	BeginAssetType(Assets, asset_type_id::Character);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-right-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-back-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-left-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/char-front-0.bmp", HeroAlignX, HeroAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleFront);
	EndAssetType(Assets);

	f32 SwordAlignX = 0.5f;
	f32 SwordAlignY = 0.828125f;

	BeginAssetType(Assets, asset_type_id::Sword);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-right.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleRight);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-back.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleBack);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-left.bmp", SwordAlignX, SwordAlignY);
	AddTag(Assets, asset_tag_id::FacingDirection, AngleLeft);
	AddBitmapAsset(Assets, (char*)"../data/bitmaps/sword-front.bmp", SwordAlignX, SwordAlignY);
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
	sound_id LastMusic = { 0 };
	for (u32 FirstSampleIndex = 0; FirstSampleIndex < TotalMusicSampleCount; FirstSampleIndex += OneMusicChunk)
	{
		u32 SampleCount = TotalMusicSampleCount = FirstSampleIndex;
		if (SampleCount > OneMusicChunk)
		{
			SampleCount = OneMusicChunk;
		}

		sound_id ThisMusic = AddSoundAsset(Assets, (char*)"../data/audio/music_test.wav", FirstSampleIndex, SampleCount);
		if (LastMusic.Value)
		{
			Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
		}
		LastMusic = ThisMusic;
	}
	EndAssetType(Assets);

	BeginAssetType(Assets, asset_type_id::Puhp);
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_00.wav");
	AddSoundAsset(Assets, (char*)"../data/audio/puhp_01.wav");
	EndAssetType(Assets);

	Out = fopen("data/test.p5a", "wb");
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
		Header.AssetTypes = Header.Tags + Header.TagCount * sizeof(p5a_tag);
		Header.Assets = Header.AssetTypes + Header.AssetTypeCount * sizeof(p5a_asset_type);

		fwrite(&Header, sizeof(Header), 1, Out);
		fwrite(Assets->Tags, TagArraySize, 1, Out);
		fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
		// fwrite(AssetsArray, AssetArraySize, 1, Out);

		fclose(Out);
	}
	else
	{
		printf("ERROR: Couldn't open file :(\n");
	}
}