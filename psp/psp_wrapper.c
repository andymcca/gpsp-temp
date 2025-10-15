/* PSP wrapper functions to replace libretro dependencies */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "psp_wrapper.h"
#include "../common.h"
#include "../gba_memory.h"

// Replacement for libretro file functions
typedef struct {
    FILE *fp;
    long size;
} psp_file_t;

static psp_file_t gamepak_file_handle = {NULL, 0};

void* filestream_open(const char *path, unsigned mode, unsigned hints)
{
    psp_file_t *file = (psp_file_t*)malloc(sizeof(psp_file_t));
    if(!file)
        return NULL;
    
    const char *fmode = "rb";
    if(mode & RETRO_VFS_FILE_ACCESS_WRITE)
        fmode = (mode & RETRO_VFS_FILE_ACCESS_READ) ? "r+b" : "wb";
    
    file->fp = fopen(path, fmode);
    if(!file->fp) {
        free(file);
        return NULL;
    }
    
    // Get file size
    fseek(file->fp, 0, SEEK_END);
    file->size = ftell(file->fp);
    fseek(file->fp, 0, SEEK_SET);
    
    return file;
}

void filestream_close(void *stream)
{
    if(!stream)
        return;
    
    psp_file_t *file = (psp_file_t*)stream;
    if(file->fp) {
        fclose(file->fp);
        file->fp = NULL;
    }
    free(file);
}

int64_t filestream_read(void *stream, void *data, int64_t len)
{
    if(!stream)
        return -1;
    
    psp_file_t *file = (psp_file_t*)stream;
    return fread(data, 1, len, file->fp);
}

int64_t filestream_write(void *stream, const void *data, int64_t len)
{
    if(!stream)
        return -1;
    
    psp_file_t *file = (psp_file_t*)stream;
    return fwrite(data, 1, len, file->fp);
}

int64_t filestream_seek(void *stream, int64_t offset, int whence)
{
    if(!stream)
        return -1;
    
    psp_file_t *file = (psp_file_t*)stream;
    
    // The RETRO_VFS constants are defined to match SEEK_* values
    // so we can use whence directly, but validate it first
    int origin = whence;
    
    // Validate that whence is a valid value
    if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END &&
       whence != RETRO_VFS_SEEK_POSITION_START && 
       whence != RETRO_VFS_SEEK_POSITION_CURRENT &&
       whence != RETRO_VFS_SEEK_POSITION_END) {
        return -1;
    }
    
    return fseek(file->fp, offset, origin);
}

int64_t filestream_tell(void *stream)
{
    if(!stream)
        return -1;
    
    psp_file_t *file = (psp_file_t*)stream;
    return ftell(file->fp);
}

int64_t filestream_get_size(void *stream)
{
    if(!stream)
        return -1;
    
    psp_file_t *file = (psp_file_t*)stream;
    return file->size;
}

// Wrapper for load_gamepak that doesn't need retro_game_info
s32 load_gamepak_raw(const char *name);

int load_gamepak_psp(const char *filename)
{
    extern char save_path[512];
    
    if(load_gamepak_raw(filename) != 0)
        return -1;
    
    // Build save filename in SAVES directory
    char save_filename[512];
    char rom_name[256];
    
    // Extract ROM filename (without path)
    const char *rom_basename = strrchr(filename, '/');
    if(rom_basename)
        rom_basename++;
    else
        rom_basename = filename;
    
    strncpy(rom_name, rom_basename, sizeof(rom_name) - 1);
    rom_name[sizeof(rom_name) - 1] = '\0';
    
    // Change extension to .sav
    char *ext_pos = strrchr(rom_name, '.');
    if(ext_pos) {
        strcpy(ext_pos, ".sav");
    } else {
        strcat(rom_name, ".sav");
    }
    
    // Build full path in SAVES directory
    snprintf(save_filename, sizeof(save_filename), "ms0:/PSP/GAME/gpsp-temp/SAVES/%s", rom_name);
    
    // Try to load save data
    FILE *save_file = fopen(save_filename, "rb");
    if(save_file) {
        fread(gamepak_backup, 1, sizeof(gamepak_backup), save_file);
        fclose(save_file);
    }
    
    return 0;
}

// Update backup (save) file
void update_backup_psp(const char *filename)
{
    extern u32 backup_type;
    extern u32 sram_bankcount;
    extern u32 flash_bank_cnt;
    extern u32 eeprom_size;
    
    // Don't save if no backup detected
    if(backup_type == BACKUP_NONE)
        return;
    
    char save_filename[512];
    char rom_name[256];
    
    // Extract ROM filename (without path)
    const char *rom_basename = strrchr(filename, '/');
    if(rom_basename)
        rom_basename++;
    else
        rom_basename = filename;
    
    strncpy(rom_name, rom_basename, sizeof(rom_name) - 1);
    rom_name[sizeof(rom_name) - 1] = '\0';
    
    // Change extension to .sav
    char *ext_pos = strrchr(rom_name, '.');
    if(ext_pos) {
        strcpy(ext_pos, ".sav");
    } else {
        strcat(rom_name, ".sav");
    }
    
    // Build full path in SAVES directory
    snprintf(save_filename, sizeof(save_filename), "ms0:/PSP/GAME/gpsp-temp/SAVES/%s", rom_name);
    
    // Calculate actual backup size based on type
    u32 backup_size = 0;
    
    switch(backup_type)
    {
        case BACKUP_SRAM:
            backup_size = 0x8000 * sram_bankcount;
            break;
        
        case BACKUP_FLASH:
            backup_size = 0x10000 * flash_bank_cnt;
            break;
        
        case BACKUP_EEPROM:
            backup_size = 0x200 * eeprom_size;
            break;
        
        default:
            return;
    }
    
    FILE *save_file = fopen(save_filename, "wb");
    if(save_file) {
        fwrite(gamepak_backup, 1, backup_size, save_file);
        fclose(save_file);
    }
}

