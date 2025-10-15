/* gpSP PSP Standalone
 * PSP video interface
 */

#ifndef PSP_VIDEO_H
#define PSP_VIDEO_H

#include <pspgu.h>
#include <pspdisplay.h>

void psp_video_init(void);
void psp_video_deinit(void);
void psp_video_render(u16 *gba_pixels);
void psp_video_clear(u32 color);
void psp_video_flip(void);

#endif

