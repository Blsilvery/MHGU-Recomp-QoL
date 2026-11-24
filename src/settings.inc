// ================================================================== Saved settings
//
// The switches and sliders in Forge's menu are kept in mhgu_recomp_qol.ini in the game folder (MHGU Recomp runs in its
// own folder, where mhgurecomp.ini and forge_imgui.ini are). The file is read once in forge_onInit, before the hooks
// are installed: a feature whose hooks fail is still switched off for that session, as before, and its saved value is
// left in the file as it was. A change made in the menu is written once the values have stayed the same for a moment,
// so a slider being dragged is written once, after it is let go.
//
// The file goes through ImGui's ImFileOpen / Read / Write / Close, which MHGU Recomp's Forge maps to a file on the PC.
// No save data is mounted or touched. Without the file (or with a Forge that can't open it) the defaults apply, as before.

typedef unsigned long long u64;

extern u32 igImFileOpen(const char *filename, const char *mode); // (0: not opened)
extern bool igImFileClose(u32 file);
extern u64 igImFileGetSize(u32 file);
extern u64 igImFileRead(void *data, u64 size, u64 count, u32 file);
extern u64 igImFileWrite(const void *data, u64 size, u64 count, u32 file);

#define SETTINGS_SECTION "[MHGU Recomp QoL]"
#define SETTINGS_MAX_FILE 4096
#define SETTINGS_SAVE_DELAY 30    // frames without a change before it is written (about half a second)
#define SETTINGS_RETRY_DELAY 600  // frames before a failed write is tried again (about 10 s)

typedef struct {
    const char *key;
    bool *flag;            // a switch...
    float *value;          // ...or a slider, within min..max
    float min, max;
    const bool *available; // its feature's hooks (NULL: always available)
    u32 *choice;           // ...or one of a list, saved by name (names[*choice])
    const char *const *names;
    u32 count;
} Setting;
#define NO_CHOICE NULL, NULL, 0 // (a switch or a slider)

// In the menu's order. Keys are never renamed: an option merged into another keeps the other's key, and the keys of
// the options merged away (hold_r_single_gather, merge_item_messages, one_notification_per_spot, send_all_trade_cart,
// send_all_meownster_hunters, auto_deliver_subquests, faster_gathering, faster_sprint, reduce_sprint_stamina) are
// skipped in an old file and left out when it is next written.
static const Setting g_settings[] = {
    {"language", NULL, NULL, 0, 0, NULL, &g_language, LANGUAGE_KEYS, CHOICE_COUNT},
    {"no_pickaxe_or_bug_net", &g_toolless, NULL, 0, 0, &g_toolless_hooked, NO_CHOICE},
    {"gather_whole_spot", &g_gather_all, NULL, 0, 0, &g_gather_hooked, NO_CHOICE},
    {"gathering_speed", NULL, &g_gathering_speed, 1.0f, 4.0f, &g_motion_rate_hooked, NO_CHOICE},
    {"send_all_rewards", &g_send_all, NULL, 0, 0, &g_send_all_ready, NO_CHOICE},
    {"transport_items_in_pouch", &g_carry_items, NULL, 0, 0, &g_carry_items_hooked, NO_CHOICE},
    {"always_combine", &g_combine_always, NULL, 0, 0, &g_combine_hooked, NO_CHOICE},
    {"auto_deliver_goals", &g_auto_deliver_goals, NULL, 0, 0, &g_auto_delivery_ready, NO_CHOICE},
    {"auto_deliver_account_items", &g_auto_deliver_account, NULL, 0, 0, &g_auto_delivery_ready, NO_CHOICE},
    {"deliver_anywhere", &g_deliver_anywhere, NULL, 0, 0, &g_delivery_hooked, NO_CHOICE},
    {"finish_quest_shortcut", &g_finish_quest, NULL, 0, 0, NULL, NO_CHOICE},
    {"skip_end_delay_shortcut", &g_skip_end_delay, NULL, 0, 0, NULL, NO_CHOICE},
    {"no_hot_cool_drinks", &g_no_extreme, NULL, 0, 0, &g_climate_hooked, NO_CHOICE},
    {"auto_display_map", &qol_field_routes.map_enabled, NULL, 0, 0, &g_map_hooked, NO_CHOICE},
    {"mark_large_monsters", &qol_field_routes.boss_enabled, NULL, 0, 0, &g_boss_hooked, NO_CHOICE},
    {"three_hunter_arts", &g_three_arts, NULL, 0, 0, &g_art_slots_hooked, NO_CHOICE},
    {"sprint_speed", NULL, &g_sprint_speed, 1.0f, 2.0f, &g_motion_rate_hooked, NO_CHOICE},
    {"sprint_stamina_use_percent", NULL, &g_sprint_stamina_percent, 0.0f, 100.0f, &g_sprint_stamina_ready, NO_CHOICE},
    {"max_palico_enthusiasm", &g_max_mood, NULL, 0, 0, &g_mood_hooked, NO_CHOICE},
};
#define SETTING_COUNT (sizeof g_settings / sizeof *g_settings)

