enum {
    CAMP_CAT_TALK = 0x24b5bc,
    CAMP_SET_NPC_STATE = 0x249ff8,
    CAMP_IS_QUEST = 0x3f8634,
    CAMP_ALLOC_BOX = 0xe840,
    CAMP_INIT_BOX = 0x712584,
    CAMP_ATTACH_WINDOW = 0x3ea218,
    CAMP_FIND_RESOURCE = 0x152960,
    CAMP_RELEASE_RESOURCE = 0x887660,
    CAMP_GMD_TYPE = 0xb84140,
    CAMP_GMD_STRING = 0xb84114,
    CAMP_ARC_TYPE = 0x2124df0,
    CAMP_VILLAGE_TEXT = 0x55bc7c,
    CAMP_BANK_TEXT = 0x55bac0,
    CAMP_BOX_UPDATE = 0x713db8,
    CAMP_BOX_GET_CHILD = 0x65cb3c,
    CAMP_RESOURCE_MANAGER = 0x2126a64,
    CAMP_WINDOW_MANAGER = 0x1899458,
    CAMP_UNIT_ROOT = 0x2126f48,
    CAMP_SYSTEM = 0x188ca48,
    CAMP_INTERACTION_CONTROL = 0x188ad90,
    CAMP_HUD_UPDATE = 0x66601c,  // the quest HUD (health, stamina, clock, party): per-frame update (hud)
    CAMP_PANEL_SHOW = 0x56df00,  // a window's child GUI: show it unless shown (panel)
    CAMP_PANEL_HIDE = 0x56e04c,  // ...hide it if shown (panel)
    CAMP_HUD_SLOT = 0x150,
    CAMP_BOX_SLOT = 0x160,
    CAMP_BOARD_SLOT = 0x16c,
    CAMP_MESSAGE_COUNT = 42,
    CAMP_WAIT_LIMIT = 120,
    CAMP_STYLE_ROW = 7,
    CAMP_HUD_HIDDEN_MAX = 64,
    // A window (the quest HUD and the item box share the base class): its child GUIs, and each child's shown flag
    WINDOW_CHILD_COUNT = 0x30,
    WINDOW_CHILDREN = 0x34,
    PANEL_SHOWN = 0x18,
};

typedef void *(*CampLoadResource)(void *, void *, const char *, u32);
typedef const char *(*CampVillageText)(void *, u32, u32);
typedef const char *(*CampBankText)(void *, u32, u32, u32);

static ForgeHook g_camp_talk_hook, g_camp_village_text_hook, g_camp_bank_text_hook, g_camp_box_update_hook,
    g_camp_hud_hook;
// The quest HUD's panels hidden while the camp box is open, to show again once it closes
static u8 *g_camp_hud_hidden[CAMP_HUD_HIDDEN_MAX];
static u32 g_camp_hud_hidden_count;
static bool g_camp_ready;
static const char g_camp_languages[][4] = {"eng", "fre", "spa", "ger", "ita", "chT", "chS"};
static struct {
    u8 *npc;
    u8 *manager;
    void *window;
    void *archive;
    void *messages[CAMP_MESSAGE_COUNT];
    u32 locale;
    u32 waits;
    bool creating;
    bool closing;
    bool texts_loaded;
} g_camp;

static u8 *camp_global(u32 offset) {
    return *(u8 **)(g_main + offset);
}

static bool camp_in_quest(void) {
    u8 *system = camp_global(CAMP_SYSTEM);
    return system && FN(u32 (*)(u8 *), CAMP_IS_QUEST)(system) == 1;
}

static u32 camp_locale(u8 *system) {
    u32 lang = system ? system[0xa5] : 1;
    if (lang >= 1 && lang <= 5) return lang - 1;
    if (lang == 7 || lang == 8) return lang - 2;
    return 0;
}

static void camp_release_resources(void) {
    for (u32 i = 1; i < CAMP_MESSAGE_COUNT; i++) {
        if (g_camp.messages[i])
            FN(void (*)(void *), CAMP_RELEASE_RESOURCE)(g_camp.messages[i]);
        g_camp.messages[i] = NULL;
    }
    if (g_camp.archive) FN(void (*)(void *), CAMP_RELEASE_RESOURCE)(g_camp.archive);
    g_camp.archive = NULL;
    g_camp.texts_loaded = false;
}

