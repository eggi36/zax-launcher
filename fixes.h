/* The fixes of the original's bugs that Zax-Launcher.dll can make in the running Zax.exe (docs/FIXES.md): one switch
 * each, /Fix Name (repeatable; /Fix:Name is the same), off unless given. The start window's Fixes page offers them with
 * these labels and tooltips and remembers them in Zax-Launcher.ini ([Fixes] Name=0/1); there DPlaySlot100 is ticked by
 * default (on_in_window), the rest is not. Used by the exe (window, help) and the DLL (the switch names).
 * The hardening fixes (from FIX_FIRST_HARDENING on: guards against crashes on modded or broken data, nothing the shipped
 * game reaches) form a group as in the port: /Fix Hardening turns on all of them, and the Fixes page shows them behind
 * one "Hardening" box. */
#pragma once
#include "mini.h"

enum {
    FIX_MULTIPLAYER_CFG, FIX_DISABLE_MINIMAP, FIX_SOUND_CHANNELS, FIX_PLAY_ONCE_AFTER_LOAD, FIX_DRAW_HUD_CRASH,
    FIX_COMPANION_SPOT, FIX_SMOOTH_TURNING, FIX_SHIELD_OVERFLOW, FIX_PICKUP_REFUSED, FIX_USE_ITEMS, FIX_OLD_PATROL_SEED,
    FIX_FIDGET_NEXT, FIX_DPLAY_SLOT100, FIX_AIM_CENTRE, FIX_NO_WRITE_BACK, FIX_QUIT_DURING_MOVIE, FIX_GROUND_BLEND,
    FIX_AMPLIFY_GREEN, FIX_MENU_BACKGROUND, FIX_OGG_TO_WAV, FIX_GOTO_AI_CROSS, FIX_LISTENER_CENTRE,
    FIX_FIRST_HARDENING,
    FIX_AMPLIFY_EDGES = FIX_FIRST_HARDENING, FIX_GLYPH_RANGE, FIX_BEAM_PARTS, FIX_ITEM_DATA, FIX_EMPTY_LISTS,
    FIX_RLE_GET_COLOR, FIX_OFFSET_GET_COLOR, FIXES
};
#define FIX_HARDENING "Hardening"   /* /Fix Hardening: every fix from FIX_FIRST_HARDENING on */

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
    {"MenuBackground", "Menu background follows any new size",
     "The original rescales the menu background only when both the width and the height of the game's frame change; a "
     "size that changes one of them keeps the old picture. With this on either change rescales it. The original's "
     "640 x 480 and 800 x 600 change both, so in the original game this changes nothing you can see.", 0},
    {"OggToWav", "Missing .ogg music falls back to .wav",
     "The music list looks a missing .wav up as .ogg, but its branch meant for the other way round can never run. With "
     "this on a missing .ogg is looked up as .wav, e.g. for a mod's music.", 0},
    {"GoToAICross", "Debug drawing: GoTo AI's target cross red",
     "With the developer's debug drawing (/DebugDraw) the GoTo AI marks its moved target with a second cross that is "
     "meant to be red but drawn yellow. With this on it is red.", 0},
    {"ListenerCentre", "3D sound heard from the view's centre",
     "The original hears the world's sounds from (320, 240) of the view's top-left corner, the middle of a 640 x 480 "
     "view, which the game never has: at 800 x 600 (a view of 800 x 544 above the panel) that is 80 pixels left of and "
     "32 above its middle, so a sound at Zax is panned to the right; at 640 x 480 (640 x 424) 28 above it. With this on "
     "the listener is the view's middle.", 0},
    /* Hardening (FIX_FIRST_HARDENING on) */
    {"AmplifyEdges", "Amplify Light soft edges",
     "A sprite drawn with Amplify Light that has soft edges calls address 0 for them (a crash). With this on the edges "
     "are left out; nothing the game draws changes.", 0},
    {"GlyphRange", "Characters past a font's end",
     "Measuring a text reads the glyph one past a font's last. With this on such a character measures as the first "
     "glyph, as it is drawn.", 0},
    {"BeamParts", "Light beams on odd parts",
     "A light beam touching a part that is no entity reads through a missing pointer (a crash). With this on it does "
     "nothing.", 0},
    {"ItemData", "Projectiles without their data",
     "An item asked for a projectile value it has no data for reads address 0 (a crash). With this on the value is 0. "
     "The port's switch also covers the HUD and given items; the launcher only this part.", 0},
    {"EmptyLists", "Item lists without records",
     "A segmented item's renderer reads through a missing catalogue record or a record whose item did not load (a "
     "crash). With this on it finds no item (the game's own report, as for an item of the wrong kind).", 0},
    {"RleGetColor", "Colour reads of RLE frames",
     "Reading the colour of a drawn pixel of a 16-bit RLE frame reads far past a colour table. With this on it gives "
     "white.", 0},
    {"OffsetGetColor", "Offset frame colour reads",
     "Reading a colour through an offset frame moves the colour pointer instead of y and writes to the wrong place. "
     "With this on y moves. No code of the game reaches it.", 0},
};

/* The Fixes page's group box for the hardening fixes */
static const char HARDENING_LABEL[] = "Hardening (matters only for modded or broken data)";
static const char HARDENING_TIP[] =
    "Guards against crashes on unusual or modded data; the shipped game reaches none of them. The box ticks or unticks "
    "every hardening fix (/Fix Hardening when all are on); the button beside it shows them one by one.";

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

/* Whether the command line turns fix i on: its own name, or /Fix Hardening for a hardening fix */
static inline int fix_on(const char *args, int i)
{
    return has_fix(args, FIX[i].name) || (i >= FIX_FIRST_HARDENING && has_fix(args, FIX_HARDENING));
}
