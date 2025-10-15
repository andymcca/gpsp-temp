/* gpSP PSP Standalone
 * PSP input implementation
 */

#include <pspkernel.h>
#include <pspctrl.h>
#include <string.h>

#include "psp_input.h"
#include "../common.h"
#include "../input.h"

static SceCtrlData pad_data;
static SceCtrlData prev_pad_data;

// Mapping from PSP buttons to GBA buttons
typedef struct {
    u32 psp_button;
    u32 gba_button;
} button_map_entry;

static const button_map_entry button_map[] = {
    { PSP_CTRL_UP,       BUTTON_UP },
    { PSP_CTRL_DOWN,     BUTTON_DOWN },
    { PSP_CTRL_LEFT,     BUTTON_LEFT },
    { PSP_CTRL_RIGHT,    BUTTON_RIGHT },
    { PSP_CTRL_CROSS,    BUTTON_A },      // X button = A
    { PSP_CTRL_CIRCLE,   BUTTON_B },      // O button = B
    { PSP_CTRL_START,    BUTTON_START },
    { PSP_CTRL_SELECT,   BUTTON_SELECT },
    { PSP_CTRL_LTRIGGER, BUTTON_L },
    { PSP_CTRL_RTRIGGER, BUTTON_R },
};

#define BUTTON_MAP_SIZE (sizeof(button_map) / sizeof(button_map_entry))

void psp_input_init(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    
    memset(&pad_data, 0, sizeof(SceCtrlData));
    memset(&prev_pad_data, 0, sizeof(SceCtrlData));
}

u32 psp_input_update(void)
{
    extern u16 io_registers[512];  // Ensure we have access to io_registers
    
    prev_pad_data = pad_data;
    sceCtrlReadBufferPositive(&pad_data, 1);
    
    // Convert PSP buttons to GBA buttons
    u32 gba_buttons = 0;
    
    for(int i = 0; i < BUTTON_MAP_SIZE; i++) {
        if(pad_data.Buttons & button_map[i].psp_button) {
            gba_buttons |= button_map[i].gba_button;
        }
    }
    
    // Update GBA input state
    // The GBA input register uses inverse logic (0 = pressed)
    // Use write_ioreg which handles byte swapping properly
    u16 input_state = ~gba_buttons & 0x3FF;
    write_ioreg(REG_P1, input_state);
    
    // Return PSP button state for menu handling
    return pad_data.Buttons;
}

u32 psp_get_buttons_pressed(void)
{
    return pad_data.Buttons & ~prev_pad_data.Buttons;
}

u32 psp_get_buttons_held(void)
{
    return pad_data.Buttons;
}

