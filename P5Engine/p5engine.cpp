#include "p5engine.h"
#include "p5engine_random.h"
#include "p5engine_render_group.cpp"
#include "p5engine_world.cpp"
#include "p5engine_sim_region.cpp"
#include "p5engine_entity.cpp"
#include "p5engine_asset.cpp"

internal void
GameOutputSound(game_state* GameState, game_sound_output_buffer* SoundBuffer, int ToneHz)
{
	int16 ToneVolume = 2000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16* SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		int SampleValue = 0;
	
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}
}

internal void
DrawRectangleOutline_(loaded_bitmap* Buffer, v2 vMin, v2 vMax, v3 Color, real32 r = 2.0f)
{
#if 0 
	// NOTE: Rop and boRRom
	DrawRectangle(Buffer, V2(vMin.x - r, vMin.y - r), V2(vMax.x + r, vMin.y + r), ToV4(Color, 1));
	DrawRectangle(Buffer, V2(vMin.x - r, vMax.y - r), V2(vMax.x + r, vMax.y + r), ToV4(Color, 1));

	// NOTE: Left and righte
	DrawRectangle(Buffer, V2(vMin.x - r, vMin.y - r), V2(vMin.x + r, vMax.y + r), ToV4(Color, 1));
	DrawRectangle(Buffer, V2(vMax.x - r, vMin.y - r), V2(vMax.x + r, vMax.y + r), ToV4(Color, 1));
#endif
}

struct add_low_entity_result
{
	low_entity* Low;
	uint32 LowIndex;
};

internal add_low_entity_result
AddLowEntity(game_state* GameState, entity_type Type, world_position Pos)
{
	Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
	uint32 EntityIndex = GameState->LowEntityCount++;

	low_entity* EntityLow = GameState->LowEntities + EntityIndex;
	*EntityLow = {};
	EntityLow->Sim.Type = Type;
	EntityLow->Sim.Collision = GameState->NullCollision;
	EntityLow->Pos = NullPosition();

	ChangeEntityLocation(&GameState->WorldArena, GameState->World, EntityIndex, EntityLow, Pos);

	add_low_entity_result Result = {};
	Result.Low = EntityLow;
	Result.LowIndex = EntityIndex;

	// TODO: Do we need to have a begin/end paradigm for adding
	// entities so that they can be brought into the high set when they
	// are added and are in the camera region?

	return(Result);
}

internal add_low_entity_result
AddGroundedEntity(game_state* GameState, entity_type Type, world_position Pos, sim_entity_collision_volume_group* Collision)
{
	add_low_entity_result Entity = AddLowEntity(GameState, Type, Pos);
	Entity.Low->Sim.Collision = Collision;

	return(Entity);
}

internal add_low_entity_result
AddStandardRoom(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32  AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Space, Pos, GameState->StandardRoomCollision);
	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Traversable);

	return(Entity);
}

internal add_low_entity_result
AddWall(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Wall, Pos, GameState->WallCollision);

	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Collides);

	return(Entity);
}

internal add_low_entity_result
AddStairs(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Stairs, Pos, GameState->StairsCollision);

	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Collides);
	Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.xy;
	Entity.Low->Sim.WalkableHeight = GameState->TypicalFloorHeight;

	return(Entity);
}

internal void
InitHitPoints(low_entity* EntityLow, uint32 HitPointCount)
{
	Assert(HitPointCount <= ArrayCount(EntityLow->Sim.HitPoint));

	EntityLow->Sim.HitPointMax = HitPointCount;
	for (uint32 HitPointIndex = 0; HitPointIndex < EntityLow->Sim.HitPointMax; ++HitPointIndex)
	{
		hit_point* HitPoint = EntityLow->Sim.HitPoint + HitPointIndex;
		HitPoint->Flags = 0;
		HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

internal add_low_entity_result
AddSword(game_state* GameState)
{
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Sword, NullPosition());
	Entity.Low->Sim.Collision = GameState->SwordCollision;
	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Moveable);

	return(Entity);
}

internal add_low_entity_result
AddPlayer(game_state* GameState)
{
	world_position Pos = GameState->CameraP;
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Hero, Pos, GameState->HeroCollision);

	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Collides | (uint32)entity_flag::Moveable);

	InitHitPoints(Entity.Low, 3);

	add_low_entity_result Sword = AddSword(GameState);
	Entity.Low->Sim.Sword.Index = Sword.LowIndex;

	if (GameState->CameraFollowingEntityIndex == 0)
	{
		GameState->CameraFollowingEntityIndex = Entity.LowIndex;
	}

	return(Entity);
}

internal add_low_entity_result
AddMonstar(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Monstar, Pos, GameState->MonstarCollision);

	InitHitPoints(Entity.Low, 3);

	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Collides | (uint32)entity_flag::Moveable);

	return(Entity);
}

internal add_low_entity_result
AddFamiliar(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Familiar, Pos, GameState->FamiliarCollision);

	AddFlags(&Entity.Low->Sim, (uint32)entity_flag::Collides | (uint32)entity_flag::Moveable);

	return(Entity);
}

internal void
DrawHitpoints(sim_entity* Entity, render_group* RenderGroup)
{
	if (Entity->HitPointMax >= 1)
	{
		v2 HealthDim = V2(0.2f, 0.2f);
		real32 SpacingX = 1.5f * HealthDim.x;
		real32 FirstX = -0.5f * (Entity->HitPointMax - 1) * SpacingX;
		v2 HitPos = V2(FirstX, -0.4f);
		v2 dHitPos = V2(SpacingX, 0.0f);
		for (uint32 HitPointIndex = 0; HitPointIndex < Entity->HitPointMax; ++HitPointIndex)
		{
			hit_point* HitPoint = Entity->HitPoint + HitPointIndex;

			v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
			if (HitPoint->FilledAmount == 0)
			{
				Color = V4(0.2f, 0.2f, 0.2f, 1.0f);
			}

			PushRect(RenderGroup, ToV3(HitPos, 0), HealthDim, Color);
			HitPos += dHitPos;
		}
	}
}