static CampLoadResource camp_resource_loader(void *manager) {
    return (CampLoadResource)(*(void ***)manager)[0x30 / 4];
}

static bool camp_has_layout(u32 id) {
    void *resource = FN(void *(*)(void *, u32, void *), CAMP_FIND_RESOURCE)(NULL, id, NULL);
    if (!resource) return false;
    FN(void (*)(void *), CAMP_RELEASE_RESOURCE)(resource);
    return true;
}

static bool camp_load_archive(void *resources, u32 locale) {
    if (g_camp.archive && g_camp.locale == locale) return true;
    camp_release_resources();
    g_camp.locale = locale;
    char path[] = "eng\\arc\\village\\common_eng";
    for (u32 i = 0; i < 3; i++) {
        path[i] = g_camp_languages[locale][i];
        path[sizeof(path) - 4 + i] = g_camp_languages[locale][i];
    }
    // The resource manager's explicit load with flags 3 publishes native
    // archive entries in the quest. Opening through sArc with flags 2 did not.
    g_camp.archive = camp_resource_loader(resources)(resources, (void *)(g_main + CAMP_ARC_TYPE), path, 3);
    return g_camp.archive != NULL;
}

// The village message banks in common_<lang>.arc, by the index the village text functions use (0 is the resident
// bank). The village archive is loaded first (camp_load_archive), so these are the archive's own entries: with a
// translation patch that replaces the archive (the Korean patch replaces common_eng.arc), its text.
static const char *const g_camp_tables[CAMP_MESSAGE_COUNT] = {
    NULL, "GUI\\06_msg\\QuestCounter", "table\\LobbyMsg", "table\\LobbyMenuMsg", "table\\LbHelp",
    "table\\OtBoardMsg", "table\\GuildCardMsg", "table\\GC_Title_1", "table\\GC_Title_2", "table\\GC_Medal",
    "table\\GC_background", "table\\GuestHunterMsg", "table\\guestQuestData", "table\\EqShop",
    "table\\LbShop_EqMakeMsg", "table\\ItemShop", "table\\LbShop_Eq_EqMakeMsg", "table\\LbShop_OtEqMsg",
    "table\\LbShop_OtEmployMsg", "table\\Kitchen", "table\\Lb_AirshipMsg", "table\\Lb_PostMsg",
    "table\\Lb_ItemBoxMsg", "table\\Lb_ArenaCounterMsg", "table\\Lb_MonnyanMsg", "table\\Lb_OtDoujoMsg",
    "table\\Lb_TradeShopMsg", "table\\Room_s", "table\\Lb_GalleryMsg", "table\\Lb_PetMsg",
    "table\\Lb_MakaRenkinMsg", "table\\Lb_HeroBlade", "table\\Lb_CaveMsg", "table\\Lb_AshiyuMsg",
    "table\\Lb_RyurekiinMsg", "table\\Lb_KarisutaMsg", "table\\CharMakeMsg", "table\\Lb_LaboratoryMsg",
    "table\\researchReinforce", "GUI\\06_msg\\NetworkVillage", "table\\KeyboardMsg", "table\\ResultMsg",
};

static void *camp_load_message(void *resources, void *type, const char *path) {
    void *message = camp_resource_loader(resources)(resources, type, path, 3);
    if (message && !*(u32 *)((u8 *)message + 0x88)) { // (no strings: not there)
        FN(void (*)(void *), CAMP_RELEASE_RESOURCE)(message);
        message = NULL;
    }
    return message;
}

// "<lang>\<table>_<lang>", the bank's path inside the village archive
static void camp_table_path(char *path, u32 size, u32 locale, u32 index) {
    const char *lang = g_camp_languages[locale], *table = g_camp_tables[index];
    u32 at = 0;
    for (u32 i = 0; i < 3 && at + 1 < size; i++) path[at++] = lang[i];
    if (at + 1 < size) path[at++] = '\\';
    while (*table && at + 1 < size) path[at++] = *table++;
    if (at + 1 < size) path[at++] = '_';
    for (u32 i = 0; i < 3 && at + 1 < size; i++) path[at++] = lang[i];
    path[at] = 0;
}

