/* Stub libretro definitions for PSP standalone build */

#ifndef LIBRETRO_STUB_H
#define LIBRETRO_STUB_H

// Minimal libretro type definitions needed for compilation
typedef void (*retro_log_printf_t)(int level, const char *fmt, ...);
typedef short (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);

// Libretro button IDs
#define RETRO_DEVICE_JOYPAD       1
#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11
#define RETRO_DEVICE_ID_JOYPAD_L2      12
#define RETRO_DEVICE_ID_JOYPAD_R2      13
#define RETRO_DEVICE_ID_JOYPAD_L3      14
#define RETRO_DEVICE_ID_JOYPAD_R3      15
#define RETRO_DEVICE_ID_JOYPAD_MASK    256

// Stub structure for game info (not used in PSP standalone)
struct retro_game_info {
    const char *path;
    const void *data;
    size_t size;
    const char *meta;
};

#endif

