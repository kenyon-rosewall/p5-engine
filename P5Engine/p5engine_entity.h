#pragma once

#ifndef P5ENGINE_ENTITY_H
#define P5ENGINE_ENTITY_H

#define InvalidPos V3(100000.0f, 100000.0f, 100000.0f)

inline b32
HasFlag(sim_entity* Entity, u32 Flag)
{
	b32 Result = Entity->Flags & Flag;

	return(Result);
}

inline void
AddFlags(sim_entity* Entity, u32 Flag)
{
	Entity->Flags |= Flag;
}

inline void
ClearFlags(sim_entity* Entity, u32 Flag)
{
	Entity->Flags &= ~Flag;
}

inline void
MakeEntityNonSpatial(sim_entity* Entity)
{
	AddFlags(Entity, (u32)entity_flag::Nonspatial);
	Entity->Pos = InvalidPos;
}

inline void
MakeEntitySpatial(sim_entity* Entity, v3 Pos, v3 dPos)
{
	ClearFlags(Entity, (u32)entity_flag::Nonspatial);
	Entity->Pos = Pos;
	Entity->dPos = dPos;
}

inline v3
GetEntityGroundPoint(sim_entity* Entity, v3 ForEntityPos)
{
	v3 Result = ForEntityPos;

	return(Result);
}

inline v3
GetEntityGroundPoint(sim_entity* Entity)
{
	v3 Result = GetEntityGroundPoint(Entity, Entity->Pos);

	return(Result);
}

inline f32
GetStairGround(sim_entity* Entity, v3 AtGroundPoint)
{
	Assert(Entity->Type == entity_type::Stairs);

	rectangle2 RegionRect = RectCenterDim(Entity->Pos.xy, Entity->WalkableDim);
	v2 Bary = Clamp01(GetBarycentric(RegionRect, AtGroundPoint.xy));
	f32 Result = Entity->Pos.z + Bary.y * Entity->WalkableHeight;

	return(Result);
}

#endif // !P5ENGINE_ENTITY_H
