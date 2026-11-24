// ================================================================== Text languages
//
// The game's language is the byte at +A5 of the system object (main+1838BB4's singleton): MT Framework's IDs 1 eng,
// 2 fre, 3 spa, 4 ger, 5 ita, 7 chT, 8 chS. A Korean patch replaces the English text and the game still says English,
// so Korean is told from the game's own English text: once a text the game hands one of the plugin's hooks holds
// Hangul (note_game_text), the English game is taken as Korean, as KatSmithy and WishFinder do. That text is the
// reward menu's "Send to the Item Box" row and the village and hub menus' texts, so the Forge menu switches to Korean
// once one of them has been shown.
//
// The "language" setting picks the text: auto (the game's language, Korean as above), or eng, fre, spa, ger, ita, chT,
// chS, kor. It sets both the Forge menu and the plugin's text in the game's menus (the "Send all" rows). The Forge
// menu's fonts are MHGU Recomp's: ImGui's own for the Latin letters (with the accented ones of Latin-1, so no œ or ’
// in the texts) and mhgurecomp-cjk.otf, which has the Hangul and Simplified Chinese characters used here but not the
// Traditional ones (個, 時, 無...). The Traditional menu is drawn with the mod's own font,
// plugins/mhgu_recomp_qol/cjk-hant.otf (fonts/: Noto Sans CJK TC cut down to the Traditional texts below by
// tools/make_hant_font.py, to be run again when they change), as WishFinder does. Without that file the Traditional
// menu is shown in Simplified Chinese.

#define SETTINGS_FILE "mhgu_recomp_qol.ini"

// The game's order, then Korean: the menu's languages, the list's order and the texts' columns
enum { LANG_ENG, LANG_FRE, LANG_SPA, LANG_GER, LANG_ITA, LANG_CHT, LANG_CHS, LANG_KOR, LANG_COUNT };
enum { CHOICE_AUTO, CHOICE_COUNT = 1 + LANG_COUNT }; // the setting: auto, then the languages
static const char *const LANGUAGE_KEYS[CHOICE_COUNT] = {"auto", "eng", "fre", "spa", "ger", "ita", "chT", "chS", "kor"};
// (繁体中文 with 体 where the menu is in MHGU Recomp's font, which has no 體: language_name)
static const char *const LANGUAGE_NAMES[LANG_COUNT] = {"English", "Français", "Español", "Deutsch", "Italiano",
                                                       "繁体中文", "简体中文", "한국어"};

static u32 g_language = CHOICE_AUTO; // (saved as "language")
static bool g_game_hangul;          // the game's text has held Hangul: a Korean patch
static u32 g_menu_language;         // the Forge menu's this frame (forge_onImGuiRender)

// Notes a text the game gave: Hangul syllables (U+AC00-D7A3, UTF-8 EA B0 80 - ED 9E A3) mean a Korean patch
static void note_game_text(const char *s) {
    if (g_game_hangul || !s) return;
    for (const u8 *p = (const u8 *)s; *p; p++) {
        if (p[0] < 0xEA || p[0] > 0xED || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) continue;
        u32 c = (u32)(p[0] & 0x0F) << 12 | (u32)(p[1] & 0x3F) << 6 | (p[2] & 0x3F);
        if (c < 0xAC00 || c > 0xD7A3) continue;
        g_game_hangul = true;
        forge_log(LOG_INFO, "personal-gathering: the game's text is Korean (a Korean patch): Korean text for auto");
        return;
    }
}

// The game's language (LANG_*), Korean for an English game whose text is Korean
static u32 game_language(void) {
    u32 holder = g_main ? *(u32 *)(g_main + 0x1838BB4) : 0;
    u32 system = holder ? *(u32 *)holder : 0;
    switch (system ? *(u8 *)(system + 0xA5) : 1) {
    case 2: return LANG_FRE;
    case 3: return LANG_SPA;
    case 4: return LANG_GER;
    case 5: return LANG_ITA;
    case 7: return LANG_CHT;
    case 8: return LANG_CHS;
    default: return g_game_hangul ? LANG_KOR : LANG_ENG; // (eng, or a language the engine has and the game doesn't)
    }
}

