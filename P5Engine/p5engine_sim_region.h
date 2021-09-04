#pragma once

#ifndef P5ENGINE_SIMULATION_REGION_H
#define	P5ENGINE_SIMULATION_REGION_H

struct move_spec
{
	bool32 UnitMaxAccelVector;
	real32 Drag;
	real32 Speed;
	real32 SpeedMult;
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	// TODO: Bake this down into one variable 
	uint8 Flags;
	uint8 FilledAmount;
};

enum class entity_type
{
	Null,

	Hero,
	Wall,
	Familiar,
	Monstar,
	Sword,
	Stairs
};

// TODO: Rename sim_entity to entity
struct sim_entity;
union entity_reference
{
	sim_entity* Ptr;
	uint32 Index;
};

enum class entity_flag
{
	Collides = (1 << 1),
	Nonspatial = (1 << 2),

	Simming = (1 << 30),
};

struct sim_entity
{
	// NOTE: These are only for the sim region
	uint32 StorageIndex;
	bool32 Updatable;

	// 

	entity_type Type;
	uint32 Flags;

	v3 Pos;
	v3 dPos;

	real32 DistanceLimit;

	v3 Dim;

	uint32 FacingDirection;
	real32 tBob;

	int32 dAbsTileZ;

	// TODO: Should hitpoints themselves be entities?
	uint32 HitPointMax;
	hit_point HitPoint[16];

	entity_reference Sword;

	// TODO: Generation index so we know how "up to date" this entity is
};

struct sim_entity_hash
{
	sim_entity* Ptr;
	uint32 Index;
};

struct sim_region
{
	// TODO: Need a hash table here to map stored entity indices
	// to sim entities

	world* World;
	real32 MaxEntityRadius;
	real32 MaxEntityVelocity;

	world_position Origin;
	rectangle3 Bounds;
	rectangle3 UpdatableBounds;

	uint32 MaxEntityCount;
	uint32 EntityCount;
	sim_entity* Entities;

	// TODO: Do I really want a hash for this?
	// NOTE: Must be a power of two
	sim_entity_hash Hash[4096];
};

#endif // !P5ENGINE_SIMULATION_REGION_H
