#pragma once

#ifndef P5ENGINE_SIMULATION_REGION_H
#define	P5ENGINE_SIMULATION_REGION_H

struct move_spec
{
	b32 UnitMaxAccelVector;
	f32 Drag;
	f32 Speed;
	f32 SpeedMult;
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	// TODO: Bake this down into one variable 
	u08 Flags;
	u08 FilledAmount;
};

enum class entity_type
{
	Null,

	Space,

	Hero,
	Wall,
	Familiar,
	Monstar,
	Sword,
	Stairs,
};

// TODO: Rename sim_entity to entity
struct sim_entity;
union entity_reference
{
	sim_entity* Ptr;
	u32 Index;
};

enum class entity_flag
{
	// TODO: Does it make more sense to have the flag be for _non_ colliding entities?
	// TODO: Collides and Zupported probably can be removed
	Collides = (1 << 0),
	Nonspatial = (1 << 1),
	Moveable = (1 << 2),
	ZSupported = (1 << 3),
	Traversable = (1 << 4),

	Simming = (1 << 30),
};

struct sim_entity_collision_volume
{
	v3 OffsetPos;
	v3 Dim;
};

struct sim_entity_collision_volume_group
{
	sim_entity_collision_volume TotalVolume;

	// TODO: VolumeCount is alwmays expected to be greater than 0 if the entity
	// has any volume... in the future, this could be compressed if necessary to say
	// that the VolumeCount can be 0 if the TotalVolume should be used as the only
	// collision volume for the entity.
	u32 VolumeCount;
	sim_entity_collision_volume* Volumes;
};

struct sim_entity
{
	// NOTE: These are only for the sim region
	u32 StorageIndex;
	b32 Updatable;

	// 

	entity_type Type;
	u32 Flags;

	v3 Pos;
	v3 dPos;

	f32 DistanceLimit;

	sim_entity_collision_volume_group* Collision;

	f32 FacingDirection;
	f32 tBob;

	s32 dAbsTileZ;

	// TODO: Should hitpoints themselves be entities?
	u32 HitPointMax;
	hit_point HitPoint[16];

	entity_reference Sword;

	// TODO: Only for stairs
	v2 WalkableDim;
	f32 WalkableHeight;

	// TODO: Generation index so we know how "up to date" this entity is
};

struct sim_entity_hash
{
	sim_entity* Ptr;
	u32 Index;
};

struct sim_region
{
	// TODO: Need a hash table here to map stored entity indices
	// to sim entities

	world* World;
	f32 MaxEntityRadius;
	f32 MaxEntityVelocity;

	world_position Origin;
	rectangle3 Bounds;
	rectangle3 UpdatableBounds;

	u32 MaxEntityCount;
	u32 EntityCount;
	sim_entity* Entities;

	f32 GroundZBase;

	// TODO: Do I really want a hash for this?
	// NOTE: Must be a power of two
	sim_entity_hash Hash[4096];
};

#endif // !P5ENGINE_SIMULATION_REGION_H
