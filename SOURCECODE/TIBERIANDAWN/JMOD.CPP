/*
** JMOD.CPP -- James's mod configuration layer for Tiberian Dawn.
** See JMOD.H.
*/
#include "function.h"
#include "JMOD.H"
#include <stdio.h>
#include <stdarg.h>

JModConfig JMod = { false, 100, 0, true, true, true, false, 3, 15, 15, true, 20, 10, true, 4,
	true, 10, 5, 50,
	true, 60, 3, 3000, 1000, 1000, 15000, 60, 240, 8, 10, 120, 20, 35,
	2, true, 30, 30, 40, 40, 16, true, 1500, 2, true, 6, 3, true, 200, true,
	true, 50, 4, true };

static char IniPath[MAX_PATH];
static char LogPath[MAX_PATH];
static char TracePath[MAX_PATH];
static char RootPath[MAX_PATH];

/*
** Work out the mod root: DLL lives in <ModRoot>\Data\TiberianDawn.dll.
*/
static void Resolve_Paths(void)
{
	HMODULE self = NULL;
	char root[MAX_PATH] = "";
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&Resolve_Paths, &self);
	GetModuleFileNameA(self, root, MAX_PATH);

	char *slash = strrchr(root, '\\');	// strip file name
	if (slash) *slash = '\0';
	slash = strrchr(root, '\\');			// strip "Data"
	if (slash) *slash = '\0';

	_snprintf(IniPath, MAX_PATH, "%s\\JAMESMOD.INI", root);
	_snprintf(LogPath, MAX_PATH, "%s\\JAMESMOD.LOG", root);
	_snprintf(TracePath, MAX_PATH, "%s\\JAMESMOD_TRACE.LOG", root);
	strncpy(RootPath, root, MAX_PATH-1);
	IniPath[MAX_PATH-1] = LogPath[MAX_PATH-1] = TracePath[MAX_PATH-1] = '\0';
}

static int Read_Int(const char *section, const char *key, int def)
{
	return (int)GetPrivateProfileIntA(section, key, def, IniPath);
}

