// MHGU Recomp QoL: derivative of Field QoL v0.7.0.
// Upstream: https://github.com/mhvuze/mhgu-field-qol (MIT; see LICENSE).
// Adds tool-free hunter mining and bug catching for verified MHGU 1.4.0.
//
// Default-on batch gathering/carving, tool-free mining/bug catching, and Chinese
// "send all" rows on quest rewards/appraisal and the trade cart's claim screen.
//
// The plugin runs in the recomp's interpreter as AArch32 code and calls the game's own functions. Addresses are
// offsets into the game's main module (MHGU 1.4.0); at runtime they are added to forge_mem_getMainTextAddr(). Each
// feature's section below explains the game code it relies on.
#include "forge.h"

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "dev"
#endif

// ================================================================== game addresses

// Gathering
#define GATHER 0x2E07C4           // uPlayer: one gather (hunter, empty_if_none, notify) -> 1 when an item was given
#define GIVE 0x2E04D4             // uPlayer: give a gathered item (hunter, item, quantity, notify, 0)
#define PLAYER_ADD 0x2880B4       // uPlayerBase: put an item in the pouch (hunter, item, quantity, flags, 0) -> 0-2: taken
#define PLAYER_ADD_QUEST 0x301784 // uPlayerQuest's override of it (the hunter on a quest)
#define IS_LOCAL 0x27BCD0         // uPlayer: is this the hunter played on this console
#define POP_USE 0x31B228          // sPop: use up one gather (spop, id, &pop) -> 1: more left, 2: that was the last
#define POP_CAN_GATHER 0x319244   // cPop: has a gather left and isn't going away
#define POP_ENEMY 0x319588        // cPop: the monster it belongs to, else 0 (most carcasses don't say)
#define EM009_SET_ACTION 0xD565F8 // uEm009_00 (Gypceros), vtable +0x204: start an action (gypceros, kind, action)
#define POP_ITEM 0x319318         // cPop: roll its next item (pop, &item, &quantity) -> 1
#define IS_CARRIED 0x1943BC       // sItem: is the item carried in the hands (items, item)
#define MESSAGE 0x65DE34          // add a line to the quest's message log (kind, message, item, quantity, 0)
#define MSG_BEGIN 0x65DCE4        // the log's next lines go in one notification box...
#define MSG_END 0x65DD24          // ...which this shows
#define POUCH_PROMPT 0x670500     // open the "Item Pouch is full" window for an item to take (item, quantity)
#define PROMPT_OPEN 0x670584      // is that window open
#define PROMPT_ITEM 0x6705C0      // the window's item to take (u16 item, u16 quantity), 0 without the window; the byte
                                  // 8 before it is set while an item waits for the window to open
#define GOT_SPOP 0x1838D94        // the game's sPop (gathering spots)
#define GOT_ITEMS 0x1838BD0       // the game's sItem (item data)

// Delivery
#define DELIVERABLE 0x188C84      // cItemPouch: is Deliver open for an item (slot, item)
#define CLEAR_ACCOUNT 0x187A60    // cItemPouch: is "Clear out Account Items" open
#define LOCAL_HUNTER 0x276278     // the player manager's hunter for this console (players)
#define GOT_PLAYERS 0x1838D80     // the game's player manager

// Rewards
#define RESULT_SECTION 0x388EE8   // uUIResult: run one section (menu, item grids, selling) for a frame (ui, section)
#define SEND_TO_BOX 0x38A2DC      // uUIResult: send one slot to the Item Box (ui, slot) -> 7: the box had no room
#define RESULT_SPECIAL 0x3C6C6C   // hand over a slot of an item marked special in its data (result, slot, 0)
#define ITEM_DATA 0x194228        // sItem: an item's data (items, item)
#define UI_SOUND 0x54F260         // play a menu sound (kind, id); Send to the Item Box plays (0, 0x1F), or (0, 3) when full
#define MENU_ROWS 0x624BF8        // cUIOReward: set the menu's rows from a list of texts (uio, list, flag)
#define MENU_LOOKS 0x624D00       // cUIOReward: set the rows' looks (uio, enabled, cursor row)
#define REWARD_UPDATE 0x622FE4    // cUIOReward: per-frame update, places the highlight bar (uio)
#define UIO_SET_TEXT 0x56E808     // cUIO: set an object's text (uio, instance, object, text)
#define UIO_SET_VISIBLE 0x56E530  // cUIO: show or hide an object (uio, instance, object, on)
#define UIO_SET_LOOK 0x56E6FC     // cUIO: set an object's look (uio, instance, object, look)
#define ROW_LOOK 0x550354         // a menu row's look (enabled, selected, 0, 0) -> look
#define ENTRY_TEXT 0x54FA20       // the string of a text list entry (entry)
#define GUI_GET_Y 0x545AF8        // cGUI: an instance's y (gui, instance)
#define GUI_SET_Y 0x545AC0        // cGUI: set an instance's y (gui, instance, y)
#define CURSOR_SYNC 0x551D58      // move a cursor's drawing to an instance (cursor, uio, instance)
#define BOX_HAS_ROOM 0x199390     // sItemBox: has room for an item (box, item)
#define BOX_ADD 0x19945C          // sItemBox: add an item (box, item, quantity, 0, &left over)
#define MONNYAN_INIT 0x1B600C     // Meownster Hunters' reward list: set up with its menu (list, uio, ...)
#define MONNYAN_UPDATE 0x1B607C   // ...every frame (list)
#define MONNYAN_EVENT 0x1ADAEC    // ...tell the screen (list, 0xB, event, info)
#define MONNYAN_INFO 0x1BBE04     // ...an event's info, made empty (info)
#define EQUIP_ADD 0x143160        // the equipment box: add a piece (box, 0, &piece) -> its slot, 0xFFFF: full
#define RESULT_ITEM_SPECIAL 0x3C6B64 // an appraised item marked at item data +0x12 (result, slot)
#define APPRAISAL_UPDATE 0x3850B0 // uUIResult: run the appraisal for a frame (ui) -> 1: done
#define APPRAISAL_HELP 0x390AAC   // uUIResult: the appraisal's help line (ui); in the menu message 0x85 + cursor row
// The trade cart (Neko, uUITradeCenter): its Claim Items screen and its menu (cUIOTradeCenterBoxMenu)
#define TRADE_CLAIM 0x7A3480      // uUITradeCenter: Claim Items for a frame (ui) -> 2: closed by B
#define TRADE_MENU_DRAW 0x63ED84  // cUIOTradeCenterBoxMenu: draw what changed (uio)
#define TRADE_CART 0x195448       // a trade cart's inbox (the GOT_ITEMS object, cart 0-2) -> cart, 0 past the last
#define TRADE_CHANGED 0x1955B8    // (the GOT_ITEMS object) after the inbox changed, as the game's Send calls it
#define TRADE_REDRAW 0x7A0B74     // uUITradeCenter: draw the inbox again (ui, 1)
#define BOX_PUT 0x1990DC          // sItemBox: put an item in (box, item, quantity, 0), as the trade's Send does
#define MSG_SET 0x56FC58          // cUIO: an object's text from a message (uio, instance, object, 0, 1, message)
#define INST_VISIBLE 0x56E528     // cUIO: show or hide an instance (uio, instance, on)
#define LOOK_OF 0x560E90          // a text look (the GOT_LOOKS object, kind): 0 as usual, 0xB greyed
#define GOT_LOOKS 0x1838BD8
#define GH_UPDATE 0x710A64        // uUIGuestHunterResult (Hunters for Hire's results): every frame (ui)
#define MENU_SET 0x624664         // cUIOReward: set the menu's rows and looks (uio, rows, ...)
#define UI_COMPONENT 0x65CB3C     // uUIBase: one of its cUIO (ui, index); the reward screen's menu is index 0
#define GOT_PAD 0x183931C         // the game's pad state
#define GOT_RESULT 0x1838DE0      // the game's quest result
#define GOT_BOX 0x183A81C         // the game's sItemBox
#define GOT_EQUIP_BOX 0x1839394   // the game's equipment box

