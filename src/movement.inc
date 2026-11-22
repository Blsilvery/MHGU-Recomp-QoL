enum {
    SET_MOTION_RATE = 0x53a0f8,
    CURRENT_STAMINA = 0x13e0,
    MAX_STAMINA = 0x13e4,
    ACTION_GROUP = 0x2506,
    ACTION_NUMBER = 0x250a,
};

typedef void (*SetMotionRate)(u32 actor, float rate);

static ForgeHook g_motion_rate_hook;
static bool g_motion_rate_hooked, g_sprint_stamina_ready;
static bool g_faster_gathering = true, g_faster_sprint = true, g_reduce_sprint_stamina = true;
static float g_gathering_speed = 1.50f, g_sprint_speed = 1.20f, g_sprint_stamina_percent = 0.0f;
static struct {
    u32 hunter;
    float stamina;
} g_sprint_stamina;

static bool is_normal_sprint(u32 hunter) {
    return *(const u16 *)(hunter + ACTION_GROUP) == 2 &&
           *(const u16 *)(hunter + ACTION_NUMBER) == 2;
}

static bool is_gathering_motion(u32 hunter) {
    if (*(const u16 *)(hunter + ACTION_GROUP) != 1) return false;
    // Gathering, mining, and bug catching, including their repeating motions.
    switch (*(const u16 *)(hunter + ACTION_NUMBER)) {
    case 0x4d: case 0x4e: case 0x51: case 0x52:
    case 0xd1: case 0xd2: case 0xd3: case 0xd4:
        return true;
    default:
        return false;
    }
}

static void motion_rate_hook(u32 actor, float rate) {
    // The motion setter is shared by several object types. Check the player
    // manager before reading fields that only exist on a hunter.
    if (actor && rate > 0.0f && (g_faster_gathering || g_faster_sprint) && actor == local_hunter()) {
        if (g_faster_gathering && is_gathering_motion(actor)) rate *= g_gathering_speed;
        if (g_faster_sprint && is_normal_sprint(actor)) rate *= g_sprint_speed;
    }
    ORIGINAL(SetMotionRate, g_motion_rate_hook)(actor, rate);
}

static void update_sprint_stamina(void) {
    // Recomp's precompiled sprint update can skip hooks in the stamina debit.
    // Observe the result once per frame while the same local hunter stays in
    // ordinary sprint. Establish a fresh baseline when entering that action.
    u32 hunter = 0;
    if (g_sprint_stamina_ready && g_reduce_sprint_stamina && g_sprint_stamina_percent < 100.0f)
        hunter = local_hunter();
    if (!hunter || !is_normal_sprint(hunter)) {
        g_sprint_stamina.hunter = 0;
        return;
    }

    float *stamina = (float *)(hunter + CURRENT_STAMINA);
    float current = *stamina;
    float limit = *(const float *)(hunter + MAX_STAMINA);
    if (!(current >= 0.0f && current <= limit && limit > 0.0f)) {
        g_sprint_stamina.hunter = 0;
        return;
    }
    if (hunter == g_sprint_stamina.hunter) {
        float previous = g_sprint_stamina.stamina;
        if (previous > limit) previous = limit;
        if (current < previous) {
            current = previous - (previous - current) * (g_sprint_stamina_percent * 0.01f);
            *stamina = current;
        }
    }
    g_sprint_stamina.hunter = hunter;
    g_sprint_stamina.stamina = current;
}

static void init_movement_options(void) {
    g_motion_rate_hooked = field_hook(&g_motion_rate_hook, SET_MOTION_RATE, motion_rate_hook,
                                       0xeeb50ac0, 0xe300150c, "gathering and sprint speed", true);
    const u32 *player_query = (const u32 *)(g_main + LOCAL_HUNTER);
    g_sprint_stamina_ready = player_query[0] == 0xe92d4010 && player_query[1] == 0xe1a04000;
    forge_log(g_sprint_stamina_ready ? LOG_INFO : LOG_WARN, "personal-gathering: sprint stamina %s",
              g_sprint_stamina_ready ? "ready" : "unavailable; normal stamina cost retained");
}

static void render_movement_options(void) {
    if (!g_main || !igTreeNode_Str("Movement and gathering speed")) return;
    if (g_motion_rate_hooked) {
        igCheckbox("Faster gathering", &g_faster_gathering);
        igSliderFloat("Gathering speed", &g_gathering_speed, 1.0f, 4.0f, "x%.2f", 0);
        igCheckbox("Faster sprint before detection", &g_faster_sprint);
        igSliderFloat("Sprint speed", &g_sprint_speed, 1.0f, 2.0f, "x%.2f", 0);
    }
    if (g_sprint_stamina_ready) {
        igCheckbox("Reduce sprint stamina before detection", &g_reduce_sprint_stamina);
        igSliderFloat("Sprint stamina use", &g_sprint_stamina_percent, 0.0f, 100.0f, "%.0f%%", 0);
    }
    if (!g_motion_rate_hooked || !g_sprint_stamina_ready)
        igTextUnformatted("Some movement options are unavailable. Check log.txt.", NULL);
    igTreePop();
}
