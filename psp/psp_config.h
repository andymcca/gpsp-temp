/* PSP Standalone Configuration */

#ifndef PSP_CONFIG_H
#define PSP_CONFIG_H

// Use stub libretro header for compilation
#ifdef PSP_STANDALONE
  #define __LIBRETRO_SDK_GBA_TYPES_H__
  #include "libretro_stub.h"
#endif

// Configuration structure
typedef struct {
    int boot_mode;        // 0 = game, 1 = bios
    int dynarec_enable;   // 0 = disabled, 1 = enabled
    int sprite_limit;     // 0 = enabled, 1 = disabled (no limit)
    int frameskip;        // 0-9
    int show_fps;         // 0 = no, 1 = yes
    int audio_enable;     // 0 = disabled, 1 = enabled
} psp_config_t;

extern psp_config_t psp_config;

void psp_config_default(void);
void psp_config_load(void);
void psp_config_save(void);
int psp_show_options_menu(void);

#endif