static float g_setting_saved[SETTING_COUNT]; // what the file holds (read, or last written; the default when absent)
static float g_setting_seen[SETTING_COUNT];  // the values last frame (to wait for a slider to be let go)
static bool g_settings_ready, g_settings_failed;
static u32 g_settings_wait; // frames the changed values have stayed the same
static u32 g_settings_status = T_COUNT; // (a text of lang.inc; T_COUNT: none yet)

static float setting_get(const Setting *s) {
    if (s->choice) return (float)*s->choice;
    return s->flag ? (*s->flag ? 1.0f : 0.0f) : *s->value;
}
static bool setting_available(const Setting *s) { return !s->available || *s->available; }

static void setting_set(const Setting *s, float v) {
    if (s->choice) {
        u32 i = v > 0.0f ? (u32)(v + 0.5f) : 0;
        *s->choice = i < s->count ? i : 0;
        return;
    }
    if (s->flag) {
        *s->flag = v != 0.0f;
        return;
    }
    if (v < s->min) v = s->min;
    if (v > s->max) v = s->max;
    *s->value = v;
}

// ------------------------------------------------------------------ the file's text

static bool text_same(const char *a, const char *b, u32 n) { // (b: n characters, a: a whole string)
    for (u32 i = 0; i < n; i++)
        if (!a[i] || a[i] != b[i]) return false;
    return !a[n];
}

// "1", "0", "true", "false", "on", "off", or a number such as "1.5" (no exponent); false when it is none of them
static bool text_value(const char *p, u32 n, float *out) {
    if (text_same("true", p, n) || text_same("on", p, n) || text_same("yes", p, n)) return *out = 1.0f, true;
    if (text_same("false", p, n) || text_same("off", p, n) || text_same("no", p, n)) return *out = 0.0f, true;
    u32 i = 0;
    bool negative = n && p[0] == '-';
    if (negative || (n && p[0] == '+')) i++;
    float v = 0.0f, scale = 1.0f;
    bool digits = false, fraction = false;
    for (; i < n; i++) {
        char c = p[i];
        if (c == '.' && !fraction) {
            fraction = true;
        } else if (c >= '0' && c <= '9') {
            digits = true;
            if (fraction) scale *= 0.1f, v += (float)(c - '0') * scale; // (no division: the interpreter's VFP)
            else v = v * 10.0f + (float)(c - '0');
        } else {
            return false;
        }
    }
    if (!digits) return false;
    *out = negative ? -v : v;
    return true;
}

static bool text_same_nocase(const char *a, const char *b, u32 n) { // (as text_same, ASCII letters in either case)
    for (u32 i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (!a[i] || x != y) return false;
    }
    return !a[n];
}

static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// Applies every "key = value" line of the file it knows; others (comments, sections, old keys) are skipped
static u32 settings_parse(const char *text, u32 size) {
    u32 applied = 0;
    for (u32 at = 0; at < size;) {
        u32 end = at;
        while (end < size && text[end] != '\n') end++;
        u32 a = at, b = end;
        at = end + 1;
        while (a < b && is_space(text[a])) a++;
        if (a == b || text[a] == ';' || text[a] == '#' || text[a] == '[') continue;
        u32 eq = a;
        while (eq < b && text[eq] != '=') eq++;
        if (eq == b) continue;
        u32 key_end = eq;
        while (key_end > a && is_space(text[key_end - 1])) key_end--;
        u32 v = eq + 1;
        while (v < b && is_space(text[v])) v++;
        while (b > v && is_space(text[b - 1])) b--;
        for (u32 i = 0; i < SETTING_COUNT; i++) {
            if (!text_same(g_settings[i].key, text + a, key_end - a)) continue;
            float value = 0.0f;
            bool named = false;
            for (u32 k = 0; g_settings[i].names && k < g_settings[i].count && !named; k++)
                if (text_same_nocase(g_settings[i].names[k], text + v, b - v)) value = (float)k, named = true;
            if (named || (!g_settings[i].names && text_value(text + v, b - v, &value))) {
                setting_set(&g_settings[i], value);
                g_setting_saved[i] = setting_get(&g_settings[i]);
                applied++;
            }
            break;
        }
    }
    return applied;
}

static u32 text_put(char *out, u32 at, const char *s) {
    while (*s && at < SETTINGS_MAX_FILE) out[at++] = *s++;
    return at;
}