// ================================================================== game structures

// sPop and cPop: the active gathering spots are a list from sPop+0x5C, linked by +0x2C
#define SPOP_ACTIVE 0x5C
#define POP_NEXT 0x2C
#define POP_ID 0x4C               // (u16) the spot's id
#define POP_LEFT 0x4E             // (u16) gathers left
#define LEFT_UNLIMITED 0xFFFF
// uEnemy, a spot's monster
#define ENEMY_CARVE_STATE 0x338   // (vtable) how a carve affects it: 0 not at all (a carcass); 1 it is told, 2 refused
#define EM009_DATA 0xCAC0         // uEm009_00: its own data...
#define EM009_DEAD_POP 0x4C       // ...(s32) the id of its playing-dead carve spot, -1: none
#define MAX_PLAYING_DEAD 8        // the Gypceros remembered at once
// the hunter
#define HUNTER_STATE 0x2500
#define STATE_CARRYING 0x10       // set while carrying something in the hands
#define HUNTER_AWAY 0x1054        // (u8) 0 in the base camp, 1 out on the map
#define HUNTER_POP_ID 0x32AE      // (s16) the id of the spot being gathered
// the pouch's answers
#define ADD_NONE 0xFFFFFFFF       // (the plugin's own: no answer seen)
#define ADD_TAKEN_MAX 2           // 0-2: the pouch took it (2: some of it, the item reached its limit)
#define ADD_AT_LIMIT 3            // not taken: the item is at its limit ("can't carry any more")
#define ADD_NO_ROOM 5             // not taken: no free slot; the pouch opens the replace window (POUCH_PROMPT)
// the message log
#define MSG_OBTAINED 9            // kind 0, message 9: "<item> xN"

// uUIResult, the reward screen
#define RESULT_STATE 0x44
#define STATE_MENU 2              // the menu is open
#define RESULT_REDRAW 0x55        // (u8) set: draw the menu and grids again
#define RESULT_LISTS 0x5C         // per section {u16 *slots, u32 count}; a slot is {u16 item, s16 quantity}
#define RESULT_CURSOR 0xA0        // the menu's cursor
#define CURSOR_ROW 0xD            // (u8) its row
#define CURSOR_ROWS 0xE           // (u8) its rows
#define CURSOR_COUNT 0x14         // (u16) its entries (the moves stop at the last)
#define SEND_FULL 7
#define ITEM_SPECIAL 0x16         // (u8, item data) handed over by RESULT_SPECIAL instead of sent
// the pad
#define PAD_PRESSED 0x6BFC        // the buttons pressed this frame
#define PAD_A 0x2000
#define PAD_B 0x4000
// Meownster Hunters' reward list (uUIMonNyan's): its own menu logic over the same reward menu (mode 7)
#define MN_UIO 0x08
#define MN_STATE 0x10             // 1: the menu; 2: the list is empty, the menu shows Done
#define MN_LIST 0x14              // {u16 *slots, u32 count}, a slot as the reward screen's
#define MN_CURSOR 0x20            // the menu's cursor (+0xD its row, +0xE its rows, +0x10 its page)
#define MN_MENU 1
#define MN_DONE 2
#define MN_EV 0xB
#define MN_EV_SENT 2
#define MN_EV_EMPTY 0x10
#define MN_ROWS_DONE 8            // (MENU_SET: the menu with Done)
#define CURSOR_PAGE 0x10
// the reward screen's appraisal: its menu is state 4 (A: state 5 + row), its list {entry *, count} at +0x7C, an entry
// 0x34 bytes: u32 kind (0 none, 1 an item, 2 a piece of equipment), the item's slot at +4, the piece at +8
#define APPRAISAL_MENU 4
#define APPRAISAL_SELL 6         // (Sell Items & Exit's confirmation: cancelled, the menu with the cursor on its row 1)
#define APPRAISAL_LIST 0x7C
#define APPRAISAL_ENTRY 0x34
#define ENTRY_ITEM 1
#define ENTRY_EQUIP 2
#define ITEM_SPECIAL_APPRAISED 0x12
#define EQUIP_FULL 0xFFFF
// uUIGuestHunterResult, Hunters for Hire's results: the same reward menu (mode 6, its component 1)
#define GH_STATE 0x4C             // 2: the menu (3 Take, 4 Send, 5 Sell)
#define GH_MENU 2
#define GH_REDRAW 0x55
#define GH_LIST 0x3C              // {u16 *slots, u32 count}
#define GH_CURSOR 0x58
#define GH_UIO_INDEX 1
// the Claim Items screen: its sub-state (u8) 0 the menu (A: 1 + row: Take, Send's grid, Sell), its own copy of the pad
#define TRADE_SUBSTATE 7
#define TRADE_PAD 0x80
#define TRADE_CURSOR 0x178
#define TRADE_CARTS 3
#define CART_CELLS 0x10           // 3 rows of 10 cells {u16 item, s16 quantity}
#define CART_CELL_COUNT 30
// its menu: rows at instances 5-7 (messages 0x1A + row, 0x1D for Exit), the fourth row (tools/make_arcs.py) 119
#define TM_FLAGS 0x4C             // what to draw: 1 the highlight, 4 the rows, 8 their visibility
#define TM_MODE 0x58              // 1: nothing to claim (Take and Send greyed, Exit)
#define TM_CURSOR 0x50
#define TM_ROW_SEND 6
#define TM_ROW_THIRD 7
#define TM_ROW_FOURTH 119
#define TM_HIGHLIGHT 18
#define TM_SPACING 14
#define TM_MSG_SELL 0x1C
#define TM_MSG_EXIT 0x1D
#define LOOK_GREYED 0xB
#define SOUND_SENT 0x1F           // (UI_SOUND kind 0)
#define SOUND_FULL 3
#define SOUND_BACK 4             // (B moving the cursor to the last row)
// cUIOReward, the reward menu, and its layout
#define UIO_GUI 0x24
#define UIO_MODE 0x4C             // which screen uses the menu: the reward screen's sections are 1, 2 and 7
#define UIO_CURSOR 0x50
#define RESULT_MODES 0xC6         // (bits 1, 2, 6 Hunters for Hire, 7 Meownster Hunters; other screens use others)
#define MODE_APPRAISAL 3          // the reward screen's appraisal: two rows, Send to Boxes and Sell Items & Exit
#define GUI_INSTANCES 0x298       // the layout's instance count
#define ROW_TAKE 6                // layout instances: the first row
#define ROW_SEND 7                // the second row (Send to the Item Box)
#define ROW_THIRD 8               // the third row, which the plugin turns into "Send all..."
#define ROW_FOURTH 89             // the fourth row, added by tools/make_arcs.py
#define ROW_OBJECT 1              // a row's text object
#define INST_SPACING 0x10         // its y is the spacing of the rows
#define INST_HIGHLIGHT 0x2F       // the highlight bar
#define MSG_RESULT 7              // the message table of the menu's texts...
#define MSG_SEND 1                // ...and "Send to the Item Box" in it

// ================================================================== plugin state and helpers

#define MAX_EXTRA 64              // the most extra gathers per press (spots have a handful)
#define MAX_QUEUED 24             // the most messages held back per press
#define BOX_LINES 6               // the lines a notification box takes
#define MAX_LEFT 16               // the items waiting for the replace window
#define PROMPT_TRIES 600          // frames without a replace window before the waiting items are dropped (about 10 s)
#define BUTTON_L 6                // (forge_input_isDown; not R, the sprint button: hunters sprint up to the box)
#define PROMPT_PENDING 8          // (PROMPT_ITEM - 8: an item waits for the window)
#define MAX_SOUNDS 4              // the most distinct sounds remembered during one Send all

