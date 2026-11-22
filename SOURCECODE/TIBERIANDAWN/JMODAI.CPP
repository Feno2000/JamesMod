/*
** JMODAI.CPP -- JamesMod: wave-based attack controller for computer players.
**
** Replaces the stock "every few minutes, send ~75% of armed units off to hunt one by one"
** (HouseClass::AI_Attack) with organised waves:
**
**   GATHER   idle combat units move to a rally point between our base and the enemy.
**            A few units nearest the base are kept back as a home guard.
**   LAUNCH   once enough units are at the rally point, they become a wave. The size
**            needed grows with each wave and with each failed wave.
**   ADVANCE  the wave moves together to a staging point just outside the enemy base.
**   ASSAULT  when most of the wave has arrived (or after a timeout) it attacks (hunt).
**   RETREAT  if the wave drops below RetreatPercent of its starting strength, the
**            survivors pull back to the rally point and the next wave is made bigger.
**
** Only skirmish (GAME_GLYPHX_MULTIPLAYER) computer houses for now. Unit membership is
** kept in side tables indexed by heap slot, so object layouts and saves are unchanged.
** Settings come from [WaveAI] in JAMESMOD.INI.
*/
#include "function.h"
#include "JMOD.H"

#define JM_MAX_SLOTS	1024
#define JM_CELL			256			// leptons per cell

enum WavePhase { WAVE_NONE, WAVE_ADVANCE, WAVE_ASSAULT };

struct HouseWaveState {
	bool		Started;
	WavePhase	Phase;
	int		WaveId;				// id of the current/last launched wave (1..)
	int		Launched;			// waves launched so far
	int		Failures;			// waves that retreated or were wiped out
	long		NextThink;
	long		PhaseStart;
	long		LastLaunch;
	long		GatherStart;
	long		StartStrength;
	long		LastStatus;
	int		BestAvgDist;		// closest average distance to staging so far this advance
	long		LastProgress;		// frame that average last improved
	CELL		Rally;
	CELL		Staging;
	HousesType	Target;
};

struct SlotTag {
	signed char House;			// HOUSE_NONE (-1) = untagged
	short		Wave;
};

static HouseWaveState State[HOUSE_COUNT];
static SlotTag UnitTag[JM_MAX_SLOTS];
static SlotTag InfTag[JM_MAX_SLOTS];
static long LastFrameSeen = -1;

/*
** Frame restarts at zero for every new game: wipe all state when that happens.
*/
void JMod_Wave_Reset(void)
{
	LastFrameSeen = -1;
}

static void Check_New_Game(void)
{
	if (LastFrameSeen < 0 || Frame < LastFrameSeen) {
		memset(State, 0, sizeof(State));
		for (int i = 0; i < JM_MAX_SLOTS; i++) {
			UnitTag[i].House = -1; UnitTag[i].Wave = 0;
			InfTag[i].House = -1;  InfTag[i].Wave = 0;
		}
	}
	LastFrameSeen = Frame;
}

bool JMod_Wave_Controls(HouseClass const * house)
{
	if (!JMod.WaveAI || house == NULL || house->IsHuman) return false;
	if (GameToPlay != GAME_GLYPHX_MULTIPLAYER) return false;		// campaign support comes later
	HousesType h = house->Class->House;
	if (h < HOUSE_MULTI1 || h > HOUSE_MULTI6 || !house->IsActive || house->IsDefeated) return false;	// skirmish players only (not Neutral/Special)
	return true;
}

/*
** A unit the wave controller may command: armed ground combat unit, not part of a
** scripted team, not a harvester/MCV.
*/
static bool Is_Combat(FootClass * f, HouseClass * house)
{
	if (f == NULL || f->IsInLimbo || f->Strength <= 0 || f->House != house) return false;
	if (f->Team != NULL) return false;
	if (!f->Is_Weapon_Equipped()) return false;
	if (f->What_Am_I() == RTTI_UNIT) {
		UnitClass * u = (UnitClass *)f;
		if (u->Class->IsToHarvest || *u == UNIT_MCV || *u == UNIT_GUNBOAT) return false;
	}
	return true;
}

