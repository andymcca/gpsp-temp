/* gpSP PSP Standalone
 * PSP video implementation using GU
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>
#include <stdlib.h>

#include "psp_video.h"
#include "../common.h"

#define BUF_WIDTH  512
#define SCR_WIDTH  480
#define SCR_HEIGHT 272
#define PIXEL_SIZE 2  // 16-bit

static unsigned int __attribute__((aligned(16))) display_list[262144];
static void* frame_buffer[2];
static int current_buffer = 0;

// Calculate screen position for centered GBA display
// GBA is 240x160, PSP is 480x272
// We can scale 2x to 480x320, but that's taller than 272
// So we'll scale to fit width: 480x320 cropped to 480x272
#define SCALE_X 2
#define SCALE_Y 2
#define DISPLAY_WIDTH  (GBA_SCREEN_WIDTH * SCALE_X)
#define DISPLAY_HEIGHT (GBA_SCREEN_HEIGHT * SCALE_Y)
#define OFFSET_X ((SCR_WIDTH - DISPLAY_WIDTH) / 2)
#define OFFSET_Y ((SCR_HEIGHT - DISPLAY_HEIGHT) / 2)

typedef struct {
    unsigned short u, v;
    unsigned short color;
    short x, y, z;
} Vertex;

void psp_video_init(void)
{
    // CRITICAL: Completely disable and clear debug screen
    
    // Turn off display temporarily
    sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
    sceDisplaySetFrameBuf((void*)0x04000000, BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_IMMEDIATE);
    
    // Clear all of VRAM (not just framebuffers)
    u32* vram = (u32*)0x04000000;
    for(int i = 0; i < (512 * 272 * 4) / 4; i++) {  // Clear more than needed
        vram[i] = 0;
    }
    
    // Wait for several vblanks to ensure clear
    sceDisplayWaitVblankStart();
    sceDisplayWaitVblankStart();
    
    // Now initialize GU (takes over display)
    sceGuInit();
    
    // Allocate framebuffers
    frame_buffer[0] = (void*)0x04000000;
    frame_buffer[1] = (void*)(0x04000000 + BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE);
    
    // Start GU
    sceGuStart(GU_DIRECT, display_list);
    
    // Set buffers
    sceGuDrawBuffer(GU_PSM_5650, frame_buffer[0], BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, frame_buffer[1], BUF_WIDTH);
    sceGuDepthBuffer((void*)(0x04000000 + BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE * 2), BUF_WIDTH);
    
    // Setup GU
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_CLIP_PLANES);
    
    sceGuTexMode(GU_PSM_5650, 0, 0, 0);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    
    sceGuFinish();
    sceGuSync(0, 0);
    
    // CRITICAL FIX: Tell display hardware to use GU framebuffer
    // This overrides pspDebugScreen's framebuffer setting
    sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
    sceDisplaySetFrameBuf(frame_buffer[0], BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_IMMEDIATE);
    
    // Clear both framebuffers to black
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void psp_video_deinit(void)
{
    sceGuTerm();
}

void psp_video_render(u16 *gba_pixels)
{
    if(!gba_pixels)
        return;
    
    sceGuStart(GU_DIRECT, display_list);
    
    // Disable depth test for 2D rendering
    sceGuDisable(GU_DEPTH_TEST);
    
    // PSP stores pixels in BGR565, but gba_pixels is RGB565 after convert_palette
    // We need to swap R and B channels before uploading
    static u16 swizzled_pixels[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];
    for(int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u16 pixel = gba_pixels[i];
        // Swap R and B: RGB565 -> BGR565
        // Extract: RRRRRGGGGGGBBBBB
        u16 r = (pixel >> 11) & 0x1F;
        u16 g = (pixel >> 5) & 0x3F;
        u16 b = pixel & 0x1F;
        // Repack: BBBBBGGGGGGRRRRR
        swizzled_pixels[i] = (b << 11) | (g << 5) | r;
    }
    
    // Make pixels available as texture
    sceKernelDcacheWritebackInvalidateRange(swizzled_pixels, GBA_SCREEN_BUFFER_SIZE);
    
    sceGuTexMode(GU_PSM_5650, 0, 0, 0);
    sceGuTexImage(0, 256, 256, GBA_SCREEN_PITCH, swizzled_pixels);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);  // Use nearest for pixel-perfect
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    
    sceGuDisable(GU_BLEND);
    
    // Draw scaled GBA screen
    Vertex* vertices = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    
    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[0].color = 0;
    vertices[0].x = 0;
    vertices[0].y = 0;
    vertices[0].z = 0;
    
    vertices[1].u = GBA_SCREEN_WIDTH;
    vertices[1].v = GBA_SCREEN_HEIGHT;
    vertices[1].color = 0;
    vertices[1].x = SCR_WIDTH;
    vertices[1].y = SCR_HEIGHT;
    vertices[1].z = 0;
    
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
    
    sceGuFinish();
    sceGuSync(0, 0);
    
    // Swap buffers
    sceDisplayWaitVblankStart();
    void* new_buffer = sceGuSwapBuffers();
    
    // Update display to show new buffer (don't call sceGuSwapBuffers again!)
    sceDisplaySetFrameBuf(new_buffer, BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
    
    frame_buffer[current_buffer] = new_buffer;
    current_buffer ^= 1;
}

void psp_video_clear(u32 color)
{
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
}

void psp_video_flip(void)
{
    sceDisplayWaitVblankStart();
    frame_buffer[current_buffer] = sceGuSwapBuffers();
    current_buffer ^= 1;
}


