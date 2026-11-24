enum {
    GOT_DELIVERY_SYSTEM = 0x1838bb4,
    GOT_QUEST_STATE = 0x1838d28,
    GOT_QUEST_POUCH = 0x183a828,
    IS_QUEST_ACTIVE = 0x3f8634,
    QUEST_GOAL_INDEX = 0x3b3bf0,
    QUEST_ITEMS_DELIVERED = 0x3af160,
    POUCH_ITEM_COUNT = 0x19c21c,
    POUCH_GET_SLOT = 0x19b670,
    HAND_IN_GOAL_ITEM = 0x3b3444,
    HAND_IN_ACCOUNT_ITEM = 0x3b29fc,
    HAND_IN_POINTS_GOAL = 0x3b2dac,
    FIND_GOAL_WITH_FLAG = 0x3a8604,
    POINTS_GOAL_COMPLETE = 0x3b39d4,
    QUEST_DELIVERY_PROGRESS = 0x1778,
    QUEST_PHASE = 0x50,
    QUEST_PHASE_STARTED = 2,
    QUEST_DELIVERY_GOALS = 0xec,
    QUEST_DELIVERY_GOAL_COUNT = 3,
    GOAL_ITEM_DELIVERY = 0x2,
    GOAL_DELIVERY_STATUS_MASK = 0x62,
    GOAL_SPECIAL_DELIVERY = 0x10000,
    GOAL_POINTS_DELIVERY = 0x8000,
    PAW_PASS_TICKET = 0x6c6,
    FIRST_CARRIED_ITEM = 0x839,
    LAST_CARRIED_ITEM = 0x843,
    ITEM_IS_ACCOUNT = 0x12,
    POUCH_SLOT_COUNT = 50,
    AUTO_DELIVERY_INTERVAL = 15,
};

typedef struct {
    u32 flags;
    u16 item;
    u16 remaining;
    u16 target;
    u16 reserved;
} DeliveryGoal;

typedef struct {
    u16 item;
    s16 quantity;
} DeliveryPouchSlot;

typedef s32 (*DeliveryQuery)(u32 owner, u32 value);
typedef s32 (*DeliveryAction)(u32 progress, u32 value, s32 quantity);
typedef const DeliveryPouchSlot *(*DeliveryGetSlot)(u32 pouch, u32 slot, u32 flags);

static bool g_auto_delivery_ready;
static bool g_auto_deliver_goals = true; // (the main quest's and the subquest's)
static bool g_auto_deliver_account = true;
static u32 g_auto_delivery_delay;

static bool auto_delivery_can_use_item(u32 item) {
    // Preserve the original carry-in-hands delivery path even if the separate
    // transport-items option moves these items into the pouch.
    return item && item != PAW_PASS_TICKET && (item < FIRST_CARRIED_ITEM || item > LAST_CARRIED_ITEM);
}

static bool auto_delivery_matches_goal(const DeliveryGoal *goals, u32 item) {
    for (u32 i = 0; i < QUEST_DELIVERY_GOAL_COUNT; ++i) {
        if ((goals[i].flags & GOAL_ITEM_DELIVERY) && goals[i].item == item) return true;
    }
    return false;
}

static bool auto_deliver_goal_items(u32 progress, u32 pouch, const DeliveryGoal *goals) {
    for (u32 i = 0; i < QUEST_DELIVERY_GOAL_COUNT; ++i) {
        const DeliveryGoal *goal = &goals[i];
        u32 item = goal->item;
        if ((goal->flags & (GOAL_DELIVERY_STATUS_MASK | GOAL_SPECIAL_DELIVERY)) != GOAL_ITEM_DELIVERY ||
            !goal->target || !auto_delivery_can_use_item(item) ||
            FN(DeliveryQuery, QUEST_GOAL_INDEX)(progress, item) != (s32)i)
            continue;
        s32 count = FN(DeliveryQuery, POUCH_ITEM_COUNT)(pouch, item);
        if (count <= 0) continue;
        s32 needed = (s32)goal->target - FN(DeliveryAction, QUEST_ITEMS_DELIVERED)(progress, item, 0);
        if (needed <= 0) continue;
        s32 quantity = count < needed ? count : needed;
        // The original quest delivery path deducts the local pouch and sends
        // the player's absolute contribution using the game's existing packet.
        if (FN(DeliveryAction, HAND_IN_GOAL_ITEM)(progress, item, quantity)) return true;
    }
    return false;
}

