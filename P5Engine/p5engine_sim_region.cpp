
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

inline sim_entity*
GetEntityByStorageIndex(sim_region* SimRegion, uint32 StorageIndex)
{
	sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	sim_entity* Result = Entry->Ptr;
	return(Result);
}

inline v3
GetSimSpacePos(sim_region* SimRegion, low_entity* Stored)
{
	// NOTE: Map the entity into camera space
	// TODO: Do we want to set this to signaling NaN in
	// debug mode to make sure nobody ever uses the position
	// of a nonspatial entity
	v3 Result = InvalidPos;

	if (!HasFlag(&Stored->Sim, entity_flag::Nonspatial))
	{
		Result = Subtract(SimRegion->World, &Stored->Pos, &SimRegion->Origin);
	}

	return(Result);
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source, v3* SimPos);
inline void
LoadEntityReference(game_state* GameState, sim_region* SimRegion, entity_reference* Ref)
{
	if (Ref->Index)
	{
		sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
		if (Entry->Ptr == 0)
		{
			Entry->Index = Ref->Index;
			low_entity* EntityLow = GetLowEntity(GameState, Ref->Index);
			v3 Pos = GetSimSpacePos(SimRegion, EntityLow);
			Entry->Ptr = AddEntity(GameState, SimRegion, Ref->Index, EntityLow, &Pos);
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
AddEntityRaw(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source)
{
	Assert(StorageIndex);
	sim_entity* Entity = 0;

	sim_entity_hash* Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	if (Entry->Ptr == 0)
	{
		if (SimRegion->EntityCount < SimRegion->MaxEntityCount)
		{
			Entity = SimRegion->Entities + SimRegion->EntityCount++;

			Entry->Index = StorageIndex;
			Entry->Ptr = Entity;

			if (Source)
			{
				// TODO: This should really be a decompression step, not
				// a copy
				*Entity = Source->Sim;
				LoadEntityReference(GameState, SimRegion, &Entity->Sword);

				Assert(!HasFlag(&Source->Sim, entity_flag::Simming));
				AddFlag(&Source->Sim, entity_flag::Simming);
			}

			Entity->StorageIndex = StorageIndex;
			Entity->Updatable = false; // IsInRectangle(SimRegion->UpdatableBounds);
		}
		else
		{
			InvalidCodePath;
		}
	}

	return(Entity);
}

internal sim_entity*
AddEntity(game_state* GameState, sim_region* SimRegion, uint32 StorageIndex, low_entity* Source, v3* SimPos)
{
	sim_entity* Dest = AddEntityRaw(GameState, SimRegion, StorageIndex, Source);
	if (Dest)
	{
		if (SimPos)
		{
			Dest->Pos = *SimPos;
			Dest->Updatable = IsInRectangle(SimRegion->UpdatableBounds, Dest->Pos);
		}
		else
		{
			Dest->Pos = GetSimSpacePos(SimRegion, Source);
		}
	}

	return(Dest);
}

internal sim_region*
BeginSim(memory_arena* SimArena, game_state* GameState, world* World, world_position Origin, rectangle3 Bounds)
{
	// TODO: If entities were stored in the world, we wouldn't nede the game state here

	// TODO: IMPORTANT Notion of active vs inactive entities for the apron

	sim_region* SimRegion = PushStruct(SimArena, sim_region);
	ZeroStruct(SimRegion->Hash);

	// TODO: IMPORTANT Calculate this eventually from the maximum value of
	// all entities radius plus their speed
	real32 UpdateSafetyMargin = 1.0f;
	real32 UpdateSafetyMarginZ = 1.0f;

	SimRegion->World = World;
	SimRegion->Origin = Origin;
	SimRegion->UpdatableBounds = Bounds;
	SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds, V3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMarginZ));

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
						if (!HasFlag(&EntityLow->Sim, entity_flag::Nonspatial))
						{
							v3 SimSpacePos = GetSimSpacePos(SimRegion, EntityLow);
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

		Assert(HasFlag(&Stored->Sim, entity_flag::Simming));
		Stored->Sim = *Entity;
		Assert(!HasFlag(&Stored->Sim, entity_flag::Simming));

		StoreEntityReference(&Stored->Sim.Sword);

		// TODO: Save state back to the stored entity, once high entities
		// do state decompression, etc.

		world_position NewPos = HasFlag(Entity, entity_flag::Nonspatial) ? NullPosition() : MapIntoChunkSpace(GameState->World, SimRegion->Origin, Entity->Pos);
		ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored, NewPos);

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
			GameState->CameraP = NewCameraP;
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

internal bool32
ShouldCollide(game_state* GameState, sim_entity* A, sim_entity* B)
{
	bool32 Result = false;

	if (A != B)
	{
		if (A->StorageIndex > B->StorageIndex)
		{
			sim_entity* Temp = A;
			A = B;
			B = Temp;
		}

		if (!HasFlag(A, entity_flag::Nonspatial) &&
			!HasFlag(B, entity_flag::Nonspatial))
		{
			// TODO: Property-based logic goes here
			Result = true;
		}

		// TODO: Better hash function
		uint32 StorageIndexA = A->StorageIndex;
		uint32 ArrayHash = (ArrayCount(GameState->CollisionRuleHash) - 1);
		uint32 HashBucket = StorageIndexA & ArrayHash;
		pairwise_collision_rule* Rule = GameState->CollisionRuleHash[HashBucket];
		while (Rule)
		{
			if ((Rule->StorageIndexA == A->StorageIndex) &&
				(Rule->StorageIndexB == B->StorageIndex))
			{
				Result = Rule->ShouldCollide;
				break;
			}

			Rule = Rule->NextInHash;
		}
	}

	return(Result);
}

internal bool32
HandleCollision(sim_entity* A, sim_entity* B)
{
	bool32 StopsOnCollision = false;

	if (A->Type == entity_type::Sword)
	{
		StopsOnCollision = false;
	}
	else
	{
		StopsOnCollision = true;
	}

	if (A->Type > B->Type)
	{
		sim_entity* Temp = A;
		A = B;
		B = Temp;
	}

	if ((A->Type == entity_type::Monstar) &&
		(B->Type == entity_type::Sword))
	{
		if (A->HitPointMax > 0)
		{
			--A->HitPointMax;
		}
	}

	// TODO: Stairs
	//EntityLow->AbsTileZ += HitLow->dAbsTileZ;

	// TODO: Real "stops on collision"
	return(StopsOnCollision);
}

internal void
MoveEntity(game_state* GameState, sim_region* SimRegion, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v3 ddP)
{
	Assert(!HasFlag(Entity, entity_flag::Nonspatial));

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

	v3 OldPlayerP = Entity->Pos;
	v3 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dPos * dt);
	Entity->dPos = ddP * dt + Entity->dPos;
	v3 NewPlayerP = OldPlayerP + PlayerDelta;

	real32 DistanceRemaining = Entity->DistanceLimit;
	if (DistanceRemaining == 0.0f)
	{
		// TODO: Do we want to formalize this number?
		DistanceRemaining = 10000.0f;
	}

	for (uint32 Iteration = 0; Iteration < 4; ++Iteration)
	{
		real32 tMin = 1.0f;

		real32 PlayerDeltaLength = Length(PlayerDelta);
		// TODO: What do we want to do for epsilons here?
		// Think this through for the final collision code
		if (PlayerDeltaLength > 0.0f)
		{
			if (PlayerDeltaLength > DistanceRemaining)
			{
				tMin = DistanceRemaining / PlayerDeltaLength;
			}

			v3 WallNormal = {};
			sim_entity* HitEntity = 0;

			v3 DesiredPosition = Entity->Pos + PlayerDelta;

			// NOTE: This is just an optimization to avoid entering the
			// loop in the case where the test entity is non-spatial
			if (!HasFlag(Entity, entity_flag::Nonspatial))
			{
				// TODO: Spatial partition here
				for (uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; ++TestEntityIndex)
				{
					sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;
					if (ShouldCollide(GameState, Entity, TestEntity))
					{
						// TODO: Entities have height
						v3 MinkowskiDiameter = V3(TestEntity->Width + Entity->Width,
												  TestEntity->Height + Entity->Height,
												  2.0f * World->TileDepthInMeters);

						v3 MinCorner = -0.5f * MinkowskiDiameter;
						v3 MaxCorner = 0.5f * MinkowskiDiameter;

						v3 Rel = Entity->Pos - TestEntity->Pos;

						// Right Wall
						if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = V3(-1, 0, 0);
							HitEntity = TestEntity;
						}

						// Left Wall
						if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
						{
							WallNormal = V3(1, 0, 0);
							HitEntity = TestEntity;
						}

						// Top Wall
						if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = V3(0, -1, 0);
							HitEntity = TestEntity;
						}

						// Bottom Wall
						if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
						{
							WallNormal = V3(0, 1, 0);
							HitEntity = TestEntity;
						}
					}
				}
			}

			Entity->Pos += tMin * PlayerDelta;
			DistanceRemaining -= tMin * PlayerDeltaLength;

			if (HitEntity)
			{
				PlayerDelta = DesiredPosition - Entity->Pos;

				bool32 StopsOnCollision = HandleCollision(Entity, HitEntity);

				if (StopsOnCollision)
				{
					PlayerDelta -= 1 * Inner(PlayerDelta, WallNormal) * WallNormal;
					Entity->dPos -= 1 * Inner(Entity->dPos, WallNormal) * WallNormal;
				}
				else
				{
					AddCollisionRule(GameState, Entity->StorageIndex, HitEntity->StorageIndex, false);
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	
	// TODO: This has to become real height handling
	if (Entity->Pos.Z < 0)
	{
		Entity->Pos.Z = 0;
	}

	if (Entity->DistanceLimit != 0.0f)
	{
		Entity->DistanceLimit = DistanceRemaining;
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
}