static int Unit_Value(FootClass * f)
{
	int v = f->Techno_Type_Class()->Cost_Of();
	return (v > 0) ? v : 100;
}

static bool Is_Idle(FootClass * f)
{
	return f->Mission == MISSION_GUARD || f->Mission == MISSION_GUARD_AREA || f->Mission == MISSION_NONE ||
		(f->Mission == MISSION_MOVE && !Target_Legal(f->NavCom));
}

static void Order_Move(FootClass * f, CELL cell)
{
	f->Assign_Target(TARGET_NONE);
	f->Assign_Destination(::As_Target(cell));
	f->Assign_Mission(MISSION_MOVE);
}

static HousesType Pick_Enemy(HouseClass * house)
{
	if (house->Enemy != HOUSE_NONE) {
		HouseClass * h = HouseClass::As_Pointer(house->Enemy);
		if (h && h->IsActive && !h->IsDefeated && !house->Is_Ally(h) && h->Center) return house->Enemy;
	}
	HousesType best = HOUSE_NONE;
	int bestdist = 0x7FFFFFFF;
	for (HousesType hh = HOUSE_FIRST; hh < HOUSE_COUNT; hh++) {
		HouseClass * h = HouseClass::As_Pointer(hh);
		if (h == NULL || h == house || !h->IsActive || h->IsDefeated || house->Is_Ally(h) || !h->Center) continue;
		if (hh == HOUSE_NEUTRAL || hh == HOUSE_JP) continue;
		int d = ::Distance(house->Center, h->Center);
		if (d < bestdist) { bestdist = d; best = hh; }
	}
	return best;
}

static CELL Point_Between(COORDINATE from, COORDINATE toward, int cells)
{
	int total = ::Distance(from, toward);
	int lep = cells * JM_CELL;
	if (lep > total * 3 / 4) lep = total * 3 / 4;		// never overshoot on small maps
	COORDINATE c = Coord_Move(from, ::Direction(from, toward), (unsigned short)lep);
	return Map.Nearby_Location(Coord_Cell(c));
}

/*
** Reachability. Tiberian Dawn has no "can a unit get there" query, so flood-fill the map from
** the rally point over terrain a tracked vehicle can cross (ignoring units and buildings).
** The staging point is then the reachable cell closest to the ideal spot outside the enemy
** base, instead of a straight-line point that may be on top of or behind a cliff.
*/
static unsigned char Reach[MAP_CELL_TOTAL];
static CELL FloodQueue[MAP_CELL_TOTAL];

static bool Crossable(CELL cell)
{
	if (!Map.In_Radar(cell)) return false;
	CellClass & c = Map[cell];
	if (Ground[c.Land_Type()].Cost[SPEED_TRACK] == 0) return false;
	if (c.Cell_Terrain() != NULL) return false;
	return true;
}

static void Flood_From(CELL start)
{
	memset(Reach, 0, sizeof(Reach));
	int head = 0, tail = 0;
	Reach[start] = 1;
	FloodQueue[tail++] = start;
	while (head < tail) {
		CELL cell = FloodQueue[head++];
		for (FacingType f = FACING_N; f < FACING_COUNT; f++) {
			CELL n = Adjacent_Cell(cell, f);
			if ((unsigned)n >= MAP_CELL_TOTAL || Reach[n]) continue;
			if (abs(Cell_X(n) - Cell_X(cell)) > 1) continue;		// wrapped across the map edge
			if (!Crossable(n)) continue;
			Reach[n] = 1;
			FloodQueue[tail++] = n;
		}
	}
}

static CELL Reachable_Staging(CELL rally, COORDINATE ideal)
{
	Flood_From(rally);
	CELL best = 0;
	int bestdist = 0x7FFFFFFF;
	for (CELL cell = 0; cell < MAP_CELL_TOTAL; cell++) {
		if (!Reach[cell]) continue;
		int d = ::Distance(Cell_Coord(cell), ideal);
		if (d < bestdist) { bestdist = d; best = cell; }
	}
	return best ? best : Coord_Cell(ideal);
}