static void auto_deliver_account_items(u32 progress, u32 items, u32 pouch, u32 quest,
                                       const DeliveryGoal *goals) {
    bool needs_points = FN(DeliveryQuery, FIND_GOAL_WITH_FLAG)(quest, GOAL_POINTS_DELIVERY) != -1 &&
                        !FN(Is, POINTS_GOAL_COMPLETE)(progress);
    u32 entry = needs_points ? HAND_IN_POINTS_GOAL : HAND_IN_ACCOUNT_ITEM;
    for (u32 slot = 0; slot < POUCH_SLOT_COUNT; ++slot) {
        const DeliveryPouchSlot *stack = FN(DeliveryGetSlot, POUCH_GET_SLOT)(pouch, slot, 1);
        if (!stack || stack->quantity <= 0 || !auto_delivery_can_use_item(stack->item) ||
            auto_delivery_matches_goal(goals, stack->item))
            continue;
        u32 data = FN(Data, ITEM_DATA)(items, stack->item);
        if (!data || !*(const u8 *)(data + ITEM_IS_ACCOUNT)) continue;
        // Let the original individual-delivery route credit points and decide
        // how much counts toward an active point objective.
        s16 quantity = stack->quantity;
        if (FN(DeliveryAction, entry)(progress, slot, quantity) || stack->quantity != quantity) return;
    }
}

static void update_auto_delivery(void) {
    if (!g_auto_delivery_ready || (!g_auto_deliver_goals && !g_auto_deliver_account)) return;
    if (g_auto_delivery_delay) {
        --g_auto_delivery_delay;
        return;
    }
    g_auto_delivery_delay = AUTO_DELIVERY_INTERVAL - 1;

    u32 system = GLOBAL(GOT_DELIVERY_SYSTEM);
    if (!system || !FN(Is, IS_QUEST_ACTIVE)(system) || !local_hunter()) return;
    u32 quest = GLOBAL(GOT_QUEST_STATE);
    u32 items = GLOBAL(GOT_ITEMS);
    u32 pouch = GLOBAL(GOT_QUEST_POUCH);
    if (!quest || *(const u8 *)(quest + QUEST_PHASE) != QUEST_PHASE_STARTED ||
        !items || !pouch)
        return;

    u32 progress = quest + QUEST_DELIVERY_PROGRESS;
    const DeliveryGoal *goals = (const DeliveryGoal *)(quest + QUEST_DELIVERY_GOALS);
    if (g_auto_deliver_goals && auto_deliver_goal_items(progress, pouch, goals)) return;
    if (g_auto_deliver_account) auto_deliver_account_items(progress, items, pouch, quest, goals);
}

static void init_auto_delivery(void) {
    static const u32 signatures[][3] = {
        {IS_QUEST_ACTIVE, 0xe590001c, 0xe2400005},
        {QUEST_GOAL_INDEX, 0xe59f007c, 0xe79f0000},
        {QUEST_ITEMS_DELIVERED, 0xe92d4830, 0xe1a04001},
        {POUCH_ITEM_COUNT, 0xe92d4010, 0xe3a0c000},
        {POUCH_GET_SLOT, 0xe92d4070, 0xe1a05001},
        {HAND_IN_GOAL_ITEM, 0xe92d47f0, 0xe24dd008},
        {HAND_IN_ACCOUNT_ITEM, 0xe92d4ff0, 0xe24dd01c},
        {HAND_IN_POINTS_GOAL, 0xe92d4ff0, 0xe24dd03c},
        {FIND_GOAL_WITH_FLAG, 0xe59020ec, 0xe0023001},
        {POINTS_GOAL_COMPLETE, 0xe92d4800, 0xe59f107c},
    };
    for (u32 i = 0; i < sizeof signatures / sizeof *signatures; ++i) {
        if (!code_is(signatures[i][0], signatures[i][1], signatures[i][2])) {
            forge_log(LOG_WARN, "personal-gathering: auto delivery unavailable at main+%x", signatures[i][0]);
            return;
        }
    }
    g_auto_delivery_ready = true;
    forge_log(LOG_INFO, "personal-gathering: automatic delivery ready for partial quest goals and account items");
}
