/*
** JMODHARV.CPP -- JamesMod: harvester self-defence machine gun.
**
** Harvesters keep their original behaviour (no Primary weapon, so they still flee to
** base and call for help when properly attacked). This adds a light, opportunistic
** machine gun that fires at nearby infantry -- by default only infantry that are
** actively shooting at the harvester (stray civilians, farmers, lone riflemen).
**
** Per-harvester cooldowns are kept here, so UnitClass's layout (and save games)
** are unchanged. Settings come from the [Harvester] section of JAMESMOD.INI.
*/
#include "function.h"
#include "JMOD.H"

#define JMOD_MAX_UNITS	1024

static long NextShot[JMOD_MAX_UNITS];

void JMod_Harvester_Defence(UnitClass * unit)
{
	if (!JMod.HarvGun || unit == NULL) return;
	if (!unit->Class->IsToHarvest || unit->IsInLimbo || unit->Strength <= 0) return;
	if (unit->House->IsHuman ? !JMod.HarvGunPlayer : !JMod.HarvGunAI) return;

	int id = Units.ID(unit);
	if (id < 0 || id >= JMOD_MAX_UNITS) return;

	/*
	**	Frame restarts at zero each game, so discard cooldowns left over from a previous one.
	*/
	if (NextShot[id] > Frame + JMod.HarvRate) NextShot[id] = 0;
	if (Frame < NextShot[id]) return;

	TARGET me = unit->As_Target();
	int range = JMod.HarvRange * 256;		// cells to leptons
	InfantryClass * best = NULL;
	int bestdist = range + 1;

	for (int index = 0; index < Infantry.Count(); index++) {
		InfantryClass * inf = Infantry.Ptr(index);
		if (inf == NULL || inf->IsInLimbo || inf->Strength <= 0) continue;
		if (unit->House->Is_Ally(inf)) continue;

		bool attacking = (inf->TarCom == me) || (inf->NavCom == me);
		if (!attacking) {
			if (!JMod.HarvGunAnyEnemy) continue;
			if (inf->House->Class->House == HOUSE_NEUTRAL) continue;	// never shoot passive civilians
		}

		int dist = unit->Distance(inf);
		if (dist < bestdist) {
			bestdist = dist;
			best = inf;
		}
	}

	if (best == NULL) return;

	BulletClass * bullet = new BulletClass(BULLET_BULLET);
	if (bullet != NULL) {
		bullet->Assign_Target(best->As_Target());
		/*
		**	No Payback (shooter): the harvester has no weapon slot, and code that inspects the
		**	shooter (range clamping, target reactions) would index Weapons[WEAPON_NONE] = Weapons[-1].
		*/
		bullet->Payback = NULL;
		bullet->Strength = (short)JMod.HarvDamage;
		if (bullet->Unlimbo(unit->Center_Coord(), ::Direction(unit->Center_Coord(), best->Center_Coord()))) {
			Sound_Effect(VOC_MGUN11, unit->Coord);

			/*
			**	Muzzle flash so the player can see the harvester firing.
			*/
			DirType dir = ::Direction(unit->Center_Coord(), best->Center_Coord());
			AnimClass * flash = new AnimClass((AnimType)(ANIM_GUN_N + Dir_Facing(dir)), unit->Center_Coord());
			if (flash) flash->Attach_To(unit);

			static int jm_shots = 0;
			if (jm_shots++ < 20) {
				JMod_Trace("Harvester %d (%s) fired at infantry %d, dist %d leptons", id, unit->House->IsHuman ? "player" : "AI", Infantry.ID(best), bestdist);
			}
		} else {
			delete bullet;
		}
	}
	NextShot[id] = Frame + JMod.HarvRate;
}


/*
** ---------------------------------------------------------------------------------------
** Player harvester auto-resume.
**
** AI harvesters left on guard go straight back to harvesting (Mission_Guard), but the
** player's sit idle -- e.g. after chasing an attacker or being moved out of danger.
** This resumes them once they've been idle for ResumeDelay seconds, haven't been hit for
** SafeTime seconds and aren't currently selected (selecting one resets its timer, so a
** deliberately parked harvester can be held by keeping it selected or re-ordering it).
** ---------------------------------------------------------------------------------------
*/
#include "DLLInterface.h"
extern void On_Message(const char* message, float timeout_seconds, EventCallbackMessageEnum message_type, long long message_id);

