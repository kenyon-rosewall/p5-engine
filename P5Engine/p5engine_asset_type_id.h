#pragma once

#ifndef P5ENGINE_ASSET_TYPE_ID_H
#define P5ENGINE_ASSET_TYPE_ID_H

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

#endif // !P5ENGINE_ASSET_TYPE_ID_H