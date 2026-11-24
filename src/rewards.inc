// ================================================================== Rewards
//
// The reward screen (uUIResult) shows one section at a time (the reward box, the Palicoes' items...) beside a menu:
// Take Items (or Receive from Palico), Send to the Item Box, Sell Items & Exit (or Done). RESULT_SECTION runs it each
// frame. A section's slots are one list (all its Palicoes' items in one), and Send to the Item Box sends a slot with
// SEND_TO_BOX (as much as the box takes) or hands it to RESULT_SPECIAL when the item is marked special.
//
// The menu is drawn by cUIOReward: MENU_ROWS sets the rows' texts (layout instances 6, 7, 8), MENU_LOOKS their looks,
// and REWARD_UPDATE places the highlight bar at the cursor's row times the row spacing. tools/make_arcs.py adds a
// fourth row to the layout (instance 89) and makes the window taller. The plugin puts "Send all..." on the third row
// and moves the game's third entry to the fourth.
//
// The game's cursor keeps its three rows. "Send all" is an extra row between Send (1) and the third (2) that only the
// plugin knows about: it is entered by moving down from Send or up from the third row, while the game's cursor stays
// on Send. Pressing A there sends every slot of the section. When that empties the section, the cursor goes to the
// game's third row (Done, by then) and the game gets the A, so the section ends as if Done was chosen. When the box
// fills up first, the menu stays with what is left, and the game never sees the A.
//
// Meownster Hunters' rewards use the same menu (mode 7) with logic of their own (MONNYAN_UPDATE, state 1 the menu, its
// cursor at +0x20, its list at [+0x14]): Send to the Item Box adds a slot to the box with BOX_ADD (after BOX_HAS_ROOM)
// and tells the screen (MONNYAN_EVENT, sent); when the list is empty it shows Done (MENU_SET, the cursor on it, state
// 2), where A closes the screen. Send all does the same for every slot, then, when the list is empty, sets up Done the
// same way and leaves the A to the game, which closes the screen.
//
// Hunters for Hire's results (uUIGuestHunterResult, GH_UPDATE) use the same menu (mode 6) too, with their own logic:
// state 2 the menu, its cursor at +0x58, its list at +0x3C; Send to the Item Box as above; A on the menu with an empty
// list closes the screen. Send all sends every slot, plays the sound once, and leaves that A to the game.
//
// Sending plays a sound per slot. The game's sound manager starts each one on its own voice, so several in one frame
// add up to a louder sound: during a Send all, each sound plays only once.

// The row's text in the plugin's text language (lang.inc: the game's language, Korean with a Korean patch, or the
// "language" setting). The menu's own Send row (entries: the menu's text list) tells a Korean patch's text apart.
static const char *send_all_label(u32 entries, u32 row) {
    if (entries) note_game_text(FN(EntryText, ENTRY_TEXT)(entries + 12 * row));
    switch (text_language()) {
    case LANG_FRE: return "Tout envoyer au coffre";
    case LANG_SPA: return "Enviar todo al baúl";
    case LANG_GER: return "Alles in die Objektbox";
    case LANG_ITA: return "Invia tutto alla cassa";
    case LANG_CHT: return "全部入箱";
    case LANG_CHS: return "全部入箱";
    case LANG_KOR: return "전부 아이템 박스로 보내고 종료";
    default: return "Send all to Item Box & Exit";
    }
}
static u32 g_menu_uio;     // the menu showing the added row (0: none)
static u32 g_result_uio;   // the reward screen's menu
static u32 g_monnyan_uio;  // Meownster Hunters' reward menu
static u32 g_gh_uio;       // Hunters for Hire's
static bool g_on_send_all; // the cursor is on the added row (the game's cursor stays on Send)

// The sounds already played during this Send all
static bool g_one_sound_each;
static u32 g_sounds, g_sound[MAX_SOUNDS];

static u32 sound_hook(u32 kind, u32 id) {
    if (g_one_sound_each) {
        u32 key = kind << 16 | (id & 0xFFFF);
        for (u32 i = 0; i < g_sounds; i++)
            if (g_sound[i] == key) return 0;
        if (g_sounds < MAX_SOUNDS) g_sound[g_sounds++] = key;
    }
    return ORIGINAL(Sound, g_sound_hook)(kind, id);
}

