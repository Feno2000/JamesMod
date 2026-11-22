/*
** JMOD.H -- James's mod configuration layer for Tiberian Dawn.
**
** Reads JAMESMOD.INI from the mod root folder (the folder that holds
** ccmod.json) at the start of every game, so values can be tuned
** without rebuilding the DLL. Missing keys fall back to the original
** EA defaults. A JAMESMOD.LOG is written beside it showing what loaded.
*/
#ifndef JMOD_H
#define JMOD_H

struct JModConfig {
	bool	Loaded;				// INI file was found
	int	CreditsPercent;		// skirmish starting credits, % of the lobby value (100 = unchanged)
	int	CreditsBonus;			// flat credits added after the percentage

	// [Harvester] self-defence machine gun
	bool	HarvGun;				// master switch
	bool	HarvGunPlayer;		// player harvesters get it
	bool	HarvGunAI;			// AI harvesters get it
	bool	HarvGunAnyEnemy;		// also fire at enemy infantry not shooting at us (never passive civilians)
	int	HarvRange;			// range in cells
	int	HarvDamage;			// damage per burst
	int	HarvRate;			// ticks between bursts
	bool	HarvResume;			// player harvesters left idle resume harvesting
	int	HarvResumeDelay;		// seconds idle before resuming
	int	HarvSafeTime;		// seconds since last hit before resuming
	bool	HarvResumeMessage;	// show an on-screen message when it resumes
	int	HarvResumeSound;		// 0 none, 1 bleep + "acknowledged", 2 bleep, 3 "vehicle reporting", 4 EVA "unit ready"
	bool	RefRepair;			// [Harvester] refineries repair docked harvesters
	int	RefRepairStep;		// hit points per repair step
	int	RefRepairRate;		// ticks between steps
	int	RefRepairCostPercent;	// % of the harvester's price for a full repair

	// [WaveAI] organised attack waves for computer players
	bool	WaveAI;
	int	WaveFirstDelay;		// seconds into the game before the controller starts
	int	WaveHomeGuard;		// units kept back at base
	int	WaveBase;			// units in the first wave
	int	WaveGrowth;			// extra units per wave launched
	int	WaveFailBonus;		// extra units per failed wave
	int	WaveMax;			// cap on wave size
	int	WaveMinGap;			// minimum seconds between launches
	int	WaveMaxWait;		// after this many seconds gathering, launch with half the wave
	int	WaveRallyDistance;	// cells from our base towards the enemy
	int	WaveStagingDistance;	// cells out from the enemy base
	int	WaveAdvanceTimeout;	// seconds before attacking even if not all arrived
	int	WaveStallSeconds;	// attack if the wave stops getting closer to staging for this long
	int	WaveRetreatPercent;	// retreat when wave strength falls below this %
	int	AIHarvPerRefinery;	// [AI] harvesters the AI keeps per refinery
	bool	AIReplaceHarvOnHard;	// [AI] also replace harvesters on Hard (stock AI never does)
	int	AIStatusSeconds;	// [AI] write an AI status line to the trace log this often (0 = off)
	int	AIStuckSeconds;		// [AI] abandon a finished building the AI can't place after this long (0 = never)
	int	AIMinUnitCap;		// [AI] minimum vehicle cap (stock: enemy count + 10)
	int	AIMinInfantryCap;	// [AI] minimum infantry cap
	int	AIPlaceRadius;		// [AI] fallback building search radius in cells (0 = off)
	bool	AIExpand;			// [AI] keep growing the base when the stock list is satisfied
	int	AIExpandMoney;		// [AI] ...only with at least this much money
	int	AIInfPerVehicle;	// [AI] max infantry per combat vehicle (0 = no limit)
	bool	AIPreferHeavy;		// [AI] weight vehicle choice towards heavier units
	int	AIMinDefences;		// [AI] always have at least this many defensive buildings
	int	AIBuildingsPerDefence;	// [AI] target is one per this many buildings if more (0 = just the minimum)
	bool	AINoSelling;		// [AI] skirmish AI never sells: no damaged-building sales, no power-plant sales, no fire sale
	int	AIRepairMinMoney;	// [AI] repair damaged buildings with at least this much money (stock 1000)
	bool	AIRebuildConYard;	// [AI] build an MCV if the construction yard is lost

	// [Production]
	bool	ProdMultiFactory;	// extra factories speed up their queue
	int	ProdBonusPercent;	// speed added per extra factory
	int	ProdMaxFactories;	// most factories that count
	bool	ProdIncludeConYards;	// extra construction yards speed up buildings too
};

extern JModConfig JMod;

void JMod_Load(void);
void JMod_Install_Crash_Logger(const char * root);	// JMODCRASH.CPP
void JMod_Enable_Crash_Logger(void);					// call from CNC_Init only
void JMod_Harvester_Defence(class UnitClass * unit);	// JMODHARV.CPP
void JMod_Harvester_Hit(class UnitClass * unit);
void JMod_Refinery_Repair(class UnitClass * unit);
int  JMod_Harvester_Guard(class UnitClass * unit);	// returns delay if it reassigned the harvester, else 0
void JMod_Wave_AI(class HouseClass * house);				// JMODAI.CPP
COORDINATE JMod_Find_Build_Location(class HouseClass const * house, class BuildingClass * building);	// fallback placement
StructType JMod_Extra_Building(class HouseClass * house);	// base growth
StructType JMod_Defence_Needed(class HouseClass * house);	// guaranteed defences
void JMod_Wave_Reset(void);								// forget wave state (e.g. after loading a save)
bool JMod_Wave_Controls(class HouseClass const * house);	// true if the wave controller owns this house's attacks
int  JMod_Factory_Time(int time, RTTIType rtti, class HouseClass const * house);	// JMODPROD.CPP
void JMod_Trace(const char *fmt, ...);	// appends a line to JAMESMOD_TRACE.LOG

#endif
