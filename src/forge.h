// The part of Forge's plugin API (github.com/Fexty12573/forge) this plugin uses, as MHGU Recomp exposes it.
#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef short s16;
typedef int s32;
typedef _Bool bool;
#define true 1
#define false 0
#define NULL ((void *)0)

#define EXPORT __attribute__((visibility("default")))

typedef struct {
    u16 major, minor, patch;
    char name[64];
} ForgePluginInfo;

// Forge's Hook (24 bytes, returned through a hidden pointer): target, detour, the original's trampoline, context...
typedef struct {
    u32 target;
    u32 detour;
    u32 original;
    u32 reserved[3];
} ForgeHook;

enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };

extern void forge_hook_create(ForgeHook *hook, u32 target, u32 detour, u32 *original);
extern u32 forge_input_isDown(u32 button); // (nn::hid NpadButton bits: A 0, B 1, X 2, Y 3, ... L 6, R 7, ZL 8, ZR 9)
extern u32 forge_mem_getMainTextAddr(void);
extern u32 forge_singleton_getInstanceByName(const char *name);
extern void forge_log(int level, const char *fmt, ...);
extern const char *forge_fs_getRomFsMountPoint(void); // ("app": the mods' romfs is <it>:/nativeNX/...)

// cimgui, as far as this plugin needs it.
extern void igSetCurrentContext(void *ctx);
extern void igSetAllocatorFunctions(void *alloc, void *free, void *user);
extern bool igTreeNode_Str(const char *label);
extern void igTreePop(void);
extern void igTextUnformatted(const char *text, const char *text_end);
extern bool igCheckbox(const char *label, bool *v);
extern bool igCombo_Str(const char *label, int *current_item, const char *items_separated_by_zeros,
                        int popup_max_height_in_items);
// Forge's ImGui bridge reads float arguments from the base AAPCS registers.
extern bool igSliderFloat(const char *label, float *v, float min, float max,
                          const char *format, int flags) __attribute__((pcs("aapcs")));
// Fonts (ImGui 1.92, as the bridge hands them to 32-bit plugins: ImGuiIO's Fonts at +2C, ImFontConfig's Flags at +6C)
#define IMGUI_IO_FONTS 0x2C
#define IMFONTCONFIG_FLAGS 0x6C
#define IMFONTFLAGS_NOLOADERROR 2 // (a file that doesn't load gives NULL, not an assertion)
extern u8 *igGetIO_Nil(void);
extern u8 *ImFontConfig_ImFontConfig(void);
extern void ImFontConfig_destroy(void *config);
extern void *ImFontAtlas_AddFontFromFileTTF(void *atlas, const char *filename, float size_pixels, const void *config,
                                            const u16 *glyph_ranges) __attribute__((pcs("aapcs")));
extern void igPushFont(void *font, float font_size_base_unscaled) __attribute__((pcs("aapcs"))); // (0: the size kept)
extern void igPopFont(void);
