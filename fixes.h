/* The fixes of the original's bugs that Zax-Launcher.dll can make in the running Zax.exe (docs/FIXES.md): one switch
 * each, /Fix Name (repeatable; /Fix:Name is the same), off unless given. The start window's Fixes page offers them with
 * these labels and tooltips and remembers them in Zax-Launcher.ini ([Fixes] Name=0/1); there DPlaySlot100 is ticked by
 * default (on_in_window), the rest is not. Used by the exe (window, help) and the DLL (the switch names). */
#pragma once
#include "mini.h"

enum {
    FIX_MULTIPLAYER_CFG, FIX_DISABLE_MINIMAP, FIX_SOUND_CHANNELS, FIX_PLAY_ONCE_AFTER_LOAD, FIX_DRAW_HUD_CRASH,
    FIX_COMPANION_SPOT, FIX_SMOOTH_TURNING, FIX_SHIELD_OVERFLOW, FIX_PICKUP_REFUSED, FIX_USE_ITEMS, FIX_OLD_PATROL_SEED,
    FIX_FIDGET_NEXT, FIX_DPLAY_SLOT100, FIX_AIM_CENTRE, FIX_NO_WRITE_BACK, FIX_QUIT_DURING_MOVIE, FIX_GROUND_BLEND,
    FIX_AMPLIFY_GREEN, FIXES
};

static const struct {
    const char *name, *label, *tip;
    int on_in_window;
} FIX[FIXES] = {
    {"MultiplayerCfg", "LAN games keep the resolution in Zax.CFG",
     "A LAN game still switches to 640 x 480, but no longer writes that into Zax.CFG: your resolution is back at the "
     "next start.", 0},
    {"DisableMiniMap", "Obey levels' \"Disable Mini-Map\" field",
     "In the original this level field does nothing and the map always shows. With this on, some original levels lose "
     "the map (10 of the 112, among them the Targ Dens and Inside Ship): the auto-map shows an empty plate. The "
     "multiplayer score board is not affected.", 0},
    {"SoundChannels", "Remember the sound channel choice",
     "The sound dialog saves its channel choice under a key the start never reads, so the original forgets it. With "
     "this on the start reads that key.", 0},
    {"PlayOnceAfterLoad", "Saved animations resume at their speed",
     "A sequence played once and saved while it plays resumes after loading at its own speed, not at one from stale "
     "memory (e.g. a Psyonic Wave's damage area that never ends after a quick load).", 0},
    {"DrawHudCrash", "No crash at the HUD's direction pointer",
     "The null test that the game's later build 1309 has: the pointer is skipped when player 0 has no current layer. "
     "Build 1308 only (1309 always tests). The crash is read from the code, not seen in play.", 0},
    {"CompanionSpot", "Companions keep their computed spot",
     "The original throws away the height of the spot computed for a companion, so companions line up near Zax's own "
     "line. With this on they stand where the formation puts them, also well above and below Zax.", 0},
    {"SmoothTurning", "Obey \"Smooth Turning\" off for chasers",
     "The original ignores this level field when an enemy chases. Most enemies in the original levels have it switched "
     "off, so with this on nearly all chasers turn differently than in the original.", 0},
    {"ShieldOverflow", "Shield lets excess damage through",
     "In the original the last bit of a shield's charge absorbs a hit of any size. With this on the damage above the "
     "charge goes through. A balance change: the game gets harder.", 0},
    {"PickupRefused", "Refused pickups stay on the ground",
     "A pickup that nobody took no longer disappears. Only modded levels can reach this case.", 0},
    {"UseItems", "Using several items uses different ones",
     "Using n items that have no count uses n different items, not the same one n times. The original levels do not "
     "reach this case.", 0},
    {"OldPatrolSeed", "Old patrol AI does not reset randomness",
     "In the original this AI resets the random generator to a fixed value when it is created, so one original level "
     "repeats the same random numbers at every start. With this on they differ from start to start.", 0},
    {"FidgetNext", "Fidget animation order 5 advances",
     "A fidget order that the original levels do not use gets stuck between two animations. With this on it goes "
     "through its list.", 0},
    {"DPlaySlot100", "LAN: a new player's slot stays in range",
     "A new player's first slot lies one behind its array in the original, and what stands behind it in memory decides "
     "whether the player can join. With this on the slot is the array's last. Ticked by default in this window.", 1},
    {"AimCentre", "Aiming centre follows the game's size",
     "Above 640 x 480 the original takes (320, 240) as the centre of the aiming stick, so the aiming ring sits off to "
     "the lower right with the cursor in the middle. With this on the centre is the screen's.", 0},
    {"NoWriteBack", "Do not rewrite definition files on load",
     "With a language pack, or for a mod's own files, the original writes definition files it has just read back below "
     "Data when their folder exists. With this on they are left alone; way point files and saves are still written.", 0},
    {"QuitDuringMovie", "No crash when closing during a movie",
     "Closing the game's window while a startup movie plays crashes the original as it exits: it unmounts its data "
     "files before it closes the movie. With this on the movie is closed first and the game ends normally.", 0},
    {"GroundBlend", "Smoother 16-bit plasma ground",
     "16-bit colour only. The plasma ground's blend mixes every colour channel with rounding, so the dark blotches of "
     "the original go away and the blend reaches its second texture.", 0},
    {"AmplifyGreen", "Exact green in Amplify Light (16-bit)",
     "16-bit colour only. Amplify Light keeps the lowest green bit that the original drops.", 0},
};

/* Whether the command line asks for the fix: /Fix Name or /Fix:Name, case-insensitive, a whole token (text inside a
 * quoted value is no switch) */
static inline int has_fix(const char *args, const char *name)
{
    int n = lstrlenA(name);
    const char *e;
    for (const char *t = next_token(args, &e); t; t = next_token(e, &e)) {
        if (e - t == 5 + n && str_nieq(t, "/Fix:", 5) == 0 && str_nieq(t + 5, name, n) == 0)
            return 1;
        if (!token_is(t, e, "/Fix"))
            continue;
        const char *v = next_token(e, &e);
        if (!v)
            return 0;
        if (*v == '"' && e - v == n + 2 && str_nieq(v + 1, name, n) == 0)
            return 1;
        if (e - v == n && str_nieq(v, name, n) == 0)
            return 1;
    }
    return 0;
}