static bool has_fourth_row(u32 uio) {
    u32 gui = *(u32 *)(uio + UIO_GUI);
    return gui && *(u32 *)(gui + GUI_INSTANCES) > ROW_FOURTH; // (the layout from tools/make_arcs.py is loaded)
}

// The menu showing the added row: its rows (3, or 2 for the appraisal) and the row of Send, which "Send all" follows.
// With three rows the game's cursor stays on Send while "Send all" is highlighted (g_on_send_all); the appraisal's
// cursor is given a third row instead (two rows wrap, so up and down couldn't be told apart), which the plugin maps.
static u32 g_menu_rows = 3, g_send_row = 1;
static bool g_cursor_extended;
static const u32 ROW_INSTANCES[] = {ROW_TAKE, ROW_SEND, ROW_THIRD, ROW_FOURTH};

// The row the player sees highlighted: the game's rows with "Send all" after Send
static u32 shown_row(u32 game_row) {
    if (g_cursor_extended) return game_row;
    return g_on_send_all ? g_send_row + 1 : game_row > g_send_row ? game_row + 1 : game_row;
}

static u32 *section_list(u32 ui, u32 section) {
    return (u32 *)(ui + RESULT_LISTS + 8 * (section <= 3 ? section : 0)); // (as the game picks it)
}

// Sends every slot of the section to the Item Box, the way Send to the Item Box does. Returns the slots sent.
static u32 send_all(u32 ui, u32 section) {
    u32 *list = section_list(ui, section);
    u16 *slots = (u16 *)list[0];
    u32 items = GLOBAL(GOT_ITEMS), result = GLOBAL(GOT_RESULT), sent = 0;
    g_one_sound_each = true, g_sounds = 0;
    for (u32 i = 0; slots && i < list[1]; i++) {
        u16 *slot = slots + 2 * i;
        if (!slot[0]) continue;
        u32 data = FN(Data, ITEM_DATA)(items, slot[0]);
        if (!data) continue;
        if (*(u8 *)(data + ITEM_SPECIAL)) {
            FN(Special, RESULT_SPECIAL)(result, (u32)slot, 0);
            slot[0] = slot[1] = 0;
        } else if (FN(Send, SEND_TO_BOX)(ui, (u32)slot) == SEND_FULL) {
            break;
        }
        sent++;
    }
    g_one_sound_each = false;
    *(u8 *)(ui + RESULT_REDRAW) = 1;
    return sent;
}

static bool section_empty(u32 ui, u32 section) {
    const u32 *list = section_list(ui, section);
    const u16 *slots = (const u16 *)list[0];
    for (u32 i = 0; slots && i < list[1]; i++)
        if (slots[2 * i]) return false;
    return true;
}

static void track_cursor(u8 *cursor, u32 before, u32 was);

// The appraisal: Send to Boxes as the game does it (items to the Item Box as the other sections', items marked at
// +0x12 to RESULT_ITEM_SPECIAL, equipment to the equipment box), every entry
static bool appraisal_empty(u32 ui) {
    const u32 *list = (const u32 *)(ui + APPRAISAL_LIST);
    for (u32 i = 0; list[0] && i < list[1]; i++)
        if (*(u32 *)(list[0] + APPRAISAL_ENTRY * i)) return false;
    return true;
}

static u32 appraisal_send_all(u32 ui) {
    const u32 *list = (const u32 *)(ui + APPRAISAL_LIST);
    u32 items = GLOBAL(GOT_ITEMS), sent = 0;
    g_one_sound_each = true, g_sounds = 0;
    for (u32 i = 0; list[0] && i < list[1]; i++) {
        u32 *entry = (u32 *)(list[0] + APPRAISAL_ENTRY * i);
        u16 *slot = (u16 *)(entry + 1);
        if (entry[0] == ENTRY_ITEM && slot[0]) {
            u32 data = FN(Data, ITEM_DATA)(items, slot[0]);
            if (!data) continue;
            if (*(u8 *)(data + ITEM_SPECIAL_APPRAISED)) {
                FN(Special, RESULT_ITEM_SPECIAL)(GLOBAL(GOT_RESULT), (u32)slot, 0);
                entry[0] = entry[1] = 0;
            } else if (FN(Send, SEND_TO_BOX)(ui, (u32)slot) == SEND_FULL) {
                continue; // (the Item Box is full: the equipment may still fit)
            } else if (!slot[1]) {
                entry[0] = 0;
            }
            sent++;
        } else if (entry[0] == ENTRY_EQUIP) {
            if (FN(EquipAdd, EQUIP_ADD)(GLOBAL(GOT_EQUIP_BOX), 0, (u32)(entry + 2)) == EQUIP_FULL) {
                FN(Sound, UI_SOUND)(0, SOUND_FULL);
                continue;
            }
            entry[0] = 0;
            FN(Sound, UI_SOUND)(0, SOUND_SENT);
            sent++;
        }
    }
    g_one_sound_each = false;
    *(u8 *)(ui + RESULT_REDRAW) = 1;
    return sent;
}

