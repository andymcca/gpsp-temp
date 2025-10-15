/* gpSP PSP Standalone
 * PSP menu and file browser implementation
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <malloc.h>

#include "psp_menu.h"
#include "psp_input.h"
#include "psp_video.h"

#define MAX_FILES 512
#define MAX_FILENAME 256
#define FILES_PER_PAGE 15

typedef struct {
    char name[MAX_FILENAME];
    int is_dir;
} file_entry;

static file_entry *file_list = NULL;
static int file_count = 0;
static int current_selection = 0;
static int scroll_offset = 0;

static char current_directory[512] = "ms0:/PSP/GAME/gpsp-temp/ROMS";

// Text rendering helpers
#define TEXT_COLOR 0xFFFFFFFF
#define BG_COLOR 0xFF000000
#define SELECTED_COLOR 0xFF0000FF

static void draw_text(int x, int y, const char *text, u32 color)
{
    pspDebugScreenSetXY(x, y);
    pspDebugScreenSetTextColor(color);
    pspDebugScreenPrintf("%s", text);
}

static int compare_files(const void *a, const void *b)
{
    const file_entry *fa = (const file_entry *)a;
    const file_entry *fb = (const file_entry *)b;
    
    // Directories first
    if(fa->is_dir && !fb->is_dir)
        return -1;
    if(!fa->is_dir && fb->is_dir)
        return 1;
    
    // Then alphabetically
    return strcasecmp(fa->name, fb->name);
}

static int scan_directory(const char *path)
{
    DIR *dir = opendir(path);
    if(!dir)
        return -1;
    
    file_count = 0;
    
    if(!file_list) {
        file_list = (file_entry*)malloc(MAX_FILES * sizeof(file_entry));
        if(!file_list) {
            closedir(dir);
            return -1;
        }
    }
    
    // Add parent directory entry if not at root
    if(strcmp(path, "ms0:/") != 0) {
        strcpy(file_list[file_count].name, "..");
        file_list[file_count].is_dir = 1;
        file_count++;
    }
    
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        // Skip hidden files and current directory
        if(entry->d_name[0] == '.')
            continue;
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if(stat(full_path, &st) == 0) {
            strncpy(file_list[file_count].name, entry->d_name, MAX_FILENAME - 1);
            file_list[file_count].name[MAX_FILENAME - 1] = '\0';
            file_list[file_count].is_dir = S_ISDIR(st.st_mode);
            
            // Only show directories and GBA files
            if(file_list[file_count].is_dir) {
                file_count++;
            } else {
                // Check extension
                char *ext = strrchr(entry->d_name, '.');
                if(ext && (strcasecmp(ext, ".gba") == 0 || 
                          strcasecmp(ext, ".bin") == 0 ||
                          strcasecmp(ext, ".agb") == 0 ||
                          strcasecmp(ext, ".gbz") == 0)) {
                    file_count++;
                }
            }
        }
    }
    
    closedir(dir);
    
    // Sort files
    qsort(file_list, file_count, sizeof(file_entry), compare_files);
    
    return 0;
}

static void draw_file_list(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    
    // Title
    pspDebugScreenSetTextColor(0xFFFFFF00);
    pspDebugScreenPrintf("gpSP-temp - File Browser\n");
    pspDebugScreenSetTextColor(TEXT_COLOR);
    pspDebugScreenPrintf("Current: %s\n", current_directory);
    pspDebugScreenPrintf("--------------------------------------------------\n");
    
    // File list
    int start = scroll_offset;
    int end = start + FILES_PER_PAGE;
    if(end > file_count)
        end = file_count;
    
    for(int i = start; i < end; i++) {
        int y = 3 + (i - start);
        
        if(i == current_selection) {
            pspDebugScreenSetTextColor(SELECTED_COLOR);
            pspDebugScreenSetXY(0, y);
            pspDebugScreenPrintf("> ");
        } else {
            pspDebugScreenSetXY(0, y);
            pspDebugScreenPrintf("  ");
        }
        
        pspDebugScreenSetTextColor(file_list[i].is_dir ? 0xFF00FF00 : TEXT_COLOR);
        
        if(file_list[i].is_dir) {
            pspDebugScreenPrintf("[%s]\n", file_list[i].name);
        } else {
            pspDebugScreenPrintf("%s\n", file_list[i].name);
        }
    }
    
    // Instructions
    pspDebugScreenSetXY(0, 20);
    pspDebugScreenSetTextColor(0xFF888888);
    pspDebugScreenPrintf("--------------------------------------------------\n");
    pspDebugScreenPrintf("Up/Down: Select  X: Choose  O: Cancel\n");
    pspDebugScreenPrintf("Files: %d\n", file_count);
    
    sceDisplayWaitVblankStart();
}

int psp_file_browser(char *selected_file, size_t max_len)
{
    current_selection = 0;
    scroll_offset = 0;
    
    // Create ROMS directory if it doesn't exist
    sceIoMkdir("ms0:/PSP", 0777);
    sceIoMkdir("ms0:/PSP/GAME", 0777);
    sceIoMkdir("ms0:/PSP/GAME/gpsp-temp", 0777);
    sceIoMkdir("ms0:/PSP/GAME/gpsp-temp/ROMS", 0777);
    sceIoMkdir("ms0:/PSP/GAME/gpsp-temp/SAVES", 0777);
    
    // Scan initial directory
    if(scan_directory(current_directory) != 0) {
        return -1;
    }
    
    int done = 0;
    int result = -1;
    
    // Initial draw
    draw_file_list();
    
    while(!done) {
        // Proper input reading
        psp_input_update();
        u32 buttons = psp_get_buttons_pressed();
        
        if(buttons & PSP_CTRL_UP) {
            if(current_selection > 0) {
                current_selection--;
                if(current_selection < scroll_offset)
                    scroll_offset = current_selection;
            }
            draw_file_list();
        } else if(buttons & PSP_CTRL_DOWN) {
            if(current_selection < file_count - 1) {
                current_selection++;
                if(current_selection >= scroll_offset + FILES_PER_PAGE)
                    scroll_offset = current_selection - FILES_PER_PAGE + 1;
            }
            draw_file_list();
        } else if(buttons & PSP_CTRL_CROSS) {
            // Select
            if(file_list[current_selection].is_dir) {
                // Enter directory
                if(strcmp(file_list[current_selection].name, "..") == 0) {
                    // Go up one level
                    char *last_slash = strrchr(current_directory, '/');
                    if(last_slash && last_slash != current_directory) {
                        *last_slash = '\0';
                    }
                } else {
                    // Go into directory
                    strcat(current_directory, "/");
                    strcat(current_directory, file_list[current_selection].name);
                }
                
                current_selection = 0;
                scroll_offset = 0;
                scan_directory(current_directory);
                draw_file_list();
            } else {
                // Select file
                snprintf(selected_file, max_len, "%s/%s", 
                        current_directory, file_list[current_selection].name);
                result = 0;
                done = 1;
            }
        } else if(buttons & PSP_CTRL_CIRCLE) {
            // Cancel
            result = -1;
            done = 1;
        }
        
        // Small delay to prevent excessive CPU usage and allow button debouncing
        sceKernelDelayThread(16666); // ~60 FPS refresh rate
    }
    
    pspDebugScreenClear();
    
    return result;
}

int psp_show_menu(void)
{
    const char *menu_items[] = {
        "Resume Game",
        "Load New ROM",
        "Save State",
        "Load State",
        "Options",
        "Exit to Menu",
        "Exit Emulator"
    };
    const int num_items = 7;
    int selection = 0;
    int done = 0;
    int result = MENU_RESUME;
    
    // Draw menu once
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    
    pspDebugScreenSetTextColor(0xFFFFFF00);
    pspDebugScreenPrintf("gpSP - Menu\n");
    pspDebugScreenSetTextColor(TEXT_COLOR);
    pspDebugScreenPrintf("--------------------------------------------------\n\n");
    
    for(int i = 0; i < num_items; i++) {
        if(i == selection) {
            pspDebugScreenSetTextColor(SELECTED_COLOR);
            pspDebugScreenPrintf("> %s\n", menu_items[i]);
        } else {
            pspDebugScreenSetTextColor(TEXT_COLOR);
            pspDebugScreenPrintf("  %s\n", menu_items[i]);
        }
    }
    
    pspDebugScreenSetXY(0, 20);
    pspDebugScreenSetTextColor(0xFF888888);
    pspDebugScreenPrintf("--------------------------------------------------\n");
    pspDebugScreenPrintf("Up/Down: Select  X: Choose  O: Cancel\n");
    sceDisplayWaitVblankStart();
    
    while(!done) {
        // Proper input reading
        psp_input_update();
        u32 buttons = psp_get_buttons_pressed();
        
        if(buttons & PSP_CTRL_UP) {
            selection = (selection - 1 + num_items) % num_items;
            // Redraw menu
            goto redraw_menu;
        } else if(buttons & PSP_CTRL_DOWN) {
            selection = (selection + 1) % num_items;
            // Redraw menu
            goto redraw_menu;
        } else if(buttons & PSP_CTRL_CROSS) {
            switch(selection) {
                case 0: result = MENU_RESUME; done = 1; break;
                case 1: result = MENU_LOAD_ROM; done = 1; break;
                case 2: result = MENU_SAVE_STATE; /* TODO */ break;
                case 3: result = MENU_LOAD_STATE; /* TODO */ break;
                case 4: /* Options */ break;
                case 5: result = MENU_EXIT; done = 1; break;
                case 6: result = MENU_EXIT; done = 1; break;
            }
        } else if(buttons & PSP_CTRL_CIRCLE) {
            result = MENU_RESUME;
            done = 1;
        }
        
        // Small delay for button debouncing
        sceKernelDelayThread(16666); // ~60 FPS
        continue;
        