typedef u32 (*Gather)(u32 hunter, u32 empty_if_none, u32 notify);
typedef void (*Give)(u32 hunter, u32 item, u32 quantity, u32 notify, u32 zero);
typedef u32 (*Add)(u32 hunter, u32 item, u32 quantity, u32 flags, u32 zero);
typedef u32 (*Is)(u32 self);
typedef u32 (*Is2)(u32 self, u32 a);
typedef u32 (*PopUse)(u32 spop, u32 id, u32 *pop);
typedef u32 (*PopItem)(u32 pop, u16 *item, u8 *quantity);
typedef u32 (*IsCarried)(u32 items, u32 item);
typedef u32 (*Message)(u32 kind, u32 id, u32 item, u32 quantity, u32 extra);
typedef u32 (*Query)(void);
typedef u32 (*Prompt)(u32 item, u32 quantity);
typedef u32 (*Deliverable)(u32 slot, u32 item);
typedef u32 (*Section)(u32 ui, u32 section);
typedef u32 (*Component)(u32 ui, u32 index);
typedef u32 (*Send)(u32 ui, u32 slot);
typedef void (*Special)(u32 result, u32 slot, u32 zero);
typedef u32 (*Data)(u32 items, u32 item);
typedef u32 (*Sound)(u32 kind, u32 id);
typedef u32 (*EquipAdd)(u32 box, u32 zero, u32 piece);
typedef u32 (*BoxAdd)(u32 box, u32 item, s32 quantity, u32 zero, s32 *left);
typedef u32 (*Event)(u32 self, u32 kind, u32 event, void *info);
typedef void (*MenuSet)(u32 uio, u32 rows, u32 a, u32 b);
typedef void (*Rows)(u32 uio, u32 list, u32 flag);
typedef void (*Looks)(u32 uio, u32 enabled, u32 row);
typedef void (*Method)(u32 self);
typedef u32 (*Method4)(u32 self, u32 a1, u32 a2, u32 a3);
typedef void (*SetText)(u32 uio, u32 inst, u32 obj, const char *text);
typedef void (*SetInt)(u32 uio, u32 inst, u32 obj, u32 v);
typedef void (*SetInt3)(u32 uio, u32 inst, u32 v);
typedef void (*MsgSet)(u32 uio, u32 inst, u32 obj, u32 zero, u32 one, u32 message);
typedef u32 (*Look)(u32 enabled, u32 selected, u32 a, u32 b);
typedef const char *(*EntryText)(u32 entry);
typedef float (*GetY)(u32 gui, u32 inst);
typedef void (*SetY)(u32 gui, u32 inst, float y);
typedef void (*Sync)(u32 cursor, u32 uio, u32 inst);

static u32 g_main; // the game's main module

// A game function, and a pointer read through the game's GOT
#define FN(type, off) ((type)(g_main + (off)))
#define GLOBAL(got) (*(u32 *)*(u32 *)(g_main + (got)))
// A hook's trampoline to the original function
#define ORIGINAL(type, hook) ((type)(hook).original)

static ForgeHook g_gather_hook, g_add_hook, g_add_quest_hook, g_message_hook, g_play_dead_hook, g_prompt_hook;
static bool g_gather_hooked;
static ForgeHook g_deliverable_hook, g_clear_account_hook;
static bool g_delivery_hooked, g_deliver_anywhere = true;
static ForgeHook g_section_hook, g_rows_hook, g_looks_hook, g_reward_update_hook, g_sound_hook;
static ForgeHook g_appraisal_hook, g_appraisal_help_hook;
// Other reward-context helpers are retained from upstream but are not initialized here.
static ForgeHook g_monnyan_init_hook, g_monnyan_hook, g_gh_hook, g_trade_hook, g_trade_menu_hook;
// One switch sends all on the quest reward and appraisal, farm trade cart and Meownster Hunters menus; each keeps its
// own hooks, and works when they did
static bool g_rewards_hooked, g_trade_hooked, g_monnyan_hooked;
static bool g_send_all = true, g_send_all_ready; // (ready: one of the three hooked)
static bool send_rewards(void) { return g_send_all && g_rewards_hooked; }
static bool send_trade(void) { return g_send_all && g_trade_hooked; }
static bool send_monnyan(void) { return g_send_all && g_monnyan_hooked; }

// The switches in Forge's menu
static bool g_gather_all = true, g_roll_all = false;


// The compiler may emit memset calls; there is no C library to provide one
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }

// ================================================================== Gathering
//
// The game's gather (GATHER) runs once per gathering motion. It asks sPop to use up one gather of the spot the hunter
// faces (POP_USE), takes the spot's item (POP_ITEM: a fixed item, or a roll of the spot's table) and gives it to the
// hunter (GIVE: skill bonuses, the pouch, the "obtained" message, or the carry motion for eggs and the like).
//
// Carving uses the same spots: a carcass's carves are a spot's gathers (made from the monster's carve table), so they
// are taken at once too.
//
// The plugin hooks GATHER. After a gather that put an item in the pouch, it repeats those three steps until the spot
// is used up, following the same rules as pressing the button again. It stops at the carve spots of a living monster,
// where the carves are meant to be limited by the monster getting up, not by the spot's count:
//   - Gypceros playing dead (uEm009_00): starting that action (its vtable +0x204, EM009_SET_ACTION) makes a spot from
//     its carve table, which it keeps at its data +0x4C and removes when it gets up. Nothing tells it about carves; it
//     gets up when the action ends, so in the game there is time for about one carve. Taken at once, every carve of
//     the spot came out, each time it played dead. The plugin notes the spot when the action starts;
//   - a spot that belongs to a monster that is told about carves (POP_ENEMY, and its uEnemy virtual 0x338 isn't 0):
//     the hunter's carve tells the monster, which may end the spot. Only uEm056_00 has one, made while it is alive;
//   - at a spot with an unlimited count (it would never run out);
//   - before an item carried in the hands (eggs, powderstone...): that gather isn't used, so the next press gathers
//     it as usual; likewise when the hunter is already carrying something;
//   - when the spot runs out or breaks early (sPop's own count and early-break roll).
// There is no single-gather button any more: it was R, the sprint button, so sprinting to a spot gathered only one.
// An item at its limit is lost, as when pressing A again in the game.
//
// A full pouch doesn't stop it: every gather is rolled, the pouch takes what it can (an item it holds already stacks),
// an item at its limit is lost as in the game ("can't carry any more"), and an item with no free slot waits. The game
// would open the replace window for it (POUCH_PROMPT), one item a press: the plugin holds those back while the spot
// is gathered and opens the window for them once it's done, one after the other as each is closed (forge_onUpdate).
// With that switch off it stops at the first item the pouch refuses, as it did before.
// The plugin rolls the item before using up the gather (the game does it the other way round), so stopping for a
// carried item costs nothing. Every item still gets its own roll.
//
// Messages: the pouch posts an "obtained" line per item (MESSAGE, kind 0, message 9), and a line when it can't take
// one (message 12, the pouch is full or the item at its limit). While a spot is gathered they are held back in order,
// with the lines of the same item merged into one with the summed quantity ("Iron Ore x3"). They are posted when the
// spot is done, all in one notification: the log puts the lines posted between MSG_BEGIN and MSG_END in one box, as
// the game does for its own lists. A box takes 6 lines (the log drops a 7th), so a longer list goes on in a new box.
// The pouch's line stays where it came, after the items the pouch took.

static u32 g_add_result = ADD_NONE; // the pouch's answer to the last item given
static u32 g_last_extra, g_total;   // for Forge's menu: extra items from the last spot, and this session
static const char *g_last_stop = "";

// The quest hunter's override calls the base function, and returns last, so the outer answer is the one kept.
static u32 add_hook(u32 hunter, u32 item, u32 quantity, u32 flags, u32 zero) {
    return g_add_result = ORIGINAL(Add, g_add_hook)(hunter, item, quantity, flags, zero);
}

static u32 add_quest_hook(u32 hunter, u32 item, u32 quantity, u32 flags, u32 zero) {
    return g_add_result = ORIGINAL(Add, g_add_quest_hook)(hunter, item, quantity, flags, zero);
}

// The messages held back while a spot is gathered
static bool g_holding;
static u32 g_queued;
static struct { u32 kind, id, item, quantity, extra; } g_queue[MAX_QUEUED];