// The appraisal's menu (its own update, not a section's), its cursor given a third row, which the game's own moves then
// use (wrapping as before): 0 Send to Boxes, 1 Send all (the plugin's), 2 Sell Items & Exit (the game's row 1)
static u32 appraisal_hook(u32 ui) {
    u32 uio = FN(Component, UI_COMPONENT)(ui, 0);
    if (uio && uio != g_result_uio) {
        g_result_uio = uio;
        *(u8 *)(ui + RESULT_REDRAW) = 1;
    }
    static bool from_sell; // (Sell's confirmation was open: cancelled, the game's cursor is on its row 1)
    u8 *cursor = (u8 *)(ui + RESULT_CURSOR);
    u32 state = *(u32 *)(ui + RESULT_STATE);
    if (state == APPRAISAL_SELL) from_sell = true; // (until the menu is back; a new appraisal starts on row 0)
    bool menu = send_rewards() && uio && uio == g_menu_uio && g_menu_rows == 2 &&
                state == APPRAISAL_MENU && (cursor[CURSOR_ROWS] == 2 || cursor[CURSOR_ROWS] == 3);
    if (!menu) {
        g_cursor_extended = false;
        return ORIGINAL(Is, g_appraisal_hook)(ui);
    }
    if (from_sell && cursor[CURSOR_ROW] == 1) cursor[CURSOR_ROW] = 2; // (back on Sell: the plugin's row 2)
    from_sell = false;
    u32 *pressed = (u32 *)(GLOBAL(GOT_PAD) + PAD_PRESSED), was = *pressed;
    if (!g_cursor_extended) *(u8 *)(ui + RESULT_REDRAW) = 1; // (the rows' looks for the third row)
    cursor[CURSOR_ROWS] = 3;
    *(u16 *)(cursor + CURSOR_COUNT) = 3;
    g_cursor_extended = true;
    u32 row = cursor[CURSOR_ROW];
    if (was & PAD_B) {
        // the game's B goes to its last row, which is Send all here: to Sell instead
        if (row != 2) {
            cursor[CURSOR_ROW] = 2;
            FN(Sound, UI_SOUND)(0, SOUND_BACK);
            *(u8 *)(ui + RESULT_REDRAW) = 1;
        }
        *pressed = was & ~(PAD_B | PAD_A);
    } else if ((was & PAD_A) && row == 1) {
        u32 sent = appraisal_send_all(ui);
        bool leaving = appraisal_empty(ui);
        forge_log(LOG_INFO, "field-qol: sent %u appraised entries to the boxes%s", sent,
                  leaving ? ", leaving" : ", a box is full");
        if (!leaving) *pressed = was & ~PAD_A; // (with entries left the game would sell them; with none its A ends
                                               // the appraisal, from any row)
    } else if ((was & PAD_A) && row == 2) {
        cursor[CURSOR_ROW] = 1; // (the game's Sell Items & Exit)
    }
    u32 r = ORIGINAL(Is, g_appraisal_hook)(ui);
    *pressed = was;
    // out of the menu (Send to Boxes' grid, Sell's confirmation): the game's rows again from this frame's drawing on,
    // or the highlight would show its row 1 as Send all for a frame
    if (*(u32 *)(ui + RESULT_STATE) != APPRAISAL_MENU) g_cursor_extended = false;
    return r;
}

