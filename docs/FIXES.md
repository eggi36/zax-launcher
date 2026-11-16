# Fixes of the original's bugs

Zax-Launcher.dll can fix some bugs of the original Zax.exe while it runs, by patching the exe in memory as it does for
its developer switches; Zax.exe on disk is not touched. Each fix has a switch of its own, `/Fix Name` (repeatable;
`/Fix:Name` is the same), and a box on the start window's **Fixes** page, remembered in `[Fixes]` of
`Zax-Launcher.ini`. All are off unless given; the start window ticks `DPlaySlot100` by default (as the port's launcher
does), a bare command line gets none. Without a `/Fix` switch the DLL changes no byte for them.

The fixes follow the port (zax-redone, its `[Fix]` switches; the bugs, their addresses and evidence are in its
`docs/ORIGINAL_BUGS.md`, cited below as "OB"). The port reimplements the code; the launcher can only change a few bytes
of the original or send a short piece of it into the DLL ("cave") and back. So each fix here is a small patch of the
original's code that gives the port's result, not the port's code.

Every patch compares the original bytes at its sites first and stays off (a line in `Zax-Launcher.log`) when they
differ. Builds 1308 and 1309: all sites of the table lie below `0x59fc70` and have the same bytes in both (compared
byte for byte); only DrawHudCrash differs, because 1309 has that test already. Every enabled fix writes
`Fix <Name> enabled` into `Zax-Launcher.log`; some log what they did (named in the table).

## Survey: what can be done in the original exe

S: a byte patch or a small cave. M: more work (a C function in the DLL, a wrapper of a virtual slot). N: not sensibly
possible in the original exe.