internal void
ClearCollisionRulesFor(game_state* GameState, uint32 StorageIndex)
{
	// TODO: Need to make a better data structure that allows
	// removal of collision rules without searching the entire table
	
	// NOTE: One way to make removal easy would be to always
	// add _both_ orders of the pairs of storage indices to the 
	// hash table, so no matter which position the entity is in,
	// you can always find it. Then, when you do your first pass 
	// through for removal, you just remember the original top
	// of the free list, and when you're done, do a pass through all
	// the new things on the free list, and remove the reverse of
	// those pairs.
	for (uint32 HashBucket = 0; HashBucket < ArrayCount(GameState->CollisionRuleHash); ++HashBucket)
	{
		pairwise_collision_rule** Rule = &GameState->CollisionRuleHash[HashBucket];
		while ((*Rule))
		{
			if (((*Rule)->StorageIndexA == StorageIndex) ||
				((*Rule)->StorageIndexB == StorageIndex))
			{
				pairwise_collision_rule* RemovedRule = *Rule;
				*Rule = (*Rule)->NextInHash;

				RemovedRule->NextInHash = GameState->FirstFreeCollisionRule;
				GameState->FirstFreeCollisionRule = RemovedRule;
			}
			else
			{
				Rule = &(*Rule)->NextInHash;
			}
		}
	}
}

internal void
AddCollisionRule(game_state* GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide)
{
	// TODO: Collapse this with CanCollide
	if (StorageIndexA > StorageIndexB)
	{
		uint32 Temp = StorageIndexA;
		StorageIndexA = StorageIndexB;
		StorageIndexB = Temp;
	}

	// TODO: Better hash function
	pairwise_collision_rule* Found = 0;
	uint32 ArrayHash = (ArrayCount(GameState->CollisionRuleHash) - 1);
	uint32 HashBucket = StorageIndexA & ArrayHash;
	pairwise_collision_rule* Rule = GameState->CollisionRuleHash[HashBucket];
	while (Rule)
	{
		if ((Rule->StorageIndexA == StorageIndexA) &&
			(Rule->StorageIndexB == StorageIndexB))
		{
			Found = Rule;
			break;
		}

		Rule = Rule->NextInHash;
	}

	if (!Found)
	{
		Found = GameState->FirstFreeCollisionRule;
		if (Found)
		{
			GameState->FirstFreeCollisionRule = Found->NextInHash;
		}
		else
		{
			Found = PushStruct(&GameState->WorldArena, pairwise_collision_rule);
		}

		Found->NextInHash = GameState->CollisionRuleHash[HashBucket];
		GameState->CollisionRuleHash[HashBucket] = Found;
	}

	if (Found)
	{
		Found->StorageIndexA = StorageIndexA;
		Found->StorageIndexB = StorageIndexB;
		Found->CanCollide = CanCollide;
	}
}

internal sim_entity_collision_volume_group*
MakeSimpleGroundedCollision(game_state* GameState, real32 DimX, real32 DimY, real32 DimZ)
{
	// TODO: NOT WORLD ARENA. Change to using the fundamental types arena, etc
	sim_entity_collision_volume_group* Group = PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
	Group->VolumeCount = 1;
	Group->Volumes = PushArray(&GameState->WorldArena, Group->VolumeCount, sim_entity_collision_volume);
	Group->TotalVolume.OffsetPos = V3(0, 0, 0.5f * DimZ);
	Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);
	Group->Volumes[0] = Group->TotalVolume;

	return(Group);
}

internal sim_entity_collision_volume_group*
MakeNullCollision(game_state* GameState)
{
	// TODO: NOT WORLD ARENA. Change to using the fundamental types arena, etc
	sim_entity_collision_volume_group* Group = PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
	Group->VolumeCount = 0;
	Group->Volumes = 0;
	Group->TotalVolume.OffsetPos = V3(0, 0, 0);
	// TODO: Should this be negative
	Group->TotalVolume.Dim = V3(0, 0, 0);

	return(Group);
}

internal task_with_memory*
BeginTaskWithMemory(transient_state* TransientState)
{
	task_with_memory* Result = 0;

	for (uint32 TaskIndex = 0; TaskIndex < ArrayCount(TransientState->Tasks); ++TaskIndex)
	{
		task_with_memory* Task = TransientState->Tasks + TaskIndex;
		if (!Task->BeingUsed)
		{
			Result = Task;
			Task->BeingUsed = true;
			Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
			break;
		}
	}

	return(Result);
}

internal void
EndTaskWithMemory(task_with_memory* Task)
{
	EndTemporaryMemory(Task->MemoryFlush);
	
	CompletePreviousWritesBeforeFutureWrites;
	Task->BeingUsed = false;
}

