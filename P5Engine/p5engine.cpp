#include "p5engine.h"
#include "p5engine_render_group.h"
#include "p5engine_render_group.cpp"
#include "p5engine_world.cpp"
#include "p5engine_sim_region.cpp"
#include "p5engine_entity.cpp"
#include "p5engine_random.h"

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
DrawRectangleOutline(loaded_bitmap* Buffer, v2 vMin, v2 vMax, v3 Color, real32 r = 2.0f)
{
	// NOTE: Rop and boRRom
	DrawRectangle(Buffer, V2(vMin.x - r, vMin.y - r), V2(vMax.x + r, vMin.y + r), Color.r, Color.g, Color.b);
	DrawRectangle(Buffer, V2(vMin.x - r, vMax.y - r), V2(vMax.x + r, vMax.y + r), Color.r, Color.g, Color.b);

	// NOTE: Left and righte
	DrawRectangle(Buffer, V2(vMin.x - r, vMin.y - r), V2(vMin.x + r, vMax.y + r), Color.r, Color.g, Color.b);
	DrawRectangle(Buffer, V2(vMax.x - r, vMin.y - r), V2(vMax.x + r, vMax.y + r), Color.r, Color.g, Color.b);
}

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;
	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorzResolution;
	int32 VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;

	uint32 RedMask;
	uint32 GreenMask;
	uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context* Thread, debug_platform_read_entire_file* ReadEntireFile, char* Filename)
{
	loaded_bitmap Result = {};

	debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);
	if (ReadResult.ContentsSize)
	{
		bitmap_header* Header = (bitmap_header*)ReadResult.Contents;
		uint32* Memory = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
		Result.Memory = Memory;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		Assert(Header->Compression == 3);

		// NOTE: If you are using this generically for some reason,
		// please remember that BMP files CAN GO IN EITHER DIRECTION and 
		// the height will be negative for top-down.
		// (Also, there can be compression, etc, etc... Don't think this
		// is complete BMP loading code, because it isn't!!

		// NOTE: Byte order in memory is determined by the Header itself, 
		// so we have to read out the masks and convert the pixels ourselves.
		int32 RedMask = Header->RedMask;
		int32 GreenMask = Header->GreenMask;
		int32 BlueMask = Header->BlueMask;
		int32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		int32 AlphaShiftDown = AlphaScan.Index;
		int32 RedShiftDown = RedScan.Index;
		int32 GreenShiftDown = GreenScan.Index;
		int32 BlueShiftDown = BlueScan.Index;

		uint32* SourceDest = Memory;
		for (int32 y = 0; y < Header->Height; ++y)
		{
			for (int32 x = 0; x < Header->Width; ++x)
			{
				uint32 C = *SourceDest;

				v4 Texel = V4(
					(real32)((C & RedMask) >> RedShiftDown),
					(real32)((C & GreenMask) >> GreenShiftDown),
					(real32)((C & BlueMask) >> BlueShiftDown),
					(real32)((C & AlphaMask) >> AlphaShiftDown)
				);
				Texel = SRGB255ToLinear1(Texel);

#if 1
				Texel.rgb *= Texel.a;
#endif

				Texel = Linear1ToSRGB255(Texel);
			
				*SourceDest++ = (((uint32)(Texel.a + 0.5f) << 24) |
								 ((uint32)(Texel.r + 0.5f) << 16) |
								 ((uint32)(Texel.g + 0.5f) <<  8) |
								 ((uint32)(Texel.b + 0.5f) <<  0));
			}
		}
	}

	Result.Pitch = -Result.Width * BITMAP_BYTES_PER_PIXEL;
	Result.Memory = (uint32*)((uint8*)Result.Memory - Result.Pitch * (Result.Height - 1));

	return(Result);
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
	AddFlags(&Entity.Low->Sim, entity_flag::Traversable);

	return(Entity);
}

internal add_low_entity_result
AddWall(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Wall, Pos, GameState->WallCollision);

	AddFlags(&Entity.Low->Sim, entity_flag::Collides);

	return(Entity);
}

internal add_low_entity_result
AddStairs(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Stairs, Pos, GameState->StairsCollision);

	AddFlags(&Entity.Low->Sim, entity_flag::Collides);
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
	AddFlags(&Entity.Low->Sim, entity_flag::Moveable);

	return(Entity);
}

