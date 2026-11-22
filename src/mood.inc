// Palico enthusiasm, independently verified against this game's ARM code.
// cOtomo's base enthusiasm byte is +0x242, effective enthusiasm s16 +0x35C.
// The original setter/add/subtract only write these and reset +0x3F4.
// Use 99 (the game's absolute maximum), including for low-level Palicoes.
#define MOOD_SET 0x0E7EC0
#define MOOD_ADD 0x0E8018
#define MOOD_SUB 0x0E8178
#define MOOD_TIER 0x0E82E0
#define GOT_OTOMO 0x1838BBC
#define OTOMO_OWNED 0x1393C
#define OTOMO_STRIDE 0x560
#define OTOMO_COUNT 84
typedef void (*MoodChange)(u32 palico, s32 amount, u32 modifier);
typedef u32 (*MoodTier)(u32 palico);
static ForgeHook g_mood_set_hook, g_mood_add_hook, g_mood_sub_hook, g_mood_tier_hook;
static bool g_max_mood = true, g_mood_hooked;

static void mood_fill(u32 palico) {
    if (!palico) return;
    // Do not repeatedly invalidate the cache when the values already match.
    if (*(u8 *)(palico + 0x242) == 99 && *(s16 *)(palico + 0x35C) == 99) return;
    *(u8 *)(palico + 0x242) = 99;
    *(s16 *)(palico + 0x35C) = 99;
    *(u32 *)(palico + 0x3F4) = 0;
}

static void mood_set_hook(u32 palico, s32 amount, u32 modifier) {
    if (g_max_mood) mood_fill(palico);
    else ORIGINAL(MoodChange, g_mood_set_hook)(palico, amount, modifier);
}
static void mood_add_hook(u32 palico, s32 amount, u32 modifier) {
    if (g_max_mood) mood_fill(palico);
    else ORIGINAL(MoodChange, g_mood_add_hook)(palico, amount, modifier);
}
static void mood_sub_hook(u32 palico, s32 amount, u32 modifier) {
    if (g_max_mood) mood_fill(palico);
    else ORIGINAL(MoodChange, g_mood_sub_hook)(palico, amount, modifier);
}
static u32 mood_tier_hook(u32 palico) {
    if (g_max_mood) mood_fill(palico);
    // Keep the game's own tier/icon selection, rather than fabricating a UI result.
    return ORIGINAL(MoodTier, g_mood_tier_hook)(palico);
}

static void mood_refresh_roster(u32 manager) {
    if (!manager) return;
    // main+2595E4 verifies this array/stride and cOtomo+271 occupancy flag.
    // Iterate physical slots, not the board's sorted index table. Empty slots,
    // hire candidates, and other sOtomo state must not be written here.
    for (u32 i = 0; i < OTOMO_COUNT; i++) {
        u32 palico = manager + OTOMO_OWNED + i * OTOMO_STRIDE;
        if (*(u8 *)(palico + 0x271)) mood_fill(palico);
    }
}

static void mood_update(void) {
    if (!g_main || !g_mood_hooked || !g_max_mood) return;
    u32 holder = *(u32 *)(g_main + GOT_OTOMO);
    mood_refresh_roster(holder ? *(u32 *)holder : 0);
}
