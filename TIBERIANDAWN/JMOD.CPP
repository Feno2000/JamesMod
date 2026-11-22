/*
** JMOD.CPP -- James's mod configuration layer for Tiberian Dawn.
** See JMOD.H.
*/
#include "function.h"
#include "JMOD.H"
#include <stdio.h>
#include <stdarg.h>

JModConfig JMod = { false, 100, 0 };

static char IniPath[MAX_PATH];
static char LogPath[MAX_PATH];
static char TracePath[MAX_PATH];

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
