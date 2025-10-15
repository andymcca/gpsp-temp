/* PSP wrapper functions header */

#ifndef PSP_WRAPPER_H
#define PSP_WRAPPER_H

#include <stdint.h>

// VFS compatibility defines
#define RETRO_VFS_FILE_ACCESS_READ            (1 << 0)
#define RETRO_VFS_FILE_ACCESS_WRITE           (1 << 1)
#define RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING (1 << 2)
#define RETRO_VFS_FILE_ACCESS_HINT_NONE              0
#define RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS   1

#define RETRO_VFS_SEEK_POSITION_START    0
#define RETRO_VFS_SEEK_POSITION_CURRENT  1
#define RETRO_VFS_SEEK_POSITION_END      2

// RFILE is the libretro file handle type - we use void* for PSP
typedef void RFILE;

// File stream functions (PSP implementations)
void* filestream_open(const char *path, unsigned mode, unsigned hints);
void filestream_close(void *stream);
int64_t filestream_read(void *stream, void *data, int64_t len);
int64_t filestream_write(void *stream, const void *data, int64_t len);
int64_t filestream_seek(void *stream, int64_t offset, int whence);
int64_t filestream_tell(void *stream);
int64_t filestream_get_size(void *stream);

// PSP-specific game loading
int load_gamepak_psp(const char *filename);
void update_backup_psp(const char *filename);

#endif

