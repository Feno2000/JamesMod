# JamesMod: a smarter AI for Tiberian Dawn (C&C Remastered Collection)

A gameplay mod for the **Tiberian Dawn** half of the *Command & Conquer Remastered Collection*, built from the
GPL source code that Electronic Arts released for `TiberianDawn.dll`.

The goal is an enemy that **plays strategically instead of on a timer**: it attacks in organised waves that grow
over time, builds and defends a real base, expands its economy, and reacts to what you build. Everything is tunable
from a plain INI file, with no rebuild needed.

> **Status: work in progress.** The build fixes and the INI config layer below work today. The AI and base-building
> features are being written now. See the [Roadmap](#roadmap). Issues and ideas are welcome.

> This is an unofficial fan mod. It is not made, endorsed or supported by Electronic Arts or Petroglyph.
> You need to own the C&C Remastered Collection to use it.

---

## Why the stock AI feels the way it does

Reading the original source makes the familiar early-game pattern clear: *"it hits hard for the first few minutes,
then once you hold off a couple of attacks it's a clean sweep."*

**Attacks run on a timer, not a plan.** `HouseClass::AI_Attack()` fires every `AttackInterval × (0.5–2 minutes)`.
About two-thirds of the time it only shuffles units around its base. When it does attack, every armed unit has a
75% chance of being set to `MISSION_HUNT`, so each one drives off on its own towards the nearest target. There's no
grouping, no rally point, no reserve and no retreat. Once you beat a couple of these trickles, its army is gone and
its base is empty.

**Base building is a shopping list with low caps.** In skirmish, `HouseClass::AI_Building()` picks the most urgent
item from power, refinery, barracks, war factory, defence, AA, tech and helipad. Each one is capped as a ratio of its
own base size, with hard limits (4 refineries, 2 war factories, 2 barracks by default). It never builds walls. Its
defence count only considers the gun turret and Obelisk. Defences go on whichever compass side has the least
defence, not the side facing you.

**The campaign AI is mostly scripted.** Missions use fixed teams and triggers from the mission file, plus a base list
the enemy rebuilds in place. That's why campaign enemies "stick to what's already built".

**It doesn't cheat on money.** AI harvesters fill the same limited storage you have, and its factories only build
when it can afford them. Its main edges are the per-difficulty multipliers for cost, build speed, firepower and
armour, and automatic harvester replacement.

---

## Features

### Working now
- **INI config layer** (`JAMESMOD.INI` in the mod folder), re-read at the start of every game:
  - AI attack timing (`AttackInterval`, `AttackDelay`).
  - AI base-building ratios and limits for refineries, barracks, war factories, defences, AA, helipads and airstrips.
  - Skirmish starting credits (percentage of the lobby setting, plus a flat bonus).
  - Delete a line to go back to EA's original value.
  - `JAMESMOD.LOG` shows exactly what loaded, next to the original defaults.
- **Builds with Visual Studio 2022.** The fixes needed to build EA's source with a modern toolchain are included.
  See [Building](#building) and [Technical notes](#technical-notes).

### Roadmap
1. **Wave-based attacks**
   - Units gather at a rally point and launch together once a wave is big enough.
   - Waves grow over the game, and get bigger after a failed attack.
   - A home guard stays back to defend the base.
   - Waves pull back when they're losing badly, instead of dying to the last unit.
2. **Base planner**
   - Defences face the direction the enemy is in.
   - Walls: sandbags early, then chain-link and concrete, with gaps for its own units.
   - Correct defence types for both sides (guard towers and advanced guard towers for GDI; turrets, SAMs and
     Obelisks for Nod), scaled to the threat.
   - The base keeps growing, rebuilds where things stood, and adds silos before storage fills.
3. **Economy**
   - Expands to new Tiberium fields with defended refineries.
   - Protects and escorts harvesters, and responds when they're attacked.
4. **Reading the player**
   - Counter-builds against your army: anti-armour against tank spam, artillery and air against turtling, flame
     units against infantry.
   - Target priorities: harvester raids, knocking out power to disable defences, probing for weak sides.
5. **Harvester self-defence**
   - Harvesters get a light machine gun that only fires at infantry actively shooting at them, such as farmers or
     stray civilians.
   - It's off by default for unarmed civilians, so missions about protecting civilians aren't broken.
6. **Campaign support** with separate switches:
   - smarter scripted teams,
   - optional AI-planned extra waves,
   - base rebuilding plus expansion on top of the mission's own base list.
7. **AI personalities** (rusher, turtle, raider) chosen in the INI.

---

## Installing

> Pre-built releases will be added once the AI features land. For now, build from source (below).

1. Build `TiberianDawn.dll` (Release | Win32).
2. Copy the mod into your Documents folder:
   ```
   Documents\CnCRemastered\Mods\Tiberian_Dawn\JamesMod\
       ccmod.json
       JAMESMOD.INI
       Data\TiberianDawn.dll
   ```
   `mod\install.ps1` does this for you:
   ```powershell
   powershell -ExecutionPolicy Bypass -File mod\install.ps1
   ```
3. In the game, go to **Options → Mods**, enable **JamesMod**, then start a skirmish.
4. Only **one** mod that replaces `TiberianDawn.dll` can be active at a time.

EA's modding support covers single-player skirmish and custom missions. DLL mods generally also work in the
single-player campaign.

---

## Configuration (`JAMESMOD.INI`)

```ini
[Game]
CreditsPercent=100     ; skirmish starting credits as % of the lobby setting
CreditsBonus=0         ; flat bonus on top

[AI]
AttackInterval=3       ; x (0.5-2 min) between attack checks   (EA default 3)
AttackDelay=5          ; x (0.5-2 min) before the first attack (EA default 5)
RefineryRatio=16       ; % of AI buildings that should be refineries
RefineryLimit=4
WarRatio=10
WarLimit=2
BarracksRatio=16
BarracksLimit=2
DefenseRatio=40
DefenseLimit=40
AARatio=14
AALimit=10
HelipadRatio=12
HelipadLimit=5
AirstripRatio=12
AirstripLimit=5
BaseSizeAdd=3
PowerSurplus=50
InfantryReserve=3000
```

New sections for waves, base planning, harvesters and the campaign will be added as those features land.

---

## Building

Requirements: **Visual Studio 2022** with *Desktop development with C++*, and a Windows 10/11 SDK. EA's README asks
for VS2017 and the Windows 8.1 SDK; you don't need those with the changes in this repo.

1. Open `SOURCECODE\CnCRemastered.sln`, accept the retarget prompt (v143 toolset, latest SDK) and choose
   **Release | Win32**. The game only loads 32-bit DLLs.
2. Build. The output is `SOURCECODE\bin\Win32\TiberianDawn.dll`.

Changes made so it builds and runs on a modern toolchain:

| Problem | Fix |
|---|---|
| `C2338 static_assert: Windows headers require the default packing option` | The game is built with 1-byte struct packing (`/Zp1`). Added `WINDOWS_IGNORE_PACKING_MISMATCH` to the preprocessor definitions, which matches how the original 8.1 SDK build behaved. |
| `RC1015 cannot open include file 'afxres.h'` | The `.rc` files now include `winres.h`, so MFC isn't needed. |
| Old C++ that the compiler rejects | *Conformance mode* set to **No**. |
| **Builds, then crashes on starting a game** | See the MMX register bug below. |

---

## Technical notes

### The MMX register bug (why VS2019+ builds crash)

If you build EA's source with a newer compiler, the DLL loads but the game crashes when a match starts. The logs show
`Failed to find RulesTypeClass type <>` and a crash in `HouseClass::Assign_Handicap` with a NULL `this`.

**Cause:** `MMX.ASM` (from 1996) has two routines that change callee-saved registers without restoring them:
- `Detect_MMX_Availability` runs `cpuid`, which overwrites `EBX`.
- `Init_MMX` uses `EBX` and `EDI`.

The x86 calling convention requires a function to preserve `EBX`, `ESI`, `EDI` and `EBP`. EA's VS2017 build
happened to save `EBX` one level up in `DLL_Startup`, which hid the bug. Newer compilers allocate registers
differently, so the corrupted `EBX` leaks back into the game's own code during `CNC_Init`. The game then reads
garbage (an empty rules name), skips `CNC_Config`, `CNC_Set_Multiplayer_Data` and `CNC_Start_Instance`, and crashes.

**Fix:** declare the registers on the procedures so MASM saves and restores them:
```asm
Detect_MMX_Availability proc C uses ebx
Init_MMX                proc C uses ebx edi
```
Applied to both `TIBERIANDAWN\MMX.ASM` and `REDALERT\MMX.ASM`.

### Debug tracing

`JAMESMOD_TRACE.LOG` in the mod folder records the first few calls the game makes into each `CNC_*` export,
which is useful for working out call order. It'll be switched off by default in a later version.

---

## Repository layout

```
SOURCECODE\            EA's released DLL source, with JamesMod changes (see MODIFICATIONS.md)
  TIBERIANDAWN\JMOD.H/.CPP   config loader and trace helper
mod\
  JamesMod\ccmod.json        mod manifest
  JamesMod\JAMESMOD.INI      default settings
  install.ps1                copies the build into your Mods folder
```

---

## Licence

The source code is released by Electronic Arts under the **GNU GPL v3 with additional terms**; see
[`LICENSE.md`](LICENSE.md). This is a **modified version** of that code and is not the original program. All
changes are listed in [`MODIFICATIONS.md`](MODIFICATIONS.md) and marked with `JamesMod` comments in the source.

Command & Conquer is a trademark of Electronic Arts Inc. This project is not affiliated with or endorsed by EA.
No game assets are included. You need your own copy of the game.