| Fix (port's `[Fix]` key) | OB | Feasible | How (1308 addresses) | Done |
|---|---|---|---|---|
| MultiplayerCfg | 1.5 | S | `Display_Set640x480` `0x590f00`: `jmp` over the two INI writes (`0x590fc7`, 2 bytes) | yes |
| DisableMiniMap | 1.6 | S | `HudMiniMap_Draw` `0x508180`: a cave at `0x5082ad` (the single-player branch) ends the function when the layer's byte `+0x3c+0x168` is set | yes |
| SoundChannels | 1.19 | S | `Sound_LoadOptions` `0x57ee90`: the INI read at `0x57ef3c` goes to a DLL function that reads "Sounds Mixing Chanels" and then "Sounds Maxing Chanels" with the first as default; logs both | yes |
| PlayOnceAfterLoad | 1.20 | M | `CEntityBehaviorPlaySequenceOnce`: Create `0x4966d0` and CreateArray `0x496720` mark the speed `+0x18` with -1 (their `Object_InitFields` calls at `0x49670d`, `0x49677c`); `Update` `0x497010` (cave at `0x497024`) gives a still-marked object Begin's values (speed from `+0x14`, play ratio 1.0, "Stationary" cleared), as the port; logs each | yes |
| DrawHudCrash | 2.39 | S | `CZaxWorld::DrawHud` `0x59e730`: 1309's test of `World::CurrentLayer` `0x4f1050` as a cave at `0x59e856`; build 1309 has it already (the switch changes nothing there) | yes |
| CompanionSpot | 1.7 | S | `CompanionAI_PickSpot` `0x46ead0`: the store of the constant over the computed y (`0x46eb6a`, 3 bytes) becomes nops | yes |
| SmoothTurning | 1.8 | S | `CEntityBehaviorMoveToPos::Update` `0x430f20`: the `or ebx, 8` (`0x430f34`) becomes nops | yes |
| ShieldOverflow | 1.10 | M | `CShieldAI::Damage` `0x56da70` through its only reference, the vtable slot `0x5ffb14`: a wrapper; a cave in its "hit at or above the charge" branch (`0x56db90`) notes the scaled hit and the charge, the wrapper returns the part above the charge in the hit's own units, as the port; logs it | yes |
| PickupRefused | 1.11 | S | `CPickupAI::Collect` `0x53e110`: a cave at `0x53e346` ends the function when there is no alternate item (the original reads address 0 there) or it refuses too | yes |
| UseItems | 1.12 | S | `Part_UseItems` `0x426930`: the definition call of the search (`0x426a79`) answers -1 for an item already marked for deletion, so it is skipped | yes |
| OldPatrolSeed | 1.14 | S | the `COldBadPatrolAI` constructor inlined in Create `0x53a990` and CreateArray `0x53aa00`: the reseed calls (`0x53a9da`, `0x53aa6b`) become `add esp, 4` | yes |
| FidgetNext | 1.15 | S | `CFidgetData::PickFidget` `0x4a0680`, order 5: a cave at `0x4a0821` counts the index on | yes |
| DPlaySlot100 | 1.16 | S | the DirectPlay message handler `0x47ff50`: the slot search starts at 99 (`0x4801cf`: 100 → 99, `+0x904` → `+0x8f8`) | yes |
| AimCentre | 1.18 | S | `ControlConfig::AnalogueVector` `0x4708c0`: a cave at `0x4708e8` takes half the game frame's size (`[[0x713f14]+0x2a8]`, slots 45 and 39) instead of (320, 240); the two `fadds` at `0x470a73`/`0x470a8b` read the DLL's centre instead of the constants `0x5eff98`/`0x5eff9c` (which other code shares); logs the centre | yes |
| NoWriteBack | 4.8 | S | `Object_CreateFromFile` `0x4164c0`: `je` → `jmp` at `0x4165a4`, past `Object_WriteFile` | yes |
| QuitDuringMovie | 1.21 | S | `App_Dtor` `0x4cdcc0`: its `Zip_UnmountAll` call (`0x4cdcca`) goes to a DLL function that first closes every open movie of the Bink system `0x62eaf0` with `CloseMovie` `0x432080`; logs each | yes |
| GroundBlend | 1.1 | S | the plasma ground's 16-bit loop of `CPlasmaTileMap` (`0x541519`–`0x5415f0`): a cave at `0x541541` blends the two texture pixels with the port's rounded formula (`fix_blend565`) and stores the result as the original does | yes |
| AmplifyGreen | 1.3 | M | the two 16-bit Amplify Light spans `0x434280` (literal) and `0x434310` (fill) are replaced by DLL functions: red and blue from the game's own tables, green with its six bits (the port's `amplify565`) | yes |
| EdgeBlend | 1.2 | N | the blend is inlined in about 240 span functions (the unrolled 16-bit alpha and opacity spans for each width, their MMX versions, the Copy blitters); each would need its own replacement: that is the port's renderer, not a patch | no |
| GroundTgaAppend | 2.26 | N | needs the port's `GroundTga` (its own TGA reader for ground textures, a hardening switch) and a rebuilt ground texture list: a reimplemented subsystem, not a patch | no |
| Hardening | group 2 | N | about 30 guards across the parser, the models, the files and the network code, many inside reimplemented functions; matters only for modded or broken data | no |

18 of the 21 are done; the three left need the port.

## Evidence

All runs on 2026-11-16 in Windows VMs of one platform (zax-testtools' `vmtest.py --launcher --cleanup`), the original
Zax.exe build 1308 (German, no copy protection), 800 × 600 × 16 unless named; the test levels are the port's (its batch
O-1b) and the bug report's. Job ids in brackets (`-hhmmss-xxxx` short for `20261116-hhmmss-…`).

