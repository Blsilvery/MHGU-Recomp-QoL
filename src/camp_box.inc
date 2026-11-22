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
    CAMP_GUI_SET_POSITION = 0x545c20,
    CAMP_GUI_GET_Y = 0x545af8,
    CAMP_RESOURCE_MANAGER = 0x2126a64,
    CAMP_WINDOW_MANAGER = 0x1899458,
    CAMP_UNIT_ROOT = 0x2126f48,
    CAMP_SYSTEM = 0x188ca48,
    CAMP_INTERACTION_CONTROL = 0x188ad90,
    CAMP_BOX_SLOT = 0x160,
    CAMP_BOARD_SLOT = 0x16c,
    CAMP_MESSAGE_COUNT = 42,
    CAMP_WAIT_LIMIT = 120,
    CAMP_STYLE_ROW = 7,
};

typedef void *(*CampLoadResource)(void *, void *, const char *, u32);
typedef const char *(*CampVillageText)(void *, u32, u32);
typedef const char *(*CampBankText)(void *, u32, u32, u32);
typedef float (__attribute__((pcs("aapcs-vfp"))) *CampGetY)(void *, u32);
typedef void (__attribute__((pcs("aapcs-vfp"))) *CampSetPosition)(void *, u32, float, float);

static ForgeHook g_camp_talk_hook, g_camp_village_text_hook, g_camp_bank_text_hook, g_camp_box_update_hook;
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

static bool camp_load_messages(void *resources, u32 locale) {
    if (!g_camp.texts_loaded) {
        void *type = FN(void *(*)(void), CAMP_GMD_TYPE)();
        CampLoadResource load = camp_resource_loader(resources);
        char path[] = "wheelpl\\qolcamp\\eng_00";
        for (u32 i = 0; i < 3; i++) path[sizeof(path) - 7 + i] = g_camp_languages[locale][i];
        for (u32 i = 1; i < CAMP_MESSAGE_COUNT; i++) {
            path[sizeof(path) - 3] = '0' + i / 10;
            path[sizeof(path) - 2] = '0' + i % 10;
            void *message = load(resources, type, path, 3);
            if (message && !*(u32 *)((u8 *)message + 0x88)) {
                FN(void (*)(void *), CAMP_RELEASE_RESOURCE)(message);
                message = NULL;
            }
            g_camp.messages[i] = message;
        }
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
    return text ? text : camp_fallback_text(category, index);
}

static const char *camp_bank_text(void *owner, u32 bank, u32 category, u32 index) {
    const char *text = ORIGINAL(CampBankText, g_camp_bank_text_hook)(owner, bank, category, index);
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

    // This child is the full-box root, including its title and cursor.
    // Submenus keep their usual positions. Native updates can reset the root.
    if (!owned || !camp_owns_item_box(box) || *(u32 *)(box + 0x40) != 1 ||
        !*(u32 *)(box + 0x30) || !*(u32 *)(box + 0x34)) return;
    u8 *menu = FN(u8 *(*)(void *, u32), CAMP_BOX_GET_CHILD)(box, 0);
    if (!menu) return;
    menu[0xa4 + CAMP_STYLE_ROW] = 0;
    void *gui = *(void **)(menu + 0x24);
    if (!gui || !*(u32 *)((u8 *)gui + 0x298) || !*(void **)((u8 *)gui + 0xf8)) return;
    float y = FN(CampGetY, CAMP_GUI_GET_Y)(gui, 0);
    // 968 is the native right-column origin on the 1280x720 menu canvas.
    FN(CampSetPosition, CAMP_GUI_SET_POSITION)(gui, 0, 968.0f, y);
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
        {CAMP_GUI_SET_POSITION, 0xe92d4010}, {CAMP_GUI_GET_Y, 0xe5902298},
    };
    for (u32 i = 0; i < sizeof expected / sizeof *expected; i++) {
        if (*(const u32 *)(g_main + expected[i][0]) != expected[i][1]) {
            forge_log(LOG_WARN, "personal-gathering: camp menu signature differs at %x", expected[i][0]);
            return;
        }
    }
    g_camp_ready = hook(&g_camp_village_text_hook, CAMP_VILLAGE_TEXT, camp_village_text) &&
                   hook(&g_camp_bank_text_hook, CAMP_BANK_TEXT, camp_bank_text) &&
                   hook(&g_camp_box_update_hook, CAMP_BOX_UPDATE, camp_item_box_update) &&
                   hook(&g_camp_talk_hook, CAMP_CAT_TALK, camp_talk);
    if (g_camp_ready)
        forge_log(LOG_INFO, "personal-gathering: camp item box ready");
}
