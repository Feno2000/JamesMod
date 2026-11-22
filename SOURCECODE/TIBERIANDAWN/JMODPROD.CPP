/*
** JMODPROD.CPP -- JamesMod: multiple factories speed up production.
**
** The sidebar only has one build queue per category, and that UI lives in the closed
** game client, so true parallel queues aren't possible from the DLL. Instead (as in
** Red Alert) each extra factory of the right type speeds up that queue:
**
**     time = time * 100 / (100 + BonusPercent * (factories - 1))
**
** With the default 50%: 2 factories = 1.5x speed, 3 = 2x, 4 = 2.5x (capped by MaxFactories).
** Applies to every house, player and AI alike. Settings: [Production] in JAMESMOD.INI.
*/
#include "function.h"
#include "JMOD.H"

static int Count_Owned(HouseClass const * house, StructType a, StructType b)
{
	int count = 0;
	for (int i = 0; i < Buildings.Count(); i++) {
		BuildingClass * bld = Buildings.Ptr(i);
		if (bld == NULL || bld->IsInLimbo || bld->House != house || bld->Strength <= 0) continue;
		StructType t = bld->Class->Type;
		if (t == a || (b != STRUCT_NONE && t == b)) count++;
	}
	return count;
}

int JMod_Factory_Time(int time, RTTIType rtti, HouseClass const * house)
{
	if (!JMod.ProdMultiFactory || house == NULL || JMod.ProdBonusPercent <= 0) return time;

	int factories = 0;
	switch (rtti) {
		case RTTI_UNITTYPE:		factories = Count_Owned(house, STRUCT_WEAP, STRUCT_AIRSTRIP); break;
		case RTTI_INFANTRYTYPE:	factories = Count_Owned(house, STRUCT_BARRACKS, STRUCT_HAND); break;
		case RTTI_AIRCRAFTTYPE:	factories = Count_Owned(house, STRUCT_HELIPAD, STRUCT_NONE); break;
		case RTTI_BUILDINGTYPE:	factories = JMod.ProdIncludeConYards ? Count_Owned(house, STRUCT_CONST, STRUCT_NONE) : 1; break;
		default:				return time;
	}
	if (factories > JMod.ProdMaxFactories) factories = JMod.ProdMaxFactories;
	if (factories <= 1) return time;

	return (int)(((long)time * 100) / (100 + JMod.ProdBonusPercent * (factories - 1)));
}
