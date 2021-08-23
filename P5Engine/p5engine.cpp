#include "p5engine.h"
#include "p5engine_world.cpp"
#include "p5engine_simulation_region.cpp"
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
DrawRectangle(game_offscreen_buffer* Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B)
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

	uint8* Row = ((uint8*)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);

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
DrawBitmap(game_offscreen_buffer* Buffer, loaded_bitmap* Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
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

	uint32* SourceRow = Bitmap->Pixels + Bitmap->Width * (Bitmap->Height - 1);
	SourceRow += -Bitmap->Width * SourceOffsetY + SourceOffsetX;
	uint8* DestRow = ((uint8*)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);
	for (int32 Y = MinY; Y < MaxY; ++Y)
	{
		uint32* Dest = (uint32*)DestRow;
		uint32* Source = SourceRow;
		for (int32 X = MinX; X < MaxX; ++X)
		{
			real32  A = (real32)((*Source >> 24) & 0xFF) / 255.0f;
			A *= CAlpha;
			real32 SR = (real32)((*Source >> 16) & 0xFF);
			real32 SG = (real32)((*Source >> 8) & 0xFF);
			real32 SB = (real32)((*Source >> 0) & 0xFF);

			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0) & 0xFF);

			real32 R = (1.0f - A) * DR + A * SR;
			real32 G = (1.0f - A) * DG + A * SG;
			real32 B = (1.0f - A) * DB + A * SB;

			*Dest = (((uint32)(R + 0.5f) << 16) | ((uint32)(G + 0.5f) << 8) | ((uint32)(B + 0.5f) << 0));

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
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
		uint32* Pixels = (uint32*)((uint8*)ReadResult.Contents + Header->BitmapOffset);
		Result.Pixels = Pixels;
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

		uint32* SourceDest = Pixels;
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

	return(Result);
}

struct add_low_entity_result
{
	low_entity* Low;
	uint32 LowIndex;
};

internal add_low_entity_result
AddLowEntity(game_state* GameState, entity_type Type, world_position* Pos)
{
	Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
	uint32 EntityIndex = GameState->LowEntityCount++;

	low_entity* EntityLow = GameState->LowEntities + EntityIndex;
	*EntityLow = {};
	EntityLow->Sim.Type = Type;

	ChangeEntityLocation(&GameState->WorldArena, GameState->World, EntityIndex, EntityLow, 0, Pos);

	add_low_entity_result Result = {};
	Result.Low = EntityLow;
	Result.LowIndex = EntityIndex;

	// TODO: Do we need to have a begin/end paradigm for adding
	// entities so that they can be brought into the high set when they
	// are added and are in the camera region?

	return(Result);
}

internal add_low_entity_result
AddWall(world* World, game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Wall, &Pos);
	
	Entity.Low->Sim.Height = GameState->World->TileSideInMeters;
	Entity.Low->Sim.Width = Entity.Low->Sim.Height;
	Entity.Low->Sim.Collides = true;

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
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Sword, 0);

	Entity.Low->Sim.Height = 0.0f;
	Entity.Low->Sim.Width = 0.0f;
	Entity.Low->Sim.Collides = false;

	return(Entity);
}

internal add_low_entity_result
AddPlayer(game_state* GameState)
{
	world_position Pos = GameState->CameraP;
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Hero, &Pos);

	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = true;

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
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Monstar, &Pos);

	InitHitPoints(Entity.Low, 3);

	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = true;

	return(Entity);
}

