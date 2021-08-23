
internal sim_entity_hash*
GetHashFromStorageIndex(sim_region* SimRegion, uint32 StorageIndex)
{
	Assert(StorageIndex);

	sim_entity_hash* Result = 0;

	uint32 HashValue = StorageIndex;
	for (uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset)
	{
		uint32 HashMask = (ArrayCount(SimRegion->Hash) - 1);
		uint32 HashIndex = ((HashValue + Offset) & HashMask);
		sim_entity_hash* Entry = SimRegion->Hash + HashIndex;
		if ((Entry->Index == 0) || (Entry->Index == StorageIndex))
		{
			Result = Entry;
			break;
		}
	}

	return(Result);
}

internal void
MapStorageIndexToEntity(sim_region* SimRegion, uint32 StorageIndex, sim_entity* Entity)
{
	sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	Assert((Entry->Index == 0) || (Entry->Index == StorageIndex));
	Entry->Index = StorageIndex;
	Entry->Ptr = Entity;
}

inline sim_entity*
GetEntityByStorageIndex(sim_region* SimRegion, uint32 StorageIndex)
{
	sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	sim_entity* Result = Entry->Ptr;
	return(Result);
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source);
inline void
LoadEntityReference(game_state* GameState, sim_region* SimRegion, entity_reference* Ref)
{
	if (Ref->Index)
	{
		sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
		if (Entry->Ptr == 0)
		{
			AddEntity(GameState, SimRegion, Ref->Index, GetLowEntity(GameState, Ref->Index));
		}

		Ref->Ptr = Entry->Ptr;
	}
}

inline void
StoreEntityReference(entity_reference* Ref)
{
	if (Ref->Ptr != 0)
	{
		Ref->Index = Ref->Ptr->StorageIndex;
	}
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source)
{
	Assert(StorageIndex);
	sim_entity* Entity = 0;

	if (SimRegion->EntityCount < SimRegion->MaxEntityCount)
	{
		Entity = SimRegion->Entities + SimRegion->EntityCount++;
		MapStorageIndexToEntity(SimRegion, StorageIndex, Entity);

		if (Source)
		{
			// TODO: This should really be a decompression step, not
			// a copy
			*Entity = Source->Sim;
			LoadEntityReference(GameState, SimRegion, &Entity->Sword);
		}

		Entity->StorageIndex = StorageIndex;
	}
	else
	{
		InvalidCodePath;
	}

	return(Entity);
}

inline v2
GetSimSpacePos(sim_region* SimRegion, low_entity* Stored)
{
	v2 Result = {};

	// NOTE: Map the entity into camera space
	v3 Diff = Subtract(SimRegion->World, &Stored->Pos, &SimRegion->Origin);
	Result.X = Diff.X;
	Result.Y = Diff.Y;

	return(Result);
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source, v2* SimPos)
{
	sim_entity* Dest = AddEntity(GameState, SimRegion, StorageIndex, Source);
	if (Dest)
	{
		if (SimPos)
		{
			Dest->Pos = *SimPos;
		}
		else
		{
			Dest->Pos = GetSimSpacePos(SimRegion, Source);
		}
	}

	return(Dest);
}

internal sim_region*
BeginSim(memory_arena* SimArena, game_state* GameState, world* World, world_position Origin, rectangle2 Bounds)
{
	// TODO: If entities were stored in the world, we wouldn't nede the game state here

	// TODO: IMPORTANT Notion of active vs inactive entities for the apron

	sim_region* SimRegion = PushStruct(SimArena, sim_region);
	ZeroStruct(SimRegion->Hash);

	SimRegion->World = World;
	SimRegion->Origin = Origin;
	SimRegion->Bounds = Bounds;

	// TODO: Need to be more specific about entity counts
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

	world_position MinChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
	world_position MaxChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));

	for (int32 ChunkY = MinChunkPos.ChunkY; ChunkY <= MaxChunkPos.ChunkY; ++ChunkY)
	{
		for (int32 ChunkX = MinChunkPos.ChunkX; ChunkX <= MaxChunkPos.ChunkX; ++ChunkX)
		{
			world_chunk* Chunk = GetWorldChunk(World, ChunkX, ChunkY, SimRegion->Origin.ChunkZ);
			if (Chunk)
			{
				for (world_entity_block* Block = &Chunk->FirstBlock; Block; Block = Block->Next)
				{
					for (uint32 EntityIndexIndex = 0; EntityIndexIndex < Block->EntityCount; ++EntityIndexIndex)
					{
						uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexIndex];
						low_entity* EntityLow = GameState->LowEntities + LowEntityIndex;

						v2 SimSpacePos = GetSimSpacePos(SimRegion, EntityLow);
						if (IsInRectangle(SimRegion->Bounds, SimSpacePos))
						{
							// TODO: Check a second rectangle to set the entity to be "movable" or not
							AddEntity(GameState, SimRegion, LowEntityIndex, EntityLow, &SimSpacePos);
						}
					}
				}
			}
		}
	}

	return(SimRegion);
}

