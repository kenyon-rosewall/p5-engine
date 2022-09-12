
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

	if (!HasFlag(&Stored->Sim, (uint32)entity_flag::Nonspatial))
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

				Assert(!HasFlag(&Source->Sim, (uint32)entity_flag::Simming));
				AddFlags(&Source->Sim, (uint32)entity_flag::Simming);
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
EntityOverlapsRectangle(v3 Pos, sim_entity_collision_volume Volume, rectangle3 Rect)
{
	bool32 Result = false;

	rectangle3 Grown = AddRadiusTo(Rect, 0.5f * Volume.Dim);
	Result = IsInRectangle(Grown, Pos + Volume.OffsetPos);

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
			Dest->Updatable = EntityOverlapsRectangle(Dest->Pos, Dest->Collision->TotalVolume, SimRegion->UpdatableBounds);
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
	// TODO: If entities were stored in the world, we wouldn't need the game state 

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
														0.0f));
	SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds, V3(UpdateSafetyMargin, 
																   UpdateSafetyMargin, 
																   UpdateSafetyMarginZ));

	// TODO: Need to be more specific about entity counts
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

	world_position MinChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
	world_position MaxChunkPos = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));

	for (int32 ChunkZ = MinChunkPos.ChunkZ; ChunkZ <= MaxChunkPos.ChunkZ; ++ChunkZ)
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
							if (!HasFlag(&EntityLow->Sim, (uint32)entity_flag::Nonspatial))
							{
								v3 SimSpacePos = GetSimSpacePos(SimRegion, EntityLow);
								if (EntityOverlapsRectangle(SimSpacePos, EntityLow->Sim.Collision->TotalVolume, SimRegion->Bounds))
								{
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

		Assert(HasFlag(&Stored->Sim, (uint32)entity_flag::Simming));
		Stored->Sim = *Entity;
		Assert(!HasFlag(&Stored->Sim, (uint32)entity_flag::Simming));

		StoreEntityReference(&Stored->Sim.Sword);

		// TODO: Save state back to the stored entity, once high entities
		// do state decompression, etc.

		world_position NewPos = HasFlag(Entity, (uint32)entity_flag::Nonspatial) ? NullPosition() : MapIntoChunkSpace(GameState->World, SimRegion->Origin, Entity->Pos);
		ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored, NewPos);

		if (Entity->StorageIndex == GameState->CameraFollowingEntityIndex)
		{
			world_position NewCameraP = GameState->CameraP;

			NewCameraP.ChunkZ = Stored->Pos.ChunkZ;

#if 0
			if (Stored->Pos.x > ((17.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkX += 17;
			}
			if (Stored->Pos.x < -((17.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkX -= 17;
			}
			if (Stored->Pos.y > ((9.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkY += 9;
			}
			if (Stored->Pos.y < -((9.0f / 2.0f) * World->TileSideInMeters))
			{
				NewCameraP.ChunkY -= 9;
			}
#else
			// real32 CamZOffset = NewCameraP.Offset.z;
			NewCameraP = Stored->Pos;
			// NewCameraP.Offset.z = CamZOffset;
#endif
			GameState->CameraP = NewCameraP;
		}
	}
}

struct test_wall
{
	real32 x;
	real32 RelX;
	real32 RelY;
	real32 DeltaX;
	real32 DeltaY;
	real32 MinY;
	real32 MaxY;
	v3 Normal;
};

internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, real32* tMin, real32 MinY, real32 MaxY)
{
	bool32 Hit = false;

	real32 tEpsilon = 0.001f;
	if (PlayerDeltaX != 0.0f)
	{
		real32 tResult = (WallX - RelX) / PlayerDeltaX;
		real32 y = RelY + tResult * PlayerDeltaY;
		if ((tResult >= 0.0f) && (*tMin > tResult))
		{
			if ((y >= MinY) && (y <= MaxY))
			{
				*tMin = Maximum(0.0f, tResult - tEpsilon);
				Hit = true;
			}
		}
	}

	return(Hit);
}

internal bool32
CanCollide(game_state* GameState, sim_entity* A, sim_entity* b)
{
	bool32 Result = false;

	if (A != b)
	{
		if (A->StorageIndex > b->StorageIndex)
		{
			sim_entity* Temp = A;
			A = b;
			b = Temp;
		}

		if (HasFlag(A, (uint32)entity_flag::Collides) &&
			HasFlag(b, (uint32)entity_flag::Collides))
		{
			if (!HasFlag(A, (uint32)entity_flag::Nonspatial) &&
				!HasFlag(b, (uint32)entity_flag::Nonspatial))
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
					(Rule->StorageIndexB == b->StorageIndex))
				{
					Result = Rule->CanCollide;
					break;
				}

				Rule = Rule->NextInHash;
			}
		}
	}

	return(Result);
}

internal bool32
HandleCollision(game_state* GameState, sim_entity* A, sim_entity* b)
{
	bool32 StopsOnCollision = false;

	if (A->Type == entity_type::Sword)
	{
		AddCollisionRule(GameState, A->StorageIndex, b->StorageIndex, false);
		StopsOnCollision = false;
	}
	else
	{
		StopsOnCollision = true;
	}

	if (A->Type > b->Type)
	{
		sim_entity* Temp = A;
		A = b;
		b = Temp;
	}

	if ((A->Type == entity_type::Monstar) &&
		(b->Type == entity_type::Sword))
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
		*Ground = GetStairGround(Region, GetEntityGroundPoint(Mover));
	}
}

internal bool32
SpeculativeCollide(sim_entity* Mover, sim_entity* Region, v3 TestPos)
{
	bool32 Result = true;

	if (Region->Type == entity_type::Stairs)
	{
		real32 StepHeight = 0.1f;
#if 0
		Result = ((AbsoluteValue(GetEntityGroundPoint(Mover).z - Ground) > StepHeight) ||
				  ((Bary.y > 0.1f) && (Bary.y < 0.9f)));
#endif
		v3 MoverGroundPoint = GetEntityGroundPoint(Mover, TestPos);
		real32 Ground = GetStairGround(Region, MoverGroundPoint);
		Result = (AbsoluteValue(MoverGroundPoint.z - Ground) > StepHeight);
	}

	return(Result);
}

internal bool32
EntitiesOverlap(sim_entity* Entity, sim_entity* TestEntity, v3 Epsilon = V3(0, 0, 0))
{
	bool32 Result = false;

	for (uint32 VolumeIndex = 0; !Result && (VolumeIndex < Entity->Collision->VolumeCount); ++VolumeIndex)
	{
		sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

		for (uint32 TestVolumeIndex = 0; !Result && (TestVolumeIndex < TestEntity->Collision->VolumeCount); ++TestVolumeIndex)
		{
			sim_entity_collision_volume* TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;
			rectangle3 EntityRect = RectCenterDim(Entity->Pos + Volume->OffsetPos, Volume->Dim + Epsilon);
			rectangle3 TestEntityRect = RectCenterDim(TestEntity->Pos + TestVolume->OffsetPos, TestVolume->Dim);

			Result = (RectanglesIntersect(EntityRect, TestEntityRect));
		}
	}

	return(Result);
}

internal void
MoveEntity(game_state* GameState, sim_region* SimRegion, sim_entity* Entity, real32 dt, move_spec* MoveSpec, v3 ddP)
{
	Assert(!HasFlag(Entity, (uint32)entity_flag::Nonspatial));

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
	v3 Drag = -MoveSpec->Drag * Entity->dPos;
	Drag.z = 0;
	ddP += Drag;
	if (!HasFlag(Entity, (uint32)entity_flag::ZSupported))
	{
		ddP += V3(0, 0, -9.8f);
	}

	v3 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dPos * dt);
	Entity->dPos = ddP * dt + Entity->dPos;
	// TODO: Upgrade physical motion routines to handle capping the
	// maximum velocity
	Assert(LengthSq(Entity->dPos) <= Square(SimRegion->MaxEntityVelocity));
	
	real32 DistanceRemaining = Entity->DistanceLimit;
	if (DistanceRemaining == 0.0f)
	{
		// TODO: Do we want to formalize this number?
		DistanceRemaining = 10000.0f;
	}

	for (uint32 Iteration = 0; Iteration < 4; ++Iteration)
	{
		real32 tMin = 1.0f;
		real32 tMax = 0.0f;

		real32 PlayerDeltaLength = Length(PlayerDelta);
		// TODO: What do we want to do for epsilons here?
		// Think this through for the final collision code
		if (PlayerDeltaLength > 0.0f)
		{
			if (PlayerDeltaLength > DistanceRemaining)
			{
				tMin = DistanceRemaining / PlayerDeltaLength;
			}

			v3 WallNormalMin = {};
			v3 WallNormalMax = {};
			sim_entity* HitEntityMin = 0;
			sim_entity* HitEntityMax = 0;

			v3 DesiredPosition = Entity->Pos + PlayerDelta;

			// NOTE: This is just an optimization to avoid entering the
			// loop in the case where the test entity is non-spatial
			if (!HasFlag(Entity, (uint32)entity_flag::Nonspatial))
			{
				// TODO: Spatial partition here
				for (uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; ++TestEntityIndex)
				{
					sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;
					v3 OverlapEpsilon = 0.001f * V3(1, 1, 1);
					if ((HasFlag(TestEntity, (uint32)entity_flag::Traversable) &&
						EntitiesOverlap(Entity, TestEntity, OverlapEpsilon)) ||
						(CanCollide(GameState, Entity, TestEntity)))
					{
						for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)
						{
							sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

							for (uint32 TestVolumeIndex = 0; TestVolumeIndex < TestEntity->Collision->VolumeCount; ++TestVolumeIndex)
							{
								sim_entity_collision_volume* TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;

								v3 MinkowskiDiameter = Volume->Dim + TestVolume->Dim;

								v3 MinCorner = -0.5f * MinkowskiDiameter;
								v3 MaxCorner = 0.5f * MinkowskiDiameter;

								v3 Rel = ((Entity->Pos + Volume->OffsetPos) - (TestEntity->Pos + TestVolume->OffsetPos));

								if ((Rel.z >= MinCorner.z) && (Rel.z < MaxCorner.z))
								{
									test_wall Walls[] = {
										// Right wall
										{ MinCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y,
										MinCorner.y, MaxCorner.y, V3(-1, 0, 0) },
										// Left wall
										{ MaxCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y,
										MinCorner.y, MaxCorner.y, V3(1, 0, 0) },
										// Top wall
										{ MinCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x,
										MinCorner.x, MaxCorner.x,V3(0, -1, 0) },
										// Bottom wall
										{ MaxCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x,
										MinCorner.x, MaxCorner.x, V3(0, 1, 0) }
									};

									if (HasFlag(TestEntity, (uint32)entity_flag::Traversable))
									{
											real32 tMaxTest = tMax;
											bool32 HitThis = false;
											v3 TestWallNormal = {};

											for (uint32 WallIndex = 0; WallIndex < ArrayCount(Walls); ++WallIndex)
											{
												test_wall* Wall = Walls + WallIndex;

												real32 tEpsilon = 0.001f;
												if (Wall->DeltaX != 0.0f)
												{
													real32 tResult = (Wall->x - Wall->RelX) / Wall->DeltaX;
													real32 y = Wall->RelY + tResult * Wall->DeltaY;
													if ((tResult >= 0.0f) && (tMaxTest < tResult))
													{
														if ((y >= Wall->MinY) && (y <= Wall->MaxY))
														{
															tMaxTest = Maximum(0.0f, tResult - tEpsilon);
															TestWallNormal = Wall->Normal;
															HitThis = true;
														}
													}
												}
											}

											if (HitThis)
											{
												tMax = tMaxTest;
												WallNormalMax = TestWallNormal;
												HitEntityMax = TestEntity;
											}
									}
									else
									{
										real32 tMinTest = tMin;
										bool32 HitThis = false;
										v3 TestWallNormal = {};

										for (uint32 WallIndex = 0; WallIndex < ArrayCount(Walls); ++WallIndex)
										{
											test_wall* Wall = Walls + WallIndex;

											real32 tEpsilon = 0.001f;
											if (Wall->DeltaX != 0.0f)
											{
												real32 tResult = (Wall->x - Wall->RelX) / Wall->DeltaX;
												real32 y = Wall->RelY + tResult * Wall->DeltaY;
												if ((tResult >= 0.0f) && (tMinTest > tResult))
												{
													if ((y >= Wall->MinY) && (y <= Wall->MaxY))
													{
														tMinTest = Maximum(0.0f, tResult - tEpsilon);
														TestWallNormal = Wall->Normal;
														HitThis = true;
													}
												}
											}
										}

										if (HitThis)
										{
											v3 TestPos = Entity->Pos + tMinTest * PlayerDelta;
											if (SpeculativeCollide(Entity, TestEntity, TestPos))
											{
												tMin = tMinTest;
												WallNormalMin = TestWallNormal;
												HitEntityMin = TestEntity;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			real32 tStop = 0.0f;
			sim_entity* HitEntity = 0;
			v3 WallNormal = {};
			if (tMin < tMax)
			{
				tStop = tMin;
				HitEntity = HitEntityMin;
				WallNormal = WallNormalMin;
			}
			else
			{
				tStop = tMax;
				HitEntity = HitEntityMax;
				WallNormal = WallNormalMax;
			}

			Entity->Pos += tStop * PlayerDelta;
			DistanceRemaining -= tStop * PlayerDeltaLength;

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

	real32 Ground = 0.0f;
	for (uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; ++TestEntityIndex)
	{
		sim_entity* TestEntity = SimRegion->Entities + TestEntityIndex;
		if (CanOverlap(GameState, Entity, TestEntity) &&
			EntitiesOverlap(Entity, TestEntity))
		{
			HandleOverlap(GameState, Entity, TestEntity, dt, &Ground);
		}
	}

	Ground += Entity->Pos.z - GetEntityGroundPoint(Entity).z;
	if ((Entity->Pos.z <= Ground) ||
		(HasFlag(Entity, (uint32)entity_flag::ZSupported) &&
		(Entity->dPos.z == 0.0f)))
	{
		Entity->Pos.z = Ground;
		Entity->dPos.z = 0;
		AddFlags(Entity, (uint32)entity_flag::ZSupported);
	}
	else
	{
		ClearFlags(Entity, (uint32)entity_flag::ZSupported);
	}

	if (Entity->DistanceLimit != 0.0f)
	{
		Entity->DistanceLimit = DistanceRemaining;
	}

	// TODO: Change to using the acceleration vector
	if ((Entity->dPos.x == 0.0f) && (Entity->dPos.y == 0.0f))
	{
		// NOTE: Leave FacingDirection whatever it was
	}
	else
	{
		Entity->FacingDirection = ATan2(Entity->dPos.y, Entity->dPos.x);
	}
}
