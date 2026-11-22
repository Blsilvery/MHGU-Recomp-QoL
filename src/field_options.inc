// These options migrate the six codes in the player's MHGU 1.4.0 cheat file.
// Recomp 0.17.9 interprets code reached by an interior Forge hook. Raw Forge
// instruction writes would leave the precompiled function unchanged.
#define MAP_CHECK 0x61BAD0
#define BOSS_CHECK 0x61CC94
#define STYLE_ART_SLOTS 0x2778AC
#define QUEST_OBJECT 0x188AD90
#define END_DELAY_OWNER 0x2126498
#define QUEST_END_STATE 0xEC
#define END_DELAY_OBJECT 0x3830
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
static bool g_three_arts = true, g_carry_items = true;
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
    const volatile u32 *code = (const volatile u32 *)(g_main + offset);
    if (!original_tail || code[0] != first || code[1] != second) {
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

static void update_field_shortcuts(void) {
    if (!g_main) return;
    if (g_finish_quest && forge_input_isDown(BUTTON_PLUS) && forge_input_isDown(BUTTON_DOWN)) {
        u32 quest = *(const u32 *)(g_main + QUEST_OBJECT);
        if (quest) *(u8 *)(quest + QUEST_END_STATE) = 0x29;
    }
    if (g_skip_end_delay && forge_input_isDown(BUTTON_MINUS) && forge_input_isDown(BUTTON_UP)) {
        u32 owner = *(const u32 *)(g_main + END_DELAY_OWNER);
        if (!owner) return;
        u32 delay = *(const u32 *)(owner + END_DELAY_OBJECT);
        if (delay) *(u16 *)(delay + END_DELAY_FRAMES) = 0;
    }
}

static void render_field_options(void) {
    if (!g_main || !igTreeNode_Str("Map, Hunter Arts and quest shortcuts")) return;
    if (g_map_hooked) igCheckbox("Automatically display map", &qol_field_routes.map_enabled);
    if (g_boss_hooked) igCheckbox("Mark large monsters", &qol_field_routes.boss_enabled);
    if (g_art_slots_hooked) igCheckbox("Three Hunter Arts for all styles", &g_three_arts);
    if (g_carry_items_hooked) igCheckbox("Put transport items in the item pouch", &g_carry_items);
    if (!g_map_hooked || !g_boss_hooked || !g_art_slots_hooked || !g_carry_items_hooked)
        igTextUnformatted("Some options are unavailable. Check log.txt for overlapping cheats or mods.", NULL);
    igCheckbox("Finish quest immediately (Start + D-pad Down)", &g_finish_quest);
    igCheckbox("Skip end-of-quest delay (Select + D-pad Up)", &g_skip_end_delay);
    igTreePop();
}