/*
** Iterate this house's combat units (vehicles then infantry). The callback receives the
** object and a pointer to its tag slot.
*/
template <class FN>
static void For_Each_Combat(HouseClass * house, FN fn)
{
	for (int i = 0; i < Units.Count(); i++) {
		UnitClass * u = Units.Ptr(i);
		if (!Is_Combat(u, house)) continue;
		int id = Units.ID(u);
		if (id < 0 || id >= JM_MAX_SLOTS) continue;
		fn((FootClass *)u, &UnitTag[id]);
	}
	for (int i = 0; i < Infantry.Count(); i++) {
		InfantryClass * n = Infantry.Ptr(i);
		if (!Is_Combat(n, house)) continue;
		int id = Infantry.ID(n);
		if (id < 0 || id >= JM_MAX_SLOTS) continue;
		fn((FootClass *)n, &InfTag[id]);
	}
}


/*
** Factory watchdog + diagnostics. A computer construction yard that finishes a building it
** can't place holds it forever, which freezes all further base building (and, with no new
** power plants, slows every factory). After StuckSeconds we abandon (refund) it.
*/
static long CompletedSince[JM_MAX_SLOTS];

static void Factory_Watchdog(HouseClass * house, bool log_status)
{
	for (int i = 0; i < Buildings.Count(); i++) {
		BuildingClass * b = Buildings.Ptr(i);
		if (b == NULL || b->IsInLimbo || b->House != house || b->Factory == NULL) continue;
		int id = Buildings.ID(b);
		if (id < 0 || id >= JM_MAX_SLOTS) continue;

		FactoryClass * f = b->Factory;
		bool done = f->Has_Completed();
		TechnoClass * obj = f->Get_Object();
		if (log_status) {
			JMod_Trace("   factory %s: making %s, %d%%, %s%s", b->Class->IniName, obj ? obj->Class_Of().IniName : "?",
				f->Completion() * 100 / 108, f->Is_Building() ? "building" : "paused", done ? ", COMPLETED (waiting to place/exit)" : "");
		}
		if (!done) { CompletedSince[id] = 0; continue; }
		if (CompletedSince[id] == 0 || CompletedSince[id] > Frame) CompletedSince[id] = Frame;

		if (*b == STRUCT_CONST && JMod.AIStuckSeconds > 0 && Frame - CompletedSince[id] > (long)JMod.AIStuckSeconds * TICKS_PER_SECOND) {
			JMod_Trace("AI %s: construction yard could not place %s for %ds - abandoning it so building can continue",
				house->Class->IniName, obj ? obj->Class_Of().IniName : "?", JMod.AIStuckSeconds);
			f->Abandon();
			delete b->Factory;
			b->Factory = NULL;
			CompletedSince[id] = 0;
		}
	}
}