internal add_low_entity_result
AddPlayer(game_state* GameState)
{
	world_position Pos = GameState->CameraP;
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Hero, Pos, GameState->HeroCollision);

	AddFlags(&Entity.Low->Sim, entity_flag::Collides | entity_flag::Moveable);

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

	AddFlags(&Entity.Low->Sim, entity_flag::Collides | entity_flag::Moveable);

	return(Entity);
}

internal add_low_entity_result
AddFamiliar(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(GameState, entity_type::Familiar, Pos, GameState->FamiliarCollision);

	AddFlags(&Entity.Low->Sim, entity_flag::Collides | entity_flag::Moveable);

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

			PushRect(RenderGroup, HitPos, 0, HealthDim, Color, 0.0f);
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

internal void
FillGroundChunk(transient_state* TransientState, game_state* GameState, ground_buffer* GroundBuffer, world_position* ChunkPos)
{
	temporary_memory Memory = BeginTemporaryMemory(&TransientState->TransientArena);
	render_group* RenderGroup = AllocateRenderGroup(&TransientState->TransientArena, Megabytes(4), 1);

	Clear(RenderGroup, V4(1, 1, 0, 1));

	loaded_bitmap* Buffer = &GroundBuffer->Bitmap;
	GroundBuffer->Pos = *ChunkPos;

	real32 Width = (real32)Buffer->Width;
	real32 Height = (real32)Buffer->Height;

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

			v2 Center = V2(ChunkOffsetX * Width, -ChunkOffsetY * Height);

			for (uint32 SoilIndex = 0; SoilIndex < 50; ++SoilIndex)
			{
				loaded_bitmap* Stamp = GameState->Soil + 2;
				v2 BitmapCenter = 0.5f * V2i(Stamp->Width, Stamp->Height);
				v2 Offset = V2(
					Width * RandomUnilateral(&Series),
					Height * RandomUnilateral(&Series)
				);
				v2 Pos = Center + Offset - BitmapCenter;

				PushBitmap(RenderGroup, Stamp, Pos, 0, V2(0, 0));
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

				v2 Center = V2(ChunkOffsetX * Width, -ChunkOffsetY * Height);

				loaded_bitmap* Stamp = {};
				switch (RandomChoice(&Series, 20))
				{
					case 1:
					{
						Stamp = &GameState->Grass;
					} break;

					case 2:
					{
						Stamp = GameState->Soil;
					} break;

					default:
					{
						Stamp = GameState->Tuft + RandomChoice(&Series, ArrayCount(GameState->Tuft));
					} break;
				}

				if (Stamp)
				{
					v2 BitmapCenter = 0.5f * V2i(Stamp->Width, Stamp->Height);
					v2 Offset = V2(
						Width * RandomUnilateral(&Series),
						Height * RandomUnilateral(&Series)
					);
					v2 Pos = Center + Offset - BitmapCenter;

					PushBitmap(RenderGroup, Stamp, Pos, 0, V2(0, 0));
				}
			}
		}
	}

	RenderGroupToOutput(RenderGroup, Buffer);
	EndTemporaryMemory(Memory);
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
	Result.Memory = (uint32*)PushSize(Arena, TotalBitmapSize);
	if (ClearToZero)
	{
		ClearBitmap(&Result);
	}

	return(Result);
}

internal void
MakeSphereNormalMap(loaded_bitmap* Bitmap, real32 Roughness)
{
	real32 InvWidth = 1.0f / (1.0f - (real32)Bitmap->Width);
	real32 InvHeight = 1.0f / (1.0f - (real32)Bitmap->Height);

	uint8* Row = (uint8*)Bitmap->Memory;
	for (int32 Y = 0; Y < Bitmap->Height; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int32 X = 0; X < Bitmap->Width; ++X)
		{
			v2 BitmapUV = V2(InvWidth * (real32)X, InvHeight * (real32)Y);

			v3 Normal = V3(2.0f * BitmapUV.x - 1.0f, 2.0f * BitmapUV.y - 1.0f, 0.0f);
			Normal.z = SquareRoot(1.0f - Minimum(1.0f, Square(Normal.x) + Square(Normal.y)));

			Normal = Normalize(Normal);

			v4 Color = V4(
				255.0f * (0.5f * (Normal.x + 1.0f)), 
				255.0f * (0.5f * (Normal.y + 1.0f)), 
				127.0f * Normal.z,
				255.0f * Roughness
			);

			*Pixel = (((uint32)(Color.a + 0.5f) << 24) |
					  ((uint32)(Color.r + 0.5f) << 16) |
					  ((uint32)(Color.g + 0.5f) <<  8) |
					  ((uint32)(Color.b + 0.5f) <<  0));
		}

		Row += Bitmap->Pitch;
	}
}