static u32 message_hook(u32 kind, u32 id, u32 item, u32 quantity, u32 extra) {
    if (!g_holding) return ORIGINAL(Message, g_message_hook)(kind, id, item, quantity, extra);
    if (id != MSG_OBTAINED) // (the same warning for each gather of an item at its limit: once)
        for (u32 i = 0; i < g_queued; i++)
            if (g_queue[i].kind == kind && g_queue[i].id == id && g_queue[i].item == item &&
                g_queue[i].quantity == quantity)
                return 0;
    if (kind == 0 && id == MSG_OBTAINED) {
        for (u32 i = 0; i < g_queued; i++) {
            if (g_queue[i].kind == 0 && g_queue[i].id == MSG_OBTAINED && g_queue[i].item == item) {
                u32 sum = g_queue[i].quantity + quantity;
                g_queue[i].quantity = sum > 0xFF ? 0xFF : sum; // (the game passes it as a byte)
                return 0;
            }
        }
    }
    if (g_queued == MAX_QUEUED) return ORIGINAL(Message, g_message_hook)(kind, id, item, quantity, extra);
    g_queue[g_queued].kind = kind, g_queue[g_queued].id = id, g_queue[g_queued].item = item;
    g_queue[g_queued].quantity = quantity, g_queue[g_queued].extra = extra;
    g_queued++;
    return 0; // (the pouch doesn't look at the result)
}

static void hold_messages(bool on) {
    g_holding = on && g_message_hook.original;
    g_queued = 0;
}

static void post_messages(void) {
    g_holding = false;
    bool boxed = g_queued > 1;
    for (u32 i = 0; i < g_queued; i++) {
        if (boxed && i % BOX_LINES == 0) {
            if (i) FN(Query, MSG_END)();
            FN(Query, MSG_BEGIN)();
        }
        ORIGINAL(Message, g_message_hook)(g_queue[i].kind, g_queue[i].id, g_queue[i].item, g_queue[i].quantity,
                                          g_queue[i].extra);
    }
    if (boxed) FN(Query, MSG_END)();
    g_queued = 0;
}

static u32 find_pop(u32 spop, u32 id) {
    for (u32 pop = *(u32 *)(spop + SPOP_ACTIVE); pop; pop = *(u32 *)(pop + POP_NEXT))
        if (*(u16 *)(pop + POP_ID) == id) return pop;
    return 0;
}

static bool pouch_took_it(void) { return g_add_result != ADD_NONE && g_add_result <= ADD_TAKEN_MAX; }

// The pouch's answer lets the spot go on: taken, or (rolling every gather) at its limit or waiting for a free slot
static bool keep_rolling(void) {
    if (pouch_took_it()) return true;
    return g_roll_all && g_prompt_hook.original && (g_add_result == ADD_AT_LIMIT || g_add_result == ADD_NO_ROOM);
}

// The items waiting for the replace window: noted while a spot is gathered, shown once it's done
static bool g_spot;
static u32 g_lefts, g_prompt_wait;
static struct { u16 item; u8 quantity; } g_left[MAX_LEFT];

static u32 prompt_hook(u32 item, u32 quantity) {
    if (!g_spot) return ORIGINAL(Prompt, g_prompt_hook)(item, quantity);
    for (u32 i = 0; i < g_lefts; i++) {
        if (g_left[i].item == item) {
            u32 sum = g_left[i].quantity + quantity;
            g_left[i].quantity = sum > 0xFF ? 0xFF : sum;
            return 0;
        }
    }
    if (g_lefts < MAX_LEFT) g_left[g_lefts].item = item, g_left[g_lefts].quantity = quantity, g_lefts++;
    return 0; // (the pouch doesn't look at it)
}

// Opens the replace window for the next waiting item when it's free. Called after a spot and every frame.
static void prompt_next(void) {
    if (!g_lefts || g_spot) return;
    u32 item = FN(Query, PROMPT_ITEM)();
    if (!item) { // (no window: the quest is over)
        if (++g_prompt_wait > PROMPT_TRIES) {
            forge_log(LOG_WARN, "field-qol: no replace window, %u waiting items left behind", g_lefts);
            g_lefts = g_prompt_wait = 0;
        }
        return;
    }
    g_prompt_wait = 0;
    u8 *pending = (u8 *)(item - PROMPT_PENDING);
    if (*pending || FN(Query, PROMPT_OPEN)()) return; // (one is waiting or open: the next waits for it)
    ORIGINAL(Prompt, g_prompt_hook)(g_left[0].item, g_left[0].quantity);
    if (!*pending) return; // (not taken: again next frame)
    for (u32 i = 1; i < g_lefts; i++) g_left[i - 1] = g_left[i];
    g_lefts--;
}

static bool carrying(u32 hunter) { return (*(u32 *)(hunter + HUNTER_STATE) & STATE_CARRYING) != 0; }

// The carve spots of Gypceros playing dead, noted each time one starts or ends an action
static struct { u32 gypceros; s32 pop; } g_playing_dead[MAX_PLAYING_DEAD];

static u32 play_dead_hook(u32 gypceros, u32 kind, u32 action, u32 a3) {
    u32 r = ORIGINAL(Method4, g_play_dead_hook)(gypceros, kind, action, a3);
    u32 data = *(u32 *)(gypceros + EM009_DATA);
    s32 pop = data ? *(s32 *)(data + EM009_DEAD_POP) : -1;
    u32 slot = MAX_PLAYING_DEAD;
    for (u32 i = 0; i < MAX_PLAYING_DEAD && slot == MAX_PLAYING_DEAD; i++)
        if (g_playing_dead[i].gypceros == gypceros) slot = i;
    for (u32 i = 0; i < MAX_PLAYING_DEAD && slot == MAX_PLAYING_DEAD; i++)
        if (!g_playing_dead[i].gypceros || g_playing_dead[i].pop < 0) slot = i; // (a free entry)
    if (slot < MAX_PLAYING_DEAD) g_playing_dead[slot].gypceros = gypceros, g_playing_dead[slot].pop = pop;
    return r;
}

// A carve spot of a living monster: Gypceros playing dead, or a monster that is told about carves
static bool living_monster(u32 pop) {
    u16 id = *(u16 *)(pop + POP_ID);
    for (u32 i = 0; i < MAX_PLAYING_DEAD; i++)
        if (g_playing_dead[i].gypceros && g_playing_dead[i].pop >= 0 && (u16)g_playing_dead[i].pop == id) return true;
    u32 enemy = FN(Is, POP_ENEMY)(pop);
    if (!enemy) return false;
    Is carve_state = *(Is *)(*(u32 *)enemy + ENEMY_CARVE_STATE);
    return carve_state(enemy) != 0;
}

// Gathers the rest of the spot after the game's own gather. Returns why it stopped (shown in Forge's menu).
static const char *gather_rest(u32 hunter, u32 notify, u32 *extra) {
    u32 spop = GLOBAL(GOT_SPOP), items = GLOBAL(GOT_ITEMS);
    s16 sid = *(s16 *)(hunter + HUNTER_POP_ID);
    if (!spop || sid < 0) return "no spot";
    u32 id = (u16)sid;
    for (*extra = 0; *extra < MAX_EXTRA;) {
        u32 pop = find_pop(spop, id);
        if (!pop) return "spot gone";
        if (living_monster(pop)) return "living monster";
        if (*(u16 *)(pop + POP_LEFT) == LEFT_UNLIMITED) return "unlimited spot";
        if (!FN(Is, POP_CAN_GATHER)(pop)) return "used up";
        if (carrying(hunter)) return "carrying";

        // roll first: a carried item is left for a press of its own
        u16 item = 0;
        u8 quantity = 0;
        if (FN(PopItem, POP_ITEM)(pop, &item, &quantity) != 1 || !item) return "no item";
        if (FN(IsCarried, IS_CARRIED)(items, item)) return "carried item";

        u32 used = 0, left = FN(PopUse, POP_USE)(spop, id, &used);
        if (left != 1 && left != 2) return "used up";
        g_add_result = ADD_NONE;
        FN(Give, GIVE)(hunter, item, quantity, notify, 0);
        ++*extra;
        if (!keep_rolling()) return "pouch refused";
        if (left == 2) return "used up";
    }
    return "limit";
}