internal void
EndSim(sim_region* SimRegion, game_state* GameState)
{
	// TODO: Maybe don't take a game state here, low entities should be stored
	// in the world?? 

	sim_entity* Entity = SimRegion->Entities;
	for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex, ++Entity)
	{
		low_entity* Stored = GameState->LowEntities + Entity->StorageIndex;
		Stored->Sim = *Entity;
		StoreEntityReference(&Stored->Sim.Sword);

		// TODO: Save state back to the stored entity, once high entities
		// do state decompression, etc.

		world_position NewPos = MapIntoChunkSpace(GameState->World, SimRegion->Origin, Entity->Pos);
		ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored, &Stored->Pos, &NewPos);

		if (Entity->StorageIndex == GameState->CameraFollowingEntityIndex)
		{
			world_position NewCameraP = GameState->CameraP;

			NewCameraP.ChunkZ = Stored->Pos.ChunkZ;

#if 0
			if (Stored->Pos.X > ((17.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkX += 17;
			}
			if (Stored->Pos.X < -((17.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkX -= 17;
			}
			if (Stored->Pos.Y > ((9.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkY += 9;
			}
			if (Stored->Pos.Y < -((9.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkY -= 9;
			}
#else
			NewCameraP = Stored->Pos;
#endif
		}
	}
}

internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, real32* tMin, real32 MinY, real32 MaxY)
{
	bool32 Hit = false;

	real32 tEpsilon = 0.001f;
	if (PlayerDeltaX != 0.0f)
	{
		real32 tResult = (WallX - RelX) / PlayerDeltaX;
		real32 Y = RelY + tResult * PlayerDeltaY;
		if ((tResult >= 0.0f) && (*tMin > tResult))
		{
			if ((Y >= MinY) && (Y <= MaxY))
			{
				*tMin = Maximum(0.0f, tResult - tEpsilon);
				Hit = true;
			}
		}
	}

	return(Hit);
}

internal void
MoveEntity(sim_region* SimRegion, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v2 ddP)
{
	world* World = SimRegion->World;

	if (MoveSpec->UnitMaxAccelVector)
	{
		real32 ddPLength = LengthSq(ddP);
		if (ddPLength > 1.0f)
		{
			ddP *= (1.0f / SquareRoot(ddPLength));
		}
	}

	ddP *= MoveSpec->Speed * MoveSpec->SpeedMult;

	// TODO: ODE here
	ddP += -MoveSpec->Drag * Entity->dPos;

	v2 OldPlayerP = Entity->Pos;
	v2 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dPos * dt);
	Entity->dPos = ddP * dt + Entity->dPos;
	v2 NewPlayerP = OldPlayerP + PlayerDelta;

	for (uint32 Iteration = 0; Iteration < 4; ++Iteration)
	{
		real32 tMin = 1.0f;
		v2 WallNormal = {};
		sim_entity* HitEntity = 0;

		v2 DesiredPosition = Entity->Pos + PlayerDelta;

		if (Entity->Collides)
		{
			// TODO: Spatial partition here
			for (uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; ++TestEntityIndex)
			{
				sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;
				if (Entity != TestEntity)
				{
					if (TestEntity->Collides)
					{
						real32 DiameterW = TestEntity->Width + Entity->Width;
						real32 DiameterH = TestEntity->Height + Entity->Height;

						v2 MinCorner = -0.5f * V2(DiameterW, DiameterH);
						v2 MaxCorner = 0.5f * V2(DiameterW, DiameterH);

						v2 Rel = Entity->Pos - TestEntity->Pos;

						// Right Wall
						if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = V2(-1, 0);
							HitEntity = TestEntity;
						}

						// Left Wall
						if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = V2(1, 0);
							HitEntity = TestEntity;
						}

						// Top Wall
						if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = V2(0, -1);
							HitEntity = TestEntity;
						}

						// Bottom Wall
						if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = V2(0, 1);
							HitEntity = TestEntity;
						}
					}
				}
			}
		}

		Entity->Pos += tMin * PlayerDelta;
		if (HitEntity)
		{
			Entity->dPos -= 1 * Inner(Entity->dPos, WallNormal) * WallNormal;
			PlayerDelta = DesiredPosition - Entity->Pos;
			PlayerDelta -= 1 * Inner(PlayerDelta, WallNormal) * WallNormal;

			// TODO: Stairs
			//Entity->AbsTileZ += HitLow->dAbsTileZ;
		}
		else
		{
			break;
		}
	}

	// TODO: Change to using the acceleration vector
	if ((Entity->dPos.X == 0.0f) && (Entity->dPos.Y == 0.0f))
	{
		// NOTE: Leave FacingDirection whatever it was
	}
	else if (AbsoluteValue(Entity->dPos.X) > AbsoluteValue(Entity->dPos.Y))
	{
		if (Entity->dPos.X > 0)
		{
			Entity->FacingDirection = 0;
		}
		else
		{
			Entity->FacingDirection = 2;
		}
	}
	else
	{
		if (Entity->dPos.Y > 0)
		{
			Entity->FacingDirection = 1;
		}
		else
		{
			Entity->FacingDirection = 3;
		}
	}

	Entity->Sword.Ptr->FacingDirection = Entity->FacingDirection;
}