internal add_low_entity_result
AddFamiliar(game_state* GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position Pos = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddLowEntity(GameState, entity_type::Familiar, &Pos);

	Entity.Low->Sim.Height = 0.5f;
	Entity.Low->Sim.Width = 1.0f;
	Entity.Low->Sim.Collides = false;

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
	Piece->OffsetZ = Group->GameState->MetersToPixels * OffsetZ;
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		// NOTE: Reserve entity slot 0 for the null entity
		AddLowEntity(GameState, entity_type::Null, 0);

		GameState->Backdrop = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/bg.bmp");
		GameState->Shadow = DEBUGLoadBMP(Context, Memory->DEBUGPlatformReadEntireFile, (char*)"P5Engine/data/shadow.bmp");
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


		InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world* World = GameState->World;
		InitializeWorld(World, 1.4f);

		int32 TileSideInPixels = 60;
		GameState->MetersToPixels = (real32)TileSideInPixels / (real32)World->TileSideInMeters;

		uint32 RandomNumberIndex = 0;
		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;
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
			// TODO: Random number generator
			Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
			uint32 RandomChoice;
			if (1 == 1) // (DoorUp || DoorDown)
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
			}

			bool32 CreatedZDoor = false;
			if (RandomChoice == 2)
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
			else if (RandomChoice == 1)
			{
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}

			for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY)
			{
				for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX)
				{
					uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

					uint32 TileValue = 1;
					if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight / 2))))
					{
						TileValue = 2;
					}

					if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight / 2))))
					{
						TileValue = 2;
					}

					if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth / 2))))
					{
						TileValue = 2;
					}

					if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth / 2))))
					{
						TileValue = 2;
					}

					if ((TileX == 10) && (TileY == 6))
					{
						if (DoorUp)
						{
							TileValue = 3;
						}

						if (DoorDown)
						{
							TileValue = 4;
						}
					}

					if (TileValue == 2)
					{
						AddWall(World, GameState, AbsTileX, AbsTileY, AbsTileZ);
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

			if (RandomChoice == 2)
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
			else if (RandomChoice == 1)
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
		AddMonstar(GameState, CameraTileX + 2, CameraTileY + 2, CameraTileZ);
		AddFamiliar(GameState, CameraTileX - 2, CameraTileY + 2, CameraTileZ);

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
				ConHero->SpeedMultiplier = 1.0f;
			}
		}
		else
		{
			ConHero->ddP = {};

			if (Controller->IsAnalogL)
			{
				// NOTE: Use analog movement tuning
				ConHero->ddP = V2(Controller->StickAverageLX, Controller->StickAverageLY);
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
				ConHero->dZ = 7.0f;
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
						ConHero->dSword = V2(1.0f, 0.0f);
					} break;

					case 1:
					{
						ConHero->dSword = V2(0.0f, 1.0f);
					} break;

					case 2:
					{
						ConHero->dSword = V2(-1.0f, 0.0f);
					} break;

					case 3:
					{
						ConHero->dSword = V2(0.0f, -1.0f);
					} break;
				}
			}
		}
	}

	// TODO: I am totally picking these numbers readonly
	uint32 TileSpanX = 17 * 3;
	uint32 TileSpanY = 9 * 3;
	rectangle2 CameraBounds = RectCenterDim(V2(0, 0), World->TileSideInMeters * V2((real32)TileSpanX, (real32)TileSpanY));

	memory_arena SimArena;
	InitializeArena(&SimArena, Memory->TransientStorageSize, Memory->TransientStorage);
	sim_region* SimRegion = BeginSim(&SimArena, GameState, GameState->World, GameState->CameraP, CameraBounds);

	//
	// NOTE: Render
	//
#if 1
	DrawRectangle(Buffer, V2(0, 0), V2((real32)Buffer->Width, (real32)Buffer->Height), 0.5f, 0.5f, 0.5f);
#else
	DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);
