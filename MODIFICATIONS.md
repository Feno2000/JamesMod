# Modifications to the original EA source

As required by GPL v3 section 5(a) and EA's additional terms, this file lists every change made to the source code
Electronic Arts released for the C&C Remastered Collection. In-source changes are marked with `JamesMod` comments.

## 2026-11-22: Build fixes and config layer (JamesMod)

### Toolchain (both games)
- `TIBERIANDAWN/TiberianDawn.vcxproj`, `REDALERT/RedAlert.vcxproj`:
  - retargeted to the v143 toolset and Windows 10 SDK,
  - added `WINDOWS_IGNORE_PACKING_MISMATCH`,
  - Conformance mode set to No (Tiberian Dawn Debug and Release configurations).
- `TIBERIANDAWN/RESOURCE/TiberianDawn.rc`, `REDALERT/RESOURCE/RedAlert.rc`: include `winres.h` instead of
  `afxres.h`.

### Bug fix: callee-saved registers (both games)
- `TIBERIANDAWN/MMX.ASM`, `REDALERT/MMX.ASM`:
  - `Detect_MMX_Availability` now declares `uses ebx`, because `cpuid` overwrites `EBX`.
  - `Init_MMX` now declares `uses ebx edi`.
  - Without this, builds from newer compilers corrupt the caller's registers and the game crashes on starting a
    match.

### Tiberian Dawn mod code
- **New:** `TIBERIANDAWN/JMOD.H`, `TIBERIANDAWN/JMOD.CPP`. Loads `JAMESMOD.INI` from the mod folder, applies the
  overrides to the AI values in `Rule`, writes `JAMESMOD.LOG`, and provides `JMod_Trace()` debug logging.
- `TIBERIANDAWN/TiberianDawn.vcxproj`: added `JMOD.H` and `JMOD.CPP`.
- `TIBERIANDAWN/DLLInterface.cpp`:
  - includes `JMOD.H`,
  - calls `JMod_Load()` in `CNC_Set_Multiplayer_Data` and `CNC_Start_Instance_Variation`,
  - applies `CreditsPercent` and `CreditsBonus` to skirmish starting credits,
  - `CNC_Set_Difficulty` skips `Set_Scenario_Difficulty` when `PlayerPtr` is NULL,
  - adds `JMod_Trace` calls at the start of each exported `CNC_*` function, and logs `DLL_Startup`'s return value.
- `TIBERIANDAWN/DLLInterfaceEditor.cpp`: includes `JMOD.H` and adds `JMod_Trace` calls to the editor exports.

## 2026-11-22: Harvester features, toolset and crash logger (JamesMod)

- Projects switched to the **v141 (VS2017) toolset** and pinned to Windows SDK 10.0.26100.0. v143 builds crashed
  mid-game.
- **New:** `TIBERIANDAWN/JMODHARV.CPP`:
  - harvester self-defence machine gun: muzzle flash; a bullet with no `Payback`, because harvesters have no weapon
    slot;
  - player harvester auto-resume from Guard.
- **New:** `TIBERIANDAWN/JMODCRASH.CPP`. Vectored-exception crash logger, installed from `CNC_Init` only and removed
  when the DLL unloads.
- `TIBERIANDAWN/UNIT.CPP`:
  - `UnitClass::AI` calls `JMod_Harvester_Defence`;
  - `UnitClass::Take_Damage` records harvester hits;
  - `UnitClass::Mission_Guard` calls `JMod_Harvester_Guard` for player harvesters.
- `TIBERIANDAWN/JMOD.H`, `JMOD.CPP`: `[Harvester]` settings and `JMod_Enable_Crash_Logger()`.
- `TIBERIANDAWN/DLLInterface.cpp`: `CNC_Init` enables the crash logger.
- `mod/build.ps1`: builds with MSBuild, then installs.