static u32 gather_hook(u32 hunter, u32 empty_if_none, u32 notify) {
    bool on = g_gather_all && hunter && FN(Is, IS_LOCAL)(hunter) == 1;
    g_add_result = ADD_NONE;
    hold_messages(on);
    g_spot = on && g_roll_all;
    u32 r = ORIGINAL(Gather, g_gather_hook)(hunter, empty_if_none, notify);
    // (when the first item became a carried item, or the pouch refused it, the press ends as usual)
    if (on && r == 1 && keep_rolling()) {
        u32 extra = 0;
        g_last_stop = gather_rest(hunter, notify, &extra);
        g_last_extra = extra;
        g_total += extra;
        if (extra)
            forge_log(LOG_INFO, "field-qol: %u more from spot %d (%s)", extra, *(s16 *)(hunter + HUNTER_POP_ID),
                      g_last_stop);
    }
    g_spot = false;
    post_messages();
    prompt_next();
    return r;
}

// Field QoL v0.7.0: relax the camp check only while the original menu queries run.
static u32 local_hunter(void) {
    u32 players = GLOBAL(GOT_PLAYERS);
    return players ? FN(Is, LOCAL_HUNTER)(players) : 0;
}

static u8 *delivery_away_flag(void) {
    if (!g_delivery_hooked || !g_deliver_anywhere) return NULL;
    u32 hunter = local_hunter();
    u8 *away = hunter ? (u8 *)(hunter + HUNTER_AWAY) : NULL;
    return away && *away ? away : NULL;
}

static u32 deliverable_hook(u32 slot, u32 item) {
    u8 *away = delivery_away_flag();
    u8 previous = away ? *away : 0;
    if (away) *away = 0;
    u32 result = ORIGINAL(Deliverable, g_deliverable_hook)(slot, item);
    if (away) *away = previous;
    return result;
}

static u32 clear_account_hook(void) {
    u8 *away = delivery_away_flag();
    u8 previous = away ? *away : 0;
    if (away) *away = 0;
    u32 result = ORIGINAL(Query, g_clear_account_hook)();
    if (away) *away = previous;
    return result;
}

#include "lang.inc"
#include "rewards.inc"

// ================================================================== Ambient temperature
// uPlayer vtable +1A8/+1AC: 0 = no heat/cold, 1-3 = protected, 4-7 = exposed.
// The original heat HP tick at main+2A5294 and cold stamina-limit calculation
// at main+2A5A64 use these exact queries. Do not alter health/stamina setters,
// armor skills, drink timers, the area's actual climate, or other damage paths.
#define HEAT_STATE 0x2A6B4C
#define COLD_STATE 0x2A6C68
static ForgeHook g_heat_hook, g_cold_hook;
static bool g_climate_hooked, g_no_extreme = true;

static u32 heat_state_hook(u32 hunter) {
    if (g_no_extreme && hunter && FN(Is, IS_LOCAL)(hunter)) return 0;
    return ORIGINAL(Is, g_heat_hook)(hunter);
}

static u32 cold_state_hook(u32 hunter) {
    if (g_no_extreme && hunter && FN(Is, IS_LOCAL)(hunter)) return 0;
    return ORIGINAL(Is, g_cold_hook)(hunter);
}

// ================================================================== Tool-free gathering
// Categories 0/1 are pickaxes/bug nets. These are not general item functions.
// Virtual iron pickaxe / bug net IDs only select the normal, non-gold icons.
#define TOOL_CHECK 0x300224
#define TOOL_BREAK 0x2E02D4
#define TOOL_RESOLVE 0x2F80F8
typedef u32 (*ToolCheck)(u32 hunter, u32 category, u16 *tool);
typedef void (*ToolBreak)(u32 hunter);
typedef u32 (*ToolResolve)(u32 hunter, u16 *kind, u16 *action, u16 *other,
                           u32 angle, u32 item, u32 extra, u32 context);
typedef u32 (*CanUseTool)(u32 hunter, u32 item, u32 *pop);
extern float atan2f(float y, float x);
static ForgeHook g_tool_check_hook, g_tool_break_hook, g_tool_resolve_hook;
static bool g_toolless = true, g_toolless_hooked;

static u32 tool_check_hook(u32 hunter, u32 category, u16 *tool) {
    if (g_toolless && category <= 1) {
        if (tool) *tool = category == 0 ? 0x3A : 0x3D;
        return 1;
    }
    return ORIGINAL(ToolCheck, g_tool_check_hook)(hunter, category, tool);
}

static void tool_break_hook(u32 hunter) {
    if (!g_toolless) ORIGINAL(ToolBreak, g_tool_break_hook)(hunter);
}

static u32 tool_resolve_hook(u32 hunter, u16 *kind, u16 *action, u16 *other,
                             u32 angle, u32 item, u32 extra, u32 context) {
    // MHGU Recomp dispatches game calls only at registered function entries.
    // Never branch into the middle of the original resolver (v1.2 did so).
    // Mirror its six tool cases, retaining its full virtual eligibility call.
    if (g_toolless && item >= 0x39 && item <= 0x3E) {
        *kind = *action = 0xFF;
        u32 pop = 0;
        u32 vtable = *(u32 *)hunter;
        CanUseTool can_use = *(CanUseTool *)(vtable + 0x5BC);
        if (!can_use(hunter, item, &pop) || !pop) return 0;
        *(u32 *)context = pop;
        if (item <= 0x3B) {
            float dx = *(float *)(pop + 4) - *(float *)(hunter + 0x40);
            float dz = *(float *)(pop + 0xC) - *(float *)(hunter + 0x48);
            // Original main+2F8B1C uses this exact angle conversion constant.
            float scaled = atan2f(dx, dz) * *(const float *)(g_main + 0x2F8E70);
            *(u32 *)angle = (u16)(s32)(scaled + 0.5f);
        }
        *kind = 1;
        *action = item <= 0x3B ? 0x51 : 0x52;
        return 1;
    }
    return ORIGINAL(ToolResolve, g_tool_resolve_hook)(hunter, kind, action, other, angle, item, extra, context);
}

#include "mood.inc"

// Shared by the combination menu and the roll that selects the recipe result.
// A null recipe keeps the game's original rate. Ingredient validation,
// consumption and yield are handled separately by the game.
#define COMBINE_RATE 0x194754
typedef float (*CombineRate)(u32 items, const void *recipe, u32 book_source);
static ForgeHook g_combine_rate_hook;
static bool g_combine_always = true, g_combine_hooked;

static float combine_rate_hook(u32 items, const void *recipe, u32 book_source) {
    if (g_combine_always && recipe) return 100.0f;
    return ORIGINAL(CombineRate, g_combine_rate_hook)(items, recipe, book_source);
}

