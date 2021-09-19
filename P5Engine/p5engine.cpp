#include "p5engine.h"
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
DrawRectangle(loaded_bitmap* Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B)
{
	int32 MinX = RoundReal32ToInt32(vMin.X);
	int32 MinY = RoundReal32ToInt32(vMin.Y);
	int32 MaxX = RoundReal32ToInt32(vMax.X);
	int32 MaxY = RoundReal32ToInt32(vMax.Y);

	if (MinX < 0)
	{
		MinX = 0;
	}

	if (MinY < 0)
	{
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) | (RoundReal32ToUInt32(G * 255.0f) << 8) | (RoundReal32ToUInt32(B * 255.0f) << 0));

	uint8* Row = ((uint8*)Buffer->Memory + MinX * BITMAP_BYTES_PER_PIXEL + MinY * Buffer->Pitch);

	for (int Y = MinY; Y < MaxY; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int X = MinX; X < MaxX; ++X)
		{
			*Pixel++ = Color;
		}

		Row += Buffer->Pitch;
	}
}

internal void
DrawBitmap(loaded_bitmap* Buffer, loaded_bitmap* Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
{
	int32 MinX = RoundReal32ToInt32(RealX);
	int32 MinY = RoundReal32ToInt32(RealY);
	int32 MaxX = MinX + Bitmap->Width;
	int32 MaxY = MinY + Bitmap->Height;

	int32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	int32 SourceOffsetY = 0;
	if (MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	int32 BytesPerPixel = BITMAP_BYTES_PER_PIXEL;
	uint8* SourceRow = (uint8*)Bitmap->Memory + Bitmap->Pitch* SourceOffsetY + BytesPerPixel * SourceOffsetX;
	uint8* DestRow = ((uint8*)Buffer->Memory + MinX * BytesPerPixel + MinY * Buffer->Pitch);
	for (int32 Y = MinY; Y < MaxY; ++Y)
	{
		uint32* Dest = (uint32*)DestRow;
		uint32* Source = (uint32*)SourceRow;
		for (int32 X = MinX; X < MaxX; ++X)
		{
			real32  SA = (real32)((*Source >> 24) & 0xFF) / 255.0f;
			SA *= CAlpha;
			real32 SR = (real32)((*Source >> 16) & 0xFF);
			real32 SG = (real32)((*Source >> 8) & 0xFF);
			real32 SB = (real32)((*Source >> 0) & 0xFF);

			real32 DA = (real32)((*Dest >> 24) & 0xFF);
			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0) & 0xFF);

			real32 A = Maximum(DA, 255.0f * SA);
			real32 R = (1.0f - SA) * DR + SA * SR;
			real32 G = (1.0f - SA) * DG + SA * SG;
			real32 B = (1.0f - SA) * DB + SA * SB;

			*Dest = (((uint32)(A + 0.5f) << 24) | ((uint32)(R + 0.5f) << 16) | ((uint32)(G + 0.5f) << 8) | ((uint32)(B + 0.5f) << 0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	}
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

		int32 RedShift = 16 - RedScan.Index;
		int32 GreenShift = 8 - GreenScan.Index;
		int32 BlueShift = 0 - BlueScan.Index;
		int32 AlphaShift = 24 - AlphaScan.Index;

		uint32* SourceDest = Memory;
		for (int32 Y = 0; Y < Header->Height; ++Y)
		{
			for (int32 X = 0; X < Header->Width; ++X)
			{
				uint32 C = *SourceDest;

				*SourceDest++ = (RotateLeft(C & RedMask, RedShift) |
					RotateLeft(C & GreenMask, GreenShift) |
					RotateLeft(C & BlueMask, BlueShift) |
					RotateLeft(C & AlphaMask, AlphaShift));
			}
		}
	}

	int32 BytesPerPixel = BITMAP_BYTES_PER_PIXEL;
	Result.Pitch = -Result.Width * BytesPerPixel;
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
	Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.XY;
	Entity.Low->Sim.WalkableHeight = GameState->World->TileDepthInMeters;

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

inline void
PushPiece(entity_visible_piece_group* Group, loaded_bitmap* Bitmap, v2 Offset, v2 Align,
	real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC)
{
	Assert(Group->PieceCount < ArrayCount(Group->Pieces));

	entity_visible_piece* Piece = Group->Pieces + Group->PieceCount++;
	Piece->Bitmap = Bitmap;
	Piece->Offset = Group->GameState->MetersToPixels * V2(Offset.X, -Offset.Y) - Align;
	Piece->OffsetZ = OffsetZ;
	Piece->Dim = Dim;
	Piece->R = Color.R;
	Piece->G = Color.G;
	Piece->B = Color.B;
	Piece->A = Color.A;
	Piece->EntityZC = EntityZC;
}

inline void
PushBitmap(entity_visible_piece_group* Group, loaded_bitmap* Bitmap, v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f, real32 EntityZC = 1.0f)
{
	v2 Dim = V2((real32)Bitmap->Width, (real32)Bitmap->Height);
	v4 Color = V4(1.0f, 1.0f, 1.0f, Alpha);
	PushPiece(Group, Bitmap, Offset, Align, OffsetZ, Dim, Color, EntityZC);
}

inline void
PushRect(entity_visible_piece_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	PushPiece(Group, 0, Offset, V2(0, 0), OffsetZ, Dim, Color, EntityZC);
}

inline void
PushRectOutline(entity_visible_piece_group* Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
	real32 Thickness = 0.1f;

	// NOTE: Top and bottom
	PushPiece(Group, 0, Offset - V2(0, 0.5f * Dim.Y), V2(0, 0), OffsetZ, V2(Dim.X, Thickness), Color, EntityZC);
	PushPiece(Group, 0, Offset + V2(0, 0.5f * Dim.Y), V2(0, 0), OffsetZ, V2(Dim.X, Thickness), Color, EntityZC);

	// NOTE: Left and right
	PushPiece(Group, 0, Offset - V2(0.5f * Dim.X, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.Y), Color, EntityZC);
	PushPiece(Group, 0, Offset + V2(0.5f * Dim.X, 0), V2(0, 0), OffsetZ, V2(Thickness, Dim.Y), Color, EntityZC);
}

internal void
DrawHitpoints(sim_entity* Entity, entity_visible_piece_group* PieceGroup)
{
	if (Entity->HitPointMax >= 1)
	{
		v2 HealthDim = V2(0.2f, 0.2f);
		real32 SpacingX = 1.5f * HealthDim.X;
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

			PushRect(PieceGroup, HitPos, 0, HealthDim, Color, 0.0f);
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
DrawTestGround(game_state* GameState, loaded_bitmap* Buffer)
{
	// TODO: Make random number generation more systemic
	random_series Series = Seed(1234);

	v2 Center = 0.5f * V2i(Buffer->Width, Buffer->Height);
	for (uint32 SoilIndex = 0; SoilIndex < 100; ++SoilIndex)
	{
		uint32 AssetChoice = RandomChoice(&Series, 3);
		loaded_bitmap* Stamp = {};
		switch (AssetChoice)
		{
			case 0:
			{
				Stamp = GameState->Tuft + RandomChoice(&Series, ArrayCount(GameState->Tuft));
			} break;

			case 1: 
			{
				Stamp = GameState->Soil + RandomChoice(&Series, ArrayCount(GameState->Soil));
			} break;

			case 2:
			{
				Stamp = &GameState->Grass;
			} break;
		}

		if (Stamp)
		{
			real32 Radius = 5.0f;
			v2 BitmapCenter = 0.5f * V2i(Stamp->Width, Stamp->Height);
			v2 Offset = V2(
				RandomBilateral(&Series),
				RandomBilateral(&Series)
			);
			v2 Pos = Center + GameState->MetersToPixels * Offset * Radius - BitmapCenter;

			DrawBitmap(Buffer, Stamp, Pos.X, Pos.Y);
		}
	}
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena* Arena, int32 Width, int32 Height)
{
	loaded_bitmap Result = {};

	Result.Width = Width;
	Result.Height = Height;
	Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
	int32 TotalBitmapSize = Width * Height * BITMAP_BYTES_PER_PIXEL;
	Result.Memory = (uint32*)PushSize_(Arena, TotalBitmapSize);

	return(Result);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;

		// TODO: Talk about this soon. Let's start partitioning our memory space.
		InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));

		// NOTE: Reserve entity slot 0 for the null entity
		AddLowEntity(GameState, entity_type::Null, NullPosition());

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world* World = GameState->World;
		InitializeWorld(World, 1.4f, 3.0f);
		int32 TileSideInPixels = 60;
		GameState->MetersToPixels = (real32)TileSideInPixels / (real32)World->TileSideInMeters;

		GameState->NullCollision = MakeNullCollision(GameState);
		GameState->SwordCollision = MakeSimpleGroundedCollision(
			GameState, 1.0f, 0.5f, 0.1f
		);
		GameState->StairsCollision = MakeSimpleGroundedCollision(
			GameState, GameState->World->TileSideInMeters, 
			2.0f * GameState->World->TileSideInMeters, 1.1f * GameState->World->TileDepthInMeters
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
			GameState, GameState->World->TileSideInMeters, 
			GameState->World->TileSideInMeters, GameState->World->TileDepthInMeters
		);
		GameState->StandardRoomCollision = MakeSimpleGroundedCollision(
			GameState, TilesPerWidth * GameState->World->TileSideInMeters,
			TilesPerHeight * GameState->World->TileSideInMeters, 0.9f * GameState->World->TileDepthInMeters
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

		random_series Series = { 0 };

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
			uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 3);

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

		GameState->GroundBuffer = MakeEmptyBitmap(&GameState->WorldArena, 512, 512);
		DrawTestGround(GameState, &GameState->GroundBuffer);

		Memory->IsInitialized = true;
	}

	world* World = GameState->World;
	real32 MetersToPixels = GameState->MetersToPixels;

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
					ConHero->ddP.Y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown)
				{
					ConHero->ddP.Y = -1.0f;
				}
				if (Controller->MoveLeft.EndedDown)
				{
					ConHero->ddP.X = -1.0f;
				}
				if (Controller->MoveRight.EndedDown)
				{
					ConHero->ddP.X = 1.0f;
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

	// TODO: I am totally picking these numbers readonly
	uint32 TileSpanX = 17 * 3;
	uint32 TileSpanY = 9 * 3;
	uint32 TileSpanZ = 1;
	rectangle3 CameraBounds = RectCenterDim(V3(0, 0, 0), 
											World->TileSideInMeters * V3((real32)TileSpanX, 
																		 (real32)TileSpanY, 
																		 (real32)TileSpanZ));

	memory_arena SimArena;
	InitializeArena(&SimArena, Memory->TransientStorageSize, Memory->TransientStorage);
	sim_region* SimRegion = BeginSim(&SimArena, GameState, GameState->World, GameState->CameraP, CameraBounds, Input->dtForFrame);

	//
	// NOTE: Render
	//
	loaded_bitmap DrawBuffer_ = {};
	loaded_bitmap* DrawBuffer = &DrawBuffer_;
	DrawBuffer->Width = Buffer->Width;
	DrawBuffer->Height = Buffer->Height;
	DrawBuffer->Pitch = Buffer->Pitch;
	DrawBuffer->Memory = (uint32*)Buffer->Memory;

	DrawRectangle(DrawBuffer, V2(0, 0), V2((real32)DrawBuffer->Width, (real32)DrawBuffer->Height), 0.5f, 0.5f, 0.5f);
	// TODO: Draw this at center
	DrawBitmap(DrawBuffer, &GameState->GroundBuffer, 0, 0);

	real32 ScreenCenterX = 0.5f * (real32)DrawBuffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)DrawBuffer->Height;

	// TODO: Move this out into p5engien_entity.cpp
	entity_visible_piece_group PieceGroup = {};
	PieceGroup.GameState = GameState;
	sim_entity* Entity = SimRegion->Entities;
	for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex, ++Entity)
	{
		if (Entity->Updatable)
		{
			PieceGroup.PieceCount = 0;
			real32 dt = Input->dtForFrame;

			// TODO: This is incorrect, should be computed after update
			real32 ShadowAlpha = 1.0f - 0.5f * Entity->Pos.Z;
			if (ShadowAlpha < 0)
			{
				ShadowAlpha = 0;
			}

			move_spec MoveSpec = DefaultMoveSpec();
			v3 ddPos = {};

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
								Entity->dPos.Z = ConHero->dZ;
							}

							MoveSpec.UnitMaxAccelVector = true;
							MoveSpec.Speed = 45.0f;
							MoveSpec.Drag = 6.0f;
							MoveSpec.SpeedMult = ConHero->SpeedMultiplier;
							ddPos = ConHero->ddP;

							if ((ConHero->dSword.X != 0.0f) || (ConHero->dSword.Y != 0.0f))
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
					PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(&PieceGroup, &HeroBitmaps->Character, V2(0, 0), 0, HeroBitmaps->Align);

					DrawHitpoints(Entity, &PieceGroup);
				} break;

				case entity_type::Wall:
				{
					PushBitmap(&PieceGroup, &GameState->Tree, V2(0, 0), 0, V2(32, 64));
				} break;

				case entity_type::Stairs:
				{
					// PushBitmap(&PieceGroup, &GameState->Stairs, V2(0, 0), 0, V2(32, 36));
					PushRect(&PieceGroup, V2(0, 0), 0, Entity->WalkableDim, V4(1, 0.5f, 0, 1), 0.0f);
					PushRect(&PieceGroup, V2(0, 0), Entity->WalkableHeight, Entity->WalkableDim, V4(1, 1, 0, 1), 0.0f);
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
					// PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(&PieceGroup, Sword, V2(0, 0), 0, V2(32, 10));
				} break;

				case entity_type::Monstar:
				{
					PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
					PushBitmap(&PieceGroup, &GameState->Monstar, V2(0, 0), 0, V2(32, 64));

					DrawHitpoints(Entity, &PieceGroup);
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

					PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), (0.5f * ShadowAlpha + 0.2f * BobSin), 0.0f);
					PushBitmap(&PieceGroup, &GameState->Familiar, V2(0, 0), 0.25f * BobSin, V2(32, 75));
				} break;

				case entity_type::Space:
				{
#if 0
					for (uint32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)
					{
						sim_entity_collision_volume* Volume = Entity->Collision->Volumes + VolumeIndex;

						PushRectOutline(&PieceGroup, Volume->OffsetPos.XY, 0, Volume->Dim.XY, V4(1, 0.5f, 1, 1), 0.0f);
					}
#endif
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

			for (uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex)
			{
				entity_visible_piece* Piece = PieceGroup.Pieces + PieceIndex;
				
				v3 EntityBasePos = GetEntityGroundPoint(Entity);
				real32 ZFudge = (1.0f + 0.1f * (EntityBasePos.Z + Piece->OffsetZ));

				real32 EntityGroundPointX = ScreenCenterX + MetersToPixels * ZFudge * EntityBasePos.X;
				real32 EntityGroundPointY = ScreenCenterY - MetersToPixels * ZFudge * EntityBasePos.Y;
				real32 EntityZ = -MetersToPixels * EntityBasePos.Z;

				v2 Center = V2(
					EntityGroundPointX + Piece->Offset.X,
					EntityGroundPointY + Piece->Offset.Y + Piece->EntityZC * EntityZ
				);

				if (Piece->Bitmap)
				{
					DrawBitmap(DrawBuffer, Piece->Bitmap, Center.X, Center.Y, Piece->A);
				}
				else
				{
					v2 HalfDim = 0.5f * MetersToPixels * Piece->Dim;
					v2 Min = Center - HalfDim;
					v2 Max = Center + HalfDim;
					DrawRectangle(DrawBuffer, Min, Max, Piece->R, Piece->G, Piece->B);
				}
			}
		}
	}

	world_position WorldOrigin = {};
	v3 Diff = Subtract(SimRegion->World, &WorldOrigin, &SimRegion->Origin);
	DrawRectangle(DrawBuffer, Diff.XY, V2(10.0f, 10.0f), 1.0, 1.0f, 1.0f);

	EndSim(SimRegion, GameState);
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
