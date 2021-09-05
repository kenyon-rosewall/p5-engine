
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
				AddFlags(&Source->Sim, entity_flag::Simming);
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

inline bool32
EntityOverlapsRectangle(v3 Pos, v3 Dim, rectangle3 Rect)
{
	bool32 Result = false;

	rectangle3 Grown = AddRadiusTo(Rect, 0.5f * Dim);
	Result = IsInRectangle(Grown, Pos);

	return(Result);
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
			Dest->Updatable = EntityOverlapsRectangle(Dest->Pos, Dest->Dim, SimRegion->UpdatableBounds);
		}
		else
		{
			Dest->Pos = GetSimSpacePos(SimRegion, Source);
		}
	}

	return(Dest);
}

internal sim_region*
BeginSim(memory_arena* SimArena, game_state* GameState, world* World, world_position Origin, rectangle3 Bounds, real32 dt)
{
	// TODO: If entities were stored in the world, we wouldn't nede the game state 

	sim_region* SimRegion = PushStruct(SimArena, sim_region);
	ZeroStruct(SimRegion->Hash);
	
	// TODO: Try to make these get enforced more rigorously
	SimRegion->MaxEntityRadius = 5.0f;
	SimRegion->MaxEntityVelocity = 30.0f;
	real32 UpdateSafetyMargin = SimRegion->MaxEntityRadius + dt * SimRegion->MaxEntityVelocity;
	real32 UpdateSafetyMarginZ = 1.0f;

	SimRegion->World = World;
	SimRegion->Origin = Origin;
	SimRegion->UpdatableBounds = AddRadiusTo(Bounds, V3(SimRegion->MaxEntityRadius, 
														SimRegion->MaxEntityRadius, 
														SimRegion->MaxEntityRadius));
	SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds, V3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMarginZ));

	// TODO: Need to be more specific about entity counts
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

	world_position MinChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
	world_position MaxChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));

	for (int32 ChunkZ = MinChunkPos.ChunkZ; ChunkZ < MaxChunkPos.ChunkZ; ++ChunkZ)
	{
		for (int32 ChunkY = MinChunkPos.ChunkY; ChunkY <= MaxChunkPos.ChunkY; ++ChunkY)
		{
			for (int32 ChunkX = MinChunkPos.ChunkX; ChunkX <= MaxChunkPos.ChunkX; ++ChunkX)
			{
				world_chunk* Chunk = GetWorldChunk(World, ChunkX, ChunkY, ChunkZ);
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
								if (EntityOverlapsRectangle(SimSpacePos, EntityLow->Sim.Dim, SimRegion->Bounds))
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
			real32 CamZOffset = NewCameraP.Offset.Z;
			NewCameraP = Stored->Pos;
			NewCameraP.Offset.Z = CamZOffset;
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
CanCollide(game_state* GameState, sim_entity* A, sim_entity* B)
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
				Result = Rule->CanCollide;
				break;
			}

			Rule = Rule->NextInHash;
		}
	}

	return(Result);
}

