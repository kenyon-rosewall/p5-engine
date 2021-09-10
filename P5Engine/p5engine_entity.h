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

#endif // !P5ENGINE_ENTITY_H
