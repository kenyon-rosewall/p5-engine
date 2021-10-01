#pragma once

#ifndef P5ENGINE_WORLD_H
#define	P5ENGINE_WORLD_H

struct world_position
{
	// TODO: It seems like we have to store Chunk* with each
	// entity beacuse even though the sim region gather doesn't need it
	// at first, and we could get by without it, entity references pull
	// in entities WITHOUT going through their word_chunk, and thus
	// still need to know the Chunk*

	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

	// NOTE: These are offsets from the chunk center
	v3 Offset;
};


// TODO: Could make this just world_chunk and then allow multiple tile chunks per x/y/z
struct world_entity_block
{
	uint32 EntityCount;
	uint32 LowEntityIndex[16];
	world_entity_block* Next;
};

struct world_chunk
{
	int32 x, y, z;

	world_entity_block FirstBlock;

	world_chunk* NextInHash;
};

struct world
{
	v3 ChunkDimInMeters;

	world_entity_block* FirstFree;

	// TODO: ChunkHash should probably switch to pointers, IF
	// 	   tile entity blocks continue to be stored en masse derictly in the tile chunk
	// TODO: At the moment,  this must be a power of two
	world_chunk ChunkHash[4096];
};

#endif // !P5ENGINE_WORLD_H