static long LastHit[JMOD_MAX_UNITS];		// frame the harvester last took damage
static long IdleSince[JMOD_MAX_UNITS];		// frame the current idle (guard) period began
static long LastGuard[JMOD_MAX_UNITS];		// frame Mission_Guard last ran for it
static long ResumedAt[JMOD_MAX_UNITS];		// frame we last auto-resumed it
static int  Backoff[JMOD_MAX_UNITS];		// grows if it keeps bouncing straight back to idle

static int Unit_Slot(UnitClass * unit)
{
	int id = Units.ID(unit);
	return (id >= 0 && id < JMOD_MAX_UNITS) ? id : -1;
}

void JMod_Harvester_Hit(UnitClass * unit)
{
	int id = Unit_Slot(unit);
	if (id >= 0) LastHit[id] = Frame;
}

int JMod_Harvester_Guard(UnitClass * unit)
{
	if (!JMod.HarvResume || unit == NULL || !unit->House->IsHuman) return 0;
	int id = Unit_Slot(unit);
	if (id < 0) return 0;

	/*
	**	New game (Frame restarted) -- clear stale state for this slot.
	*/
	if (LastGuard[id] > Frame || IdleSince[id] > Frame || LastHit[id] > Frame || ResumedAt[id] > Frame) {
		LastGuard[id] = IdleSince[id] = LastHit[id] = ResumedAt[id] = 0;
		Backoff[id] = 0;
	}

	/*
	**	Mission_Guard runs every second or so while idle; a gap means a new idle period.
	*/
	if (Frame - LastGuard[id] > TICKS_PER_SECOND * 3) {
		IdleSince[id] = Frame;

		/*
		**	Went idle again soon after we resumed it (no Tiberium reachable, refinery gone...):
		**	back off so we don't nag every few seconds.
		*/
		if (ResumedAt[id] != 0 && Frame - ResumedAt[id] < TICKS_PER_SECOND * 15) {
			if (Backoff[id] < 4) Backoff[id]++;
		} else {
			Backoff[id] = 0;
		}
	}
	LastGuard[id] = Frame;

	if (unit->Is_Selected_By_Player()) {
		IdleSince[id] = Frame;			// player is looking at it -- leave it alone
		return 0;
	}
	if (LastHit[id] != 0 && Frame - LastHit[id] < JMod.HarvSafeTime * TICKS_PER_SECOND) {
		return 0;						// still under fire recently
	}

	long delay = (long)JMod.HarvResumeDelay * TICKS_PER_SECOND << Backoff[id];
	if (Frame - IdleSince[id] < delay) return 0;

	unit->Assign_Mission(MISSION_HARVEST);
	ResumedAt[id] = Frame;
	IdleSince[id] = Frame;
	if (JMod.HarvResumeMessage) {
		On_Message("Harvester resuming harvesting", 6.0f, MESSAGE_TYPE_DIRECT, -1);	// client may not display free text
	}
	/*
	**	Audio cue: 1 = computer bleep (heard anywhere) + "acknowledged" at the harvester,
	**	2 = bleep only, 3 = "vehicle reporting" at the harvester, 4 = EVA "unit ready", 0 = silent.
	*/
	switch (JMod.HarvResumeSound) {
		case 1: Sound_Effect(VOC_BLEEPY3, (COORDINATE)0); Sound_Effect(VOC_ACKNOWL, unit->Coord); break;
		case 2: Sound_Effect(VOC_BLEEPY3, (COORDINATE)0); break;
		case 3: Sound_Effect(VOC_VEHIC, unit->Coord); break;
		case 4: Speak(VOX_UNIT_READY, unit->House); break;
		default: break;
	}
	return TICKS_PER_SECOND;
}