redraw_menu:
        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 0);
        
        pspDebugScreenSetTextColor(0xFFFFFF00);
        pspDebugScreenPrintf("gpSP-temp - Menu\n");
        pspDebugScreenSetTextColor(TEXT_COLOR);
        pspDebugScreenPrintf("--------------------------------------------------\n\n");
        
        for(int i = 0; i < num_items; i++) {
            if(i == selection) {
                pspDebugScreenSetTextColor(SELECTED_COLOR);
                pspDebugScreenPrintf("> %s\n", menu_items[i]);
            } else {
                pspDebugScreenSetTextColor(TEXT_COLOR);
                pspDebugScreenPrintf("  %s\n", menu_items[i]);
            }
        }
        
        pspDebugScreenSetXY(0, 20);
        pspDebugScreenSetTextColor(0xFF888888);
        pspDebugScreenPrintf("--------------------------------------------------\n");
        pspDebugScreenPrintf("Up/Down: Select  X: Choose  O: Cancel\n");
        sceDisplayWaitVblankStart();
    }
    
    pspDebugScreenClear();
    
    return result;
}

void psp_show_error(const char *message)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 10);
    pspDebugScreenSetTextColor(0xFF0000FF);
    pspDebugScreenPrintf("Error: %s\n\n", message);
    pspDebugScreenSetTextColor(TEXT_COLOR);
    pspDebugScreenPrintf("Press X to continue...\n");
    
    sceDisplayWaitVblankStart();
    
    // Wait for button
    while(1) {
        sceKernelDelayThread(100000);
        u32 buttons = psp_get_buttons_pressed();
        if(buttons & PSP_CTRL_CROSS)
            break;
    }
    
    pspDebugScreenClear();
}

void psp_show_message(const char *message)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 10);
    pspDebugScreenSetTextColor(0xFF00FF00);
    pspDebugScreenPrintf("%s\n\n", message);
    pspDebugScreenSetTextColor(TEXT_COLOR);
    pspDebugScreenPrintf("Press X to continue...\n");
    
    sceDisplayWaitVblankStart();
    
    // Wait for button
    while(1) {
        sceKernelDelayThread(100000);
        u32 buttons = psp_get_buttons_pressed();
        if(buttons & PSP_CTRL_CROSS)
            break;
    }
    
    pspDebugScreenClear();
}