// Its help line, for the row the game sees: Send all gets Send to Boxes' line, Sell Items & Exit its own
static void appraisal_help_hook(u32 ui) {
    u8 *cursor = (u8 *)(ui + RESULT_CURSOR);
    if (!g_cursor_extended || *(u32 *)(ui + RESULT_STATE) != APPRAISAL_MENU) {
        ORIGINAL(Method, g_appraisal_help_hook)(ui);
        return;
    }
    u8 row = cursor[CURSOR_ROW];
    cursor[CURSOR_ROW] = row == 2 ? 1 : 0;
    ORIGINAL(Method, g_appraisal_help_hook)(ui);
    cursor[CURSOR_ROW] = row;
}

static u32 section_hook(u32 ui, u32 section) {
    // the reward screen's menu: known from its first frame (the rows are drawn again to show the added one)
    u32 uio = FN(Component, UI_COMPONENT)(ui, 0);
    if (uio && uio != g_result_uio) {
        g_result_uio = uio;
        *(u8 *)(ui + RESULT_REDRAW) = 1;
    }

    u8 *cursor = (u8 *)(ui + RESULT_CURSOR);
    g_cursor_extended = false;
    bool menu = send_rewards() && g_menu_uio && *(u32 *)(ui + RESULT_STATE) == STATE_MENU && cursor[CURSOR_ROWS] == 3;
    if (!menu) {
        g_on_send_all = false;
        return ORIGINAL(Section, g_section_hook)(ui, section);
    }

    // A on "Send all": send, then either exit through the game's third row or hide the A from the game
    u32 *pressed = (u32 *)(GLOBAL(GOT_PAD) + PAD_PRESSED), was = *pressed;
    bool leaving = false;
    if (g_on_send_all && (was & PAD_A)) {
        u32 sent = send_all(ui, section);
        leaving = section_empty(ui, section);
        forge_log(LOG_INFO, "field-qol: sent %u reward slots to the Item Box (section %u)%s", sent, section,
                  leaving ? ", leaving" : ", the box is full");
        if (leaving) {
            cursor[CURSOR_ROW] = 2;
            g_on_send_all = false;
        } else {
            *pressed = was & ~PAD_A; // (the game would open Send's grid)
        }
    }

    u32 before = cursor[CURSOR_ROW];
    u32 r = ORIGINAL(Section, g_section_hook)(ui, section);
    *pressed = was;
    if (leaving || *(u32 *)(ui + RESULT_STATE) != STATE_MENU) {
        g_on_send_all = false;
        return r;
    }

    track_cursor(cursor, before, was);
    return r;
}

// The cursor's moves after the game's frame, with the extra row between Send (1) and the third (2)
static void track_cursor(u8 *cursor, u32 before, u32 was) {
    u32 after = cursor[CURSOR_ROW];
    if (was & PAD_B) {
        g_on_send_all = false; // (the game moved its cursor to the third row)
    } else if (g_on_send_all) {
        if (before == 1 && after == 0) { // up: back to Send
            cursor[CURSOR_ROW] = 1;
            g_on_send_all = false;
        } else if (after != before) {    // down: the game is on its third row already
            g_on_send_all = false;
        }
    } else if (before == 1 && after == 2) { // down from Send: stop on Send all
        cursor[CURSOR_ROW] = 1;
        g_on_send_all = true;
    } else if (before == 2 && after == 1) { // up from the third row: stop on Send all
        g_on_send_all = true;
    }
}

// Meownster Hunters: the menu is known before the setup draws its rows
static u32 monnyan_init_hook(u32 list, u32 uio, u32 a2) {
    if (uio) g_monnyan_uio = uio;
    return ORIGINAL(Method4, g_monnyan_init_hook)(list, uio, a2, 0);
}

static bool monnyan_empty(u32 list) {
    const u32 *l = *(const u32 **)(list + MN_LIST);
    const u16 *slots = l ? (const u16 *)l[0] : 0;
    for (u32 i = 0; slots && i < l[1]; i++)
        if (slots[2 * i]) return false;
    return true;
}