struct fill_ground_chunk_work
{
	render_group* RenderGroup;
	loaded_bitmap* Buffer;
	task_with_memory* Task;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(FillGroundChunkWork)
{
	fill_ground_chunk_work* Work = (fill_ground_chunk_work*)Data;

	RenderGroupToOutput(Work->RenderGroup, Work->Buffer);
	
	EndTaskWithMemory(Work->Task);
}

#if 0
internal int32
PickBest(int32 InfoCount, asset_bitmap_info* Infos, asset_tag* Tags, real32* MatchVector, real32* WeightVector)
{
	real32 BestDiff = Real32Maximum;
	int32 BestIndex = 0;

	for (int32 InfoIndex = 0; InfoIndex < InfoCount; ++InfoIndex)
	{
		asset_bitmap_info* Info = Infos + InfoIndex;

		real32 TotalWeightedDiff = 0.0f;
		for (uint32 TagIndex = Info->FirstTagIndex; TagIndex < Info->OnePastLastTagIndex; ++TagIndex)
		{
			asset_tag* Tag = Tags + TagIndex;
			real32 Difference = MatchVector[Tag->ID] - Tag->Value;
			real32 Weighted = WeightVector[Tag->ID] * AbsoluteValue(Difference);
			TotalWeightedDiff += Weighted;
		}

		if (BestDiff > TotalWeightedDiff)
		{
			BestDiff = TotalWeightedDiff;
			BestIndex = InfoIndex;
		}
	}
}
#endif

internal void
FillGroundChunk(transient_state* TransientState, game_state* GameState, ground_buffer* GroundBuffer, world_position* ChunkPos)
{
	task_with_memory* Task = BeginTaskWithMemory(TransientState);
	if (Task)
	{
		fill_ground_chunk_work* Work = PushStruct(&Task->Arena, fill_ground_chunk_work);

		loaded_bitmap* Buffer = &GroundBuffer->Bitmap;
		Buffer->AlignPercentage = V2(0.5f, 0.5f);
		Buffer->WidthOverHeight = 1.0f;

		real32 Width = GameState->World->ChunkDimInMeters.x;
		real32 Height = GameState->World->ChunkDimInMeters.y;
		Assert(Width == Height);
		v2 HalfDim = 0.5f * V2(Width, Height);

		// TODO: Decide what our pushbuffer size is!
		// TODO: Safe cast from memory_uint to uint32?
		render_group* RenderGroup = AllocateRenderGroup(TransientState->Assets, &Task->Arena, 0); // (uint32)GetArenaSizeRemaining(&Task->Arena));
		Orthographic(RenderGroup, Buffer->Width, Buffer->Height, (Buffer->Width - 2) / Width);
		Clear(RenderGroup, V4(0.25f, 0.44f, 0.3f, 1));

		Work->Buffer = Buffer;
		Work->RenderGroup = RenderGroup;
		Work->Task = Task;

		for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ++ChunkOffsetY)
		{
			for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ++ChunkOffsetX)
			{
				int32 ChunkX = ChunkPos->ChunkX + ChunkOffsetX;
				int32 ChunkY = ChunkPos->ChunkY + ChunkOffsetY;
				int32 ChunkZ = ChunkPos->ChunkZ;

				// TODO: Make random number generation more systemic
				// TODO: Look into wang hashing here or some other spatial seed generation
				random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

				v2 Center = V2(ChunkOffsetX * Width, ChunkOffsetY * Height);

				bitmap_id Stamp = GetFirstBitmapID(TransientState->Assets, asset_type_id::Soil);
				Stamp.Value += 2;

				for (uint32 SoilIndex = 0; SoilIndex < 32; ++SoilIndex)
				{
					v2 Pos = Center + Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));

					PushBitmap(RenderGroup, Stamp, ToV3(Pos, 0), 4.0f, V4(0.5f, 0.5f, 0.5f, 0.1f));
				}
			}
		}

		for (int32 ChunkOffsetY = -1; ChunkOffsetY <= 1; ++ChunkOffsetY)
		{
			for (int32 ChunkOffsetX = -1; ChunkOffsetX <= 1; ++ChunkOffsetX)
			{
				for (uint32 GrassIndex = 0; GrassIndex < 5; ++GrassIndex)
				{
					int32 ChunkX = ChunkPos->ChunkX + ChunkOffsetX;
					int32 ChunkY = ChunkPos->ChunkY + ChunkOffsetY;
					int32 ChunkZ = ChunkPos->ChunkZ;

					// TODO: Make random number generation more systemic
					// TODO: Look into wang hashing here or some other spatial seed generation
					random_series Series = RandomSeed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);

					v2 Center = V2(ChunkOffsetX * Width, ChunkOffsetY * Height);

					bitmap_id Stamp = {};

					if (RandomChoice(&Series, 12) == 1)
					{
						Stamp = RandomAssetFrom(TransientState->Assets, asset_type_id::Grass, &Series);
					}
					else
					{
						Stamp = RandomAssetFrom(TransientState->Assets, asset_type_id::Tuft, &Series);
					}

					if (Stamp.Value)
					{
						v2 Pos = Center + Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));

						PushBitmap(RenderGroup, Stamp, ToV3(Pos, 0), 1.3f, V4(1, 1, 1, 0.4f));
					}
				}
			}
		}


		if (AllResourcesPresent(RenderGroup))
		{
			GroundBuffer->Pos = *ChunkPos;

			PlatformAddEntry(TransientState->LowPriorityQueue, FillGroundChunkWork, Work);
		}
		else
		{
			EndTaskWithMemory(Task);
		}
	}
}

internal void
ClearBitmap(loaded_bitmap* Bitmap)
{
	if (Bitmap->Memory)
	{
		int32 TotalBitmapSize = Bitmap->Width * Bitmap->Height * BITMAP_BYTES_PER_PIXEL;
		ZeroSize(TotalBitmapSize, Bitmap->Memory);
	}
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena* Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
	loaded_bitmap Result = {};

	Result.Width = Width;
	Result.Height = Height;
	Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
	int32 TotalBitmapSize = Width * Height * BITMAP_BYTES_PER_PIXEL;
	Result.Memory = (uint32*)PushSize(Arena, TotalBitmapSize, 16);
	if (ClearToZero)
	{
		ClearBitmap(&Result);
	}

	return(Result);
}

internal void
MakeSphereDiffuseMap(loaded_bitmap* Bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
	real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
	real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

	uint8* Row = (uint8*)Bitmap->Memory;
	for (int32 Y = 0; Y < Bitmap->Height; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int32 X = 0; X < Bitmap->Width; ++X)
		{
			v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);

			real32 Nx = Cx * (2.0f * BitmapUV.x - 1.0f);
			real32 Ny = Cy * (2.0f * BitmapUV.y - 1.0f);

			real32 RootTerm = 1.0f - Nx * Nx - Ny * Ny;
			real32 Alpha = 0.0f;
			if (RootTerm >= 0)
			{
				Alpha = 1.0f;
			}
			else
			{

			}

			v3 BaseColor = V3(0, 0, 0);
			Alpha *= 255.0f;
			v4 Color = V4(
				Alpha * BaseColor.x,
				Alpha * BaseColor.y,
				Alpha * BaseColor.z,
				Alpha
			);

			*Pixel++ = (((uint32)(Color.a + 0.5f) << 24) |
						((uint32)(Color.r + 0.5f) << 16) |
						((uint32)(Color.g + 0.5f) <<  8) |
						((uint32)(Color.b + 0.5f) <<  0));
		}

		Row += Bitmap->Pitch;
	}
}