// Each bank from the village archive the game loaded, so a translation patch's text shows. The mod's copies of the
// original banks (wheelpl\qolcamp, from tools/make_arcs.py) are kept for a bank the archive doesn't give.
static bool camp_load_messages(void *resources, u32 locale) {
    if (!g_camp.texts_loaded) {
        void *type = FN(void *(*)(void), CAMP_GMD_TYPE)();
        char copy[] = "wheelpl\\qolcamp\\eng_00";
        for (u32 i = 0; i < 3; i++) copy[sizeof(copy) - 7 + i] = g_camp_languages[locale][i];
        u32 native = 0, copies = 0;
        for (u32 i = 1; i < CAMP_MESSAGE_COUNT; i++) {
            char path[64];
            camp_table_path(path, sizeof path, locale, i);
            void *message = camp_load_message(resources, type, path);
            if (message) {
                native++;
            } else {
                copy[sizeof(copy) - 3] = '0' + i / 10;
                copy[sizeof(copy) - 2] = '0' + i % 10;
                message = camp_load_message(resources, type, copy);
                if (message) copies++;
            }
            g_camp.messages[i] = message;
        }
        forge_log(LOG_INFO, "personal-gathering: camp texts (%s): %u from the village archive, %u from the mod's copies",
                  g_camp_languages[locale], native, copies);
        if (g_camp.messages[22]) note_game_text(FN(const char *(*)(void *, u32), CAMP_GMD_STRING)(g_camp.messages[22], 0));
        g_camp.texts_loaded = true;
    }
    return g_camp.messages[22] && g_camp.messages[2] &&
           g_camp.messages[3] && g_camp.messages[4] && g_camp.messages[36];
}

static bool camp_owns_text(void) {
    return (g_camp.window || g_camp.creating) && camp_global(CAMP_WINDOW_MANAGER) == g_camp.manager;
}

static const char *camp_fallback_text(u32 category, u32 index) {
    if (category >= CAMP_MESSAGE_COUNT || !g_camp.messages[category] || !camp_owns_text()) return NULL;
    return FN(const char *(*)(void *, u32), CAMP_GMD_STRING)(g_camp.messages[category], index);
}

static const char *camp_village_text(void *owner, u32 category, u32 index) {
    const char *text = ORIGINAL(CampVillageText, g_camp_village_text_hook)(owner, category, index);
    if (text) note_game_text(text);
    return text ? text : camp_fallback_text(category, index);
}

static const char *camp_bank_text(void *owner, u32 bank, u32 category, u32 index) {
    const char *text = ORIGINAL(CampBankText, g_camp_bank_text_hook)(owner, bank, category, index);
    if (text) note_game_text(text);
    if (text) return text;
    if (bank == 1) return camp_fallback_text(category, index);
    // The native item-box preview uses the character creation text.
    if (bank == 5 && category == 2) return camp_fallback_text(36, index);
    return NULL;
}

static void **camp_window_slot(u8 *manager) {
    return (void **)(manager + CAMP_BOX_SLOT);
}

static bool camp_owns_item_box(const u8 *box) {
    return box && g_camp.window == box && camp_global(CAMP_WINDOW_MANAGER) == g_camp.manager &&
           *camp_window_slot(g_camp.manager) == box;
}