| Fix | Test | Without the switch (the bug) | With it |
|---|---|---|---|
| QuitDuringMovie | `/Movies`, Alt+F4 five seconds after the start, during the first movie | Zax.exe ends with `0xC000041D` (job `20261116-222322-ca33`) | exit code 0; log "movie 0 closed before the archives are unmounted" (`-222405-3734`) |
| PlayOnceAfterLoad | the bug report's level (a Psyonic Wave fired at three pedestals, quick save 1 s after the shot, quick load, Zax steps into the impact) | 5 s in the spot: Zax dead (health 0), the blue light still there (`-222520-2df5`) | health 100, standing; log "behaviour … resumes at its own speed (no Begin since the load)" (`-222623-e122`) |
| ShieldOverflow | a shield of charge 7 given to Zax, a turret's 20-point shots; the health every 0.5 s | 100, 100, 80, 60, …: the first hit absorbed whole (`-221556-0750`) | 100, 100, 87, 67, …: 13 of the first hit pass; log "hit 20, charge 7, 13 passes through" (`-221652-7fc8`) |
| PickupRefused | a refused "Fruit Single" in a `CSimultaneousAIsAI` beside a `CPassThroughTriggerAI`, Zax walks over it at full health | the pedestal is gone, nothing given (`-223435-20f5`) | the pedestal stays (`-223448-c37d`) |
| UseItems | three "Key, Stone", use 2, a check for 2 left that shows a marker | the marker shows: at most one key was used (`-221450-f99a`) | no marker (`-221504-d269`) |
| AimCentre | 800 × 600, the pistol given, the cursor moved to the screen's middle | the aiming ring lower right of Zax (`-221749-9c87`) | on Zax; log "the aiming centre is (400, 300)" (`-221830-67a8`) |
| CompanionSpot | a drone created next to Zax, its height on six shots 3 s apart | within 22 pixels of one line (`-221915-b432`) | spread over 110 pixels, above and below Zax (`-222017-b0cf`) |
| FidgetNext | a Korbo Slave with "Sequential Order", ten shots 0.5 s apart | two poses in turn (`-222120-ed43`) | several different fidgets (`-222259-9887`) |
| DisableMiniMap | `ACT 2/M10/10 Mining TargDens` (the field set), auto-map opened (M) | the map in the panel (`-223223-582b`) | the panel empty (`-223232-c82a`) |
| SoundChannels | Zax.CFG with `Sounds Maxing Chanels=5` (what the dialog writes for "Faster"), the sound dialog opened | "Best" pressed: 32 channels (`-224527-abf9`) | "Faster" pressed; log "\"Sounds Mixing Chanels\" gives 32, \"Sounds Maxing Chanels\" 5" (`-224527-3a07`) |
| MultiplayerCfg | `-connect 127.0.0.1` with an 800 × 600 Zax.CFG | the game at 640 × 480, Zax.CFG afterwards 640/480 (`-222638-123a`) | the game at 640 × 480, Zax.CFG afterwards 800/600 (`-222659-7ef6`) |
| NoWriteBack | the German language pack beside the game, a new game from the main menu | 4 definition files written below `Data` (`Targ Horn.txt`, `Jungle Creature Jaws.txt`, `Shield Level 1.txt`, `Zelon Gun.txt`; `-222412-a4f6`) | none (`-222451-18a8`) |
| GroundBlend | a test level with plasma ground, 800 × 600 × 16, one shot after loading; two runs without the switch for control | the two runs without it are the same picture (`-223232-1b16`, `-223311-1df6`) | 272,852 pixels differ, the ground lighter (mean red 66.1 → 70.3) and without the dark blotches (`-223346-7639`) |
| AmplifyGreen | the turret's shots (Amplify Light sprites); the DLL computes the first 20,000 Amplify Light pixels also with the original's tables | — | "of the first 20000 Amplify Light pixels 13966 got another green than the original's, 0 another red or blue" (`-224424-ac54`). Screenshots cannot show it: the shots' timing differs from run to run more than the green bit (as in the port) |
| DrawHudCrash | — | the crash is read from the code (OB 2.39), not seen in play; not reproduced | installed and played with all fixes (below); build 1309: "has the test already, nothing to change" |
| SmoothTurning, OldPatrolSeed | — | their effect needs the port's frame log (steering, the random generator's state), which the original exe has not | installed and played with all fixes (below); `ACT 3/M21/21 Small Volcanic` (the one level with `COldBadPatrolAI`) loads and runs with OldPatrolSeed [effect: confirmed by code, not measured] |
| DPlaySlot100 | — | needs a LAN game whose join depends on the heap (OB 1.16); not run | installed and played with all fixes (below) [effect: confirmed by code; a live LAN game with the launcher is still open] |

All 18 at once: `Act 1/M1M2/01 Main` with every `/Fix`, walked: 18 times "Fix … enabled", the game runs (build 1308:
`20261116-224812-f3c1`, the final build; build 1309, the Polish Zax.exe in the same folder: `-221316-3f6f`, 18 enabled,
DrawHudCrash as above). Without any switch: no fix line in the log; the test level's picture is the same as before the
fixes outside the area of Zax's idle animation, which differs between any two runs.

The start window: the **Fixes** page with the 18 boxes and their tooltips, `DPlaySlot100` ticked by default, the
shortcut target `/Fix DPlaySlot100` (`-223718-2f0e`).
