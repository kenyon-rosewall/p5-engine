#pragma once

#ifndef P5ENGINE_WORLD_H
#define	P5ENGINE_WORLD_H

struct world_position
{
	// TODO: Puzzler! How can we get rid of abstil* here,
	// and still allow references to entities to be able to figure
	// out _where they are_ (or rather, which world_chunk they are in?)

	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

	// NOTE: These are offsets from the chunk center
	v2 Offset;
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
	int32 X, Y, Z;

	world_entity_block FirstBlock;

	world_chunk* NextInHash;
};

struct world
{
	real32 TileSideInMeters;
	real32 ChunkSideInMeters;

	world_entity_block* FirstFree;

	// TODO: ChunkHash should probably switch to pointers, IF
	// 	   tile entity blocks continue to be stored en masse derictly in the tile chunk
	// TODO: At the moment,  this must be a power of two
	world_chunk ChunkHash[4096];
};

#endif // !P5ENGINE_WORLD_H