#endif

	real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;

	entity_visible_piece_group PieceGroup = {};
	PieceGroup.GameState = GameState;
	sim_entity* Entity = SimRegion->Entities;
	for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex, ++Entity)
	{
		PieceGroup.PieceCount = 0;
		real32 dt = Input->dtForFrame;

		// TODO: This is incorrect, should be computed after update
		real32 ShadowAlpha = 1.0f - 0.5f * Entity->Z;
		if (ShadowAlpha < 0)
		{
			ShadowAlpha = 0;
		}

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
						Entity->dZ = ConHero->dZ;
						move_spec Spec = DefaultMoveSpec();
						Spec.UnitMaxAccelVector = true;
						Spec.Speed = 65.0f;
						Spec.Drag = 6.0f;
						Spec.SpeedMult = ConHero->SpeedMultiplier;
						
						MoveEntity(SimRegion, Entity, dt, &Spec, ConHero->ddP);

						if ((ConHero->dSword.X != 0.0f) || (ConHero->dSword.Y != 0.0f))
						{
							sim_entity* Sword = Entity->Sword.Ptr;
							if (Sword)
							{
								Sword->Pos = Entity->Pos;
								Sword->DistanceRemaining = 5.0f;
								Sword->FacingDirection = Entity->FacingDirection;
								Sword->dPos = 20.0f * ConHero->dSword;
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

			case entity_type::Sword:
			{
				UpdateSword(SimRegion, Entity, dt);

				loaded_bitmap* Sword = &GameState->Sword[Entity->FacingDirection];
				// PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
				PushBitmap(&PieceGroup, Sword, V2(0, 0), 0, V2(32, 10));
			} break;

			case entity_type::Monstar:
			{
				UpdateMonster(SimRegion, Entity, dt);
				
				PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), ShadowAlpha, 0.0f);
				PushBitmap(&PieceGroup, &GameState->Monstar, V2(0, 0), 0, V2(32, 64));

				DrawHitpoints(Entity, &PieceGroup);
			} break;

			case entity_type::Familiar:
			{
				UpdateFamiliar(SimRegion, Entity, dt);
				
				real32 BobSin = Sin(2.0f * Entity->tBob);
				
				PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, V2(24, 0), (0.5f * ShadowAlpha + 0.2f * BobSin), 0.0f);
				PushBitmap(&PieceGroup, &GameState->Familiar, V2(0, 0), 0.25f * BobSin, V2(32, 75));
			} break;

			default:
			{
				InvalidCodePath;
			} break;
		}

		real32 ddZ = -30.0f;
		Entity->Z = 0.5f * ddZ * Square(dt) + Entity->dZ * dt + Entity->Z;
		Entity->dZ = ddZ * dt + Entity->dZ;
		if (Entity->Z < 0)
		{
			Entity->Z = 0;
		}

		real32 EntityGroundPointX = ScreenCenterX + MetersToPixels * Entity->Pos.X;
		real32 EntityGroundPointY = ScreenCenterY - MetersToPixels * Entity->Pos.Y;
		real32 EntityZ = -MetersToPixels * Entity->Z;

#if 0
		v2 EntityLeftTop = V2(EntityGroundPointX - 0.5f * MetersToPixels * EntityLow->Width, 
							  EntityGroundPointY - 0.5f * MetersToPixels * EntityLow->Height);
		v2 EntityWidthHeight = V2(EntityLow->Width, 
								  EntityLow->Height);
		DrawRectangle(Buffer, EntityLeftTop, EntityLeftTop + MetersToPixels * EntityWidthHeight, 0.0f, 1.0f, 1.0f);
#endif

		for (uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex)
		{
			entity_visible_piece* Piece = PieceGroup.Pieces + PieceIndex;
			v2 Center = V2(EntityGroundPointX + Piece->Offset.X,
						   EntityGroundPointY + Piece->Offset.Y + Piece->OffsetZ + Piece->EntityZC * EntityZ);

			if (Piece->Bitmap)
			{
				DrawBitmap(Buffer, Piece->Bitmap, Center.X, Center.Y, Piece->A);
			}
			else
			{
				v2 HalfDim = 0.5f * MetersToPixels * Piece->Dim;
				v2 Min = Center - HalfDim;
				v2 Max = Center + HalfDim;
				DrawRectangle(Buffer, Min, Max, Piece->R, Piece->G, Piece->B);
			}
		}
	}

	EndSim(SimRegion, GameState);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state* GameState = (game_state*)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 500);
}

/*
internal void
RenderWeirdGradient(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset)
{
	uint8* Row = (uint8*)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			uint8 Blue = (uint8)(X + BlueOffset);
			uint8 Green = (uint8)(Y + GreenOffset);
			uint8 Red = 0;

			*Pixel++ = ((Red << 16) | (Green << 8) | Blue);
		}

		Row += Buffer->Pitch;
	}
}
*/