// The setting's language (LANG_*): the Forge menu's and the plugin's text in the game's menus
static u32 text_language(void) {
    return g_language == CHOICE_AUTO || g_language >= CHOICE_COUNT ? game_language() : g_language - 1;
}

// ------------------------------------------------------------------ the Forge menu's texts

enum {
    T_TITLE, T_LANGUAGE, T_AUTO, T_OFF, T_UNAVAILABLE, T_PARTLY,
    T_GATHERING, T_TOOLLESS, T_GATHER_ALL, T_GATHERING_SPEED, T_FULL_POUCH,
    T_ITEMS, T_SEND_ALL, T_CARRY_ITEMS, T_COMBINE,
    T_QUEST, T_AUTO_GOALS, T_AUTO_ACCOUNT, T_DELIVER_ANYWHERE, T_FINISH_QUEST, T_SKIP_DELAY,
    T_HUNTING, T_NO_EXTREME, T_AUTO_MAP, T_MARK_LARGE, T_THREE_ARTS, T_SPRINT_SPEED, T_STAMINA_USE, T_MAX_MOOD,
    T_SETTINGS_DEFAULTS, T_SETTINGS_LOADED, T_SETTINGS_SAVED, T_SETTINGS_NOT_SAVED,
    T_COUNT
};

// In LANG_* order: English, French, Spanish, German, Italian, Traditional and Simplified Chinese, Korean. A label's
// "###name" keeps its ImGui ID (a tree's open state) the same in every language. The Korean is the translation from
// issue #1 of the repository, except the lines marked (Korean draft); the French, Spanish, German and Italian are the
// plugin's own drafts, to be reviewed. After a change to the Traditional texts, run tools/make_hant_font.py.
static const char *const g_texts[T_COUNT][LANG_COUNT] = {
    [T_TITLE] = {"MHGU Recomp QoL###mhgu-recomp-qol", "MHGU Recomp QoL###mhgu-recomp-qol",
                 "MHGU Recomp QoL###mhgu-recomp-qol", "MHGU Recomp QoL###mhgu-recomp-qol",
                 "MHGU Recomp QoL###mhgu-recomp-qol", "MHGU Recomp QoL###mhgu-recomp-qol",
                 "MHGU Recomp QoL###mhgu-recomp-qol", "몬스터헌터 더블크로스 Recomp QoL###mhgu-recomp-qol"},
    [T_LANGUAGE] = {"Language###language", "Langue###language", "Idioma###language", "Sprache###language",
                    "Lingua###language", "語言###language", "语言###language", "언어###language"},
    [T_AUTO] = {"Auto", "Auto", "Automático", "Automatisch", "Automatica", "自動", "自动", "자동"},
    // (a slider at the value that changes nothing; an option whose hooks failed; a merged option partly so)
    [T_OFF] = {" (off)", " (désactivé)", " (desactivado)", " (aus)", " (disattivato)", "（關閉）", "（关闭）",
               " (끔)"}, // (Korean draft)
    [T_UNAVAILABLE] = {" (unavailable: check log.txt)", " (indisponible : voir log.txt)",
                       " (no disponible: consulta log.txt)", " (nicht verfügbar: siehe log.txt)",
                       " (non disponibile: vedi log.txt)", "（無法使用，請查看 log.txt）", "（无法使用，请查看 log.txt）",
                       " (사용할 수 없음: log.txt를 확인하세요)"}, // (Korean draft)
    [T_PARTLY] = {"Some of it is unavailable: check log.txt.", "Une partie est indisponible : voir log.txt.",
                  "Una parte no está disponible: consulta log.txt.", "Teilweise nicht verfügbar: siehe log.txt.",
                  "In parte non disponibile: vedi log.txt.", "其中部分無法使用，請查看 log.txt。",
                  "其中部分无法使用，请查看 log.txt。", "일부를 사용할 수 없습니다. log.txt를 확인하세요."}, // (Korean draft)

    [T_GATHERING] = {"Gathering###gathering", "Récolte###gathering", "Recolección###gathering", "Sammeln###gathering",
                     "Raccolta###gathering", "採集###gathering", "采集###gathering", "채집###gathering"}, // (Korean draft)
    [T_TOOLLESS] = {"No pickaxe or bug net required", "Ni pioche ni filet à insectes requis",
                    "Sin necesidad de pico ni red para bichos", "Keine Spitzhacke oder Käfernetz nötig",
                    "Nessun piccone o retino richiesto", "免工具採集（無需礦鎬和捕蟲網）", "免工具采集（不带工具也能采矿和捕虫）",
                    "채집 도구 없이 채집"},
    [T_GATHER_ALL] = {"One action gathers/carves the whole spot", "Une action récolte/dépèce tout le point",
                      "Una acción recoge/trincha todo el punto", "Eine Aktion sammelt/schnitzt die ganze Stelle",
                      "Un'azione raccoglie/intaglia tutto il punto", "一次互動採集／剝取整個採集點", "一次互动采集/剥取整个采集点",
                      "한 번의 동작으로 전부 채집/갈무리"},
    [T_GATHERING_SPEED] = {"Gathering speed", "Vitesse de récolte", "Velocidad de recolección", "Sammelgeschwindigkeit",
                           "Velocità di raccolta", "採集速度", "采集速度", "채집 속도"},
    [T_FULL_POUCH] = {"Stops on a full pouch. Eggs and living monsters keep their rules.",
                      "S'arrête quand la sacoche est pleine. Les objets à transporter et les monstres vivants gardent "
                      "leurs règles.",
                      "Se detiene con la bolsa llena. Los huevos y los monstruos vivos siguen sus reglas.",
                      "Stoppt bei vollem Itembeutel. Eier und lebende Monster behalten ihre Regeln.",
                      "Si ferma con la borsa piena. Uova e mostri vivi seguono le loro regole.",
                      "道具袋滿時停止。蛋和活體怪物仍按原規則處理。", "道具袋满时停止。蛋和活体怪物仍按原规则处理。",
                      "파우치가 가득찼을 때 멈춥니다. 알이나 살아있는 몬스터는 원래 규칙을 따릅니다."},

    [T_ITEMS] = {"Items and rewards###items", "Objets et récompenses###items", "Objetos y recompensas###items",
                 "Items und Belohnungen###items", "Oggetti e ricompense###items", "道具與報酬###items",
                 "道具与报酬###items", "아이템과 보상###items"}, // (Korean draft)
    [T_SEND_ALL] = {"Send all rewards to the Item Box (quests, appraisal, farm trade cart, Meownster Hunters)",
                    "Tout envoyer au coffre (quêtes, expertise, chariot de la ferme, Chasseurs Palicos)",
                    "Enviar todo al baúl (misiones, tasación, carro de la granja, Cazadores Felyne)",
                    "Alles in die Objektbox (Quests, Begutachtung, Farm-Handelskarren, Miez-Jäger)",
                    "Invia tutto alla cassa (missioni, perizia, carro della fattoria, Cacciatori Felyne)",
                    "報酬道具一鍵入箱（任務報酬、鑑定、農場交易車、貓貓探險隊）",
                    "报酬道具一键入箱（任务报酬、鉴定、农场交易车、猫猫探险队）",
                    "보상 아이템을 아이템 박스로 일괄 전송 (퀘스트 보상, 감정, 농장 교역, 몬냥대)"}, // (Korean draft)
    [T_CARRY_ITEMS] = {"Put transport items in the item pouch", "Mettre les objets à transporter dans la sacoche",
                       "Guardar los objetos de transporte en la bolsa", "Transport-Items in den Itembeutel",
                       "Metti gli oggetti da trasporto nella borsa", "搬運道具放入道具袋", "搬运道具放入道具袋",
                       "운송 아이템을 아이템 파우치에 수납"},
    [T_COMBINE] = {"Always succeed at item combinations (100%)", "Combinaisons toujours réussies (100 %)",
                   "Combinaciones siempre con éxito (100%)", "Kombinationen immer erfolgreich (100 %)",
                   "Combinazioni sempre riuscite (100%)", "道具調合必定成功（100%）", "道具调合必定成功（100%）",
                   "아이템 조합 항상 100% 성공"},

    [T_QUEST] = {"Quests and delivery###quest", "Quêtes et livraisons###quest", "Misiones y entregas###quest",
                 "Quests und Ablieferung###quest", "Missioni e consegne###quest", "任務與繳納###quest",
                 "任务与缴纳###quest", "퀘스트와 납품###quest"}, // (Korean draft)
    [T_AUTO_GOALS] = {"Automatically deliver quest goal items (subquests too)",
                      "Livrer automatiquement les objets d'objectif (sous-quêtes comprises)",
                      "Entregar automáticamente los objetos del objetivo (también submisiones)",
                      "Zielitems automatisch abliefern (auch Nebenquests)",
                      "Consegna automatica degli oggetti obiettivo (anche sottomissioni)", "自動繳納任務目標道具（含副任務）",
                      "自动缴纳任务目标道具（含副任务）", "퀘스트 목표 아이템 자동 납품 (서브 퀘스트 포함)"}, // (Korean draft)
    [T_AUTO_ACCOUNT] = {"Automatically settle other account items", "Régler automatiquement les autres objets à remettre",
                        "Liquidar automáticamente otros objetos a entregar", "Andere Abgabe-Items automatisch abrechnen",
                        "Salda automaticamente gli altri oggetti da consegnare", "自動結算其他精算道具", "自动结算其他精算道具",
                        "다른 계정 아이템 자동 정산"},
    [T_DELIVER_ANYWHERE] = {"Deliver from the Item Pouch anywhere", "Livrer depuis la sacoche n'importe où",
                            "Entregar desde la bolsa en cualquier lugar", "Überall aus dem Itembeutel abliefern",
                            "Consegna dalla borsa ovunque", "在任何區域從道具袋繳納", "在任何区域从道具袋缴纳",
                            "어디서든 아이템 파우치에서 납품"},
    [T_FINISH_QUEST] = {"Finish quest immediately (Start + D-pad Down)",
                        "Terminer la quête immédiatement (Start + croix bas)",
                        "Completar la misión al instante (Start + cruceta abajo)",
                        "Quest sofort abschließen (Start + Steuerkreuz unten)",
                        "Completa subito la missione (Start + croce giù)", "立即完成任務（Start + 十字鍵下）",
                        "立即完成任务（Start + 十字键下）", "퀘스트 즉시 완료(Start + 십자키 아래)"},
    [T_SKIP_DELAY] = {"Skip end-of-quest delay (Select + D-pad Up)", "Passer l'attente de fin de quête (Select + croix haut)",
                      "Saltar la espera al final de la misión (Select + cruceta arriba)",
                      "Wartezeit am Questende überspringen (Select + Steuerkreuz oben)",
                      "Salta l'attesa di fine missione (Select + croce su)", "跳過任務結束等待（Select + 十字鍵上）",
                      "跳过任务结束等待（Select + 十字键上）", "퀘스트 끝날 때 딜레이 넘기기(Select + 십자키 위)"},

    [T_HUNTING] = {"Hunting###hunting", "Chasse###hunting", "Caza###hunting", "Jagd###hunting", "Caccia###hunting",
                   "狩獵###hunting", "狩猎###hunting", "수렵###hunting"}, // (Korean draft)
    [T_NO_EXTREME] = {"No hot/cool drinks required (ambient temperature)",
                      "Pas besoin de boissons chaudes/fraîches (température ambiante)",
                      "Sin bebidas calientes/frías (temperatura ambiente)",
                      "Keine Heiß-/Kühlgetränke nötig (Umgebungstemperatur)",
                      "Niente bevande calde/fresche (temperatura ambientale)", "無需冷熱飲（忽略環境溫度）",
                      "无需冷热饮（忽略环境温度）", "핫/쿨드링크 필요없음(환경 온도 무시)"},
    [T_AUTO_MAP] = {"Automatically display map", "Afficher la carte automatiquement", "Mostrar el mapa automáticamente",
                    "Karte automatisch anzeigen", "Mostra la mappa automaticamente", "自動顯示地圖", "自动显示地图",
                    "자동 지도 표시"},
    [T_MARK_LARGE] = {"Mark large monsters", "Marquer les grands monstres", "Marcar los monstruos grandes",
                      "Große Monster markieren", "Segna i mostri grandi", "標記大型怪物", "标记大型怪物", "대형 몬스터 표시"},
    [T_THREE_ARTS] = {"Three Hunter Arts for all styles", "Trois Arts de chasse pour tous les styles",
                      "Tres Artes de caza en todos los estilos", "Drei Jagdkünste für alle Stile",
                      "Tre Arti di caccia per tutti gli stili", "所有狩獵風格均可裝備三個狩技", "所有狩猎风格均可装备三个狩技",
                      "모든 스타일에 수기 3개 제공"},
    [T_SPRINT_SPEED] = {"Sprint speed before detection", "Vitesse de course avant d'être repéré",
                        "Velocidad de carrera antes de ser detectado", "Sprinttempo vor dem Entdecktwerden",
                        "Velocità di corsa prima di essere scoperto", "未被發現時的衝刺速度", "未被发现时的冲刺速度",
                        "발각되기 전 질주 속도"}, // (Korean draft)
    [T_STAMINA_USE] = {"Sprint stamina use before detection", "Endurance de course avant d'être repéré",
                       "Aguante al correr antes de ser detectado", "Sprint-Ausdauerverbrauch vor dem Entdecktwerden",
                       "Vigore della corsa prima di essere scoperto", "未被發現時的衝刺耐力消耗", "未被发现时的冲刺耐力消耗",
                       "발각되기 전 질주 스테미나 사용"}, // (Korean draft)
    [T_MAX_MOOD] = {"Keep Palico enthusiasm at maximum (99)", "Motivation du Palico toujours au maximum (99)",
                    "Entusiasmo del Palico siempre al máximo (99)", "Palico-Motivation immer maximal (99)",
                    "Entusiasmo del Palico sempre al massimo (99)", "隨從艾路貓幹勁保持最大（99）", "随从猫干劲保持最大（99）",
                    "동반자 아이루 의욕 최대로 유지"},

    // (the Korean of these four: draft)
    [T_SETTINGS_DEFAULTS] = {"Settings: defaults (saved when changed)",
                             "Réglages : par défaut (enregistrés à chaque changement)",
                             "Ajustes: predeterminados (se guardan al cambiar)",
                             "Einstellungen: Standard (beim Ändern gespeichert)",
                             "Impostazioni: predefinite (salvate quando cambiano)", "設定：預設值（修改後自動儲存）",
                             "设置：默认值（修改后自动保存）", "설정: 기본값 (변경하면 저장됩니다)"},
    [T_SETTINGS_LOADED] = {"Settings: loaded from " SETTINGS_FILE, "Réglages : lus dans " SETTINGS_FILE,
                           "Ajustes: leídos de " SETTINGS_FILE, "Einstellungen: aus " SETTINGS_FILE " geladen",
                           "Impostazioni: lette da " SETTINGS_FILE, "設定：已從 " SETTINGS_FILE " 讀取",
                           "设置：已从 " SETTINGS_FILE " 读取", "설정: " SETTINGS_FILE "에서 불러옴"},
    [T_SETTINGS_SAVED] = {"Settings: saved to " SETTINGS_FILE, "Réglages : enregistrés dans " SETTINGS_FILE,
                          "Ajustes: guardados en " SETTINGS_FILE, "Einstellungen: in " SETTINGS_FILE " gespeichert",
                          "Impostazioni: salvate in " SETTINGS_FILE, "設定：已儲存到 " SETTINGS_FILE,
                          "设置：已保存到 " SETTINGS_FILE, "설정: " SETTINGS_FILE "에 저장됨"},
    [T_SETTINGS_NOT_SAVED] = {"Settings: could not save (check log.txt)",
                              "Réglages : échec de l'enregistrement (voir log.txt)",
                              "Ajustes: no se pudieron guardar (consulta log.txt)",
                              "Einstellungen: Speichern fehlgeschlagen (siehe log.txt)",
                              "Impostazioni: salvataggio non riuscito (vedi log.txt)", "設定：無法儲存（請查看 log.txt）",
                              "设置：无法保存（请查看 log.txt）", "설정: 저장하지 못했습니다 (log.txt를 확인하세요)"},
};

