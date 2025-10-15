/* gpSP PSP Standalone
 * Configuration menu and settings
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psp_config.h"
#include "psp_input.h"

#define CONFIG_FILE "ms0:/PSP/GAME/gpsp-temp/gpsp_config.txt"

// Global config
psp_config_t psp_config;

// Default settings
void psp_config_default(void)
{
    psp_config.boot_mode = 0;          // 0 = game, 1 = bios
    psp_config.dynarec_enable = 1;     // 1 = enabled
    psp_config.sprite_limit = 0;       // 0 = limit enabled (accurate)
    psp_config.frameskip = 0;          // 0 = no frameskip
    psp_config.show_fps = 0;           // 0 = hidden
    psp_config.audio_enable = 1;       // 1 = enabled
}

// Load config from file
void psp_config_load(void)
{
    FILE *f = fopen(CONFIG_FILE, "r");
    
    if(!f) {
        psp_config_default();
        return;
    }
    
    char line[256];
    while(fgets(line, sizeof(line), f)) {
        char key[128], value[128];
        if(sscanf(line, "%127[^=]=%127s", key, value) == 2) {
            if(strcmp(key, "boot_mode") == 0)
                psp_config.boot_mode = atoi(value);
            else if(strcmp(key, "dynarec_enable") == 0)
                psp_config.dynarec_enable = atoi(value);
            else if(strcmp(key, "sprite_limit") == 0)
                psp_config.sprite_limit = atoi(value);
            else if(strcmp(key, "frameskip") == 0)
                psp_config.frameskip = atoi(value);
            else if(strcmp(key, "show_fps") == 0)
                psp_config.show_fps = atoi(value);
            else if(strcmp(key, "audio_enable") == 0)
                psp_config.audio_enable = atoi(value);
        }
    }
    
    fclose(f);
}

// Save config to file
void psp_config_save(void)
{
    sceIoMkdir("ms0:/PSP/GAME/gpsp-temp", 0777);
    
    FILE *f = fopen(CONFIG_FILE, "w");
    if(!f)
        return;
    
    fprintf(f, "boot_mode=%d\n", psp_config.boot_mode);
    fprintf(f, "dynarec_enable=%d\n", psp_config.dynarec_enable);
    fprintf(f, "sprite_limit=%d\n", psp_config.sprite_limit);
    fprintf(f, "frameskip=%d\n", psp_config.frameskip);
    fprintf(f, "show_fps=%d\n", psp_config.show_fps);
    fprintf(f, "audio_enable=%d\n", psp_config.audio_enable);
    
    fclose(f);
}

// Show options menu
int psp_show_options_menu(void)
{
    const int num_items = 9;
    int selection = 0;
    
    psp_config_t temp_config = psp_config;  // Working copy
    
    while(1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 0);
        
        pspDebugScreenSetTextColor(0xFFFFFF00);
        pspDebugScreenPrintf("gpSP-temp - Options\n");
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("--------------------------------------------------\n\n");
        
        // Boot Mode
        pspDebugScreenSetTextColor(selection == 0 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Boot Mode: %s\n", 
            selection == 0 ? ">" : " ",
            temp_config.boot_mode ? "BIOS" : "Game");
        
        // Dynarec
        pspDebugScreenSetTextColor(selection == 1 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Dynarec: %s\n",
            selection == 1 ? ">" : " ",
            temp_config.dynarec_enable ? "Enabled" : "Disabled");
        
        // Sprite Limit
        pspDebugScreenSetTextColor(selection == 2 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Sprite Limit: %s\n",
            selection == 2 ? ">" : " ",
            temp_config.sprite_limit ? "Disabled" : "Enabled");
        
        // Frameskip
        pspDebugScreenSetTextColor(selection == 3 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Frameskip: %d\n",
            selection == 3 ? ">" : " ",
            temp_config.frameskip);
        
        // Show FPS
        pspDebugScreenSetTextColor(selection == 4 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Show FPS: %s\n",
            selection == 4 ? ">" : " ",
            temp_config.show_fps ? "Yes" : "No");
        
        // Audio
        pspDebugScreenSetTextColor(selection == 5 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Audio: %s\n",
            selection == 5 ? ">" : " ",
            temp_config.audio_enable ? "Enabled" : "Disabled");
        
        pspDebugScreenPrintf("\n");
        
        // Save and Continue
        pspDebugScreenSetTextColor(selection == 7 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Save and Continue\n", selection == 7 ? ">" : " ");
        
        // Cancel
        pspDebugScreenSetTextColor(selection == 8 ? 0xFF0000FF : 0xFFFFFFFF);
        pspDebugScreenPrintf("%s Cancel\n", selection == 8 ? ">" : " ");
        
        pspDebugScreenSetXY(0, 20);
        pspDebugScreenSetTextColor(0xFF888888);
        pspDebugScreenPrintf("--------------------------------------------------\n");
        pspDebugScreenPrintf("Up/Down: Navigate  Left/Right: Change  X: Select\n");
        
        sceDisplayWaitVblankStart();
        
        // Input
        psp_input_update();
        u32 buttons = psp_get_buttons_pressed();
        
        if(buttons & PSP_CTRL_UP) {
            selection--;
            if(selection < 0) selection = num_items - 1;
            if(selection == 6) selection = 5;  // Skip empty line
        }
        else if(buttons & PSP_CTRL_DOWN) {
            selection++;
            if(selection >= num_items) selection = 0;
            if(selection == 6) selection = 7;  // Skip empty line
        }
        else if(buttons & PSP_CTRL_LEFT || buttons & PSP_CTRL_RIGHT) {
            int delta = (buttons & PSP_CTRL_RIGHT) ? 1 : -1;
            
            switch(selection) {
                case 0: temp_config.boot_mode = !temp_config.boot_mode; break;
                case 1: temp_config.dynarec_enable = !temp_config.dynarec_enable; break;
                case 2: temp_config.sprite_limit = !temp_config.sprite_limit; break;
                case 3: 
                    temp_config.frameskip += delta;
                    if(temp_config.frameskip < 0) temp_config.frameskip = 0;
                    if(temp_config.frameskip > 9) temp_config.frameskip = 9;
                    break;
                case 4: temp_config.show_fps = !temp_config.show_fps; break;
                case 5: temp_config.audio_enable = !temp_config.audio_enable; break;
            }
        }
        else if(buttons & PSP_CTRL_CROSS) {
            if(selection == 7) {
                // Save and continue
                psp_config = temp_config;
                psp_config_save();
                return 0;
            }
            else if(selection == 8) {
                // Cancel
                return -1;
            }
        }
        else if(buttons & PSP_CTRL_CIRCLE) {
            // Cancel
            return -1;
        }
        
        sceKernelDelayThread(16666);
    }
}