// Sends every slot to the Item Box as Send to the Item Box does. Returns the slots sent; stops when the box is full.
static u32 monnyan_send_all(u32 list, bool *full) {
    u32 *l = *(u32 **)(list + MN_LIST), box = GLOBAL(GOT_BOX), sent = 0;
    u16 *slots = l ? (u16 *)l[0] : 0;
    *full = false;
    g_one_sound_each = true, g_sounds = 0;
    for (u32 i = 0; slots && i < l[1]; i++) {
        u16 *slot = slots + 2 * i;
        if (!slot[0]) continue;
        if (!FN(Is2, BOX_HAS_ROOM)(box, slot[0])) { *full = true; break; }
        u32 info[4] = {0};
        FN(Is, MONNYAN_INFO)((u32)info);
        info[1] = slot[0] | (u32)slot[1] << 16;
        s32 left = 0;
        FN(BoxAdd, BOX_ADD)(box, slot[0], (s16)slot[1], 0, &left);
        if (left < 1) slot[0] = 0, left = 0;
        slot[1] = (u16)left;
        FN(Event, MONNYAN_EVENT)(list, MN_EV, MN_EV_SENT, info);
        sent++;
    }
    g_one_sound_each = false;
    return sent;
}

static u32 monnyan_hook(u32 list) {
    u32 uio = *(u32 *)(list + MN_UIO);
    u8 *cursor = (u8 *)(list + MN_CURSOR);
    bool menu = send_monnyan() && uio && uio == g_monnyan_uio && uio == g_menu_uio &&
                *(u32 *)(list + MN_STATE) == MN_MENU && cursor[CURSOR_ROWS] == 3;
    if (!menu) {
        g_on_send_all = false;
        return ORIGINAL(Is, g_monnyan_hook)(list);
    }
    u32 *pressed = (u32 *)(GLOBAL(GOT_PAD) + PAD_PRESSED), was = *pressed;
    bool leaving = false;
    if (g_on_send_all && (was & PAD_A)) {
        bool full;
        u32 sent = monnyan_send_all(list, &full);
        leaving = monnyan_empty(list);
        forge_log(LOG_INFO, "field-qol: sent %u Meownster Hunters slots to the Item Box%s", sent,
                  leaving ? ", leaving" : ", the box is full");
        if (leaving) { // (as the game does after the last slot: Done, which the A then closes)
            FN(Event, MONNYAN_EVENT)(list, MN_EV, MN_EV_EMPTY, 0);
            FN(MenuSet, MENU_SET)(uio, MN_ROWS_DONE, 1, 6);
            cursor[CURSOR_ROW] = 2, *(u16 *)(cursor + CURSOR_PAGE) = 0;
            *(u32 *)(list + MN_STATE) = MN_DONE;
            g_on_send_all = false;
        } else {
            *pressed = was & ~PAD_A; // (the game would open Send's grid)
        }
    }
    u32 before = cursor[CURSOR_ROW];
    u32 r = ORIGINAL(Is, g_monnyan_hook)(list);
    *pressed = was;
    if (leaving || *(u32 *)(list + MN_STATE) != MN_MENU) {
        g_on_send_all = false;
        return r;
    }
    track_cursor(cursor, before, was);
    return r;
}

// Hunters for Hire: Send to the Item Box as the game's (an item with data, BOX_HAS_ROOM, BOX_ADD), one sound for all.
// Its menu closes the screen on A when the list is empty, from any row: Send all leaves it the A then.
static bool gh_empty(u32 ui) {
    const u32 *l = (const u32 *)(ui + GH_LIST);
    const u16 *slots = (const u16 *)l[0];
    for (u32 i = 0; slots && i < l[1]; i++)
        if (slots[2 * i]) return false;
    return true;
}

static u32 gh_send_all(u32 ui, bool *full) {
    u32 *l = (u32 *)(ui + GH_LIST), box = GLOBAL(GOT_BOX), items = GLOBAL(GOT_ITEMS), sent = 0;
    u16 *slots = (u16 *)l[0];
    *full = false;
    for (u32 i = 0; slots && i < l[1]; i++) {
        u16 *slot = slots + 2 * i;
        if (!slot[0] || !FN(Data, ITEM_DATA)(items, slot[0])) continue;
        if (!FN(Is2, BOX_HAS_ROOM)(box, slot[0])) { *full = true; break; }
        s32 left = 0;
        FN(BoxAdd, BOX_ADD)(box, slot[0], (s16)slot[1], 0, &left);
        if (left < 1) slot[0] = 0, left = 0;
        slot[1] = (u16)left;
        sent++;
    }
    if (sent || *full) FN(Sound, UI_SOUND)(0, *full ? SOUND_FULL : SOUND_SENT);
    *(u8 *)(ui + GH_REDRAW) = 1;
    return sent;
}

