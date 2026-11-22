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
};

extern JModConfig JMod;

void JMod_Load(void);
void JMod_Trace(const char *fmt, ...);	// appends a line to JAMESMOD_TRACE.LOG

#endif