#ifdef PERSONAL_RUNTIME_PROBE
// Logs which of the box's child GUIs are shown (PANEL_SHOWN) and where their root instances sit, once the set of
// shown children or the box state has stayed the same for half a second (so slide-in animations have finished).
static void camp_probe_children(u8 *box) {
    static u32 last_key, settle;
    u32 count = *(u32 *)(box + WINDOW_CHILD_COUNT);
    u8 **children = *(u8 ***)(box + WINDOW_CHILDREN);
    if (!children) return;
    u32 key = *(u32 *)(box + 0x40) * 0x9e3779b1u;
    for (u32 i = 0; i < count && i < 32; i++)
        if (children[i] && children[i][PANEL_SHOWN]) key ^= 1u << i;
    if (key != last_key) {
        last_key = key;
        settle = 30;
        return;
    }
    if (!settle || --settle) return;
    forge_log(LOG_INFO, "camp-probe: state %u row %u, %u children", *(u32 *)(box + 0x40), *(u32 *)(box + 0x70),
              count);
    for (u32 i = 0; i < count; i++) {
        u8 *child = children[i];
        if (!child || !child[PANEL_SHOWN]) continue;
        u8 *gui = *(u8 **)(child + 0x24);
        u8 *root = gui && *(u32 *)(gui + 0x298) && *(u8 ***)(gui + 0xf8) ? (*(u8 ***)(gui + 0xf8))[0] : NULL;
        if (!root) {
            forge_log(LOG_INFO, "camp-probe:   child %u shown, no root instance", i);
            continue;
        }
        forge_log(LOG_INFO, "camp-probe:   child %u x %d y %d (%u instances, flags %x)", i,
                  (int)*(float *)(root + 0x90), (int)*(float *)(root + 0x94), *(u32 *)(gui + 0x298),
                  *(u32 *)(gui + 0x280));
    }
}
#endif

static void camp_hide_hud(void);

static void camp_item_box_update(u8 *box) {
    bool owned = camp_owns_item_box(box);
    u32 *pressed = NULL;
    if (owned && *(u32 *)(box + 0x40) == 1 && *(u32 *)(box + 0x70) == CAMP_STYLE_ROW) {
        // The village editor cannot equip live Hunter Arts and can request
        // a player rebuild while a quest is running. Keep its row disabled.
        u32 pad = GLOBAL(GOT_PAD);
        if (pad && (*(u32 *)(pad + PAD_PRESSED) & PAD_A)) {
            pressed = (u32 *)(pad + PAD_PRESSED);
            *pressed &= ~PAD_A;
            FN(Sound, UI_SOUND)(0, 3);
        }
    }

    ORIGINAL(void (*)(u8 *), g_camp_box_update_hook)(box);
    if (pressed) *pressed |= PAD_A;
    if (owned && camp_owns_item_box(box)) {
#ifdef PERSONAL_RUNTIME_PROBE
        camp_probe_children(box);
#endif
        camp_hide_hud();
    }

    // This child is the full-box root menu; its Hunter Arts row stays greyed out. The box keeps its village layout,
    // with the quest HUD hidden (camp_hide_hud).
    if (!owned || !camp_owns_item_box(box) || *(u32 *)(box + 0x40) != 1 ||
        !*(u32 *)(box + WINDOW_CHILD_COUNT) || !*(u32 *)(box + WINDOW_CHILDREN)) return;
    u8 *menu = FN(u8 *(*)(void *, u32), CAMP_BOX_GET_CHILD)(box, 0);
    if (menu) menu[0xa4 + CAMP_STYLE_ROW] = 0;
}

static bool camp_hud_hid(const u8 *panel) {
    for (u32 i = 0; i < g_camp_hud_hidden_count; i++)
        if (g_camp_hud_hidden[i] == panel) return true;
    return false;
}

// The item box's submenus keep their village layout, much of it at the top left, where the quest HUD (health,
// stamina, clock, party) is drawn over them. While the camp box is open the HUD's panels are hidden, as in the
// village (from the box's update); the HUD's own update shows the ones hidden here again once the box has closed,
// and the panels the HUD itself had hidden stay hidden.
static void camp_hide_hud(void) {
    u8 *hud = g_camp.manager ? *(u8 **)(g_camp.manager + CAMP_HUD_SLOT) : NULL;
    u8 **panels = hud ? *(u8 ***)(hud + WINDOW_CHILDREN) : NULL;
    if (!panels) return;
    u32 count = *(u32 *)(hud + WINDOW_CHILD_COUNT), hidden = 0;
    for (u32 i = 0; i < count; i++) {
        u8 *panel = panels[i];
        if (!panel || !panel[PANEL_SHOWN]) continue;
        FN(void (*)(u8 *), CAMP_PANEL_HIDE)(panel);
        if (!camp_hud_hid(panel) && g_camp_hud_hidden_count < CAMP_HUD_HIDDEN_MAX)
            g_camp_hud_hidden[g_camp_hud_hidden_count++] = panel;
        hidden++;
    }
    if (hidden) forge_log(LOG_INFO, "personal-gathering: camp box hid %u of %u HUD panels", hidden, count);
}

