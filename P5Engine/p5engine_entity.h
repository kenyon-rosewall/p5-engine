#pragma once

#ifndef P5ENGINE_ENTITY_H
#define P5ENGINE_ENTITY_H

#define InvalidPos V3(100000.0f, 100000.0f, 100000.0f)

inline bool32
HasFlag(sim_entity* Entity, uint32 Flag)
{
	bool32 Result = Entity->Flags & Flag;

	return(Result);
}

inline void
AddFlags(sim_entity* Entity, uint32 Flag)
{
	Entity->Flags |= Flag;
}

inline void
ClearFlags(sim_entity* Entity, uint32 Flag)
{
	Entity->Flags &= ~Flag;
}

inline void
MakeEntityNonSpatial(sim_entity* Entity)
{
	AddFlags(Entity, entity_flag::Nonspatial);
	Entity->Pos = InvalidPos;
}

inline void
MakeEntitySpatial(sim_entity* Entity, v3 Pos, v3 dPos)
{
	ClearFlags(Entity, entity_flag::Nonspatial);
	Entity->Pos = Pos;
	Entity->dPos = dPos;
}

inline v3
GetEntityGroundPoint(sim_entity* Entity)
{
	v3 Result = Entity->Pos + V3(0, 0, -0.5f * Entity->Dim.Z);

	return(Result);
}

inline real32
GetStairGround(sim_entity* Entity, v3 AtGroundPoint)
{
	Assert(Entity->Type == entity_type::Stairs);

	rectangle3 RegionRect = RectCenterDim(Entity->Pos, Entity->Dim);
	v3 Bary = Clamp01(GetBarycentric(RegionRect, AtGroundPoint));
	real32 Result = RegionRect.Min.Z + Bary.Y * Entity->WalkableHeight;

	return(Result);
}

#endif // !P5ENGINE_ENTITY_H