#ifdef PERSONAL_RUNTIME_PROBE
// Only in the isolated test build: exercise the real runtime's guest dispatch
// and floating-point import, using plugin-owned dummy objects (no saved hunter).
static u32 probe_hunter[32], probe_vtable[0x600 / 4], probe_pop[4];
static bool probe_allowed = true;
static u32 probe_can_use(u32 hunter, u32 item, u32 *pop) {
    if (hunter != (u32)probe_hunter || item < 0x39 || item > 0x3E) return 0;
    *pop = probe_allowed ? (u32)probe_pop : 0;
    return probe_allowed;
}
static bool runtime_probe(void) {
    probe_hunter[0] = (u32)probe_vtable;
    probe_vtable[0x5BC / 4] = (u32)probe_can_use;
    *(float *)((u32)probe_pop + 4) = 1.0f;
    *(float *)((u32)probe_pop + 0xC) = 1.0f;
    for (u32 item = 0x39; item <= 0x3E; item++) {
        u16 kind = 0, action = 0, other = 0;
        u32 angle = 0, context = 0;
        u32 r = tool_resolve_hook((u32)probe_hunter, &kind, &action, &other,
                                  (u32)&angle, item, 0, (u32)&context);
        if (r != 1 || kind != 1 || action != (item < 0x3C ? 0x51 : 0x52) ||
            context != (u32)probe_pop || (item < 0x3C && angle != 8192)) return false;
        probe_allowed = false;
        if (tool_resolve_hook((u32)probe_hunter, &kind, &action, &other,
                              (u32)&angle, item, 0, (u32)&context) != 0) return false;
        probe_allowed = true;
    }
    return true;
}
static u32 probe_climate_hunter[0x13D0 / 4];
static bool climate_runtime_probe(void) {
    u32 hunter = (u32)probe_climate_hunter;
    probe_climate_hunter[0x13CC / 4] = 1; // The actual game's IS_LOCAL reads this bit.
    if (FN(Is, HEAT_STATE)(hunter) || FN(Is, COLD_STATE)(hunter)) return false;
    // Prowler flag: the original query exits before touching area services.
    // This checks the real runtime's trampoline as well as the enabled detour.
    probe_climate_hunter[0x13CC / 4] = 9;
    g_no_extreme = false;
    bool restored = FN(Is, HEAT_STATE)(hunter) == 0 && FN(Is, COLD_STATE)(hunter) == 0;
    g_no_extreme = true;
    probe_climate_hunter[0x13CC / 4] = 8; // Remote: use the original query.
    return restored && FN(Is, HEAT_STATE)(hunter) == 0 && FN(Is, COLD_STATE)(hunter) == 0;
}
static u32 probe_palico[OTOMO_STRIDE / 4];
static bool mood_runtime_probe(void) {
    u32 palico = (u32)probe_palico;
    *(u32 *)(palico + 0x23C) = 12345;
    *(u8 *)(palico + 0x240) = 0;
    u32 entries[] = {MOOD_SET, MOOD_ADD, MOOD_SUB};
    for (u32 i = 0; i < 3; i++) {
        *(u8 *)(palico + 0x242) = 1;
        *(s16 *)(palico + 0x35C) = 1;
        FN(MoodChange, entries[i])(palico, 32767, 0);
        if (*(u8 *)(palico + 0x242) != 99 || *(s16 *)(palico + 0x35C) != 99 ||
            FN(MoodTier, MOOD_TIER)(palico) != 4 ||
            *(u32 *)(palico + 0x23C) != 12345 || *(u8 *)(palico + 0x240) != 0) return false;
    }
    g_max_mood = false;
    *(s16 *)(palico + 0x35C) = 0;
    bool restored = FN(MoodTier, MOOD_TIER)(palico) == 0;
    g_max_mood = true;
    return restored && FN(MoodTier, MOOD_TIER)(palico) == 4;
}

static bool combine_runtime_probe(void) {
    u32 recipe[8] = {0};
    CombineRate rate = FN(CombineRate, COMBINE_RATE);
    return rate(0, recipe, 0) == 100.0f && rate(0, recipe, 1) == 100.0f &&
           rate(0, NULL, 0) == 0.0f;
}
#endif

// Another Forge plugin's hook: Forge writes `ldr pc, [pc, #-4]; detour` over the hooked function's first two words.
// MHGU Recomp's Forge keeps the game's words under it and chains a second hook after the first (forge.c hook_install:
// the new hook's original runs the earlier detour), so a function another plugin hooked can be hooked or called here
// too. Before 1.16.2 such a function failed its fingerprint below and the whole plugin turned itself off ("hooks
// unavailable" on every line), depending only on which mod loaded first (reported with Faster Field Hunter).
#define FORGE_JUMP 0xE51FF004

// The game's code at main+off is MHGU 1.4.0's (its first two words; second 0: the first only), or another plugin's
// Forge hook is there
static bool code_is(u32 off, u32 first, u32 second) {
    const volatile u32 *code = (const volatile u32 *)(g_main + off);
    if (code[0] == first && (!second || code[1] == second)) return true;
    if (code[0] != FORGE_JUMP) return false;
    forge_log(LOG_WARN, "personal-gathering: main+%x is already hooked by another plugin; sharing it", off);
    return true;
}

// The features the fingerprints below belong to
enum {
    FEAT_GATHER = 1 << 0, FEAT_TOOLS = 1 << 1, FEAT_REWARDS = 1 << 2, FEAT_TRADE = 1 << 3,
    FEAT_MONNYAN = 1 << 4, FEAT_CLIMATE = 1 << 5, FEAT_MOOD = 1 << 6, FEAT_COMBINE = 1 << 7,
    FEAT_ALL = 0xFF,
};
#define WRONG_BUILD_MISMATCHES 4  // this many fingerprints differing: not MHGU 1.4.0 at all

// Fingerprints from original MHGU 1.4.0 main FB08F1D20FD1204F..., checked before any hook is installed. Returns the
// features to leave off: one whose code differs (a cheat, a patch or another mod changed it) is skipped on its own
// and the others still work; when several differ the game is not 1.4.0 and everything stays off, as before.
static u32 incompatible_features(void) {
    static const u32 signature[][4] = {
        {GATHER, 0xE92D48F0, 0xE24DD010, FEAT_GATHER},
        {PLAYER_ADD, 0xE92D4070, 0xE1A04003, FEAT_GATHER},
        {PLAYER_ADD_QUEST, 0xE92D41F0, 0xE24DD008, FEAT_GATHER},
        {MESSAGE, 0xE92D48F0, 0xE24DD008, FEAT_GATHER},
        {POUCH_PROMPT, 0xE92D48F0, 0xE1A05000, FEAT_GATHER},
        {EM009_SET_ACTION, 0xE92D41F0, 0xE1A05000, FEAT_GATHER},
        {TOOL_CHECK, 0xE92D47F0, 0xE1A08002, FEAT_TOOLS},
        {TOOL_BREAK, 0xE92D41F0, 0xE24DD008, FEAT_TOOLS},
        {TOOL_RESOLVE, 0xE92D47F0, 0xE24DD008, FEAT_TOOLS},
        {0x2F8E70, 0x4622F983, 0xE1A00006, FEAT_TOOLS},
        {RESULT_SECTION, 0xE92D4FF0, 0xE24DD01C, FEAT_REWARDS},
        {MENU_ROWS, 0xE92D4070, 0xE1A05001, FEAT_REWARDS | FEAT_MONNYAN},
        {MENU_LOOKS, 0xE92D41F0, 0xE1A07001, FEAT_REWARDS | FEAT_MONNYAN},
        {REWARD_UPDATE, 0xE92D4010, 0xED2D8B02, FEAT_REWARDS | FEAT_MONNYAN},
        {APPRAISAL_HELP, 0xE92D4830, 0xE24DD010, FEAT_REWARDS},
        {APPRAISAL_UPDATE, 0xE92D4FF0, 0xE24DD00C, FEAT_REWARDS},
        {UI_SOUND, 0xE3500006, 0x812FFF1E, FEAT_REWARDS | FEAT_MONNYAN},
        {TRADE_CLAIM, 0xE92D4FF0, 0xE24DD034, FEAT_TRADE},
        {TRADE_MENU_DRAW, 0xE92D4BF0, 0xE24DD008, FEAT_TRADE},
        {MONNYAN_INIT, 0xE92D4010, 0xE1A04000, FEAT_MONNYAN},
        {MONNYAN_UPDATE, 0xE92D41F0, 0xE24DD010, FEAT_MONNYAN},
        {0x1535A0, 0xE5950000, 0xE5D010A5, FEAT_REWARDS | FEAT_TRADE | FEAT_MONNYAN},
        {HEAT_STATE, 0xE92D4070, 0xE1A04000, FEAT_CLIMATE},
        {COLD_STATE, 0xE92D4070, 0xE1A04000, FEAT_CLIMATE},
        {MOOD_SET, 0xE92D41F0, 0xE24DD018, FEAT_MOOD},
        {MOOD_ADD, 0xE92D4BF0, 0xE24DD018, FEAT_MOOD},
        {MOOD_SUB, 0xE92D4BF0, 0xE24DD018, FEAT_MOOD},
        {MOOD_TIER, 0xE59F1088, 0xE5D0035C, FEAT_MOOD},
        {0x2595F0, 0xE3510053, 0x8A00000E, FEAT_MOOD},
        {0x259608, 0xE7F60000, 0xE1005780, FEAT_MOOD},
        {0x0E92A8, 0xE5D00271, 0xE12FFF1E, FEAT_MOOD},
        {COMBINE_RATE, 0xE92D4070, 0xED2D8B04, FEAT_COMBINE},
    };
    u32 off = 0, mismatches = 0;
    for (u32 i = 0; i < sizeof signature / sizeof *signature; i++) {
        if (code_is(signature[i][0], signature[i][1], signature[i][2])) continue;
        forge_log(LOG_ERROR, "personal-gathering: MHGU 1.4.0 code mismatch at main+%x (changed by a cheat, patch or "
                  "another mod?); the features using it are off", signature[i][0]);
        off |= signature[i][3];
        mismatches++;
    }
    if (mismatches >= WRONG_BUILD_MISMATCHES) {
        forge_log(LOG_ERROR, "personal-gathering: %u code mismatches: not MHGU 1.4.0; disabled", mismatches);
        return FEAT_ALL;
    }
    return off;
}