#if 0
internal void
RequestGroundBuffers(world_position CenterPos, rectangle3 Bounds)
{
	Bounds = Offset(Bounds, CenterPos.Offset);
	CenterPos.Offset = V3(0, 0, 0);

	for (uint32 ZIndex = Bounds.Min.z; ZIndex < Bounds.Max.z; ++ZIndex)
	{

	}

	FillGroundChunk(TransientState, GameState, TrasientState->GroundBuffers, &GameState->CameraP);
}
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));

	uint32 GroundBufferWidth = 256;
	uint32 GroundBufferHeight = 256;

	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;

		GameState->TypicalFloorHeight = 3.0f;
		GameState->MetersToPixels = 43.0f;
		GameState->PixelsToMeters = 1.0f / GameState->MetersToPixels;

		v3 WorldChunkDimInMeters = V3(
			GameState->PixelsToMeters * (real32)GroundBufferWidth,
			GameState->PixelsToMeters * GroundBufferHeight,
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

		GameState->Grass = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/grass1.bmp");

		GameState->Soil[0] = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/soil1.bmp");
		GameState->Soil[1] = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/soil2.bmp");
		GameState->Soil[2] = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/soil3.bmp");

		GameState->Tuft[0] = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/tuft1.bmp");
		GameState->Tuft[1] = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/tuft2.bmp");

		GameState->Backdrop = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/bg.bmp");
		GameState->Shadow = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/shadow.bmp");
		GameState->Stairs = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/stairs.bmp");
		GameState->Tree = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/tree.bmp");
		GameState->Monstar = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/enemy.bmp");
		GameState->Familiar = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/orb.bmp");

		hero_bitmaps* HeroBitmap;

		HeroBitmap = GameState->Hero;
		HeroBitmap->Character = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/char-right-0.bmp");
		HeroBitmap->Align = V2(32, 56);
		++HeroBitmap;
		HeroBitmap->Character = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/char-back-0.bmp");
		HeroBitmap->Align = V2(32, 56);
		++HeroBitmap;
		HeroBitmap->Character = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/char-left-0.bmp");
		HeroBitmap->Align = V2(32, 56);
		++HeroBitmap;
		HeroBitmap->Character = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/char-front-0.bmp");
		HeroBitmap->Align = V2(32, 56);
		++HeroBitmap;

		loaded_bitmap* SwordBitmap;

		SwordBitmap = GameState->Sword;
		*SwordBitmap = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/sword-right.bmp");
		++SwordBitmap;
		*SwordBitmap = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/sword-back.bmp");
		++SwordBitmap;
		*SwordBitmap = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/sword-left.bmp");
		++SwordBitmap;
		*SwordBitmap = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/sword-front.bmp");
		++SwordBitmap;

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
			// uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 3);
			uint32 DoorDirection = RandomChoice(&Series, 2);

			bool32 CreatedZDoor = false;
			if (DoorDirection == 2)
			{
				CreatedZDoor = true;
				if (AbsTileZ == ScreenBaseZ)
				{
					DoorUp = true;
				}
				else
				{
					DoorDown = true;
				}
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
						if ((TileX == 10) && (TileY == 5))
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

			if (DoorDirection == 2)
			{
				if (AbsTileZ == ScreenBaseZ)
				{
					AbsTileZ = ScreenBaseZ + 1;
				}
				else
				{
					AbsTileZ = ScreenBaseZ;
				}
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

		Memory->IsInitialized = true;
	}

	Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
	transient_state* TransientState = (transient_state*)Memory->TransientStorage;
	if (!TransientState->IsInitialized)
	{
		InitializeArena(&TransientState->TransientArena, Memory->TransientStorageSize - sizeof(transient_state), (uint8*)Memory->TransientStorage + sizeof(transient_state));

		TransientState->GroundBufferCount = 64;
		TransientState->GroundBuffers = PushArray(&TransientState->TransientArena, TransientState->GroundBufferCount, ground_buffer);
		for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
		{
			ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
			GroundBuffer->Bitmap = MakeEmptyBitmap(&TransientState->TransientArena, GroundBufferWidth, GroundBufferHeight, false);
			GroundBuffer->Pos = NullPosition();
		}

		TransientState->IsInitialized = true;
	}

	if (Input->ExecutableReloaded)
	{
		for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
		{
			ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
			GroundBuffer->Pos = NullPosition();
		}
	}

	world* World = GameState->World;
	real32 MetersToPixels = GameState->MetersToPixels;
	real32 PixelsToMeters = GameState->PixelsToMeters;

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

			if (Controller->ActionDown.EndedDown)
			{
				ConHero->dZ = 3.0f;
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

	//
	// NOTE: Render
	//

	temporary_memory RenderMemory = BeginTemporaryMemory(&TransientState->TransientArena);
	// TODO: Decide what our push buffer size is
	render_group* RenderGroup = AllocateRenderGroup(&TransientState->TransientArena, Megabytes(4), GameState->MetersToPixels);

	loaded_bitmap DrawBuffer_ = {};
	loaded_bitmap* DrawBuffer = &DrawBuffer_;
	DrawBuffer->Width = Buffer->Width;
	DrawBuffer->Height = Buffer->Height;
	DrawBuffer->Pitch = Buffer->Pitch;
	DrawBuffer->Memory = (uint32*)Buffer->Memory;

	Clear(RenderGroup, V4(0.5f, 0.5f, 0.5f, 0));

	v2 ScreenCenter = V2(
		0.5f * (real32)DrawBuffer->Width,
		0.5f * (real32)DrawBuffer->Height
	);

	real32 ScreenWidthInMeters = DrawBuffer->Width * PixelsToMeters;
	real32 ScreenHeightInMeters = DrawBuffer->Height * PixelsToMeters;
	rectangle3 CameraBoundsInMeters = RectCenterDim(
		V3(0, 0, 0),
		V3(ScreenWidthInMeters, ScreenHeightInMeters, 0.0f)
	);

	for (uint32 GroundBufferIndex = 0; GroundBufferIndex < TransientState->GroundBufferCount; ++GroundBufferIndex)
	{
		ground_buffer* GroundBuffer = TransientState->GroundBuffers + GroundBufferIndex;
		if (IsValid(GroundBuffer->Pos))
		{
			loaded_bitmap* Bitmap = &GroundBuffer->Bitmap;

			v3 Delta = Subtract(GameState->World, &GroundBuffer->Pos, &GameState->CameraP);

			PushBitmap(RenderGroup, Bitmap, Delta.xy, Delta.z, 0.5f * V2i(Bitmap->Width, Bitmap->Height));
		}
	}

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
				v2 ScreenPos = V2(
					ScreenCenter.x + MetersToPixels * RelPos.x,
					ScreenCenter.y - MetersToPixels * RelPos.y
				);
				v2 ScreenDim = MetersToPixels * World->ChunkDimInMeters.xy;

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

				PushRectOutline(RenderGroup, RelPos.xy, RelPos.z, World->ChunkDimInMeters.xy, V4(1, 1, 0, 1));
			}
		}
	}

	// TODO: Howbig do we actually want to expand here?
	v3 SimBoundsExpansion = V3(15.0f, 15.0f, 15.0f);
	rectangle3 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
	temporary_memory SimMemory = BeginTemporaryMemory(&TransientState->TransientArena);
	sim_region* SimRegion = BeginSim(&TransientState->TransientArena, GameState, GameState->World, 
									 GameState->CameraP, SimBounds, Input->dtForFrame);

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

			render_basis* Basis = PushStruct(&TransientState->TransientArena, render_basis);
			RenderGroup->DefaultBasis = Basis;

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
								if (Sword && HasFlag(Sword, entity_flag::Nonspatial))
								{
									Sword->DistanceLimit = 5.0f;
									MakeEntitySpatial(Sword, Entity->Pos, 20.0f * ConHero->dSword);
									AddCollisionRule(GameState, Sword->StorageIndex, Entity->StorageIndex, false);
								}
							}
						}
					}

					hero_bitmaps* HeroBitmaps = &GameState->Hero[Entity->FacingDirection];
					PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(RenderGroup, &HeroBitmaps->Character, V2(0, 0), 0, HeroBitmaps->Align);

					DrawHitpoints(Entity, RenderGroup);
				} break;

				case entity_type::Wall:
				{
					PushBitmap(RenderGroup, &GameState->Tree, V2(0, 0), 0, V2(32, 64));
				} break;

				case entity_type::Stairs:
				{
					// PushBitmap(RenderGroup, &GameState->Stairs, V2(0, 0), 0, V2(32, 36));
					PushRect(RenderGroup, V2(0, 0), 0, Entity->WalkableDim, V4(1, 0.5f, 0, 1), 0.0f);
					PushRect(RenderGroup, V2(0, 0), Entity->WalkableHeight, Entity->WalkableDim, V4(1, 1, 0, 1), 0.0f);
				} break;

				case entity_type::Sword:
				{
					MoveSpec.UnitMaxAccelVector = false;
					MoveSpec.Speed = 0.0f;
					MoveSpec.Drag = 0.0f;

					ddPos = V3(0, 0, 0);

					// TODO: Add the ability in the collision routines to understand a movement
					// limit for an entity, and then update this routine to use that to know when 
					// to kill the sword
					// TODO: Need to handle the fact that DistanceTraveled
					// might not have enough distance for the total entity move
					// for the frame
					if (Entity->DistanceLimit == 0.0f)
					{
						ClearCollisionRulesFor(GameState, Entity->StorageIndex);
						MakeEntityNonSpatial(Entity);
					}

					loaded_bitmap* Sword = &GameState->Sword[Entity->FacingDirection];
					// PushBitmap(&RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(RenderGroup, Sword, V2(0, 0), 0, V2(32, 10));
				} break;

				case entity_type::Monstar:
				{
					PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(RenderGroup, &GameState->Monstar, V2(0, 0), 0, V2(32, 64));

					DrawHitpoints(Entity, RenderGroup);
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

					Entity->tBob += dt;
					if (Entity->tBob > 2.0f * Pi32)
					{
						Entity->tBob -= 2.0f * Pi32;
					}

					real32 BobSin = Sin(2.0f * Entity->tBob);

					PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), (0.5f * ShadowAlpha + 0.2f * BobSin), 0.0f);
					PushBitmap(RenderGroup, &GameState->Familiar, V2(0, 0), 0.25f * BobSin, V2(32, 75));
				} break;

				case entity_type::Space:
				{
					for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)
					{
						sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

						PushRectOutline(RenderGroup, Volume->OffsetPos.xy, 0, Volume->Dim.xy, V4(1, 0.5f, 1, 1), 0.0f);
					}
				} break;

				default:
				{
					InvalidCodePath;
				} break;
			}

			if (!HasFlag(Entity, entity_flag::Nonspatial) &&
				HasFlag(Entity, entity_flag::Moveable))
			{
				MoveEntity(GameState, SimRegion, Entity, dt, &MoveSpec, ddPos);
			}

			Basis->Pos = GetEntityGroundPoint(Entity);
		}
	}

	GameState->Time += Input->dtForFrame;
	real32 Angle = 0.1f * GameState->Time;
	if (Angle > 2.0f * Pi32)
	{
		Angle -= 2.0 * Pi32;
	}
	real32 Disp = 100.0f * Cos(5.0f * Angle);
	Angle = 0;

	v2 Origin = ScreenCenter;

#if 1
	v2 XAxis = 100.0f * V2(Cos(Angle), Sin(Angle));
	v2 YAxis = Perp(XAxis);
#else
	v2 XAxis = V2(300, 0);
	v2 YAxis = V2(0, 300);
#endif

	uint32 PointIndex = 0;
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
		/*V2(Disp, 0) + */Origin - 0.5f * XAxis - 0.5f * YAxis, 
		XAxis, 
		YAxis, 
		Color, 
		&GameState->Tree,
		0, 0, 0, 0
	);

	RenderGroupToOutput(RenderGroup, DrawBuffer);

	EndSim(SimRegion, GameState);
	EndTemporaryMemory(SimMemory);
	EndTemporaryMemory(RenderMemory);

	CheckArena(&GameState->WorldArena);
	CheckArena(&TransientState->TransientArena);
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