internal void
MakeSphereNormalMap(loaded_bitmap* Bitmap, real32 Roughness, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
	real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
	real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

	uint8* Row = (uint8*)Bitmap->Memory;
	for (int32 Y = 0; Y < Bitmap->Height; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int32 X = 0; X < Bitmap->Width; ++X)
		{
			v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);
			
			real32 Nx = Cx * (2.0f * BitmapUV.x - 1.0f);
			real32 Ny = Cy * (2.0f * BitmapUV.y - 1.0f);

			real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
			v3 Normal = V3(0, 0.707106781188f, 0.707106781188f);
			real32 Nz = 0.0f;
			if (RootTerm >= 0)
			{
				Nz = SquareRoot(RootTerm);
				Normal = V3(Nx, Ny, Nz);
			}

			v4 Color = V4(
				255.0f * (0.5f * (Normal.x + 1.0f)), 
				255.0f * (0.5f * (Normal.y + 1.0f)), 
				255.0f * (0.5f * (Normal.z + 1.0f)),
				255.0f * Roughness
			);

			*Pixel++ = (((uint32)(Color.a + 0.5f) << 24) |
						((uint32)(Color.r + 0.5f) << 16) |
						((uint32)(Color.g + 0.5f) <<  8) |
						((uint32)(Color.b + 0.5f) <<  0));
		}

		Row += Bitmap->Pitch;
	}
}

internal void
MakePyramidNormalMap(loaded_bitmap* Bitmap, real32 Roughness)
{
	real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
	real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

	uint8* Row = (uint8*)Bitmap->Memory;
	for (int32 Y = 0; Y < Bitmap->Height; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int32 X = 0; X < Bitmap->Width; ++X)
		{
			v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);

			int32 InvX = (Bitmap->Width - 1) - X;
			real32 Seven = 0.707106781188f;
			v3 Normal = V3(0, 0, Seven);
			if (X < Y)
			{
				if (InvX < Y)
				{
					Normal.x = -Seven;
				}
				else
				{
					Normal.y = Seven;
				}
			}
			else
			{
				if (InvX < Y)
				{
					Normal.y = -Seven;
				}
				else
				{
					Normal.x = Seven;
				}
			}

			v4 Color = V4(
				255.0f * (0.5f * (Normal.x + 1.0f)),
				255.0f * (0.5f * (Normal.y + 1.0f)),
				255.0f * (0.5f * (Normal.z + 1.0f)),
				255.0f * Roughness
			);

			*Pixel++ = (((uint32)(Color.a + 0.5f) << 24) |
				((uint32)(Color.r + 0.5f) << 16) |
				((uint32)(Color.g + 0.5f) << 8) |
				((uint32)(Color.b + 0.5f) << 0));
		}

		Row += Bitmap->Pitch;
	}
}

#if P5ENGINE_INTERNAL
game_memory* DebugGlobalMemory;
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	PlatformAddEntry = Memory->PlatformAddEntry;
	PlatformCompleteAllWork = Memory->PlatformCompleteAllWork;
	PlatformReadEntireFile = Memory->DEBUGPlatformReadEntireFile;

#if P5ENGINE_INTERNAL
	DebugGlobalMemory = Memory;
#endif
	
	BEGIN_TIMED_BLOCK(GameUpdateRender);
	
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));

	uint32 GroundBufferWidth = 256;
	uint32 GroundBufferHeight = 256;

	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!GameState->IsInitialized)
	{
		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;

		GameState->TypicalFloorHeight = 3.0f;

		// TODO: Remove this!
		real32 PixelsToMeters = 1.0f / 42.0f;
		v3 WorldChunkDimInMeters = V3(
			PixelsToMeters * (real32)GroundBufferWidth,
			PixelsToMeters * GroundBufferHeight,
			GameState->TypicalFloorHeight
		);

		// TODO: Talk about this soon. Let's start partitioning our memory space.
		InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));

		// NOTE: Reserve entity slot 0 for the null entity
		AddLowEntity(GameState, entity_type::Null, NullPosition());

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world* World = GameState->World;
		InitializeWorld(World, WorldChunkDimInMeters);

		real32 TileSideInMeters = 1.4f;
		real32 TileDepthInMeters = GameState->TypicalFloorHeight;

		GameState->NullCollision = MakeNullCollision(GameState);
		GameState->SwordCollision = MakeSimpleGroundedCollision(
			GameState, 1.0f, 0.5f, 0.1f
		);
		GameState->StairsCollision = MakeSimpleGroundedCollision(
			GameState, TileSideInMeters, 2.0f * TileSideInMeters, 1.1f * TileDepthInMeters
		);
		GameState->HeroCollision = MakeSimpleGroundedCollision(
			GameState, 1.0f, 0.5f, 1.2f
		);
		GameState->MonstarCollision = MakeSimpleGroundedCollision(
			GameState, 1.0f, 0.5f, 0.5f
		);
		GameState->FamiliarCollision = MakeSimpleGroundedCollision(
			GameState, 1.0f, 0.5f, 0.5f
		);
		GameState->WallCollision = MakeSimpleGroundedCollision(
			GameState, TileSideInMeters, TileSideInMeters, TileDepthInMeters
		);
		GameState->StandardRoomCollision = MakeSimpleGroundedCollision(
			GameState, TilesPerWidth * TileSideInMeters, TilesPerHeight * TileSideInMeters, 0.9f * TileDepthInMeters
		);

		random_series Series = RandomSeed(0);

		uint32 ScreenBaseX = 0;
		uint32 ScreenBaseY = 0;
		uint32 ScreenBaseZ = 0;
		uint32 ScreenX = ScreenBaseX;
		uint32 ScreenY = ScreenBaseY;
		uint32 AbsTileZ = ScreenBaseZ;

		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;
		for (uint32 ScreenIndex = 0; ScreenIndex < 2000; ++ScreenIndex)
		{

#if 1
			uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 4);
#else
			uint32 DoorDirection = RandomChoice(&Series, 2);