// A text of the Forge menu in this frame's language
static const char *tr(u32 id) {
    const char *s = id < T_COUNT ? g_texts[id][g_menu_language] : NULL;
    return s ? s : id < T_COUNT ? g_texts[id][LANG_ENG] : "";
}

static u32 text_append(char *out, u32 at, u32 size, const char *s) {
    while (*s && at + 1 < size) out[at++] = *s++;
    out[at] = 0;
    return at;
}

// ------------------------------------------------------------------ the Traditional Chinese font

#define HANT_FONT ":/nativeNX/plugins/mhgu_recomp_qol/cjk-hant.otf"
#define HANT_FONT_PX 12.0f // (the overlay's size; the menu keeps the size it has: igPushFont's 0)
static void *g_hant_font;

// Loads the mod's Traditional Chinese font into the overlay's atlas (forge_onImGuiInit), as WishFinder does
static void load_hant_font(void) {
    static char path[96];
    const char *mount = forge_fs_getRomFsMountPoint();
    u32 at = text_append(path, 0, sizeof path, mount && *mount ? mount : "app");
    text_append(path, at, sizeof path, HANT_FONT);
    u8 *io = igGetIO_Nil();
    void *atlas = io ? *(void **)(io + IMGUI_IO_FONTS) : NULL;
    u8 *config = atlas ? ImFontConfig_ImFontConfig() : NULL;
    if (config) {
        *(u32 *)(config + IMFONTCONFIG_FLAGS) |= IMFONTFLAGS_NOLOADERROR;
        g_hant_font = ImFontAtlas_AddFontFromFileTTF(atlas, path, HANT_FONT_PX, config, NULL);
        ImFontConfig_destroy(config);
    }
    if (g_hant_font) forge_log(LOG_INFO, "personal-gathering: Traditional Chinese menu font %s", path);
    else forge_log(LOG_WARN, "personal-gathering: %s did not load: the Traditional Chinese menu is shown in Simplified",
                   path);
}

// A language's name in the list; 繁體中文 where the menu is in the mod's Traditional font
static const char *language_name(u32 language) {
    return language == LANG_CHT && g_menu_language == LANG_CHT ? "繁體中文" : LANGUAGE_NAMES[language];
}

// The language list: "Auto (<the language auto picks>)", then the languages by their own names
static void render_language(void) {
    static char items[256];
    u32 at = text_append(items, 0, sizeof items, tr(T_AUTO));
    at = text_append(items, at, sizeof items, " (");
    at = text_append(items, at, sizeof items, language_name(game_language()));
    at = text_append(items, at, sizeof items, ")");
    for (u32 i = 0; i < LANG_COUNT; i++) {
        at++; // (the items are separated by zeros)
        at = text_append(items, at, sizeof items - 1, language_name(i));
    }
    items[at + 1] = 0; // (and end with two)
    int choice = g_language < CHOICE_COUNT ? (int)g_language : CHOICE_AUTO;
    if (igCombo_Str(tr(T_LANGUAGE), &choice, items, CHOICE_COUNT) && choice >= 0 && choice < CHOICE_COUNT)
        g_language = (u32)choice;
}
