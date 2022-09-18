
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITALIZED INT32_MAX

#define TILES_PER_CHUNK 8

inline world_position
NullPosition(void)
{
	world_position Result = {};

	Result.ChunkX = TILE_CHUNK_UNINITALIZED;

	return(Result);
}

inline b32
IsValid(world_position Pos)
{
	b32 Result = (Pos.ChunkX != TILE_CHUNK_UNINITALIZED);

	return(Result);
}

inline b32
IsCanonical(f32 ChunkDim, f32 TileRel)
{
	// TODO: Fix floating point math so this can be exact
	f32 Epsilon = 0.01f;
	b32 Result = ((TileRel >= -(0.5f * ChunkDim + Epsilon)) && 
					 (TileRel <= (0.5f * ChunkDim + Epsilon)));

	return(Result);
}

inline b32
IsCanonical(world* World, v3 Offset)
{
	b32 Result = (IsCanonical(World->ChunkDimInMeters.x, Offset.x) && 
					 IsCanonical(World->ChunkDimInMeters.y, Offset.y) &&
					 IsCanonical(World->ChunkDimInMeters.z, Offset.z));

	return(Result);
}

inline b32
AreInSameChunk(world* World, world_position* A, world_position* b)
{
	Assert(IsCanonical(World, A->Offset));
	Assert(IsCanonical(World, b->Offset));

	b32 Result = ((A->ChunkX == b->ChunkX) && 
					 (A->ChunkY == b->ChunkY) && 
					 (A->ChunkZ == b->ChunkZ));

	return(Result);
}

inline world_chunk*
GetWorldChunk(world* World, i32 x, i32 y, i32 z, memory_arena* Arena = 0)
{
	Assert(x > -TILE_CHUNK_SAFE_MARGIN);
	Assert(y > -TILE_CHUNK_SAFE_MARGIN);
	Assert(z > -TILE_CHUNK_SAFE_MARGIN);
	Assert(x < TILE_CHUNK_SAFE_MARGIN);
	Assert(y < TILE_CHUNK_SAFE_MARGIN);
	Assert(z < TILE_CHUNK_SAFE_MARGIN);

	// TODO: Better hash function
	u32 HashValue = 19 * x + 7 * y + 3 * z;
	u32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
	Assert(HashSlot < ArrayCount(World->ChunkHash));

	world_chunk* Chunk = World->ChunkHash + HashSlot;
	do
	{
		if ((x == Chunk->x) && (y == Chunk->y) && (z == Chunk->z))
		{
			break;
		}

		if (Arena && (Chunk->x != TILE_CHUNK_UNINITALIZED) && (!Chunk->NextInHash))
		{
			Chunk->NextInHash = PushStruct(Arena, world_chunk);
			Chunk = Chunk->NextInHash;
			Chunk->x = TILE_CHUNK_UNINITALIZED;
		}

		if (Arena && (Chunk->x == TILE_CHUNK_UNINITALIZED))
		{
			Chunk->x = x;
			Chunk->y = y;
			Chunk->z = z;

			Chunk->NextInHash = 0;

			break;
		}

		Chunk = Chunk->NextInHash;
	} while (Chunk);

	return(Chunk);
}

internal void
InitializeWorld(world* World, v3 ChunkDimInMeters)
{
	World->ChunkDimInMeters = ChunkDimInMeters;
	World->FirstFree = 0;

	for (u32 WorldChunkIndex = 0; WorldChunkIndex < ArrayCount(World->ChunkHash); ++WorldChunkIndex)
	{
		World->ChunkHash[WorldChunkIndex].x = TILE_CHUNK_UNINITALIZED;
		World->ChunkHash[WorldChunkIndex].FirstBlock.EntityCount = 0;
	}
}

inline void
RecanonicalizeCoord(f32 ChunkDim, i32* Tile, f32* TileRel)
{
	// TODO: Need to do something that doesn't use the divide/multiply method
	// for recanonicalizing because this can end up rounding back on to the tile
	// you just came from.

	// NOTO: World is assumed to be toroidal, if you step off one end you
	// come back on the other.

	i32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim);
	*Tile += Offset;
	*TileRel -= Offset * ChunkDim;

	Assert(IsCanonical(ChunkDim, *TileRel));
}

inline world_position
MapIntoChunkSpace(world* World, world_position BasePos, v3 Offset)
{
	world_position Result = BasePos;

	Result.Offset += Offset;
	RecanonicalizeCoord(World->ChunkDimInMeters.x, &Result.ChunkX, &Result.Offset.x);
	RecanonicalizeCoord(World->ChunkDimInMeters.y, &Result.ChunkY, &Result.Offset.y);
	RecanonicalizeCoord(World->ChunkDimInMeters.z, &Result.ChunkZ, &Result.Offset.z);

	return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world* World, i32 AbsTileX, i32 AbsTileY, i32 AbsTileZ, v3 AdditionalOffset = V3(0, 0, 0))
{
	world_position BasePos = {};

	f32 TileSideInMeters = 1.4f;
	f32 TileDepthInMeters = 3.0f;

	v3 TileDim = V3(TileSideInMeters, TileSideInMeters, TileDepthInMeters);
	v3 Offset = Hadamard(TileDim, V3((f32)AbsTileX, (f32)AbsTileY, (f32)AbsTileZ));
	world_position Result = MapIntoChunkSpace(World, BasePos, Offset + AdditionalOffset);

	Assert(IsCanonical(World, Result.Offset));

	return(Result);
}