#endif

			// DoorDirection = 3;

			bool32 CreatedZDoor = false;
			if (DoorDirection == 3)
			{
				CreatedZDoor = true;
				DoorDown = true;
			}
			else if (DoorDirection == 2)
			{
				CreatedZDoor = true;
				DoorUp = true;
			}
			else if (DoorDirection == 1)
			{
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}

			AddStandardRoom(GameState, 
				ScreenX * TilesPerWidth + TilesPerWidth / 2,
				ScreenY * TilesPerHeight + TilesPerHeight / 2,
				AbsTileZ);

			for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY)
			{
				for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX)
				{
					uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

					bool32 ShouldBeDoor = false;
					if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight / 2))))
					{
						ShouldBeDoor = true;
					}

					if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight / 2))))
					{
						ShouldBeDoor = true;
					}

					if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth / 2))))
					{
						ShouldBeDoor = true;
					}

					if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth / 2))))
					{
						ShouldBeDoor = true;
					}

					if (ShouldBeDoor)
					{
						AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);
					}
					else if (CreatedZDoor)
					{
						if (((AbsTileZ % 2) && (TileX == 8) && (TileY == 5)) ||
							(!(AbsTileZ % 2) && (TileX == 12) && TileY == 5))
						{
							AddStairs(GameState, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ);
						}
					}
				}
			}

			DoorLeft = DoorRight;
			DoorBottom = DoorTop;

			if (CreatedZDoor)
			{
				DoorDown = !DoorDown;
				DoorUp = !DoorUp;
			}
			else
			{
				DoorDown = false;
				DoorUp = false;
			}

			DoorRight = false;
			DoorTop = false;

			if (DoorDirection == 3)
			{
				AbsTileZ -= 1;
			}
			else if (DoorDirection == 2)
			{
				AbsTileZ += 1;
			}
			else if (DoorDirection == 1)
			{
				ScreenX += 1;
			}
			else
			{
				ScreenY += 1;
			}
		}

		world_position NewCameraP = {};
		uint32 CameraTileX = ScreenBaseX * TilesPerWidth + 17 / 2;
		uint32 CameraTileY = ScreenBaseY * TilesPerHeight + 9 / 2;
		uint32 CameraTileZ = ScreenBaseZ;
		NewCameraP = ChunkPositionFromTilePosition(World, 
													CameraTileX,
													CameraTileY,
													CameraTileZ);
		GameState->CameraP = NewCameraP;

		AddMonstar(GameState, CameraTileX - 4, CameraTileY + 2, CameraTileZ);
		AddFamiliar(GameState, CameraTileX - 2, CameraTileY + 2, CameraTileZ);

		GameState->IsInitialized = true;
	}

	// NOTE: Transient initialization
	Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
	transient_state* TransientState = (transient_state*)Memory->TransientStorage;
	if (!TransientState->IsInitialized)
	{
		InitializeArena(&TransientState->TransientArena, Memory->TransientStorageSize - sizeof(transient_state), (uint8*)Memory->TransientStorage + sizeof(transient_state));

		TransientState->HighPriorityQueue = Memory->HighPriorityQueue;
		TransientState->LowPriorityQueue = Memory->LowPriorityQueue;
		for (uint32 TaskIndex = 0; TaskIndex < ArrayCount(TransientState->Tasks); ++TaskIndex)
		{
			task_with_memory* Task = TransientState->Tasks + TaskIndex;

			Task->BeingUsed = false;
			SubArena(&Task->Arena, &TransientState->TransientArena, Megabytes(1));
		}

		TransientState->Assets = AllocateGameAssets(&TransientState->TransientArena, Megabytes(64), TransientState);

		TransientState->GroundBufferCount = 256;
		TransientState->GroundBuffers = PushArray(&TransientState->TransientArena, TransientState->GroundBufferCount, ground_buffer);
		for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
		{
			ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
			GroundBuffer->Bitmap = MakeEmptyBitmap(&TransientState->TransientArena, GroundBufferWidth, GroundBufferHeight, false);
			GroundBuffer->Pos = NullPosition();
		}

		GameState->TestDiffuse = MakeEmptyBitmap(&TransientState->TransientArena, 256, 256, false);
		
		GameState->TestNormal = MakeEmptyBitmap(&TransientState->TransientArena, GameState->TestDiffuse.Width, GameState->TestDiffuse.Height, false);
		MakeSphereNormalMap(&GameState->TestNormal, 0.0f);
		MakeSphereDiffuseMap(&GameState->TestDiffuse);
		// MakePyramidNormalMap(&GameState->TestNormal, 0.0f);

		TransientState->EnvMapWidth = 512;
		TransientState->EnvMapHeight = 256;
		for (uint32 MapIndex = 0; MapIndex < ArrayCount(TransientState->EnvMaps); ++MapIndex)
		{
			environment_map* Map = TransientState->EnvMaps + MapIndex;
			uint32 Width = TransientState->EnvMapWidth;
			uint32 Height = TransientState->EnvMapHeight;
			for (uint32 LODIndex = 0; LODIndex < ArrayCount(Map->LOD); ++LODIndex)
			{
				Map->LOD[LODIndex] = MakeEmptyBitmap(&TransientState->TransientArena, Width, Height, false);
				Width >>= 1;
				Height >>= 1;
			}
		}

		TransientState->IsInitialized = true;
	}

#if 0
	if (Input->ExecutableReloaded)
	{
		for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
		{
			ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
			GroundBuffer->Pos = NullPosition();
		}
	}
#endif

	world* World = GameState->World;

	for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
	{
		game_controller_input* Controller = GetController(Input, ControllerIndex);
		controlled_hero* ConHero = GameState->ControlledHeroes + ControllerIndex;

		if (ConHero->EntityIndex == 0)
		{
			if (Controller->Start.EndedDown)
			{
				*ConHero = {};
				ConHero->EntityIndex = AddPlayer(GameState).LowIndex;
			}
		}
		else
		{
			ConHero->ddP = {};
			ConHero->SpeedMultiplier = 1.0f;
			ConHero->dZ = 0.0f;

			if (Controller->IsAnalogL)
			{
				// NOTE: Use analog movement tuning
				ConHero->ddP = V3(Controller->StickAverageLX, Controller->StickAverageLY, 0);
			}
			else
			{
				// NOTE: Use digital movement tuning
				if (Controller->MoveUp.EndedDown)
				{
					ConHero->ddP.y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown)
				{
					ConHero->ddP.y = -1.0f;
				}
				if (Controller->MoveLeft.EndedDown)
				{
					ConHero->ddP.x = -1.0f;
				}
				if (Controller->MoveRight.EndedDown)
				{
					ConHero->ddP.x = 1.0f;
				}
			}

			if (Controller->ActionRight.EndedDown)
			{
				ConHero->SpeedMultiplier = 2.2f;
			}

			ConHero->dSword = {};
			if (Controller->ActionLeft.EndedDown)
			{
				sim_entity ControllingEntity = GetLowEntity(GameState, ConHero->EntityIndex)->Sim;
				switch (ControllingEntity.FacingDirection)
				{
				case 0:
				{
					ConHero->dSword = V3(1.0f, 0.0f, 0.0f);
				} break;

				case 1:
				{
					ConHero->dSword = V3(0.0f, 1.0f, 0.0f);
				} break;

				case 2:
				{
					ConHero->dSword = V3(-1.0f, 0.0f, 0.0f);
				} break;

				case 3:
				{
					ConHero->dSword = V3(0.0f, -1.0f, 0.0f);
				} break;
				}
			}
		}
	}

	loaded_bitmap DrawBuffer_ = {};
	loaded_bitmap* DrawBuffer = &DrawBuffer_;
	DrawBuffer->Width = Buffer->Width;
	DrawBuffer->Height = Buffer->Height;
	DrawBuffer->Pitch = Buffer->Pitch;
	DrawBuffer->Memory = Buffer->Memory;

