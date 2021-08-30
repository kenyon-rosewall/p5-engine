
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline world_position
NullPosition(void)
{
	world_position Result = {};

	Result.ChunkX = TILE_CHUNK_UNINITALIZED;

	return(Result);
}

inline bool32
IsValid(world_position Pos)
{
	bool32 Result = (Pos.ChunkX != TILE_CHUNK_UNINITALIZED);

	return(Result);
}

inline bool32
IsCanonical(real32 ChunkDim, real32 TileRel)
{
	// TODO: Fix floating point math so this can be exact
	real32 Epsilon = 0.0001f;
	bool32 Result = ((TileRel >= -(0.5f * ChunkDim + Epsilon)) && 
					 (TileRel <= (0.5f * ChunkDim + Epsilon)));

	return(Result);
}

inline bool32
IsCanonical(world* World, v3 Offset)
{
	bool32 Result = (IsCanonical(World->ChunkDimInMeters.X, Offset.X) && 
					 IsCanonical(World->ChunkDimInMeters.Y, Offset.Y) &&
					 IsCanonical(World->ChunkDimInMeters.Z, Offset.Z));

	return(Result);
}

inline bool32
AreInSameChunk(world* World, world_position* A, world_position* B)
{
	Assert(IsCanonical(World, A->Offset));
	Assert(IsCanonical(World, B->Offset));

	bool32 Result = ((A->ChunkX == B->ChunkX) && 
					 (A->ChunkY == B->ChunkY) && 
					 (A->ChunkZ == B->ChunkZ));

	return(Result);
}

inline world_chunk*
GetWorldChunk(world* World, int32 X, int32 Y, int32 Z, memory_arena* Arena = 0)
{
	Assert(X > -TILE_CHUNK_SAFE_MARGIN);
	Assert(Y > -TILE_CHUNK_SAFE_MARGIN);
	Assert(Z > -TILE_CHUNK_SAFE_MARGIN);
	Assert(X < TILE_CHUNK_SAFE_MARGIN);
	Assert(Y < TILE_CHUNK_SAFE_MARGIN);
	Assert(Z < TILE_CHUNK_SAFE_MARGIN);

	// TODO: Better hash function
	uint32 HashValue = 19 * X + 7 * Y + 3 * Z;
	uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
	Assert(HashSlot < ArrayCount(World->ChunkHash));

	world_chunk* Chunk = World->ChunkHash + HashSlot;
	do
	{
		if ((X == Chunk->X) && (Y == Chunk->Y) && (Z == Chunk->Z))
		{
			break;
		}

		if (Arena && (Chunk->X != TILE_CHUNK_UNINITALIZED) && (!Chunk->NextInHash))
		{
			Chunk->NextInHash = PushStruct(Arena, world_chunk);
			Chunk = Chunk->NextInHash;
			Chunk->X = TILE_CHUNK_UNINITALIZED;
		}

		if (Arena && (Chunk->X == TILE_CHUNK_UNINITALIZED))
		{
			Chunk->X = X;
			Chunk->Y = Y;
			Chunk->Z = Z;

			Chunk->NextInHash = 0;

			break;
		}

		Chunk = Chunk->NextInHash;
	} while (Chunk);

	return(Chunk);
}

internal void
InitializeWorld(world* World, real32 TileSideInMeters)
{
	World->TileSideInMeters = TileSideInMeters;
	World->ChunkDimInMeters = V3((real32)TILES_PER_CHUNK * TileSideInMeters,
								 (real32)TILES_PER_CHUNK * TileSideInMeters,
								 (real32)TileSideInMeters);
	World->TileDepthInMeters = (real32)TileSideInMeters;
	World->FirstFree = 0;

	for (uint32 WorldChunkIndex = 0; WorldChunkIndex < ArrayCount(World->ChunkHash); ++WorldChunkIndex)
	{
		World->ChunkHash[WorldChunkIndex].X = TILE_CHUNK_UNINITALIZED;
		World->ChunkHash[WorldChunkIndex].FirstBlock.EntityCount = 0;
	}
}

inline void
RecanonicalizeCoord(real32 ChunkDim, int32* Tile, real32* TileRel)
{
	// TODO: Need to do something that doesn't use the divide/multiply method
	// for recanonicalizing because this can end up rounding back on to the tile
	// you just came from.

	// NOTO: World is assumed to be toroidal, if you step off one end you
	// come back on the other.

	int32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim);
	*Tile += Offset;
	*TileRel -= Offset * ChunkDim;

	Assert(IsCanonical(ChunkDim, *TileRel));
}

inline world_position
MapIntoChunkSpace(world* World, world_position BasePos, v3 Offset)
{
	world_position Result = BasePos;

	Result.Offset += Offset;
	RecanonicalizeCoord(World->ChunkDimInMeters.X, &Result.ChunkX, &Result.Offset.X);
	RecanonicalizeCoord(World->ChunkDimInMeters.Y, &Result.ChunkY, &Result.Offset.Y);
	RecanonicalizeCoord(World->ChunkDimInMeters.Z, &Result.ChunkZ, &Result.Offset.Z);

	return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world* World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ)
{
	world_position BasePos = {};

	v3 Offset = Hadamard(World->ChunkDimInMeters, V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ));
	world_position Result = MapIntoChunkSpace(World, BasePos, Offset);

	return(Result);
}

internal v3
Subtract(world* World, world_position* A, world_position* B)
{
	v3 Result = {};

	v3 dTile = V3((real32)A->ChunkX - (real32)B->ChunkX, 
				  (real32)A->ChunkY - (real32)B->ChunkY,
				  (real32)A->ChunkZ - (real32)B->ChunkZ);

	Result = Hadamard(World->ChunkDimInMeters, dTile) + (A->Offset - B->Offset);

	return(Result);
}

inline world_position
CenteredChunkPoint(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
	world_position Result = {};

	Result.ChunkX = ChunkX;
	Result.ChunkY = ChunkY;
	Result.ChunkZ = ChunkZ;

	return(Result);
}

inline void
ChangeEntityLocationRaw(memory_arena* Arena, world* World, uint32 LowEntityIndex, 
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
				bool32 NotFound = true;
				world_entity_block* FirstBlock = &Chunk->FirstBlock;
				for (world_entity_block* Block = FirstBlock; Block && NotFound; Block = Block->Next)
				{
					for (uint32 Index = 0; (Index < Block->EntityCount) && NotFound; ++Index)
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
ChangeEntityLocation(memory_arena* Arena, world* World, uint32 LowEntityIndex, low_entity* LowEntity, world_position NewPosInit)
{
	world_position* OldPos = 0;
	world_position* NewPos = 0;

	if (!HasFlag(&LowEntity->Sim, entity_flag::Nonspatial) && IsValid(LowEntity->Pos))
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
		ClearFlag(&LowEntity->Sim, entity_flag::Nonspatial);
	}
	else
	{
		LowEntity->Pos = NullPosition();
		AddFlag(&LowEntity->Sim, entity_flag::Nonspatial);
	}
}
