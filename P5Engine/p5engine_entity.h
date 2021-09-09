#pragma once

#ifndef P5ENGINE_ENTITY_H
#define P5ENGINE_ENTITY_H

#define InvalidPos V3(100000.0f, 100000.0f, 100000.0f)

inline bool32
HasFlag(sim_entity* Entity, entity_flag Flag)
{
	bool32 Result = Entity->Flags & (uint32)Flag;

	return(Result);
}

inline void
AddFlag(sim_entity* Entity, entity_flag Flag)
{
	Entity->Flags |= (uint32)Flag;
}

inline void
ClearFlag(sim_entity* Entity, entity_flag Flag)
{
	Entity->Flags &= ~(uint32)Flag;
}

inline void
MakeEntityNonSpatial(sim_entity* Entity)
{
	AddFlag(Entity, entity_flag::Nonspatial);
	Entity->Pos = InvalidPos;
}

inline void
MakeEntitySpatial(sim_entity* Entity, v3 Pos, v3 dPos)
{
	ClearFlag(Entity, entity_flag::Nonspatial);
	Entity->Pos = Pos;
	Entity->dPos = dPos;
}

#endif // !P5ENGINE_ENTITY_H