#if 0
	// NOTE: Enable this to test weird buffer sizes in the renderer
	DrawBuffer->Width = 1279;
	DrawBuffer->Height = 719;
#endif

	//
	// NOTE: Render
	//
	temporary_memory RenderMemory = BeginTemporaryMemory(&TransientState->TransientArena);
	
	// TODO: Decide what our push buffer size is
	render_group* RenderGroup = AllocateRenderGroup(TransientState->Assets, &TransientState->TransientArena, Megabytes(4));
	real32 WidthOfMonitor = 0.635f; // NOTE: Horizontal measurement of monitor in meters (approximate)
	real32 MetersToPixels = (real32)DrawBuffer->Width * WidthOfMonitor;
	real32 FocalLength = 0.6f;
	real32 DistanceAboveGround = 9.0f;
	Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);
	Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0));

	rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
	rectangle3 CameraBoundsInMeters = RectMinMax(ToV3(ScreenBounds.Min, 0), ToV3(ScreenBounds.Max, 0));
	CameraBoundsInMeters.Min.z = -3.0f * GameState->TypicalFloorHeight;
	CameraBoundsInMeters.Max.z = 1.0f * GameState->TypicalFloorHeight;

	// NOTE: Ground chuck rendering
	for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
	{
		ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
		if (IsValid(GroundBuffer->Pos))
		{
			loaded_bitmap* Bitmap = &GroundBuffer->Bitmap;

			v3 Delta = Subtract(GameState->World, &GroundBuffer->Pos, &GameState->CameraP);

			if ((Delta.z >= -1.0f) && (Delta.z < 1.0f))
			{
				real32 GroundSideInMeters = World->ChunkDimInMeters.x;
				PushBitmap(RenderGroup, Bitmap, Delta, GroundSideInMeters);
#if 0 
				PushRectOutline(RenderGroup, Delta, V2(GroundSideInMeters, GroundSideInMeters), V4(1, 1, 0, 1));
#endif
			}
		}
	}

	// NOTE: Ground chunk updating
	world_position MinChunkPos = MapIntoChunkSpace(World, GameState->CameraP, GetMinCorner(CameraBoundsInMeters));
	world_position MaxChunkPos = MapIntoChunkSpace(World, GameState->CameraP, GetMaxCorner(CameraBoundsInMeters));

	for (int32 ChunkZ = MinChunkPos.ChunkZ; ChunkZ <= MaxChunkPos.ChunkZ; ++ChunkZ)
	{
		for (int32 ChunkY = MinChunkPos.ChunkY; ChunkY <= MaxChunkPos.ChunkY; ++ChunkY)
		{
			for (int32 ChunkX = MinChunkPos.ChunkX; ChunkX <= MaxChunkPos.ChunkX; ++ChunkX)
			{
				world_position ChunkCenterPos = CenteredChunkPoint(ChunkX, ChunkY, ChunkZ);
				v3 RelPos = Subtract(World, &ChunkCenterPos, &GameState->CameraP);

				// TODO: This is super inefficient, fix it tomorrow
				real32 FurthestBufferLengthSq = 0.0f;
				ground_buffer* FurthestBuffer = 0;
				for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
				{
					ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
					if (AreInSameChunk(World, &GroundBuffer->Pos, &ChunkCenterPos))
					{
						FurthestBuffer = 0;
						break;
					}
					else if (IsValid(GroundBuffer->Pos))
					{
						v3 RelPos = Subtract(World, &GroundBuffer->Pos, &GameState->CameraP);
						real32 BufferLengthSq = LengthSq(RelPos.xy);
						if (FurthestBufferLengthSq < BufferLengthSq)
						{
							FurthestBufferLengthSq = BufferLengthSq;
							FurthestBuffer = GroundBuffer;
						}
					}
					else
					{
						FurthestBufferLengthSq = Real32Maximum;
						FurthestBuffer = GroundBuffer;
					}
				}

				if (FurthestBuffer)
				{
					FillGroundChunk(TransientState, GameState, FurthestBuffer, &ChunkCenterPos);
				}
			}
		}
	}

	// TODO: How big do we actually want to expand here?
	// TODO: Do we want to simulate upper floors, etc?
	v3 SimBoundsExpansion = V3(15.0f, 15.0f, 0.0f);
	rectangle3 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
	temporary_memory SimMemory = BeginTemporaryMemory(&TransientState->TransientArena);
	world_position SimCenterPos = GameState->CameraP;
	sim_region* SimRegion = BeginSim(&TransientState->TransientArena, GameState, GameState->World, 
									 GameState->CameraP, SimBounds, Input->dtForFrame);

	// NOTE: This is the camera position relative to the origin
	v3 CameraPos = Subtract(World, &GameState->CameraP, &SimCenterPos);

	PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(ScreenBounds), V4(1, 1, 0, 1));
	PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(SimBounds).xy, V4(0, 1, 1, 1));
	PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(SimRegion->Bounds).xy, V4(1, 0, 1, 1));

	for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex)
	{
		sim_entity* Entity = SimRegion->Entities + EntityIndex;
		if (Entity->Updatable)
		{
			real32 dt = Input->dtForFrame;

			// TODO: This is incorrect, should be computed after update
			real32 ShadowAlpha = 1.0f - 0.5f * Entity->Pos.z;
			if (ShadowAlpha < 0)
			{
				ShadowAlpha = 0;
			}

			move_spec MoveSpec = DefaultMoveSpec();
			v3 ddPos = {};

			// TODO: Probably indicates we want to separate update and render
			// for entities sometime soon?
			v3 CameraRelativeGroundPos = GetEntityGroundPoint(Entity) - CameraPos;
			real32 FadeTopEndZ = 0.75f * GameState->TypicalFloorHeight;
			real32 FadeTopStartZ = 0.5f * GameState->TypicalFloorHeight;
			real32 FadeBottomStartZ = -2.0f * GameState->TypicalFloorHeight;
			real32 FadeBottomEndZ = -2.25f * GameState->TypicalFloorHeight;
			RenderGroup->GlobalAlpha = 1.0f;
			if (CameraRelativeGroundPos.z > FadeTopStartZ)
			{
				RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeTopEndZ, CameraRelativeGroundPos.z, FadeTopStartZ);
			}
			else if (CameraRelativeGroundPos.z < FadeBottomStartZ)
			{
				RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeBottomEndZ, CameraRelativeGroundPos.z, FadeBottomStartZ);
			}

			//
			// NOTE: Pre-physics entity work
			//
			switch (Entity->Type)
			{
				case entity_type::Hero:
				{
					// TODO: Now that we have some real usage examples, let's solidify
					// the positioning system

					for (uint32 ControlIndex = 0; ControlIndex < ArrayCount(GameState->ControlledHeroes); ++ControlIndex)
					{
						controlled_hero* ConHero = GameState->ControlledHeroes + ControlIndex;

						if (Entity->StorageIndex == ConHero->EntityIndex)
						{
							if (ConHero->dZ != 0.0f)
							{
								Entity->dPos.z = ConHero->dZ;
							}

							MoveSpec.UnitMaxAccelVector = true;
							MoveSpec.Speed = 45.0f;
							MoveSpec.Drag = 6.0f;
							MoveSpec.SpeedMult = ConHero->SpeedMultiplier;
							ddPos = ConHero->ddP;

							if ((ConHero->dSword.x != 0.0f) || (ConHero->dSword.y != 0.0f))
							{
								sim_entity* Sword = Entity->Sword.Ptr;
								if (Sword && HasFlag(Sword, (uint32)entity_flag::Nonspatial))
								{
									Sword->DistanceLimit = 5.0f;
									MakeEntitySpatial(Sword, Entity->Pos, 20.0f * ConHero->dSword);
									AddCollisionRule(GameState, Sword->StorageIndex, Entity->StorageIndex, false);
								}
							}
						}
					}
				} break;
				
				case entity_type::Sword:
				{
					MoveSpec.UnitMaxAccelVector = false;
					MoveSpec.Speed = 0.0f;
					MoveSpec.Drag = 0.0f;

					if (Entity->DistanceLimit == 0.0f)
					{
						ClearCollisionRulesFor(GameState, Entity->StorageIndex);
						MakeEntityNonSpatial(Entity);
					}
				} break;

				case entity_type::Familiar:
				{
					sim_entity* ClosestHero = 0;
					real32 ClosestHeroDSq = Square(17.0f); // NOTE: Ten meter maximum search

#if 0
					// TODO: Make spatial queries easy for things
					for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex)
					{
						sim_entity* TestEntity = SimRegion->Entities + EntityIndex;
						if (TestEntity->Type == entity_type::Hero)
						{
							real32 TestDSq = LengthSq(TestEntity->Pos - Entity->Pos);

							if (ClosestHeroDSq > TestDSq)
							{
								ClosestHero = TestEntity;
								ClosestHeroDSq = TestDSq;
							}
						}
					}
#endif

					if (ClosestHero && (ClosestHeroDSq > Square(1.5f)))
					{
						real32 Acceleration = 0.5f;
						real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
						ddPos = OneOverLength * (ClosestHero->Pos - Entity->Pos);
					}

					MoveSpec.UnitMaxAccelVector = true;
					MoveSpec.SpeedMult = 1.0f;
					MoveSpec.Speed = 45.0f;
					MoveSpec.Drag = 6.0;
				} break;

				case entity_type::Space:
				{
#if 0
					for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)
					{
						sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

						PushRectOutline(RenderGroup, Volume->OffsetPos - V3(0, 0, 0.5f * Volume->Dim.z), Volume->Dim.xy, V4(1, 0.5f, 1, 1));
					}