static void camp_hud_update(u8 *hud) {
    ORIGINAL(void (*)(u8 *), g_camp_hud_hook)(hud);
    u32 count = *(u32 *)(hud + WINDOW_CHILD_COUNT);
    u8 **panels = *(u8 ***)(hud + WINDOW_CHILDREN);
    if (panels && g_camp_hud_hidden_count && !(g_camp.window && camp_owns_item_box(g_camp.window))) {
        u32 shown = 0;
        for (u32 i = 0; i < count; i++) {
            u8 *panel = panels[i];
            if (!panel || !camp_hud_hid(panel) || panel[PANEL_SHOWN]) continue;
            FN(void (*)(u8 *), CAMP_PANEL_SHOW)(panel);
            shown++;
        }
        forge_log(LOG_INFO, "personal-gathering: camp box closed, %u HUD panels shown again", shown);
        g_camp_hud_hidden_count = 0;
    }
}

static void camp_clear_interaction(void) {
    g_camp.npc = NULL;
    g_camp.manager = NULL;
    g_camp.window = NULL;
    g_camp.waits = 0;
    g_camp.closing = false;
}

static void camp_finish(u8 *npc, u8 *manager) {
    FN(void (*)(u8 *, u32), CAMP_SET_NPC_STATE)(npc, 0);
    u8 *control = camp_global(CAMP_INTERACTION_CONTROL);
    u8 *system = camp_global(CAMP_SYSTEM);
    if (control) control[0x16ac] = 0;
    if (system) system[0xa0] = 0;
    if (manager) manager[0x1a2] = 0;
    camp_clear_interaction();
}

static bool camp_open_window(u8 *root) {
    g_camp.creating = true;
    void *object = FN(void *(*)(u32, u32), CAMP_ALLOC_BOX)(0x1190, 0x10);
    if (object) {
        // Mode 0 is the unabridged home box, the same mode BetterBox uses.
        FN(void (*)(void *, u32), CAMP_INIT_BOX)(object, 0);
        *camp_window_slot(g_camp.manager) = object;
        g_camp.window = object;
        const u32 args[2] = {0, 0};
        FN(void (*)(void *, void *, u32, const u32 *, u32), CAMP_ATTACH_WINDOW)(root, object, 0x18, args, 1);
    }
    g_camp.creating = false;
    return object != NULL;
}

static void camp_talk(u8 *npc) {
    u8 *manager = camp_global(CAMP_WINDOW_MANAGER);
    if (g_camp.window && npc == g_camp.npc) {
        if (manager != g_camp.manager || *camp_window_slot(manager) != g_camp.window)
            camp_finish(npc, manager);
        return;
    }
    if (!g_camp_ready || !camp_in_quest() || npc[4] != 0 || (g_camp.npc && g_camp.npc != npc)) {
        ORIGINAL(void (*)(u8 *), g_camp_talk_hook)(npc);
        return;
    }

    u8 *root = camp_global(CAMP_UNIT_ROOT);
    u8 *resources = camp_global(CAMP_RESOURCE_MANAGER);
    if (!manager || !root || !resources || *camp_window_slot(manager) ||
        *(void **)(manager + CAMP_BOARD_SLOT)) {
        camp_clear_interaction();
        ORIGINAL(void (*)(u8 *), g_camp_talk_hook)(npc);
        return;
    }
    if (!g_camp.npc) {
        g_camp.npc = npc;
        g_camp.manager = manager;
    }
    u32 locale = camp_locale(camp_global(CAMP_SYSTEM));
    static const u32 left_menu_ids[] = {0xe694, 0xee39, 0xeea0, 0xef07, 0xef6e, 0xe694, 0xe694};
    u32 left = left_menu_ids[locale];
    const char *missing = "village archive";
    if (manager == g_camp.manager && camp_load_archive(resources, locale)) {
        bool layouts = camp_has_layout(left) && camp_has_layout(left + 4);
        if (!layouts) {
            if (++g_camp.waits < CAMP_WAIT_LIMIT) return;
            missing = "menu layout";
        } else if (!camp_load_messages(resources, locale)) {
            missing = "village messages";
        } else {
            if (camp_open_window(root)) {
                npc[0x13a9] = 0;
                npc[4] = 3;
                forge_log(LOG_INFO, "personal-gathering: CatCart opened full home Item Box (%s)",
                          g_camp_languages[locale]);
                return;
            }
            missing = "window allocation";
        }
    }
    forge_log(LOG_WARN, "personal-gathering: camp %s unavailable (%s); using native courier",
              missing, g_camp_languages[locale]);
    camp_clear_interaction();
    ORIGINAL(void (*)(u8 *), g_camp_talk_hook)(npc);
}