static u32 gh_hook(u32 ui) {
    u32 uio = FN(Component, UI_COMPONENT)(ui, GH_UIO_INDEX);
    if (uio) g_gh_uio = uio; // (before the screen's first frame draws its rows)
    u8 *cursor = (u8 *)(ui + GH_CURSOR);
    bool menu = send_rewards() && uio && uio == g_menu_uio && *(u32 *)(ui + GH_STATE) == GH_MENU &&
                cursor[CURSOR_ROWS] == 3;
    if (!menu) {
        g_on_send_all = false;
        return ORIGINAL(Is, g_gh_hook)(ui);
    }
    u32 *pressed = (u32 *)(GLOBAL(GOT_PAD) + PAD_PRESSED), was = *pressed;
    bool leaving = false;
    if (g_on_send_all && (was & PAD_A)) {
        bool full;
        u32 sent = gh_send_all(ui, &full);
        leaving = gh_empty(ui);
        forge_log(LOG_INFO, "field-qol: sent %u Hunters for Hire slots to the Item Box%s", sent,
                  leaving ? ", leaving" : ", the box is full");
        if (leaving) g_on_send_all = false; // (the game closes the screen on this A)
        else *pressed = was & ~PAD_A;
    }
    u32 before = cursor[CURSOR_ROW];
    u32 r = ORIGINAL(Is, g_gh_hook)(ui);
    *pressed = was;
    if (leaving || *(u32 *)(ui + GH_STATE) != GH_MENU) {
        g_on_send_all = false;
        return r;
    }
    track_cursor(cursor, before, was);
    return r;
}

// The trade cart's Claim Items (Neko): every cell of the three carts' inboxes to the Item Box as its Send does (room
// for the whole quantity, BOX_PUT, the cell emptied), then the inbox saved and drawn once, with one sound.
static bool g_trade_row; // (its menu has the fourth row: the layout from tools/make_arcs.py)
static u32 g_trade_uio;
static void trade_menu_hook(u32 uio);

static const char *trade_all_label(void) {
    return send_all_label(0, 0);
}

static bool trade_empty(void) {
    u32 items = GLOBAL(GOT_ITEMS);
    for (u32 cart = 0; cart < TRADE_CARTS; cart++) {
        u32 c = FN(Is2, TRADE_CART)(items, cart);
        if (!c) continue;
        const u16 *cell = (const u16 *)(c + CART_CELLS);
        for (u32 i = 0; i < CART_CELL_COUNT; i++)
            if (cell[2 * i]) return false;
    }
    return true;
}

static u32 trade_send_all(u32 ui, bool *full) {
    u32 items = GLOBAL(GOT_ITEMS), box = GLOBAL(GOT_BOX), sent = 0;
    *full = false;
    for (u32 cart = 0; cart < TRADE_CARTS; cart++) {
        u32 c = FN(Is2, TRADE_CART)(items, cart);
        if (!c) continue;
        u16 *cell = (u16 *)(c + CART_CELLS);
        for (u32 i = 0; i < CART_CELL_COUNT; i++) {
            u16 item = cell[2 * i];
            s16 qty = (s16)cell[2 * i + 1];
            if (!item) continue;
            if ((s32)FN(Is2, BOX_HAS_ROOM)(box, item) < qty) { *full = true; continue; }
            FN(Method4, BOX_PUT)(box, item, (u32)(s32)qty, 0);
            cell[2 * i] = cell[2 * i + 1] = 0;
            sent++;
        }
    }
    if (sent) {
        FN(Method, TRADE_CHANGED)(items);
        FN(Is2, TRADE_REDRAW)(ui, 1);
    }
    if (sent || *full) FN(Sound, UI_SOUND)(0, *full ? SOUND_FULL : SOUND_SENT);
    return sent;
}