static bool hook(ForgeHook *h, u32 target, void *detour) {
    u32 original = 0;
    forge_hook_create(h, g_main + target, (u32)detour, &original);
    return h->original != 0;
}

static bool install_delivery_hooks(void) {
    if (!code_is(DELIVERABLE, 0xE92D4830, 0xE59F0134) || !code_is(CLEAR_ACCOUNT, 0xE92D4FF0, 0xE24DD004))
        return false;

    bool deliver_ok = hook(&g_deliverable_hook, DELIVERABLE, deliverable_hook);
    bool clear_ok = hook(&g_clear_account_hook, CLEAR_ACCOUNT, clear_account_hook);
    return deliver_ok && clear_ok;
}

#include "better_box.inc"
#include "camp_box.inc"
#include "field_options.inc"
#include "movement.inc"
#include "auto_delivery.inc"
#include "settings.inc"

EXPORT void forge_onLoad(ForgePluginInfo *info) {
    static const char name[] = "MHGU Recomp QoL 1.17.1";
    if (!info) return;
    // Forge checks these fields against the loader version before initialization.
    info->major = 1, info->minor = 5, info->patch = 1;
    memset(info->name, 0, sizeof info->name);
    for (u32 i = 0; i < sizeof name; i++) info->name[i] = name[i];
}

EXPORT void forge_onInit(void) {
    g_main = forge_mem_getMainTextAddr();
    u32 off = g_main ? incompatible_features() : FEAT_ALL;
    if (off == FEAT_ALL) {
        g_toolless = g_gather_all = false;
        g_main = 0;
        return;
    }
    // (a feature whose code differs: none of its hooks; it shows "unavailable" in the menu as a failed hook does)
    bool gather = !(off & FEAT_GATHER), tools = !(off & FEAT_TOOLS), rewards = !(off & FEAT_REWARDS);
    bool trade = !(off & FEAT_TRADE), monnyan = !(off & FEAT_MONNYAN), climate = !(off & FEAT_CLIMATE);
    bool mood = !(off & FEAT_MOOD), combine = !(off & FEAT_COMBINE);
    settings_load(); // (before the hooks: a feature whose hooks fail is still switched off below)
    init_better_box();
    init_camp_box();
    init_field_options();
    init_movement_options();
    init_auto_delivery();
    bool tool_check_ok = tools && hook(&g_tool_check_hook, TOOL_CHECK, tool_check_hook);
    bool tool_break_ok = tools && hook(&g_tool_break_hook, TOOL_BREAK, tool_break_hook);
    bool tool_resolve_ok = tools && hook(&g_tool_resolve_hook, TOOL_RESOLVE, tool_resolve_hook);
    g_toolless_hooked = tool_check_ok && tool_break_ok && tool_resolve_ok;
    if (!g_toolless_hooked) g_toolless = false;
    forge_log(g_toolless_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_toolless_hooked ? "tool-free mining and bug catching enabled; entry-only calls; normal tool icons" : "tool hooks FAILED; tools required");
    bool add_ok = gather && hook(&g_add_hook, PLAYER_ADD, add_hook);
    bool quest_add_ok = gather && hook(&g_add_quest_hook, PLAYER_ADD_QUEST, add_quest_hook);
    bool pouch = add_ok && quest_add_ok;
    if (gather && !hook(&g_message_hook, MESSAGE, message_hook))
        forge_log(LOG_WARN, "personal-gathering: message hook failed; original notifications retained");
    bool carve_guard = gather && hook(&g_play_dead_hook, EM009_SET_ACTION, play_dead_hook);
    g_gather_hooked = pouch && carve_guard && hook(&g_gather_hook, GATHER, gather_hook);
    if (!g_gather_hooked) g_gather_all = false;
    forge_log(g_gather_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_gather_hooked ? "batch gathering/carving enabled; localized game messages; stop on full pouch"
                              : "gathering hooks FAILED; single gathering retained");
    g_delivery_hooked = install_delivery_hooks();
    forge_log(g_delivery_hooked ? LOG_INFO : LOG_WARN, "personal-gathering: %s",
              g_delivery_hooked ? "manual item pouch delivery enabled in any area"
                                : "delivery hooks unavailable; normal delivery rules retained");
    bool menu = rewards || monnyan; // (the reward menu's hooks serve both)
    bool rows_ok = menu && hook(&g_rows_hook, MENU_ROWS, rows_hook);
    bool looks_ok = menu && hook(&g_looks_hook, MENU_LOOKS, looks_hook);
    bool update_ok = menu && hook(&g_reward_update_hook, REWARD_UPDATE, reward_update_hook);
    bool sound_ok = menu && hook(&g_sound_hook, UI_SOUND, sound_hook);
    bool help_ok = rewards && hook(&g_appraisal_help_hook, APPRAISAL_HELP, appraisal_help_hook);
    bool appraisal_ok = rows_ok && looks_ok && update_ok && help_ok &&
                        hook(&g_appraisal_hook, APPRAISAL_UPDATE, appraisal_hook);
    g_rewards_hooked = rows_ok && looks_ok && update_ok && sound_ok && appraisal_ok &&
                       hook(&g_section_hook, RESULT_SECTION, section_hook);
    forge_log(g_rewards_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_rewards_hooked ? "quest reward send-all enabled; Chinese menu row; stop on full box"
                               : "reward hooks FAILED; original reward menu retained");
    bool trade_menu_ok = trade && hook(&g_trade_menu_hook, TRADE_MENU_DRAW, trade_menu_hook);
    bool trade_claim_ok = trade && hook(&g_trade_hook, TRADE_CLAIM, trade_hook);
    g_trade_hooked = trade_menu_ok && trade_claim_ok;
    forge_log(g_trade_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_trade_hooked ? "farm trade send-all enabled; all three carts; preserve items on full box"
                             : "trade hooks FAILED; original farm claim menu retained");
    bool monnyan_init_ok = monnyan && hook(&g_monnyan_init_hook, MONNYAN_INIT, monnyan_init_hook);
    bool monnyan_update_ok = monnyan && hook(&g_monnyan_hook, MONNYAN_UPDATE, monnyan_hook);
    g_monnyan_hooked = rows_ok && looks_ok && update_ok && sound_ok && monnyan_init_ok && monnyan_update_ok;
    forge_log(g_monnyan_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_monnyan_hooked ? "Meownster Hunters send-all enabled; retain rewards on full box"
                               : "Meownster Hunters hooks FAILED; original menu retained");
    g_send_all_ready = g_rewards_hooked || g_trade_hooked || g_monnyan_hooked;
    if (!g_send_all_ready) g_send_all = false;
    forge_log(LOG_INFO, "personal-gathering: text language %s (auto: the game's eng/fre/spa/ger/ita/chT/chS, "
              "kor when its text is Korean)", LANGUAGE_KEYS[g_language < CHOICE_COUNT ? g_language : CHOICE_AUTO]);
    bool heat_ok = climate && hook(&g_heat_hook, HEAT_STATE, heat_state_hook);
    bool cold_ok = climate && hook(&g_cold_hook, COLD_STATE, cold_state_hook);
    g_climate_hooked = heat_ok && cold_ok;
    if (!g_climate_hooked) g_no_extreme = false;
    forge_log(g_climate_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_climate_hooked ? "ambient heat/cold disabled for local hunter; hot/cool drinks not required"
                               : "climate hooks FAILED; original temperature rules retained");
    bool mood_set_ok = mood && hook(&g_mood_set_hook, MOOD_SET, mood_set_hook);
    bool mood_add_ok = mood && hook(&g_mood_add_hook, MOOD_ADD, mood_add_hook);
    bool mood_sub_ok = mood && hook(&g_mood_sub_hook, MOOD_SUB, mood_sub_hook);
    bool mood_tier_ok = mood && hook(&g_mood_tier_hook, MOOD_TIER, mood_tier_hook);
    g_mood_hooked = mood_set_ok && mood_add_ok && mood_sub_ok && mood_tier_ok;
    if (!g_mood_hooked) g_max_mood = false;
    forge_log(g_mood_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_mood_hooked ? "Palico enthusiasm locked at 99; all owned slots; activity costs bypassed"
                            : "Palico mood hooks FAILED; original enthusiasm retained");
    g_combine_hooked = combine && hook(&g_combine_rate_hook, COMBINE_RATE, combine_rate_hook);
    if (!g_combine_hooked) g_combine_always = false;
    forge_log(g_combine_hooked ? LOG_INFO : LOG_ERROR, "personal-gathering: %s",
              g_combine_hooked ? "item combination success rate set to 100%"
                               : "combination rate hook FAILED; original rates retained");
#ifdef PERSONAL_RUNTIME_PROBE
    bool probe_ok = g_toolless_hooked && runtime_probe();
    forge_log(probe_ok ? LOG_INFO : LOG_ERROR,
              "personal-gathering: runtime action probe %s (six tools, mining angle, rejected state)",
              probe_ok ? "PASSED" : "FAILED");
    bool climate_probe_ok = g_climate_hooked && climate_runtime_probe();
    forge_log(climate_probe_ok ? LOG_INFO : LOG_ERROR,
              "personal-gathering: climate runtime probe %s (heat/cold queries, local/remote, original trampolines)",
              climate_probe_ok ? "PASSED" : "FAILED");
    bool mood_probe_ok = g_mood_hooked && mood_runtime_probe();
    forge_log(mood_probe_ok ? LOG_INFO : LOG_ERROR,
              "personal-gathering: Palico mood runtime probe %s (set/add/sub, original tier, independent toggle)",
              mood_probe_ok ? "PASSED" : "FAILED");
    bool combine_probe_ok = g_combine_hooked && combine_runtime_probe();
    forge_log(combine_probe_ok ? LOG_INFO : LOG_ERROR,
              "personal-gathering: combination rate runtime probe %s",
              combine_probe_ok ? "PASSED" : "FAILED");
#endif
    settings_start();
}