internal v3
Subtract(world* World, world_position* A, world_position* b)
{
	v3 Result = {};

	v3 dTile = V3((f32)A->ChunkX - (f32)b->ChunkX, 
				  (f32)A->ChunkY - (f32)b->ChunkY,
				  (f32)A->ChunkZ - (f32)b->ChunkZ);

	Result = Hadamard(World->ChunkDimInMeters, dTile) + (A->Offset - b->Offset);

	return(Result);
}

inline world_position
CenteredChunkPoint(u32 ChunkX, u32 ChunkY, u32 ChunkZ)
{
	world_position Result = {};

	Result.ChunkX = ChunkX;
	Result.ChunkY = ChunkY;
	Result.ChunkZ = ChunkZ;

	return(Result);
}

inline world_position
CenteredChunkPoint(world_chunk* Chunk)
{
	world_position Result = CenteredChunkPoint(Chunk->x, Chunk->y, Chunk->z);

	return(Result);
}

inline void
ChangeEntityLocationRaw(memory_arena* Arena, world* World, u32 LowEntityIndex, 
	world_position* OldPos, world_position* NewPos)
{
	// TODO: If this moves an entity into the camera bound, should it automatically
	// go into the high set immediately?

	Assert(!OldPos || IsValid(*OldPos));
	Assert(!NewPos || IsValid(*NewPos));

	if (OldPos && NewPos && AreInSameChunk(World, OldPos, NewPos))
	{
		// NOTE: Leave entity where it is
	}
	else
	{
		if (OldPos)
		{
			// NOTE: Pull this entity out of its old entity block
			world_chunk* Chunk = GetWorldChunk(World, OldPos->ChunkX, OldPos->ChunkY, OldPos->ChunkZ);
			Assert(Chunk);
			if (Chunk)
			{
				b32 NotFound = true;
				world_entity_block* FirstBlock = &Chunk->FirstBlock;
				for (world_entity_block* Block = FirstBlock; Block && NotFound; Block = Block->Next)
				{
					for (u32 Index = 0; (Index < Block->EntityCount) && NotFound; ++Index)
					{
						if (Block->LowEntityIndex[Index] == LowEntityIndex)
						{
							Assert(FirstBlock->EntityCount > 0);
							Block->LowEntityIndex[Index] = FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];

							if (FirstBlock->EntityCount == 0)
							{
								if (FirstBlock->Next)
								{
									world_entity_block* NextBlock = FirstBlock->Next;
									*FirstBlock = *NextBlock;

									NextBlock->Next = World->FirstFree;
									World->FirstFree = NextBlock;
								}
							}

							NotFound = false;
						}
					}
				}
			}
		}

		if (NewPos)
		{
			// NOTE: Insert the entity into its new entity block
			world_chunk* Chunk = GetWorldChunk(World, NewPos->ChunkX, NewPos->ChunkY, NewPos->ChunkZ, Arena);
			Assert(Chunk);

			if (Chunk)
			{
				world_entity_block* Block = &Chunk->FirstBlock;
				if (Block->EntityCount == ArrayCount(Block->LowEntityIndex))
				{
					// NOTE: We're out of room, get a new block
					world_entity_block* OldBlock = World->FirstFree;
					if (OldBlock)
					{
						World->FirstFree = OldBlock->Next;
					}
					else
					{
						OldBlock = PushStruct(Arena, world_entity_block);
					}

					*OldBlock = *Block;
					Block->Next = OldBlock;
					Block->EntityCount = 0;
				}

				Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
				if (Block->EntityCount < ArrayCount(Block->LowEntityIndex))
				{
					Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
				}
			}
		}
	}
}

internal void
ChangeEntityLocation(memory_arena* Arena, world* World, u32 LowEntityIndex, low_entity* LowEntity, world_position NewPosInit)
{
	world_position* OldPos = 0;
	world_position* NewPos = 0;

	if (!HasFlag(&LowEntity->Sim, (u32)entity_flag::Nonspatial) && IsValid(LowEntity->Pos))
	{
		OldPos = &LowEntity->Pos;
	}

	if (IsValid(NewPosInit))
	{
		NewPos = &NewPosInit;
	}

	ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldPos, NewPos);

	if (NewPos)
	{
		LowEntity->Pos = *NewPos;
		ClearFlags(&LowEntity->Sim, (u32)entity_flag::Nonspatial);
	}
	else
	{
		LowEntity->Pos = NullPosition();
		AddFlags(&LowEntity->Sim, (u32)entity_flag::Nonspatial);
	}
}