static u32 trade_hook(u32 ui) {
    u8 *cursor = (u8 *)(ui + TRADE_CURSOR);
    bool menu = send_trade() && g_trade_row && *(u8 *)(ui + TRADE_SUBSTATE) == 0 && cursor[CURSOR_ROWS] == 3;
    if (!menu) {
        g_on_send_all = false;
        return ORIGINAL(Is, g_trade_hook)(ui);
    }
    u32 *pad = (u32 *)(ui + TRADE_PAD), was = *pad;
    bool leaving = false;
    if (g_on_send_all && (was & PAD_A) && !trade_empty()) { // (with nothing to claim, the A goes to Send, greyed)
        bool full;
        u32 sent = trade_send_all(ui, &full);
        leaving = trade_empty();
        forge_log(LOG_INFO, "field-qol: sent %u trade cart slots to the Item Box%s", sent,
                  leaving ? ", leaving" : ", the box is full");
        if (leaving) {
            *pad = PAD_B; // (the game's B closes Claim Items)
            g_on_send_all = false;
        } else {
            *pad = was & ~PAD_A; // (the game would open Send's grid)
        }
    }
    u32 before = cursor[CURSOR_ROW];
    u32 r = ORIGINAL(Is, g_trade_hook)(ui);
    *pad = was;
    if (leaving || *(u8 *)(ui + TRADE_SUBSTATE) != 0) {
        g_on_send_all = false;
        return r;
    }
    track_cursor(cursor, before, was);
    // Original drawing runs inside the frame before track_cursor updates the
    // virtual extra row. Redraw its highlight after that update as well.
    if (g_trade_uio) {
        *(u32 *)(g_trade_uio + TM_FLAGS) |= 1;
        trade_menu_hook(g_trade_uio);
    }
    return r;
}

// Its menu: Send all after Send (on the third row), Sell Items & Exit on the fourth, the highlight on the row the
// player sees. Turning the option off draws the game's rows again.
static void trade_menu_hook(u32 uio) {
    static bool was_ours;
    g_trade_uio = uio;
    u32 gui = *(u32 *)(uio + UIO_GUI);
    g_trade_row = gui && *(u32 *)(gui + GUI_INSTANCES) > TM_ROW_FOURTH;
    bool ours = send_trade() && g_trade_row;
    if (ours != was_ours) *(u32 *)(uio + TM_FLAGS) |= 1 | 4 | 8;
    u32 flags = *(u32 *)(uio + TM_FLAGS);
    ORIGINAL(Method, g_trade_menu_hook)(uio);
    if (!g_trade_row) return;
    if (!ours) {
        if (was_ours) FN(SetInt3, INST_VISIBLE)(uio, TM_ROW_FOURTH, 0);
        was_ours = false;
        return;
    }
    was_ours = true;
    bool nothing = *(u32 *)(uio + TM_MODE) == 1;
    if (flags & 4) {
        FN(SetText, UIO_SET_TEXT)(uio, TM_ROW_THIRD, ROW_OBJECT, trade_all_label());
        FN(SetInt, UIO_SET_LOOK)(uio, TM_ROW_THIRD, ROW_OBJECT,
                                 FN(Is2, LOOK_OF)(GLOBAL(GOT_LOOKS), nothing ? LOOK_GREYED : 0));
        FN(MsgSet, MSG_SET)(uio, TM_ROW_FOURTH, ROW_OBJECT, 0, 1, nothing ? TM_MSG_EXIT : TM_MSG_SELL);
        FN(SetInt, UIO_SET_LOOK)(uio, TM_ROW_FOURTH, ROW_OBJECT, FN(Is2, LOOK_OF)(GLOBAL(GOT_LOOKS), 0));
    }
    u32 cursor = *(u32 *)(uio + TM_CURSOR);
    if (flags & 8) // (the fourth row as the game's third: shown with three entries)
        FN(SetInt3, INST_VISIBLE)(uio, TM_ROW_FOURTH, cursor && *(u16 *)(cursor + CURSOR_COUNT) > 2);
    if (cursor) {
        float spacing = FN(GetY, GUI_GET_Y)(gui, TM_SPACING);
        u32 row = *(u8 *)(cursor + CURSOR_ROW); // (Send is its second row: Send all and Sell one lower)
        u32 shown = g_on_send_all ? 2 : row >= 2 ? row + 1 : row;
        FN(SetY, GUI_SET_Y)(gui, TM_HIGHLIGHT, spacing * (float)shown);
    }
}

