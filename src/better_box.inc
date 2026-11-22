enum {
    ITEM_BOX_MENU = 0x713b74,
    OPEN_ITEM_BOX = 0x55e854,
    OPEN_PALICO_BOX = 0x55ea1c,
};

static ForgeHook g_box_menu_hook, g_box_open_hook;
static bool g_box_hooked;

static bool box_allows_equipment(void) {
    u32 quest = forge_singleton_getInstanceByName("sQuest");
    return !quest || *(const u32 *)(quest + 0xd0) == 0;
}

static u32 box_menu_hook(u8 *menu) {
    // A zero flag selects the full equipment-box menu in the original initializer.
    if (g_box_hooked && menu && box_allows_equipment())
        *(u32 *)(menu + 0x3c) = 0;
    return ((u32 (*)(u8 *))g_box_menu_hook.original)(menu);
}

static u32 box_open_hook(u32 owner, u32 arg, u32 x, u32 y, u32 mode) {
    if (g_box_hooked && forge_input_isDown(BUTTON_R) && box_allows_equipment())
        return ((u32 (*)(u32, u32, u32, u32))(g_main + OPEN_PALICO_BOX))(owner, arg, 0, 0);
    return ((u32 (*)(u32, u32, u32, u32, u32))g_box_open_hook.original)(owner, arg, x, y, mode);
}

static void init_better_box(void) {
    const u32 *menu = (const u32 *)(g_main + ITEM_BOX_MENU);
    const u32 *open = (const u32 *)(g_main + OPEN_ITEM_BOX);
    const u32 *palico = (const u32 *)(g_main + OPEN_PALICO_BOX);
    if (menu[0] != 0xE92D41F0 || menu[1] != 0xE24DD008 ||
        open[0] != 0xE92D4070 || open[1] != 0xE24DD010 ||
        palico[0] != 0xE92D4830 || palico[1] != 0xE24DD010) {
        forge_log(LOG_WARN, "personal-gathering: box code differs; disable standalone BetterBox or other box patches");
        return;
    }

    bool menu_ok = hook(&g_box_menu_hook, ITEM_BOX_MENU, box_menu_hook);
    bool open_ok = hook(&g_box_open_hook, OPEN_ITEM_BOX, box_open_hook);
    g_box_hooked = menu_ok && open_ok;
    forge_log(g_box_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_box_hooked ? "full village item box enabled; hold R and confirm for Palico equipment"
                           : "box hooks failed; original box menus retained");
}
