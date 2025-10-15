/* gpSP PSP Standalone
 * PSP input interface
 */

#ifndef PSP_INPUT_H
#define PSP_INPUT_H

#include <pspctrl.h>

void psp_input_init(void);
u32 psp_input_update(void);
u32 psp_get_buttons_pressed(void);
u32 psp_get_buttons_held(void);

// PSP button masks that return button states
#define PSP_CTRL_HOME      0x1000

#endif