/*
** Fallback building placement. The stock search (Find_Build_Location -> Find_Cell_In_Zone)
** only considers cells inside its fixed zones around the base centre, so open ground just
** outside them is never tried and the AI thinks it has no room. This scans every cell within
** PlacementRadius of the base for a legal, adjacent spot. Combat buildings prefer the side
** facing the enemy; everything else prefers to stay close to the base.
*/
COORDINATE JMod_Find_Build_Location(HouseClass const * house, BuildingClass * building)
{
	static int traces = 0;
	if (house == NULL || building == NULL || !house->Center || JMod.AIPlaceRadius <= 0) return 0;
	TechnoTypeClass const * ttype = building->Techno_Type_Class();
	short const * list = building->Occupy_List(true);

	bool defensive = building->Anti_Air() || building->Anti_Armor() || building->Anti_Infantry();
	COORDINATE aim = house->Center;
	if (defensive && house->Enemy != HOUSE_NONE) {
		HouseClass * e = HouseClass::As_Pointer(house->Enemy);
		if (e && e->Center) aim = Coord_Move(house->Center, ::Direction(house->Center, e->Center), 5 * JM_CELL);
	}

	CELL centre = Coord_Cell(house->Center);
	int cx = Cell_X(centre), cy = Cell_Y(centre), r = JMod.AIPlaceRadius;
	CELL best = 0;
	int bestdist = 0x7FFFFFFF;
	for (int y = cy - r; y <= cy + r; y++) {
		for (int x = cx - r; x <= cx + r; x++) {
			if (x < 0 || y < 0 || x >= MAP_CELL_W || y >= MAP_CELL_H) continue;
			CELL cell = XY_Cell(x, y);
			if (!Map.In_Radar(cell)) continue;
			if (!ttype->Legal_Placement(cell)) continue;
			if (list && !Map.Passes_Proximity_Check(ttype, house->Class->House, list, cell)) continue;
			int d = ::Distance(Cell_Coord(cell), aim);
			if (d < bestdist) { bestdist = d; best = cell; }
		}
	}
	if (traces < 40) {
		traces++;
		if (best) JMod_Trace("AI %s: stock placement failed for %s - fallback found cell %d,%d", house->Class->IniName, ttype->IniName, Cell_X(best), Cell_Y(best));
		else JMod_Trace("AI %s: no legal spot within %d cells for %s", house->Class->IniName, r, ttype->IniName);
	}
	return best ? Cell_Coord(best) : 0;
}


/*
** How many defensive buildings the AI should have: max(MinDefences, buildings / BuildingsPerDefence),
** capped by Rule.DefenseLimit. Used both to guarantee a minimum and to stop base growth
** spending everything on towers.
*/
int Defence_Target(HouseClass * house)
{
	int want = JMod.AIMinDefences;
	if (JMod.AIBuildingsPerDefence > 0) {
		int scaled = (int)house->CurBuildings / JMod.AIBuildingsPerDefence;
		if (scaled > want) want = scaled;
	}
	if (want > Rule.DefenseLimit) want = Rule.DefenseLimit;
	return want;
}

int Defence_Count(HouseClass * house)
{
	return house->BQuantity[STRUCT_TURRET] + house->BQuantity[STRUCT_OBELISK] + house->BQuantity[STRUCT_GTOWER] + house->BQuantity[STRUCT_ATOWER];
}

/*
** Next defensive building to build, alternating heavy (advanced tower / Obelisk) and light
** (guard tower / turret). STRUCT_NONE if none can be built or afforded.
*/
static StructType Pick_Defence(HouseClass * house, long money, int have)
{
	static StructType const heavy[] = {STRUCT_OBELISK, STRUCT_ATOWER};
	static StructType const light[] = {STRUCT_TURRET, STRUCT_GTOWER};
	StructType const * first = (have & 1) ? light : heavy;
	StructType const * second = (have & 1) ? heavy : light;
	for (int pass = 0; pass < 2; pass++) {
		StructType const * list = pass ? second : first;
		for (int k = 0; k < 2; k++) {
			BuildingTypeClass const & b = BuildingTypeClass::As_Reference(list[k]);
			if (house->Can_Build(&b, house->ActLike) && b.Cost_Of() < money) return list[k];
		}
	}
	return STRUCT_NONE;
}

/*
** Guaranteed defences: below the target, the next tower is built ahead of the stock build
** list (power still comes first).
*/
StructType JMod_Defence_Needed(HouseClass * house)
{
	if (house == NULL || house->IsHuman || GameToPlay != GAME_GLYPHX_MULTIPLAYER) return STRUCT_NONE;
	if (JMod.AIMinDefences <= 0 && JMod.AIBuildingsPerDefence <= 0) return STRUCT_NONE;
	if (house->Power < house->Drain + 50) return STRUCT_NONE;
	int have = Defence_Count(house), want = Defence_Target(house);
	if (have >= want) return STRUCT_NONE;
	StructType pick = Pick_Defence(house, house->Available_Money(), have);
	if (pick != STRUCT_NONE) {
		static int traces = 0;
		if (traces++ < 60) JMod_Trace("AI %s: defences %d of %d -> %s", house->Class->IniName, have, want, BuildingTypeClass::As_Reference(pick).IniName);
	}
	return pick;
}

