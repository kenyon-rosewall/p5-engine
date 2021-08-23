
/*
 
inline uint32
GetTileValueUnchecked(world* TileMap, world_chunk* WorldChunk, int32 TileX, int32 TileY)
{
	Assert(TileX < TileMap->ChunkDim);
	Assert(TileY < TileMap->ChunkDim);

	uint32 TileChunkValue = 0;
	if (WorldChunk && WorldChunk->Tiles)
	{
		TileChunkValue = WorldChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
	}

	return (TileChunkValue);
}

inline void
SetTileValueUnchecked(world* TileMap, world_chunk* WorldChunk, int32 TileX, int32 TileY, int32 TileValue)
{
	Assert(TileX < TileMap->ChunkDim);
	Assert(TileY < TileMap->ChunkDim);

	if (WorldChunk && WorldChunk->Tiles)
	{
		WorldChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
	}
}

inline uint32
GetTileValue(world* TileMap, world_chunk* WorldChunk, uint32 TestTileX, uint32 TestTileY)
{
	uint32 TileChunkValue = 0;

	if (WorldChunk && WorldChunk->Tiles)
	{
		TileChunkValue = GetTileValueUnchecked(TileMap, WorldChunk, TestTileX, TestTileY);
	}

	return(TileChunkValue);
}

internal uint32
GetTileValue(world* TileMap, uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
	tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, ChunkX, ChunkY, ChunkZ);
	world_chunk* WorldChunk = GetWorldChunk(TileMap, ChunkPos.X, ChunkPos.Y, ChunkPos.Z);
	uint32 TileChunkValue = GetTileValue(TileMap, WorldChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);

	return(TileChunkValue);
}

internal uint32
GetTileValue(world* TileMap, world_position Pos)
{
	uint32 TileChunkValue = GetTileValue(TileMap, Pos.ChunkX, Pos.ChunkY, Pos.ChunkZ);

	return(TileChunkValue);
}

inline void
SetTileValue(world* TileMap, world_chunk* WorldChunk, uint32 TestTileX, uint32 TestTileY, uint32 TileValue)
{
	if (WorldChunk && WorldChunk->Tiles)
	{
		SetTileValueUnchecked(TileMap, WorldChunk, TestTileX, TestTileY, TileValue);
	}
}

internal void
SetTileValue(memory_arena* Arena, world* TileMap, uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ, uint32 TileValue)
{
	tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, ChunkX, ChunkY, ChunkZ);
	world_chunk* WorldChunk = GetWorldChunk(TileMap, ChunkPos.X, ChunkPos.Y, ChunkPos.Z, Arena);
	SetTileValue(TileMap, WorldChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

internal bool32
IsTileValueEmpty(uint32 TileValue)
{
	bool32 Empty = ((TileValue == 1) || (TileValue == 3) || (TileValue == 4));

	return(Empty);
}

internal bool32
IsTileMapPointEmpty(world* TileMap, world_position CanPos)
{
	uint32 TileChunkValue = GetTileValue(TileMap, CanPos);
	bool32 Empty = IsTileValueEmpty(TileChunkValue);

	return(Empty);
}

*/
