// These options migrate the six codes in the player's MHGU 1.4.0 cheat file.
// Recomp 0.17.9 interprets code reached by an interior Forge hook. Raw Forge
// instruction writes would leave the precompiled function unchanged.
#define MAP_CHECK 0x61BAD0
#define BOSS_CHECK 0x61CC94
#define STYLE_ART_SLOTS 0x2778AC
#define QUEST_OBJECT 0x188AD90
#define QUEST_END_STATE 0xEC
// The quest-end wait, as the cheat found it: owner+0x3830, then +0xBC (u16 frames). The owner (main+0x2126498, made at
// main+B87A88) is the game's stack of scene units: its depth at +0x3820 and up to 8 units from +0x3828
// (main+12730 returns the top one; popped levels are deleted and their slots cleared, main+B883C8). So +0x3830 is
// not a field of the quest-end wait but whatever unit is the third level at the time. The cheat wrote to it on every
// frame the buttons were held, in any scene: after the quest, in the village or while the game saved, it cleared a
// halfword of some other unit, which could crash the game later (reported during the save after the quest).
#define END_DELAY_OWNER 0x2126498
#define SCENE_DEPTH 0x3820
#define SCENE_UNITS 0x3828
#define END_DELAY_LEVEL 2          // (owner + SCENE_UNITS + 4 * 2 = owner+0x3830)
#define END_DELAY_FRAMES 0xBC

enum { BUTTON_PLUS = 10, BUTTON_MINUS = 11, BUTTON_UP = 13, BUTTON_DOWN = 15 };

// Layout shared with field_options.S.
struct {
    u32 map_visible, map_hidden, boss_skill_query, boss_continue;
    bool map_enabled, boss_enabled;
} qol_field_routes = {0, 0, 0, 0, true, true};
_Static_assert(__builtin_offsetof(__typeof__(qol_field_routes), map_enabled) == 16 &&
               __builtin_offsetof(__typeof__(qol_field_routes), boss_enabled) == 17,
               "field option assembly layout");

extern void qol_map_check(void);
extern void qol_boss_check(void);

static ForgeHook g_map_hook, g_boss_hook, g_art_slots_hook, g_carry_items_hook;
static bool g_map_hooked, g_boss_hooked, g_art_slots_hooked, g_carry_items_hooked;
static bool g_three_arts = false, g_carry_items = true; // (three Hunter Arts change the balance: off by default)
static bool g_finish_quest = true, g_skip_end_delay = true;

static u32 art_slots_hook(u32 player, u32 style) {
    if (g_three_arts) return style <= 5 ? 3 : 0;
    return ORIGINAL(Deliverable, g_art_slots_hook)(player, style);
}

static u32 carry_items_hook(u32 items, u32 item) {
    return g_carry_items ? 0 : ORIGINAL(IsCarried, g_carry_items_hook)(items, item);
}

static bool field_hook(ForgeHook *state, u32 offset, void *detour,
                       u32 first, u32 second, const char *label, bool original_tail) {
    // A function's entry another plugin hooked is shared (code_is); the map and marker hooks sit inside functions and
    // their routes (field_options.S) rebuild the game's own instructions there, so another hook there leaves them off.
    bool inside = offset == MAP_CHECK || offset == BOSS_CHECK;
    const volatile u32 *code = (const volatile u32 *)(g_main + offset);
    bool same = inside ? code[0] == first && code[1] == second : code_is(offset, first, second);
    if (!original_tail || !same) {
        forge_log(LOG_WARN, "personal-gathering: %s unavailable; original instructions are already changed", label);
        return false;
    }
    bool installed = hook(state, offset, detour);
    forge_log(installed ? LOG_INFO : LOG_WARN, "personal-gathering: %s %s",
              label, installed ? "ready" : "hook failed");
    return installed;
}

static void init_field_options(void) {
    qol_field_routes.map_visible = g_main + 0x61BAD8;
    qol_field_routes.map_hidden = g_main + 0x61BBE4;
    qol_field_routes.boss_skill_query = g_main + 0x2864C4;
    qol_field_routes.boss_continue = g_main + 0x61CC9C;
    g_map_hooked = field_hook(&g_map_hook, MAP_CHECK, qol_map_check,
                              0xDA000043, 0xE59820A4, "automatic map display", true);
    g_boss_hooked = field_hook(&g_boss_hook, BOSS_CHECK, qol_boss_check,
                               0xEBF1A60A, 0xE3A05008, "large monster markers", true);
    g_art_slots_hooked = field_hook(&g_art_slots_hook, STYLE_ART_SLOTS, art_slots_hook,
                                    0xE3510005, 0x83A00000, "three Hunter Arts for all styles",
                                    *(const u32 *)(g_main + 0x2778C0) == 0xE7900101);
    g_carry_items_hooked = field_hook(&g_carry_items_hook, IS_CARRIED, carry_items_hook,
                                      0xE3000839, 0xE0411000, "transport items use the item pouch",
                                      *(const u32 *)(g_main + 0x1943CC) == 0x33000001);
}

static bool g_skip_held; // (Select + Up was down last frame: the wait is skipped once per press)

// Both shortcuts act only on a quest (the game's own check, main+3F8634: game modes 5, 6 and 8). Outside one there is
// no quest to finish and no wait to skip, and the objects they write to belong to other scenes.
static void update_field_shortcuts(void) {
    if (!g_main) return;
    bool skip = g_skip_end_delay && forge_input_isDown(BUTTON_MINUS) && forge_input_isDown(BUTTON_UP);
    bool skip_pressed = skip && !g_skip_held;
    g_skip_held = skip;
    bool finish = g_finish_quest && forge_input_isDown(BUTTON_PLUS) && forge_input_isDown(BUTTON_DOWN);
    if ((!finish && !skip_pressed) || !camp_in_quest()) return;
    if (finish) {
        u32 quest = *(const u32 *)(g_main + QUEST_OBJECT);
        if (quest) *(u8 *)(quest + QUEST_END_STATE) = 0x29;
    }
    if (skip_pressed) {
        // Once per press, not on every frame held: the scene can change under held buttons (the wait ends at once).
        u32 owner = *(const u32 *)(g_main + END_DELAY_OWNER);
        if (!owner || *(const u32 *)(owner + SCENE_DEPTH) <= END_DELAY_LEVEL) return;
        u32 unit = *(const u32 *)(owner + SCENE_UNITS + 4 * END_DELAY_LEVEL);
        if (!unit) return;
        u16 *frames = (u16 *)(unit + END_DELAY_FRAMES);
        // (its class logged for the record: a later version can check it instead of the quest and the depth)
        forge_log(LOG_INFO, "personal-gathering: skip end delay: %u frames (scene depth %u, unit vtable main+%x)",
                  *frames, *(const u32 *)(owner + SCENE_DEPTH), *(const u32 *)unit - g_main);
        if (*frames) *frames = 0;
    }
}