/*
** Base growth. The stock AI only allows each building type as a ratio of its current base size,
** so once every ratio is met it stops building (and its defence list only knows Nod's turret and
** Obelisk). When the stock list has nothing to build and the AI has money, pick from a rotating
** list of useful buildings, each up to the [AI] limits.
*/
StructType JMod_Extra_Building(HouseClass * house)
{
	if (!JMod.AIExpand || house == NULL || house->IsHuman) return STRUCT_NONE;
	if (GameToPlay != GAME_GLYPHX_MULTIPLAYER) return STRUCT_NONE;
	long money = house->Available_Money();
	if (money < JMod.AIExpandMoney) return STRUCT_NONE;

	auto can = [&](StructType t) {
		BuildingTypeClass const & b = BuildingTypeClass::As_Reference(t);
		return house->Can_Build(&b, house->ActLike) && b.Cost_Of() < money;
	};
#define q(t) (house->BQuantity[t])		// macro, not a lambda: BQuantity is private (friend access)

	/*
	**	Power first if the margin is thin, then silos if storage is nearly full.
	*/
	if (house->Power < house->Drain + 150) {
		if (can(STRUCT_ADVANCED_POWER)) return STRUCT_ADVANCED_POWER;
		if (can(STRUCT_POWER)) return STRUCT_POWER;
	}
	if (house->Capacity > 0 && house->Tiberium * 100 > house->Capacity * 80 && can(STRUCT_STORAGE)) return STRUCT_STORAGE;

	int defences = Defence_Count(house);
	int deftarget = Defence_Target(house);

	static int rotate[HOUSE_COUNT];
	int & r = rotate[house->Class->House];
	for (int tries = 0; tries < 9; tries++) {
		int slot = (r + tries) % 9;
		StructType pick = STRUCT_NONE;
		switch (slot) {
			case 0: if (q(STRUCT_REFINERY) < Rule.RefineryLimit && can(STRUCT_REFINERY)) pick = STRUCT_REFINERY; break;
			case 1: if (defences < deftarget) pick = Pick_Defence(house, money, defences); break;
			case 2: if (q(STRUCT_WEAP) + q(STRUCT_AIRSTRIP) < Rule.WarLimit) {
					if (can(STRUCT_WEAP)) pick = STRUCT_WEAP; else if (can(STRUCT_AIRSTRIP)) pick = STRUCT_AIRSTRIP;
				} break;
			case 3: if (house->Power < house->Drain + 300) {		// spare slot: extra power headroom
					if (can(STRUCT_ADVANCED_POWER)) pick = STRUCT_ADVANCED_POWER; else if (can(STRUCT_POWER)) pick = STRUCT_POWER;
				} break;
			case 4: if (q(STRUCT_RADAR) == 0 && can(STRUCT_RADAR)) pick = STRUCT_RADAR; break;
			case 5: if (q(STRUCT_REPAIR) == 0 && q(STRUCT_WEAP) + q(STRUCT_AIRSTRIP) > 0 && can(STRUCT_REPAIR)) pick = STRUCT_REPAIR; break;
			case 6: if (q(STRUCT_BARRACKS) + q(STRUCT_HAND) < Rule.BarracksLimit) {
					if (can(STRUCT_BARRACKS)) pick = STRUCT_BARRACKS; else if (can(STRUCT_HAND)) pick = STRUCT_HAND;
				} break;
			case 7: if (q(STRUCT_SAM) < Rule.AALimit && q(STRUCT_SAM) < 1 + (int)house->CurBuildings / 8 && can(STRUCT_SAM)) pick = STRUCT_SAM; break;
			case 8: if (q(STRUCT_HELIPAD) < Rule.HelipadLimit && q(STRUCT_HELIPAD) < 1 + (int)house->CurBuildings / 10 && can(STRUCT_HELIPAD)) pick = STRUCT_HELIPAD;
					else if (q(STRUCT_EYE) + q(STRUCT_TEMPLE) == 0) {
						if (can(STRUCT_EYE)) pick = STRUCT_EYE; else if (can(STRUCT_TEMPLE)) pick = STRUCT_TEMPLE;
					}
					break;
		}
		if (pick != STRUCT_NONE) {
			r = slot + 1;
			static int traces = 0;
			if (traces++ < 60) JMod_Trace("AI %s: base growth -> %s", house->Class->IniName, BuildingTypeClass::As_Reference(pick).IniName);
			return pick;
		}
	}
	return STRUCT_NONE;
#undef q
}