static void update_camp_box(void) {
    if (!g_camp_ready || (!g_camp.npc && !g_camp.archive) || camp_in_quest()) return;
    u8 *manager = camp_global(CAMP_WINDOW_MANAGER);
    if (g_camp.window && manager == g_camp.manager && *camp_window_slot(manager) == g_camp.window) {
        if (!g_camp.closing) {
            g_camp.closing = true;
            void **methods = *(void ***)g_camp.window;
            ((void (*)(void *))methods[0x40 / 4])(g_camp.window);
        }
        return;
    }
    camp_clear_interaction();
    camp_release_resources();
}

static void init_camp_box(void) {
    static const u32 expected[][2] = {
        {CAMP_CAT_TALK, 0xe92d41f0}, {CAMP_SET_NPC_STATE, 0xe301237b},
        {CAMP_IS_QUEST, 0xe590001c}, {CAMP_ALLOC_BOX, 0xe92d4070},
        {CAMP_INIT_BOX, 0xe92d47f0}, {CAMP_ATTACH_WINDOW, 0xe92d4010},
        {CAMP_FIND_RESOURCE, 0xe30026cf}, {CAMP_RELEASE_RESOURCE, 0xe1a01000},
        {CAMP_GMD_TYPE, 0xe59f0004}, {CAMP_GMD_STRING, 0xe5902088},
        {CAMP_VILLAGE_TEXT, 0xe590005c}, {CAMP_BANK_TEXT, 0xe92d4070},
        {CAMP_BOX_UPDATE, 0xe92d4070}, {CAMP_BOX_GET_CHILD, 0xe5900034},
    };
    for (u32 i = 0; i < sizeof expected / sizeof *expected; i++) {
        if (!code_is(expected[i][0], expected[i][1], 0)) {
            forge_log(LOG_WARN, "personal-gathering: camp menu signature differs at %x", expected[i][0]);
            return;
        }
    }
    g_camp_ready = hook(&g_camp_village_text_hook, CAMP_VILLAGE_TEXT, camp_village_text) &&
                   hook(&g_camp_bank_text_hook, CAMP_BANK_TEXT, camp_bank_text) &&
                   hook(&g_camp_box_update_hook, CAMP_BOX_UPDATE, camp_item_box_update) &&
                   hook(&g_camp_talk_hook, CAMP_CAT_TALK, camp_talk);
    if (!g_camp_ready) return;
    forge_log(LOG_INFO, "personal-gathering: camp item box ready");
    // Without these the box still opens, under the HUD
    if (code_is(CAMP_HUD_UPDATE, 0xe92d4830, 0xe1a04000) && code_is(CAMP_PANEL_SHOW, 0xe92d4070, 0xe24dd018) &&
        code_is(CAMP_PANEL_HIDE, 0xe92d4070, 0xe24dd018) && hook(&g_camp_hud_hook, CAMP_HUD_UPDATE, camp_hud_update))
        return;
    forge_log(LOG_WARN, "personal-gathering: quest HUD signature differs; the camp box opens under it");
}