// The rows: the added one after Send, on a three-row menu with Send second (the reward screen's sections, Meownster
// Hunters, Hunters for Hire) or the appraisal's two rows (Send to Boxes, Sell Items & Exit)
static void rows_hook(u32 uio, u32 list, u32 flag) {
    ORIGINAL(Rows, g_rows_hook)(uio, list, flag);
    if (!has_fourth_row(uio)) return;
    u32 entries = *(u32 *)list, count = *(u32 *)(list + 4), mode = *(u32 *)(uio + UIO_MODE);
    bool known = uio == g_result_uio || uio == g_monnyan_uio || uio == g_gh_uio;
    bool three = count == 3 && mode < 32 && (RESULT_MODES >> mode & 1) && entries &&
                 *(u16 *)(entries + 12 + 6) == MSG_RESULT && *(u16 *)(entries + 12 + 8) == MSG_SEND;
    bool two = count == 2 && mode == MODE_APPRAISAL && uio == g_result_uio && entries;
    bool feature = uio == g_monnyan_uio ? send_monnyan() : send_rewards();
    bool ours = feature && flag && known && (three || two);
    if (!ours) {
        if ((uio == g_monnyan_uio || uio == g_gh_uio) && feature && flag && entries && count == 3) // (why it stays)
            forge_log(LOG_INFO, "field-qol: %s menu kept (mode %u, second %u:%u)",
                      uio == g_gh_uio ? "Hunters for Hire" : "Meownster Hunters", mode,
                      *(u16 *)(entries + 12 + 6), *(u16 *)(entries + 12 + 8));
        if (g_menu_uio == uio) {
            g_menu_uio = 0;
            if (count < 3) FN(SetInt, UIO_SET_VISIBLE)(uio, ROW_THIRD, ROW_OBJECT, 0); // (the appraisal's third row)
        }
        FN(SetInt, UIO_SET_VISIBLE)(uio, ROW_FOURTH, ROW_OBJECT, 0);
        return;
    }
    g_menu_uio = uio, g_menu_rows = count, g_send_row = two ? 0 : 1;
    if (three) g_cursor_extended = false;
    // the rows after Send move one down, "Send all" goes after Send
    for (u32 k = count - 1; k > g_send_row; k--) {
        FN(SetText, UIO_SET_TEXT)(uio, ROW_INSTANCES[k + 1], ROW_OBJECT, FN(EntryText, ENTRY_TEXT)(entries + 12 * k));
        FN(SetInt, UIO_SET_VISIBLE)(uio, ROW_INSTANCES[k + 1], ROW_OBJECT, 1);
    }
    FN(SetText, UIO_SET_TEXT)(uio, ROW_INSTANCES[g_send_row + 1], ROW_OBJECT, send_all_label(entries, g_send_row));
    FN(SetInt, UIO_SET_VISIBLE)(uio, ROW_INSTANCES[g_send_row + 1], ROW_OBJECT, 1);
    if (count == 2) FN(SetInt, UIO_SET_VISIBLE)(uio, ROW_FOURTH, ROW_OBJECT, 0);
}

// The rows' looks (enabled, highlighted) for the rows the player sees
static void looks_hook(u32 uio, u32 enabled, u32 row) {
    ORIGINAL(Looks, g_looks_hook)(uio, enabled, row);
    if (uio != g_menu_uio || *(u32 *)(enabled + 4) < g_menu_rows) return;
    const u8 *on = *(const u8 **)enabled;
    u32 shown = shown_row(row);
    Look look = FN(Look, ROW_LOOK);
    SetInt set = FN(SetInt, UIO_SET_LOOK);
    for (u32 k = 0; k < g_menu_rows; k++) {
        u32 v = k <= g_send_row ? k : k + 1;
        set(uio, ROW_INSTANCES[v], ROW_OBJECT, look(on[k], shown == v, 0, 0));
    }
    set(uio, ROW_INSTANCES[g_send_row + 1], ROW_OBJECT, look(on[g_send_row], shown == g_send_row + 1, 0, 0));
}

// The highlight bar, on the row the player sees
static void reward_update_hook(u32 uio) {
    ORIGINAL(Method, g_reward_update_hook)(uio);
    if (uio != g_menu_uio) return;
    u32 gui = *(u32 *)(uio + UIO_GUI), cursor = *(u32 *)(uio + UIO_CURSOR);
    if (!gui || !cursor) return;
    float spacing = FN(GetY, GUI_GET_Y)(gui, INST_SPACING);
    FN(SetY, GUI_SET_Y)(gui, INST_HIGHLIGHT, spacing * (float)shown_row(*(u8 *)(cursor + CURSOR_ROW)));
    FN(Sync, CURSOR_SYNC)(cursor, uio, INST_HIGHLIGHT);
}