// A switch as 1 / 0; a list by name; a slider with two decimals (every slider is 0 or more)
static u32 text_put_value(char *out, u32 at, const Setting *s, float v) {
    if (s->choice) {
        u32 i = v > 0.0f ? (u32)(v + 0.5f) : 0;
        return text_put(out, at, s->names[i < s->count ? i : 0]);
    }
    if (s->flag) return text_put(out, at, v != 0.0f ? "1" : "0");
    if (!(v > 0.0f)) v = 0.0f;
    if (v > 1000000.0f) v = 1000000.0f;
    u32 hundredths = (u32)(v * 100.0f + 0.5f), whole = hundredths / 100;
    char digits[16];
    u32 n = 0;
    do digits[n++] = (char)('0' + whole % 10), whole /= 10; while (whole && n < 10);
    char text[24];
    u32 k = 0;
    while (n) text[k++] = digits[--n];
    text[k++] = '.';
    text[k++] = (char)('0' + hundredths % 100 / 10);
    text[k++] = (char)('0' + hundredths % 10);
    text[k] = 0;
    return text_put(out, at, text);
}

// ------------------------------------------------------------------ reading and writing

static bool settings_write(void) {
    static char text[SETTINGS_MAX_FILE];
    float values[SETTING_COUNT];
    u32 at = text_put(text, 0, "; MHGU Recomp QoL settings. Written by the plugin when they change in the Forge menu (Insert).\n"
                                "; Delete this file to go back to the defaults. Switches: 1 on, 0 off.\n"
                                "; Speeds: 1 is off. Stamina use: 100 is off.\n"
                                "; language: auto (the game's; Korean when its text is Korean), eng, chS, chT or kor.\n");
    at = text_put(text, at, SETTINGS_SECTION "\n");
    for (u32 i = 0; i < SETTING_COUNT; i++) {
        const Setting *s = &g_settings[i];
        // (an unavailable feature, switched off this session: its value in the file stays)
        values[i] = setting_available(s) ? setting_get(s) : g_setting_saved[i];
        at = text_put(text, at, s->key);
        at = text_put(text, at, " = ");
        at = text_put_value(text, at, s, values[i]);
        at = text_put(text, at, "\n");
    }
    if (at >= SETTINGS_MAX_FILE) {
        forge_log(LOG_ERROR, "personal-gathering: settings text too long; not saved");
        return false;
    }
    u32 file = igImFileOpen(SETTINGS_FILE, "wb");
    if (!file) {
        forge_log(LOG_WARN, "personal-gathering: could not open %s for writing; settings not saved", SETTINGS_FILE);
        return false;
    }
    bool ok = igImFileWrite(text, 1, at, file) == at;
    ok = igImFileClose(file) && ok;
    if (!ok) {
        forge_log(LOG_WARN, "personal-gathering: could not write %s; settings not saved", SETTINGS_FILE);
        return false;
    }
    for (u32 i = 0; i < SETTING_COUNT; i++) g_setting_saved[i] = values[i];
    return true;
}

// Before the hooks (forge_onInit): the file's values over the defaults
static void settings_load(void) {
    for (u32 i = 0; i < SETTING_COUNT; i++) g_setting_saved[i] = setting_get(&g_settings[i]);
    u32 file = igImFileOpen(SETTINGS_FILE, "rb");
    if (!file) {
        g_settings_status = T_SETTINGS_DEFAULTS;
        forge_log(LOG_INFO, "personal-gathering: no %s; default settings", SETTINGS_FILE);
        return;
    }
    static char text[SETTINGS_MAX_FILE];
    u64 size = igImFileGetSize(file);
    u32 want = size > SETTINGS_MAX_FILE ? SETTINGS_MAX_FILE : (u32)size;
    u32 got = want ? (u32)igImFileRead(text, 1, want, file) : 0;
    igImFileClose(file);
    u32 applied = settings_parse(text, got > want ? want : got);
    g_settings_status = T_SETTINGS_LOADED;
    forge_log(LOG_INFO, "personal-gathering: %u settings loaded from %s", applied, SETTINGS_FILE);
}

// After the hooks (end of forge_onInit): from here on, a change in the menu is written
static void settings_start(void) {
    for (u32 i = 0; i < SETTING_COUNT; i++) g_setting_seen[i] = setting_get(&g_settings[i]);
    g_settings_ready = true;
}

// Every frame: writes the values once they differ from the file's and have stayed the same for a moment
static void settings_update(void) {
    if (!g_settings_ready) return;
    bool moving = false, changed = false;
    for (u32 i = 0; i < SETTING_COUNT; i++) {
        const Setting *s = &g_settings[i];
        if (!setting_available(s)) continue;
        float v = setting_get(s);
        if (v != g_setting_seen[i]) g_setting_seen[i] = v, moving = true;
        if (v != g_setting_saved[i]) changed = true;
    }
    if (moving || !changed) {
        g_settings_wait = 0;
        if (!changed) g_settings_failed = false;
        return;
    }
    if (++g_settings_wait < (g_settings_failed ? SETTINGS_RETRY_DELAY : SETTINGS_SAVE_DELAY)) return;
    g_settings_wait = 0;
    g_settings_failed = !settings_write();
    g_settings_status = g_settings_failed ? T_SETTINGS_NOT_SAVED : T_SETTINGS_SAVED;
}

static void render_settings_status(void) {
    if (g_settings_ready && g_settings_status < T_COUNT) igTextUnformatted(tr(g_settings_status), NULL);
}