#endif
				} break;
			}

			if (!HasFlag(Entity, (uint32)entity_flag::Nonspatial) &&
				HasFlag(Entity, (uint32)entity_flag::Moveable))
			{
				MoveEntity(GameState, SimRegion, Entity, dt, &MoveSpec, ddPos);
			}

			RenderGroup->Transform.OffsetPos = GetEntityGroundPoint(Entity);

			//
			// NOTE: Post-physics entity work
			//
			switch (Entity->Type)
			{
				case entity_type::Hero:
				{
					real32 CharacterSizeC = 0.9f;
					hero_bitmaps* HeroBitmaps = &TransientState->Assets->Hero[Entity->FacingDirection];
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Shadow), V3(0, 0, 0), 0.25f, V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, &HeroBitmaps->Character, V3(0, 0, 0), CharacterSizeC * 1.2f);

					DrawHitpoints(Entity, RenderGroup);
				} break;

				case entity_type::Wall:
				{
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Tree), V3(0, 0, 0), 2.0f);
				} break;

				case entity_type::Stairs:
				{
					PushRect(RenderGroup, V3(0, 0, 0), Entity->WalkableDim, V4(1, 0.5f, 0, 1));
					PushRect(RenderGroup, V3(0, 0, Entity->WalkableHeight), Entity->WalkableDim, V4(1, 1, 0, 1));
				} break;

				case entity_type::Sword:
				{
					loaded_bitmap* Sword = &TransientState->Assets->Sword[Entity->FacingDirection];
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Shadow), V3(0, 0, 0), 0.25f, V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, Sword, V3(0, 0, 0), 0.5f);
				} break;

				case entity_type::Monstar:
				{
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Shadow), V3(0, 0, 0), 0.25f, V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Monstar), V3(0, 0, 0), 1.0f);

					DrawHitpoints(Entity, RenderGroup);
				} break;

				case entity_type::Familiar:
				{
					Entity->tBob += dt;
					if (Entity->tBob > 2.0f * Pi32)
					{
						Entity->tBob -= 2.0f * Pi32;
					}
					real32 BobSin = Sin(2.0f * Entity->tBob);

					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Shadow), V3(0, 0, 0), 0.25f, V4(1, 1, 1, 1.0f - (0.5f * ShadowAlpha + 0.2f * BobSin)));
					PushBitmap(RenderGroup, GetFirstBitmapID(TransientState->Assets, asset_type_id::Familiar), V3(0, 0, 0.25f * BobSin), 0.6f);
				} break;

				case entity_type::Space:
				{
	#if 0
					for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)
					{
						sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

						PushRectOutline(RenderGroup, Volume->OffsetPos - V3(0, 0, 0.5f * Volume->Dim.z), Volume->Dim.xy, V4(1, 0.5f, 1, 1));
					}
	#endif
				} break;

				default:
				{
					InvalidCodePath;
				} break;
			}
		}
	}

	RenderGroup->GlobalAlpha = 1.0f;