EXPORT void forge_onUpdate(float dt) {
    (void)dt;
    update_camp_box();
    update_sprint_stamina();
    update_auto_delivery();
    mood_update();
    update_field_shortcuts();
    settings_update();
}

EXPORT void forge_onImGuiInit(void *ctx, void *alloc, void *freefn) {
    if (!ctx || !alloc || !freefn) return;
    igSetCurrentContext(ctx);
    igSetAllocatorFunctions(alloc, freefn, NULL);
    load_hant_font();
}

// ------------------------------------------------------------------ the Forge menu
//
// The language list, then the options in groups: gathering, items and rewards, quests and delivery, hunting. An option
// whose hooks failed shows its name with "unavailable" (log.txt says why). A speed slider at x1, or the stamina use
// at 100%, changes nothing and shows "off".

static void render_unavailable(u32 id) {
    static char line[256];
    u32 at = text_append(line, 0, sizeof line, tr(id));
    text_append(line, at, sizeof line, tr(T_UNAVAILABLE));
    igTextUnformatted(line, NULL);
}

static void render_switch(u32 id, bool *on, bool available) {
    if (available) igCheckbox(tr(id), on);
    else render_unavailable(id);
}

static void render_slider(u32 id, float *v, float min, float max, const char *format, float off, bool available) {
    if (!available) {
        render_unavailable(id);
        return;
    }
    static char shown[64];
    if (*v == off) text_append(shown, text_append(shown, 0, sizeof shown, format), sizeof shown, tr(T_OFF));
    igSliderFloat(tr(id), v, min, max, *v == off ? shown : format, 0);
}

static void render_menu(void) {
    if (!igTreeNode_Str(tr(T_TITLE))) return;
    render_language();
    if (igTreeNode_Str(tr(T_GATHERING))) {
        render_switch(T_TOOLLESS, &g_toolless, g_toolless_hooked);
        render_switch(T_GATHER_ALL, &g_gather_all, g_gather_hooked);
        render_slider(T_GATHERING_SPEED, &g_gathering_speed, 1.0f, 4.0f, "x%.2f", 1.0f, g_motion_rate_hooked);
        if (g_gather_hooked) igTextUnformatted(tr(T_FULL_POUCH), NULL);
        igTreePop();
    }
    if (igTreeNode_Str(tr(T_ITEMS))) {
        render_switch(T_SEND_ALL, &g_send_all, g_send_all_ready);
        if (g_send_all_ready && !(g_rewards_hooked && g_trade_hooked && g_monnyan_hooked))
            igTextUnformatted(tr(T_PARTLY), NULL);
        render_switch(T_CARRY_ITEMS, &g_carry_items, g_carry_items_hooked);
        render_switch(T_COMBINE, &g_combine_always, g_combine_hooked);
        igTreePop();
    }
    if (igTreeNode_Str(tr(T_QUEST))) {
        render_switch(T_AUTO_GOALS, &g_auto_deliver_goals, g_auto_delivery_ready);
        render_switch(T_AUTO_ACCOUNT, &g_auto_deliver_account, g_auto_delivery_ready);
        render_switch(T_DELIVER_ANYWHERE, &g_deliver_anywhere, g_delivery_hooked);
        render_switch(T_FINISH_QUEST, &g_finish_quest, g_main != 0);
        render_switch(T_SKIP_DELAY, &g_skip_end_delay, g_main != 0);
        igTreePop();
    }
    if (igTreeNode_Str(tr(T_HUNTING))) {
        render_switch(T_NO_EXTREME, &g_no_extreme, g_climate_hooked);
        render_switch(T_AUTO_MAP, &qol_field_routes.map_enabled, g_map_hooked);
        render_switch(T_MARK_LARGE, &qol_field_routes.boss_enabled, g_boss_hooked);
        render_switch(T_THREE_ARTS, &g_three_arts, g_art_slots_hooked);
        render_slider(T_SPRINT_SPEED, &g_sprint_speed, 1.0f, 2.0f, "x%.2f", 1.0f, g_motion_rate_hooked);
        render_slider(T_STAMINA_USE, &g_sprint_stamina_percent, 0.0f, 100.0f, "%.0f%%", 100.0f,
                      g_sprint_stamina_ready);
        render_switch(T_MAX_MOOD, &g_max_mood, g_mood_hooked);
        igTreePop();
    }
    render_settings_status();
    igTreePop();
}

EXPORT void forge_onImGuiRender(void) {
    g_menu_language = text_language();
    if (g_menu_language == LANG_CHT && !g_hant_font) g_menu_language = LANG_CHS; // (no Traditional font: lang.inc)
    if (g_menu_language == LANG_CHT) igPushFont(g_hant_font, 0.0f);
    render_menu();
    if (g_menu_language == LANG_CHT) igPopFont();
}