internal bool32
HandleCollision(game_state* GameState, sim_entity* A, sim_entity* B)
{
	bool32 StopsOnCollision = false;

	if (A->Type == entity_type::Sword)
	{
		AddCollisionRule(GameState, A->StorageIndex, B->StorageIndex, false);
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

	return(StopsOnCollision);
}

internal bool32
CanOverlap(game_state* GameState, sim_entity* Mover, sim_entity* Region)
{
	bool32 Result = false;

	if (Mover != Region)
	{
		if (Region->Type == entity_type::Stairs)
		{
			Result = true;
		}
	}

	return(Result);
}

internal void
HandleOverlap(game_state* GameState, sim_entity* Mover, sim_entity* Region, real32 dt, real32* Ground)
{
	if (Region->Type == entity_type::Stairs)
	{
		rectangle3 RegionRect = RectCenterDim(Region->Pos, Region->Dim);
		v3 Bary = Clamp01(GetBarycentric(RegionRect, Mover->Pos));
		
		*Ground = Lerp(RegionRect.Min.Z, Bary.Y, RegionRect.Max.Z);
	}
}

internal bool32
SpeculativeCollide(sim_entity* Mover, sim_entity* Region)
{
	bool32 Result = true;

	if (Region->Type == entity_type::Stairs)
	{
		rectangle3 RegionRect = RectCenterDim(Region->Pos, Region->Dim);
		v3 Bary = Clamp01(GetBarycentric(RegionRect, Mover->Pos));

		real32 Ground = Lerp(RegionRect.Min.Z, Bary.Y, RegionRect.Max.Z);
		real32 StepHeight = 0.1f;
		Result = ((AbsoluteValue(Mover->Pos.Z - Ground) > StepHeight) ||
				  ((Bary.Y > StepHeight) && (Bary.Y < (1.0f - StepHeight))));
	}

	return(Result);
}

internal void
MoveEntity(game_state* GameState, sim_region* SimRegion, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v3 ddP)
{
	Assert(!HasFlag(Entity, entity_flag::Nonspatial));

	world* World = SimRegion->World;

	if (Entity->Type == entity_type::Hero)
	{
		int BreakHere = 0;
	}

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
	if (!HasFlag(Entity, entity_flag::ZSupported))
	{
		ddP += V3(0, 0, -9.8f);
	}
	
	v3 OldPlayerP = Entity->Pos;
	v3 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dPos * dt);
	Entity->dPos = ddP * dt + Entity->dPos;
	// TODO: Upgrade physical motion routines to handle capping the
	// maximum velocity
	Assert(LengthSq(Entity->dPos) <= Square(SimRegion->MaxEntityVelocity));
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
					if (CanCollide(GameState, Entity, TestEntity))
					{
						// TODO: Entities have height
						v3 MinkowskiDiameter = V3(TestEntity->Dim.X + Entity->Dim.X,
												  TestEntity->Dim.Y + Entity->Dim.Y,
												  TestEntity->Dim.Z + Entity->Dim.Z);

						v3 MinCorner = -0.5f * MinkowskiDiameter;
						v3 MaxCorner = 0.5f * MinkowskiDiameter;

						v3 Rel = Entity->Pos - TestEntity->Pos;

						real32 tMinTest = tMin;
						v3 TestWallNormal = {};
						sim_entity* TestHitEntity = 0;

						// Right Wall
						if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMinTest, MinCorner.Y, MaxCorner.Y))
						{
							TestWallNormal = V3(-1, 0, 0);
							TestHitEntity = TestEntity;
						}

						// Left Wall
						if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMinTest, MinCorner.Y, MaxCorner.Y))
						{
							TestWallNormal = V3(1, 0, 0);
							TestHitEntity = TestEntity;
						}

						// Top Wall
						if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMinTest, MinCorner.X, MaxCorner.X))
						{
							TestWallNormal = V3(0, -1, 0);
							TestHitEntity = TestEntity;
						}

						// Bottom Wall
						if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMinTest, MinCorner.X, MaxCorner.X))
						{
							TestWallNormal = V3(0, 1, 0);
							TestHitEntity = TestEntity;
						}

						if (TestHitEntity)
						{
							v3 TestPos = Entity->Pos + tMinTest * PlayerDelta;
							if (SpeculativeCollide(Entity, TestEntity))
							{
								tMin = tMinTest;
								WallNormal = TestWallNormal;
								HitEntity = TestHitEntity;
							}
						}
					}
				}
			}

			Entity->Pos += tMin * PlayerDelta;
			DistanceRemaining -= tMin * PlayerDeltaLength;

			if (HitEntity)
			{
				PlayerDelta = DesiredPosition - Entity->Pos;

				bool32 StopsOnCollision = HandleCollision(GameState, Entity, HitEntity);
				if (StopsOnCollision)
				{
					PlayerDelta -= 1 * Inner(PlayerDelta, WallNormal) * WallNormal;
					Entity->dPos -= 1 * Inner(Entity->dPos, WallNormal) * WallNormal;
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

	real32 Ground = SimRegion->GroundZBase;
	// NOTE: Handle events based on area overlapping
	// TODO: Handle overlapping precisely by moving it into the collision loop?
	{
		rectangle3 EntityRect = RectCenterDim(Entity->Pos, Entity->Dim);
		for (uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; ++TestEntityIndex)
		{
			sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;
			if (CanOverlap(GameState, Entity, TestEntity))
			{
				rectangle3 TestEntityRect = RectCenterDim(TestEntity->Pos, TestEntity->Dim);
				if (RectanglesIntersect(EntityRect, TestEntityRect))
				{
					HandleOverlap(GameState, Entity, TestEntity, dt, &Ground);
				}
			}
		}
	}

	// TODO: This has to become real height handling
	if ((Entity->Pos.Z <= Ground) ||
		(HasFlag(Entity, entity_flag::ZSupported) &&
		(Entity->dPos.Z == 0.0f)))
	{
		Entity->Pos.Z = Ground;
		Entity->dPos.Z = 0;
		AddFlags(Entity, entity_flag::ZSupported);
	}
	else
	{
		ClearFlags(Entity, entity_flag::ZSupported);
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