#if 0
	GameState->Time += Input->dtForFrame;

	v3 MapColor[] =
	{
		V3(1, 0, 0),
		V3(0, 1, 0),
		V3(0, 0, 1)
	};
	for (uint32 MapIndex = 0; MapIndex < ArrayCount(TransientState->EnvMaps); ++MapIndex)
	{
		environment_map* Map = TransientState->EnvMaps + MapIndex;
		loaded_bitmap* LOD = Map->LOD + 0;
		bool32 RowCheckerOn = false;
		int32 CheckerWidth = 16;
		int32 CheckerHeight = 16;
		for (int32 Y = 0; Y < LOD->Height; Y += CheckerHeight)
		{
			bool32 CheckerOn = RowCheckerOn;
			for (int32 X = 0; X < LOD->Width; X += CheckerWidth)
			{
				v4 Color = CheckerOn ? ToV4(MapColor[MapIndex], 1.0f) : V4(0, 0, 0, 1);
				v2 MinP = V2i(X, Y);
				v2 MaxP = MinP + V2i(CheckerWidth, CheckerHeight);
				DrawRectangle(LOD, MinP, MaxP, Color);
				CheckerOn = !CheckerOn;
			}
			RowCheckerOn = !RowCheckerOn;
		}
	}
	TransientState->EnvMaps[0].Pz = -1.5f;
	TransientState->EnvMaps[1].Pz = 0.0f;
	TransientState->EnvMaps[2].Pz = 1.5f;
	
	//Angle = 0;

	v2 Origin = ScreenCenter;

	real32 Angle = 0.1f * GameState->Time;
	if (Angle > 2.0f * Pi32)
	{
		Angle -= 2.0 * Pi32;
	}
#if 1
	v2 Disp = V2(100.0f * Cos(5.0f * Angle), 100.0f * Sin(3.0f * Angle));
#else
	v2 Disp = V2(0, 0);
#endif

#if 1
	v2 XAxis = 100.0f * V2(Cos(10.0f * Angle), Sin(10.0f * Angle));
	v2 YAxis = Perp(XAxis);
#else
	v2 XAxis = V2(100, 0);
	v2 YAxis = V2(0, 100);
#endif

	real32 CAngle = 5.0f * Angle;

#if 0
	v4 Color = V4(
		0.5f + 0.5f * Sin(CAngle),
		0.5f + 0.5f * Sin(2.9f * CAngle),
		0.5f + 0.5f * Cos(9.9f * CAngle),
		0.5f + 0.5f * Sin(10.0f * CAngle)
	);
#else
	v4 Color = V4(1, 1, 1, 1);
#endif

	render_entry_coordinate_system* C = CoordinateSystem(
		RenderGroup, 
		Disp + Origin - 0.5f * XAxis - 0.5f * YAxis, 
		XAxis, 
		YAxis, 
		Color, 
		&GameState->TestDiffuse,
		&GameState->TestNormal,
		TransientState->EnvMaps + 2, 
		TransientState->EnvMaps + 1, 
		TransientState->EnvMaps + 0
	);

	v2 MapP = V2(0.0f, 0.0f);
	for (uint32 MapIndex = 0; MapIndex < ArrayCount(TransientState->EnvMaps); ++MapIndex)
	{
		environment_map* Map = TransientState->EnvMaps + MapIndex;
		loaded_bitmap* LOD = Map->LOD + 0;

		XAxis = 0.5f * V2((real32)LOD->Width, 0.0f);
		YAxis = 0.5f * V2(0.0f, (real32)LOD->Height);

		CoordinateSystem(RenderGroup, MapP, XAxis, YAxis, V4(1.0f, 1.0f, 1.0f, 1.0f), LOD, 0, 0, 0, 0);
		MapP += YAxis + V2(0.0f, 6.0f);
	}
#endif

	TiledRenderGroupToOutput(TransientState->HighPriorityQueue, RenderGroup, DrawBuffer);

	// TODO: Make sure we hoist the camera update out to a place where the renderer
	// can know about the location of the camera at the end of cthe frame so there isn't
	// a frame of lag in camera updating compared to the hero.
	EndSim(SimRegion, GameState);
	EndTemporaryMemory(SimMemory);
	EndTemporaryMemory(RenderMemory);

	CheckArena(&GameState->WorldArena);
	CheckArena(&TransientState->TransientArena);

	END_TIMED_BLOCK(GameUpdateRender);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state* GameState = (game_state*)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 500);
}

/*
internal void
RenderWeirdGradient(game_offscreen_buffer* DrawBuffer, int BlueOffset, int GreenOffset)
{
	uint8* Row = (uint8*)DrawBuffer->Memory;
	for (int DimY = 0; DimY < DrawBuffer->Height; ++DimY)
	{
		uint32* Pixel = (uint32*)Row;
		for (int DimX = 0; DimX < DrawBuffer->Width; ++DimX)
		{
			uint8 Blue = (uint8)(DimX + BlueOffset);
			uint8 Green = (uint8)(DimY + GreenOffset);
			uint8 Red = 0;

			*Pixel++ = ((Red << 16) | (Green << 8) | Blue);
		}

		Row += DrawBuffer->Pitch;
	}
}
*/