void JMod_Wave_AI(HouseClass * house)
{
	if (!JMod_Wave_Controls(house)) return;
	Check_New_Game();

	int hid = house->Class->House;
	if (hid < 0 || hid >= HOUSE_COUNT) return;
	HouseWaveState & s = State[hid];

	if (!s.Started) {
		s.Started = true;
		s.Phase = WAVE_NONE;
		s.NextThink = Frame;
		s.LastLaunch = Frame;
		s.GatherStart = Frame;
		s.Target = HOUSE_NONE;
	}
	/*
	**	The stock AI caps itself at (enemy count + 10) units/infantry, counting harvesters.
	**	Raise those caps to the configured minimums so it can build a real army.
	*/
	//	Note: CurUnits counts infantry too (FootClass ctor), so the vehicle cap is "vehicles + current infantry".
	if (JMod.AIMinUnitCap > 0 && house->MaxUnit < (unsigned)JMod.AIMinUnitCap + house->CurInfantry) house->MaxUnit = JMod.AIMinUnitCap + house->CurInfantry;
	if (JMod.AIMinInfantryCap > 0 && house->MaxInfantry < (unsigned)JMod.AIMinInfantryCap) house->MaxInfantry = JMod.AIMinInfantryCap;

	if (Frame < s.NextThink) return;
	s.NextThink = Frame + TICKS_PER_SECOND * 2;

	/*
	**	Periodic status line so behaviour can be checked from the trace log.
	*/
	if (JMod.AIStatusSeconds > 0 && Frame - s.LastStatus >= (long)JMod.AIStatusSeconds * TICKS_PER_SECOND) {
		s.LastStatus = Frame;
		int inf = 0, veh = 0;
		For_Each_Combat(house, [&](FootClass * f, SlotTag *) { if (f->What_Am_I() == RTTI_UNIT) veh++; else inf++; });
		JMod_Trace("AI %s status: money %ld, refineries %d, harvesters %d, war factories %d, barracks %d, buildings %d, combat vehicles %d, infantry %d, building unit %d / infantry %d / structure %d, tiberium short %d",
			house->Class->IniName, house->Available_Money(), (int)house->BQuantity[STRUCT_REFINERY], (int)house->UQuantity[UNIT_HARVESTER],
			(int)house->BQuantity[STRUCT_WEAP], (int)(house->BQuantity[STRUCT_BARRACKS] + house->BQuantity[STRUCT_HAND]), (int)house->CurBuildings,
			veh, inf, (int)house->BuildUnit, (int)house->BuildInfantry, (int)house->BuildStructure, (int)house->IsTiberiumShort);
		JMod_Trace("   power %d/%d, units %u/%u, infantry %u/%u, buildings %u/%u, IQ %d, base-building %d, con yards %d",
			house->Power, house->Drain, house->CurUnits, house->MaxUnit, house->CurInfantry, house->MaxInfantry,
			house->CurBuildings, house->MaxBuilding, house->IQ, (int)house->IsBaseBuilding, (int)house->BQuantity[STRUCT_CONST]);
		Factory_Watchdog(house, true);
	} else {
		Factory_Watchdog(house, false);
	}

	if (!house->Center) return;
	if (Frame < (long)JMod.WaveFirstDelay * TICKS_PER_SECOND) return;	// let the AI set up first

	HousesType enemy = Pick_Enemy(house);
	if (enemy == HOUSE_NONE) return;
	HouseClass * eh = HouseClass::As_Pointer(enemy);
	if (enemy != s.Target) {
		s.Target = enemy;
		JMod_Trace("WaveAI %s: target is now %s", house->Class->IniName, eh->Class->IniName);
	}
	s.Rally   = Point_Between(house->Center, eh->Center, JMod.WaveRallyDistance);
	if (s.Phase == WAVE_NONE) s.Staging = Point_Between(eh->Center, house->Center, JMod.WaveStagingDistance);	// final choice made at launch
	COORDINATE rally = Cell_Coord(s.Rally);
	COORDINATE staging = Cell_Coord(s.Staging);

	/*
	**	Pick the home guard: the HomeGuard untagged combat units nearest our base centre.
	*/
	FootClass * guard[32];
	int guarddist[32];
	int nguard = 0;
	int homeguard = JMod.WaveHomeGuard;
	if (homeguard > 32) homeguard = 32;
	For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
		if (tag->House == hid && tag->Wave == s.WaveId && s.Phase != WAVE_NONE) return;	// in the active wave
		int d = ::Distance(f->Center_Coord(), house->Center);
		if (nguard < homeguard) {
			guard[nguard] = f; guarddist[nguard] = d; nguard++;
		} else {
			int worst = 0;
			for (int k = 1; k < nguard; k++) if (guarddist[k] > guarddist[worst]) worst = k;
			if (nguard > 0 && d < guarddist[worst]) { guard[worst] = f; guarddist[worst] = d; }
		}
	});
	auto is_guard = [&](FootClass * f) { for (int k = 0; k < nguard; k++) if (guard[k] == f) return true; return false; };

	/*
	**	Track the active wave.
	*/
	if (s.Phase != WAVE_NONE) {
		int alive = 0, arrived = 0, totaldist = 0;
		long strength = 0;
		For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
			if (tag->House != hid || tag->Wave != s.WaveId) return;
			alive++;
			strength += f->Strength;
			if (::Distance(f->Center_Coord(), staging) < 5 * JM_CELL) arrived++;
			totaldist += ::Distance(f->Center_Coord(), staging) / JM_CELL;
		});

		if (s.Phase == WAVE_ADVANCE && alive > 0) {
			int avg = totaldist / alive;
			if (avg < s.BestAvgDist) { s.BestAvgDist = avg; s.LastProgress = Frame; }
		}
		bool retreat = false;
		if (alive == 0) {
			s.Failures++;
			JMod_Trace("WaveAI %s: wave %d wiped out (failures %d)", house->Class->IniName, s.WaveId, s.Failures);
			s.Phase = WAVE_NONE;
			s.GatherStart = Frame;
		} else if (s.StartStrength > 0 && strength * 100 < s.StartStrength * JMod.WaveRetreatPercent) {
			retreat = true;
		}

		if (retreat) {
			s.Failures++;
			JMod_Trace("WaveAI %s: wave %d retreating, %d units left at %ld%% strength", house->Class->IniName, s.WaveId, alive, strength * 100 / s.StartStrength);
			For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
				if (tag->House != hid || tag->Wave != s.WaveId) return;
				tag->House = -1;
				Order_Move(f, s.Rally);
			});
			s.Phase = WAVE_NONE;
			s.GatherStart = Frame;
		} else if (s.Phase == WAVE_ADVANCE &&
				(arrived * 100 >= alive * 60 || Frame - s.PhaseStart > (long)JMod.WaveAdvanceTimeout * TICKS_PER_SECOND ||
				 (alive > 0 && Frame - s.LastProgress > (long)JMod.WaveStallSeconds * TICKS_PER_SECOND))) {
			if (arrived * 100 < alive * 60) JMod_Trace("WaveAI %s: wave %d stopped closing on staging (avg %d cells away) - attacking from here", house->Class->IniName, s.WaveId, alive ? totaldist / alive : 0);
			JMod_Trace("WaveAI %s: wave %d assaulting (%d of %d at staging)", house->Class->IniName, s.WaveId, arrived, alive);
			For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
				if (tag->House != hid || tag->Wave != s.WaveId) return;
				f->Assign_Mission(MISSION_HUNT);
			});
			s.Phase = WAVE_ASSAULT;
			s.PhaseStart = Frame;
		} else if (s.Phase == WAVE_ADVANCE) {
			/*
			**	Keep stragglers moving to the staging point.
			*/
			For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
				if (tag->House != hid || tag->Wave != s.WaveId) return;
				if (Is_Idle(f) && ::Distance(f->Center_Coord(), staging) >= 5 * JM_CELL) Order_Move(f, s.Staging);
			});
		}
	}

	/*
	**	Gather everyone else (except the home guard) at the rally point.
	*/
	int ready = 0, readyvalue = 0, readyveh = 0;
	For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
		if (s.Phase != WAVE_NONE && tag->House == hid && tag->Wave == s.WaveId) return;
		if (is_guard(f)) return;
		int d = ::Distance(f->Center_Coord(), rally);
		if (d < 5 * JM_CELL) {
			ready++;
			readyvalue += Unit_Value(f);
			if (f->What_Am_I() == RTTI_UNIT) readyveh++;
		} else if (Is_Idle(f)) {
			Order_Move(f, s.Rally);
		}
	});

	/*
	**	Launch a new wave?
	*/
	int needed = JMod.WaveBase + JMod.WaveGrowth * s.Launched + JMod.WaveFailBonus * s.Failures;
	if (needed > JMod.WaveMax) needed = JMod.WaveMax;
	bool can_launch = (s.Phase == WAVE_NONE) ||
		(s.Phase == WAVE_ASSAULT && Frame - s.LastLaunch > (long)JMod.WaveMinGap * TICKS_PER_SECOND);
	bool waited_long = Frame - s.GatherStart > (long)JMod.WaveMaxWait * TICKS_PER_SECOND;
	bool capped = house->CurUnits >= house->MaxUnit && house->CurInfantry >= house->MaxInfantry;
	if (capped && ready >= 2) waited_long = true;	// army can't grow - go with what we have

	if (can_launch && Frame - s.LastLaunch >= (long)JMod.WaveMinGap * TICKS_PER_SECOND &&
			(readyvalue >= needed || (waited_long && readyvalue >= needed / 2 && ready >= 2))) {
		s.Staging = Reachable_Staging(s.Rally, Cell_Coord(Point_Between(eh->Center, house->Center, JMod.WaveStagingDistance)));
		staging = Cell_Coord(s.Staging);
		s.BestAvgDist = 0x7FFF;
		s.LastProgress = Frame;
		s.WaveId++;
		s.Launched++;
		s.StartStrength = 0;
		int count = 0;
		For_Each_Combat(house, [&](FootClass * f, SlotTag * tag) {
			if (is_guard(f)) return;
			if (::Distance(f->Center_Coord(), rally) >= 6 * JM_CELL) return;
			if (tag->House == hid && tag->Wave != s.WaveId && s.Phase == WAVE_ASSAULT && tag->Wave == s.WaveId - 1) return;
			tag->House = (signed char)hid;
			tag->Wave = (short)s.WaveId;
			s.StartStrength += f->Strength;
			count++;
			Order_Move(f, s.Staging);
		});
		s.Phase = WAVE_ADVANCE;
		s.PhaseStart = Frame;
		s.LastLaunch = Frame;
		s.GatherStart = Frame;
		JMod_Trace("WaveAI %s: launching wave %d: %d units (%d vehicles), worth %d credits (needed %d, failures %d) toward %s",
			house->Class->IniName, s.WaveId, count, readyveh, readyvalue, needed, s.Failures, eh->Class->IniName);
	}
}
