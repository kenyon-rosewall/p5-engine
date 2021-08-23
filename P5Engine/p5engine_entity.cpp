
inline move_spec
DefaultMoveSpec(void)
{
	move_spec Result = {};

	Result.UnitMaxAccelVector = false;
	Result.Speed = 1.0f;
	Result.Drag = 0.0f;
	Result.SpeedMult = 1.0f;

	return(Result);
}

inline void
UpdateFamiliar(sim_region* SimRegion, sim_entity* Entity, real32 dt)
{
	sim_entity* ClosestHero = 0;
	real32 ClosestHeroDSq = Square(17.0f); // NOTE: Ten meter maximum search

	// TODO: Make spatial queries easy for things
	for (uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; ++EntityIndex)
	{
		sim_entity* TestEntity = SimRegion->Entities + EntityIndex;
		if (TestEntity->Type == entity_type::Hero)
		{
			real32 TestDSq = LengthSq(TestEntity->Pos - Entity->Pos);
			TestDSq *= 0.75f;
			
			if (ClosestHeroDSq > TestDSq)
			{
				ClosestHero = TestEntity;
				ClosestHeroDSq = TestDSq;
			}
		}
	}

	v2 ddP = {};
	if (ClosestHero && (ClosestHeroDSq > Square(1.5f)))
	{
		real32 Acceleration = 0.5f;
		real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
		ddP = OneOverLength * (ClosestHero->Pos - Entity->Pos);
	}

	move_spec Spec = DefaultMoveSpec();
	Spec.UnitMaxAccelVector = true;
	Spec.SpeedMult = 1.0f;
	Spec.Speed = 65.0f;
	Spec.Drag = 6.0;

	MoveEntity(SimRegion, Entity, dt, &Spec, ddP);

	Entity->tBob += dt;
	if (Entity->tBob > 2.0f * Pi32)
	{
		Entity->tBob -= 2.0f * Pi32;
	}
}

inline void
UpdateMonster(sim_region* SimRegion, sim_entity* Entity, real32 dt)
{

}

inline void
UpdateSword(sim_region* SimRegion, sim_entity* Entity, real32 dt)
{
	move_spec Spec = DefaultMoveSpec();
	Spec.UnitMaxAccelVector = false;
	Spec.Speed = 0.0f;
	Spec.Drag = 0.0f;

	v2 OldPos = Entity->Pos;

	MoveEntity(SimRegion, Entity, dt, &Spec, V2(0, 0));

	real32 DistanceTraveled = Length(Entity->Pos - OldPos);
	Entity->DistanceRemaining -= DistanceTraveled;
	if (Entity->DistanceRemaining < 0.0f)
	{
		Assert(!"NEED TO MAKE ENTITIES BE ABLE TO NOT BE THERE");
	}
}