/*
** Each macro captures the original EA value the first time it runs (static local),
** so removing a key from the INI restores the default on the next game.
*/
#define JMOD_INT(section, key, field) { \
	static const int def_##field = Rule.field; \
	Rule.field = Read_Int(section, key, def_##field); \
	if (log) fprintf(log, "  %-22s = %d (default %d)\n", key, (int)Rule.field, def_##field); }

/*
** Ratios are stored as 8.8 fixed point (0x0100 = 100%). The INI uses whole percent.
*/
#define JMOD_RATIO(section, key, field) { \
	static const int def_##field = (Rule.field * 100 + 128) / 256; \
	int pct = Read_Int(section, key, def_##field); \
	if (pct < 0) pct = 0; if (pct > 100) pct = 100; \
	Rule.field = (unsigned short)((pct * 256) / 100); \
	if (log) fprintf(log, "  %-22s = %d%% (default %d%%)\n", key, pct, def_##field); }

void JMod_Load(void)
{
	Resolve_Paths();
	JMod.Loaded = (GetFileAttributesA(IniPath) != INVALID_FILE_ATTRIBUTES);

	FILE *log = fopen(LogPath, "w");
	if (log) fprintf(log, "JamesMod config\nINI: %s (%s)\n\n[AI]\n", IniPath, JMod.Loaded ? "found" : "NOT FOUND - using defaults");

	/*
	**	AI timing and base-building priorities.
	*/
	JMOD_INT  ("AI", "AttackInterval",  AttackInterval);	// x (0.5-2 min) between attack checks
	JMOD_INT  ("AI", "AttackDelay",     AttackDelay);		// x (0.5-2 min) before first attack check
	JMOD_INT  ("AI", "BaseSizeAdd",     BaseSizeAdd);
	JMOD_INT  ("AI", "PowerSurplus",    PowerSurplus);
	JMOD_RATIO("AI", "RefineryRatio",   RefineryRatio);
	JMOD_INT  ("AI", "RefineryLimit",   RefineryLimit);
	JMOD_RATIO("AI", "BarracksRatio",   BarracksRatio);
	JMOD_INT  ("AI", "BarracksLimit",   BarracksLimit);
	JMOD_RATIO("AI", "WarRatio",        WarRatio);
	JMOD_INT  ("AI", "WarLimit",        WarLimit);
	JMOD_RATIO("AI", "DefenseRatio",    DefenseRatio);
	JMOD_INT  ("AI", "DefenseLimit",    DefenseLimit);
	JMOD_RATIO("AI", "AARatio",         AARatio);
	JMOD_INT  ("AI", "AALimit",         AALimit);
	JMOD_RATIO("AI", "HelipadRatio",    HelipadRatio);
	JMOD_INT  ("AI", "HelipadLimit",    HelipadLimit);
	JMOD_RATIO("AI", "AirstripRatio",   AirstripRatio);
	JMOD_INT  ("AI", "AirstripLimit",   AirstripLimit);
	JMOD_INT  ("AI", "InfantryReserve", InfantryReserve);

	/*
	**	Skirmish game tweaks.
	*/
	JMod.CreditsPercent = Read_Int("Game", "CreditsPercent", 100);
	JMod.CreditsBonus   = Read_Int("Game", "CreditsBonus", 0);
	/*
	**	Harvester self-defence machine gun.
	*/
	JMod.HarvGun         = Read_Int("Harvester", "SelfDefence", 1) != 0;
	JMod.HarvGunPlayer   = Read_Int("Harvester", "PlayerHarvesters", 1) != 0;
	JMod.HarvGunAI       = Read_Int("Harvester", "AIHarvesters", 1) != 0;
	JMod.HarvGunAnyEnemy = Read_Int("Harvester", "FireAtAnyEnemy", 0) != 0;
	JMod.HarvRange       = Read_Int("Harvester", "Range", 3);
	JMod.HarvDamage      = Read_Int("Harvester", "Damage", 15);
	JMod.HarvRate        = Read_Int("Harvester", "RateOfFire", 15);
	JMod.HarvResume        = Read_Int("Harvester", "AutoResume", 1) != 0;
	JMod.HarvResumeDelay   = Read_Int("Harvester", "ResumeDelay", 20);
	JMod.HarvSafeTime      = Read_Int("Harvester", "SafeTime", 10);
	JMod.HarvResumeMessage = Read_Int("Harvester", "ResumeMessage", 1) != 0;
	JMod.HarvResumeSound   = Read_Int("Harvester", "ResumeSound", 4);
	if (JMod.HarvResumeDelay < 3) JMod.HarvResumeDelay = 3;
	JMod.RefRepair          = Read_Int("Harvester", "RefineryRepairs", 1) != 0;
	JMod.RefRepairStep      = Read_Int("Harvester", "RepairStep", 10);
	JMod.RefRepairRate      = Read_Int("Harvester", "RepairRate", 5);
	JMod.RefRepairCostPercent = Read_Int("Harvester", "RepairCostPercent", 50);
	if (JMod.RefRepairRate < 1) JMod.RefRepairRate = 1;
	if (JMod.RefRepairStep < 1) JMod.RefRepairStep = 1;
	if (JMod.HarvRange < 1) JMod.HarvRange = 1;
	if (JMod.HarvRate < 5) JMod.HarvRate = 5;
	if (log) fprintf(log, "\n[Harvester]\n  SelfDefence=%d Player=%d AI=%d AnyEnemy=%d Range=%d Damage=%d RateOfFire=%d\n",
		(int)JMod.HarvGun, (int)JMod.HarvGunPlayer, (int)JMod.HarvGunAI, (int)JMod.HarvGunAnyEnemy, JMod.HarvRange, JMod.HarvDamage, JMod.HarvRate);
	if (log) fprintf(log, "  AutoResume=%d ResumeDelay=%d SafeTime=%d ResumeMessage=%d ResumeSound=%d\n", (int)JMod.HarvResume, JMod.HarvResumeDelay, JMod.HarvSafeTime, (int)JMod.HarvResumeMessage, JMod.HarvResumeSound);

	/*
	**	Wave AI.
	*/
	JMod.WaveAI              = Read_Int("WaveAI", "Enabled", 1) != 0;
	JMod.WaveFirstDelay      = Read_Int("WaveAI", "FirstDelay", 60);
	JMod.WaveHomeGuard       = Read_Int("WaveAI", "HomeGuard", 3);
	JMod.WaveBase            = Read_Int("WaveAI", "FirstWaveValue", 3000);
	JMod.WaveGrowth          = Read_Int("WaveAI", "GrowthPerWave", 1000);
	JMod.WaveFailBonus       = Read_Int("WaveAI", "GrowthPerFailure", 1000);
	JMod.WaveMax             = Read_Int("WaveAI", "MaxWaveValue", 15000);
	JMod.WaveMinGap          = Read_Int("WaveAI", "MinSecondsBetweenWaves", 60);
	JMod.WaveMaxWait         = Read_Int("WaveAI", "MaxGatherSeconds", 240);
	JMod.WaveRallyDistance   = Read_Int("WaveAI", "RallyDistance", 8);
	JMod.WaveStagingDistance = Read_Int("WaveAI", "StagingDistance", 10);
	JMod.WaveAdvanceTimeout  = Read_Int("WaveAI", "AdvanceTimeout", 120);
	JMod.WaveRetreatPercent  = Read_Int("WaveAI", "RetreatPercent", 35);
	JMod.WaveStallSeconds    = Read_Int("WaveAI", "StallSeconds", 20);
	JMod.AIHarvPerRefinery   = Read_Int("AI", "HarvestersPerRefinery", 2);
	JMod.AIReplaceHarvOnHard = Read_Int("AI", "ReplaceHarvestersOnHard", 1) != 0;
	JMod.AIStatusSeconds     = Read_Int("AI", "StatusLogSeconds", 30);
	JMod.AIStuckSeconds      = Read_Int("AI", "AbandonUnplaceableAfter", 30);
	JMod.AIMinUnitCap        = Read_Int("AI", "MinVehicleCap", 40);
	JMod.AIMinInfantryCap    = Read_Int("AI", "MinInfantryCap", 40);
	JMod.AIPlaceRadius       = Read_Int("AI", "PlacementRadius", 16);
	JMod.AIExpand            = Read_Int("AI", "KeepGrowingBase", 1) != 0;
	JMod.AIExpandMoney       = Read_Int("AI", "GrowthMinMoney", 1500);
	JMod.AIInfPerVehicle     = Read_Int("AI", "MaxInfantryPerVehicle", 2);
	JMod.AIPreferHeavy       = Read_Int("AI", "PreferHeavyUnits", 1) != 0;
	JMod.AIMinDefences       = Read_Int("AI", "MinDefences", 6);
	JMod.AIBuildingsPerDefence = Read_Int("AI", "BuildingsPerDefence", 3);
	JMod.AINoSelling         = Read_Int("AI", "NeverSell", 1) != 0;
	JMod.AIRepairMinMoney    = Read_Int("AI", "RepairMinMoney", 200);
	JMod.AIRebuildConYard    = Read_Int("AI", "RebuildConstructionYard", 1) != 0;
	if (JMod.AIHarvPerRefinery < 1) JMod.AIHarvPerRefinery = 1;
	if (JMod.WaveBase < 100) JMod.WaveBase = 100;
	JMod.ProdMultiFactory    = Read_Int("Production", "MultipleFactoriesSpeedUp", 1) != 0;
	JMod.ProdBonusPercent    = Read_Int("Production", "BonusPercentPerExtraFactory", 50);
	JMod.ProdMaxFactories    = Read_Int("Production", "MaxFactoriesCounted", 4);
	JMod.ProdIncludeConYards = Read_Int("Production", "IncludeConstructionYards", 1) != 0;
	if (log) fprintf(log, "\n[Production]\n  MultipleFactoriesSpeedUp=%d BonusPercentPerExtraFactory=%d MaxFactoriesCounted=%d IncludeConstructionYards=%d\n",
		(int)JMod.ProdMultiFactory, JMod.ProdBonusPercent, JMod.ProdMaxFactories, (int)JMod.ProdIncludeConYards);
	if (JMod.WaveMax < JMod.WaveBase) JMod.WaveMax = JMod.WaveBase;
	if (log) fprintf(log, "\n[WaveAI]\n  Enabled=%d FirstDelay=%d HomeGuard=%d FirstWaveValue=%d GrowthPerWave=%d GrowthPerFailure=%d MaxWaveValue=%d\n  MinSecondsBetweenWaves=%d MaxGatherSeconds=%d RallyDistance=%d StagingDistance=%d AdvanceTimeout=%d RetreatPercent=%d\n",
		(int)JMod.WaveAI, JMod.WaveFirstDelay, JMod.WaveHomeGuard, JMod.WaveBase, JMod.WaveGrowth, JMod.WaveFailBonus, JMod.WaveMax,
		JMod.WaveMinGap, JMod.WaveMaxWait, JMod.WaveRallyDistance, JMod.WaveStagingDistance, JMod.WaveAdvanceTimeout, JMod.WaveRetreatPercent);

	if (log) {
		fprintf(log, "\n[Game]\n  %-22s = %d\n  %-22s = %d\n", "CreditsPercent", JMod.CreditsPercent, "CreditsBonus", JMod.CreditsBonus);
		fclose(log);
	}
}

void JMod_Trace(const char *fmt, ...)
{
	if (TracePath[0] == '\0') Resolve_Paths();
	FILE *f = fopen(TracePath, "a");
	if (!f) return;
	SYSTEMTIME t;
	GetLocalTime(&t);
	fprintf(f, "%02d:%02d:%02d.%03d  ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
	fprintf(f, "\n");
	fclose(f);
}

/*
** Only called from CNC_Init, i.e. in the game-server process that keeps the DLL loaded.
** (The client briefly loads and unloads the DLL for a version check -- a handler left
** registered there would point at unloaded code.)
*/
void JMod_Enable_Crash_Logger(void)
{
	Resolve_Paths();
	JMod_Install_Crash_Logger(RootPath);
}
