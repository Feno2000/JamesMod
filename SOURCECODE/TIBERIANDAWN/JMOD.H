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
};

extern JModConfig JMod;

void JMod_Load(void);
void JMod_Install_Crash_Logger(const char * root);	// JMODCRASH.CPP
void JMod_Enable_Crash_Logger(void);					// call from CNC_Init only
void JMod_Harvester_Defence(class UnitClass * unit);	// JMODHARV.CPP
void JMod_Harvester_Hit(class UnitClass * unit);
int  JMod_Harvester_Guard(class UnitClass * unit);	// returns delay if it reassigned the harvester, else 0
void JMod_Trace(const char *fmt, ...);	// appends a line to JAMESMOD_TRACE.LOG

#endif
