#pragma once

#ifndef P5ENGINE_FILE_FORMATS_H
#define P5ENGINE_FILE_FORMATS_H

enum class asset_font_type
{
	Default = 0,
	Debug = 10,
};

enum class asset_tag_id
{
	Smoothness,
	Flatness,
	FacingDirection,
	UnicodeCodepoint,
	FontType, // NOTE: 0 - Default Game Font, 10 - Debug Font?

	Count,
};

enum class asset_type_id
{
	None,

	//
	// NOTE: Bitmaps
	//

	Shadow,
	Tree,
	Monstar,
	Familiar,
	Grass,

	Soil,
	Tuft,

	Character,
	Sword,

	Font,
	FontGlyph, 

	//
	// NOTE: Sounds
	//

	Bloop,
	Crack,
	Drop,
	Glide,
	Music,
	Puhp,

	Count,
};

#define P5A_CODE(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#pragma pack(push, 1)
struct bitmap_id
{
	u32 Value;
};

struct sound_id
{
	u32 Value;
};

struct font_id
{
	u32 Value;
};

struct p5a_header
{
#define P5A_MAGIC_VALUE P5A_CODE('p', '5', 'a', 'f')
	u32 MagicValue;

#define P5A_VERSION 0
	u32 Version;

	u32 TagCount;
	u32 AssetCount;
	u32 AssetTypeCount;

	u64 Tags; // p5a_tag[TagCount]
	u64 AssetTypes; // p5a_asset_type[AssetTypeCount]
	u64 Assets; // p5a_asset[AssetCount]

	// TODO: Primacy numbers for asset files?

	// TODO:
	//
	// u32 FileGUID[8];
	// u32 RemovalCount;
	// 
	// struct p5a_asset_removal
	// {
	//		u32 FileGUID[8];
	//		u32 AssetIndex;
	// };
	//
};

struct p5a_tag
{
	u32 ID;
	f32 Value;
};

struct p5a_asset_type
{
	u32 TypeID;
	u32 FirstAssetIndex;
	u32 OnePastLastAssetIndex;
};

enum class p5a_sound_chain
{
	None,
	Loop,
	Advance
};

struct p5a_bitmap
{
	u32 Dim[2];
	f32 AlignPercentage[2];
	/* NOTE: Data is:
	
		u32 Pixels[Dim[1]][Dim[0]]
	*/
};

struct p5a_sound
{
	u32 SampleCount;
	u32 ChannelCount;
	p5a_sound_chain Chain; // NOTE: p5a_sound_chain
	/* NOTE: Data is: 
	
		i16 Channels[ChannelCount][SampleCount];
	*/
};

struct p5a_font_glyph
{
	u32 UnicodeCodepoint;
	bitmap_id BitmapID;
};

struct p5a_font
{
	u32 OnePastHighestCodepoint;
	u32 GlyphCount;
	f32 AscenderHeight;
	f32 DescenderHeight;
	f32 ExternalLeading;
	/* NOTE: Data is: 
	
		p5a_font_glyph Codepoints[GlyphCount];
		f32 HorizontalAdvance[GlyphCount][GlyphCount];
	*/
};

struct p5a_asset
{
	u64 DataOffset;
	u32 FirstTagIndex;
	u32 OnePastLastTagIndex;
	union
	{
		p5a_bitmap Bitmap;
		p5a_sound Sound;
		p5a_font Font;
	};
};
#pragma pack(pop)

#endif // !P5ENGINE_FILE_FORMATS_H