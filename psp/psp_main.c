/* gpSP PSP Standalone
 * PSP-specific main file
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <psppower.h>
#include <psputility.h>
#include <psprtc.h>
#include <pspaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../gba_memory.h"
#include "../gba_cc_lut.h"
#include "../cpu.h"
#include "../sound.h"
#include "../input.h"
#include "../video.h"
#include "psp_video.h"
#include "psp_audio.h"
#include "psp_input.h"
#include "psp_menu.h"
#include "psp_wrapper.h"
#include "psp_config.h"

PSP_MODULE_INFO("gpSP-temp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(12 * 1024);

// GBA frame timing
#define GBA_FPS 59.72750057f
#define FRAME_TIME_US (1000000.0f / GBA_FPS)

// Global variables required by emulator core
u32 frame_counter = 0;
u32 cpu_ticks = 0;
u32 execute_cycles = 0;
s32 video_count = 0;

u32 last_frame = 0;
u32 flush_ram_count = 0;
u32 gbc_update_count = 0;
u32 oam_update_count = 0;

// Dynarec variables
u32 idle_loop_target_pc = 0xFFFFFFFF;
u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];
u32 translation_gate_targets = 0;

// Video variables
u32 skip_next_frame = 0;

// Timer array
timer_type timer[4];

// Random number generator state
static u32 random_state = 0;

extern u16* gba_screen_pixels;
extern u8 bios_rom[0x4000];

int dynarec_enable = 1;
boot_mode selected_boot_mode = boot_game;
int sprite_limit = 1;

char main_path[512];
char save_path[512];

static int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

// Random number generator functions
u16 rand_gen(void)
{
    random_state = ((random_state * 1103515245) + 12345) & 0x7fffffff;
    return random_state;
}

void rand_seed(u32 data)
{
    random_state ^= rand_gen() ^ data;
}

// Fastforward override stub (not needed on PSP standalone)
void set_fastforward_override(bool fastforward)
{
    // PSP standalone doesn't use libretro fastforward
    (void)fastforward;
}

// Savestate functions stubs (to be implemented later)
bool main_check_savestate(const u8 *src)
{
    // TODO: Implement savestate checking
    (void)src;
    return true;
}

bool main_read_savestate(const u8 *src)
{
    // TODO: Implement savestate loading
    (void)src;
    return false;
}

unsigned main_write_savestate(u8 *dst)
{
    // TODO: Implement savestate saving
    (void)dst;
    return 0;
}

// Initialize timer state
void init_main(void)
{
    u32 i;
    for(i = 0; i < 4; i++)
    {
        timer[i].status = TIMER_INACTIVE;
        timer[i].prescale = 0;
        timer[i].irq = 0;
        timer[i].reload = timer[i].count = 0x10000;
        timer[i].direct_sound_channels = TIMER_DS_CHANNEL_NONE;
        timer[i].frequency_step = 0;
    }

    timer[0].direct_sound_channels = TIMER_DS_CHANNEL_BOTH;
    timer[1].direct_sound_channels = TIMER_DS_CHANNEL_NONE;

    frame_counter = 0;
    cpu_ticks = 0;
    execute_cycles = 960;
    video_count = 960;

#ifdef HAVE_DYNAREC
    init_dynarec_caches();
    init_emitter(gamepak_must_swap());
#endif
}

// Adjust direct sound buffer index based on CPU timing (from TempGBA)
static void adjust_direct_sound_buffer(u8 channel, u32 cpu_ticks)
{
    extern u32 gbc_sound_buffer_index;
    extern u32 gbc_sound_last_cpu_ticks;
    extern u32 gbc_sound_partial_ticks;
    extern direct_sound_struct direct_sound_channel[2];
    
    u64 count_ticks;
    u32 buffer_ticks, partial_ticks;
    
    // Calculate elapsed ticks scaled by sound frequency
    u32 tick_delta = cpu_ticks - gbc_sound_last_cpu_ticks;
    count_ticks = (u64)tick_delta * (u64)GBA_SOUND_FREQUENCY;
    
    // Convert to buffer ticks (fixed point 8.24 format)
    buffer_ticks = (u32)(count_ticks >> 24);
    partial_ticks = gbc_sound_partial_ticks + (count_ticks & 0x00FFFFFF);
    
    if(partial_ticks > 0x00FFFFFF) {
        buffer_ticks++;
        partial_ticks &= 0x00FFFFFF;
    }
    
    // Update buffer index for this channel
    #define BUFFER_SIZE (65536)
    direct_sound_channel[channel].buffer_index = (gbc_sound_buffer_index + (buffer_ticks << 1)) % BUFFER_SIZE;
    #undef BUFFER_SIZE
}

// Update timers and return DMA cycles
static unsigned update_timers(irq_type *irq_raised, unsigned completed_cycles)
{
    unsigned i, ret = 0;
    for (i = 0; i < 4; i++)
    {
        if(timer[i].status == TIMER_INACTIVE)
            continue;

        if(timer[i].status != TIMER_CASCADE)
        {
            timer[i].count -= completed_cycles;
            write_ioreg(REG_TMXD(i), -(timer[i].count >> timer[i].prescale));
        }

        if(timer[i].count > 0)
            continue;

        if(timer[i].irq)
            *irq_raised |= (IRQ_TIMER0 << i);

        if((i != 3) && (timer[i + 1].status == TIMER_CASCADE))
        {
            timer[i + 1].count--;
            write_ioreg(REG_TMXD(i + 1), -timer[i+1].count);
        }

        if(i < 2)
        {
            // CRITICAL FIX: Update buffer index BEFORE calling sound_timer (like TempGBA)
            if(timer[i].direct_sound_channels & 0x01)
            {
                adjust_direct_sound_buffer(0, cpu_ticks + (timer[i].reload << timer[i].prescale));
                ret += sound_timer(timer[i].frequency_step, 0);
            }

            if(timer[i].direct_sound_channels & 0x02)
            {
                adjust_direct_sound_buffer(1, cpu_ticks + (timer[i].reload << timer[i].prescale));
                ret += sound_timer(timer[i].frequency_step, 1);
            }
        }

        timer[i].count += (timer[i].reload << timer[i].prescale);
    }
    return ret;
}

// Reset GBA emulation
void reset_gba(void)
{
    init_memory();  // Critical: Initialize memory regions and I/O
    init_main();
    init_cpu();
    reset_sound();
}

// Update GBA (main emulation loop function)
u32 function_cc update_gba(int remaining_cycles)
{
    u32 changed_pc = 0;
    u32 frame_complete = 0;
    irq_type irq_raised = IRQ_NONE;
    int dma_cycles;

    remaining_cycles = MAX(remaining_cycles, -64);

    do
    {
        unsigned i;
        unsigned completed_cycles = execute_cycles - remaining_cycles;
        cpu_ticks += completed_cycles;

        remaining_cycles = 0;

        dma_cycles = update_timers(&irq_raised, completed_cycles);
        if (update_serial(completed_cycles))
            irq_raised |= IRQ_SERIAL;

        video_count -= completed_cycles;

        if(video_count <= 0)
        {
            u32 vcount = read_ioreg(REG_VCOUNT);
            u32 dispstat = read_ioreg(REG_DISPSTAT);

            if ((dispstat & 0x02) == 0)
            {
                dispstat |= 0x02;
                video_count += (272);

                if ((dispstat & 0x01) == 0)
                {
                    if(reg[OAM_UPDATED])
                        oam_update_count++;

                    update_scanline();

                    for (i = 0; i < 4; i++)
                    {
                        if(dma[i].start_type == DMA_START_HBLANK)
                            dma_transfer(i, &dma_cycles);
                    }
                }

                if (dispstat & 0x10)
                    irq_raised |= IRQ_HBLANK;
            }
            else
            {
                video_count += 960;
                dispstat &= ~0x02;
                vcount++;

                if(vcount == 160)
                {
                    dispstat |= 0x01;
                    video_reload_counters();

                    if (dispstat & 0x8)
                        irq_raised |= IRQ_VBLANK;

                    for (i = 0; i < 4; i++)
                    {
                        if(dma[i].start_type == DMA_START_VBLANK)
                            dma_transfer(i, &dma_cycles);
                    }
                }
                else if (vcount == 228)
                {
                    vcount = 0;
                    dispstat &= ~0x01;

                    if (cheat_master_hook == ~0U)
                        process_cheats();

                    gbc_update_count = 0;
                    oam_update_count = 0;
                    flush_ram_count = 0;

                    render_gbc_sound();

                    frame_complete = 0x80000000;
                    frame_counter++;
                }

                if(vcount == (dispstat >> 8))
                {
                    dispstat |= 0x04;
                    if(dispstat & 0x20)
                        irq_raised |= IRQ_VCOUNT;
                }
                else
                    dispstat &= ~0x04;

                write_ioreg(REG_VCOUNT, vcount);
            }
            write_ioreg(REG_DISPSTAT, dispstat);
        }

        if (irq_raised)
            flag_interrupt(irq_raised);

        if (check_and_raise_interrupts())
            changed_pc = 0x40000000;

        execute_cycles = MAX(video_count, 0);
        
        {
            u32 cc = serial_next_event();
            execute_cycles = MIN(execute_cycles, cc);
        }

        if (reg[CPU_HALT_STATE] == CPU_DMA) {
            u32 dma_cyc = reg[REG_SLEEP_CYCLES];
            if (dma_cyc & 0x80000000)
                dma_cyc &= 0x7FFFFFFF;
            else
                dma_cyc -= MIN(dma_cyc, completed_cycles);

            reg[REG_SLEEP_CYCLES] = dma_cyc;
            if (!dma_cyc)
                reg[CPU_HALT_STATE] = CPU_ACTIVE;
            else
                execute_cycles = MIN(execute_cycles, dma_cyc);
        }

        for (i = 0; i < 4; i++)
        {
            if (timer[i].status == TIMER_PRESCALE &&
                timer[i].count < execute_cycles)
                execute_cycles = timer[i].count;
        }
    } while(reg[CPU_HALT_STATE] != CPU_ACTIVE && !frame_complete);

    dma_cycles = MIN(64, dma_cycles);
    dma_cycles = MIN(execute_cycles, dma_cycles);

    return (execute_cycles - dma_cycles) | changed_pc | frame_complete;
}

void init_gpsp(void)
{
    // Set CPU clock speed
    scePowerSetClockFrequency(333, 333, 166);
    
    // DON'T initialize video here - it conflicts with pspDebugScreen
    // We'll initialize it when a ROM is loaded
    
    // Initialize audio
    psp_audio_init();
    
    // Initialize input
    psp_input_init();
    
    // Initialize emulator core
    init_gamepak_buffer();
    init_sound();
    
    // Allocate screen buffer
    if(!gba_screen_pixels)
        gba_screen_pixels = (uint16_t*)malloc(GBA_SCREEN_BUFFER_SIZE);
    
    // Initialize with black pixels
    if(gba_screen_pixels) {
        for(int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++)
            gba_screen_pixels[i] = 0x0000;
    }
}

void deinit_gpsp(void)
{
    memory_term();
    
    if(gba_screen_pixels) {
        free(gba_screen_pixels);
        gba_screen_pixels = NULL;
    }
    
    psp_audio_deinit();
    psp_video_deinit();
}

static char current_rom[512];

int load_rom(const char *filename)
{
    // Set paths
    strncpy(main_path, filename, sizeof(main_path) - 1);
    main_path[sizeof(main_path) - 1] = '\0';
    
    strncpy(current_rom, filename, sizeof(current_rom) - 1);
    current_rom[sizeof(current_rom) - 1] = '\0';
    
    // Extract directory for saves
    char *last_slash = strrchr(main_path, '/');
    if(last_slash) {
        size_t dir_len = last_slash - main_path;
        strncpy(save_path, main_path, dir_len);
        save_path[dir_len] = '\0';
    } else {
        strcpy(save_path, "ms0:/PSP/GAME/gpsp-temp/SAVES");
    }
    
    // NOW initialize video - after debug screen is done
    // This prevents conflicts with pspDebugScreen
    static int video_initialized = 0;
    if(!video_initialized) {
        psp_video_init();
        video_initialized = 1;
    }
    
    // Create BIOS directory if it doesn't exist
    sceIoMkdir("ms0:/PSP/GAME/gpsp-temp/BIOS", 0777);
    
    // Try to load genuine BIOS from BIOS folder
    const char *bios_path = "ms0:/PSP/GAME/gpsp-temp/BIOS/gba_bios.bin";
    FILE *bios_file = fopen(bios_path, "rb");
    
    if(bios_file) {
        // Load genuine BIOS
        size_t bios_read = fread(bios_rom, 1, sizeof(bios_rom), bios_file);
        fclose(bios_file);
        
        if(bios_read == sizeof(bios_rom)) {
            // Successfully loaded genuine BIOS
        } else {
            // Invalid BIOS file, fall back to built-in
            memcpy(bios_rom, open_gba_bios_rom, sizeof(bios_rom));
        }
    } else {
        // No BIOS file found, use built-in open-source BIOS
        memcpy(bios_rom, open_gba_bios_rom, sizeof(bios_rom));
    }
    
    // Load ROM using PSP wrapper
    memset(gamepak_backup, -1, sizeof(gamepak_backup));
    
    if(load_gamepak_psp(filename) != 0) {
        psp_show_error("Could not load ROM");
        return -1;
    }
    
    // Reset GBA
    reset_gba();
    
    return 0;
}

void run_emulation(void)
{
    u64 last_time, current_time;
    u32 frame_count = 0;
    u32 fps_count = 0;
    u64 fps_time;
    u64 last_save_time;
    
    sceRtcGetCurrentTick(&last_time);
    fps_time = last_time;
    last_save_time = last_time;
    
    int running = 1;
    int paused = 0;
    
    while(running) {
        // Handle input
        u32 buttons = psp_input_update();
        
        // Check for menu button (HOME)
        if(buttons & PSP_CTRL_HOME) {
            paused = 1;
            int menu_result = psp_show_menu();
            if(menu_result == MENU_RESUME) {
                paused = 0;
                sceRtcGetCurrentTick(&last_time);
                fps_time = last_time;
            } else if(menu_result == MENU_EXIT) {
                running = 0;
                break;
            } else if(menu_result == MENU_LOAD_ROM) {
                running = 0;
                break;
            }
        }
        
        if(!paused) {
            // Run one frame of emulation
            skip_next_frame = 0;
            
            #ifdef HAVE_DYNAREC
            if(dynarec_enable)
                execute_arm_translate(execute_cycles);
            else
            #endif
            {
                clear_gamepak_stickybits();
                execute_arm(execute_cycles);
            }
            
            // Render video
            if(!skip_next_frame)
                psp_video_render(gba_screen_pixels);
            
            // Update audio AFTER timing to not slow down emulation
            // Audio blocks so do it last
            psp_audio_update();
            
            frame_count++;
            fps_count++;
            
            // Frame timing
            sceRtcGetCurrentTick(&current_time);
            u64 frame_time = current_time - last_time;
            u64 target_time = FRAME_TIME_US;
            
            // Normal frame delay
            if(frame_time < target_time) {
                sceKernelDelayThread(target_time - frame_time);
            }
            
            last_time = current_time;
            
            // Calculate FPS every second
            if(current_time - fps_time >= 1000000) {
                fps_time = current_time;
                fps_count = 0;
            }
            
            // Auto-save every 5 seconds
            if(current_time - last_save_time >= 5000000) {
                update_backup_psp(current_rom);
                last_save_time = current_time;
            }
        }
    }
    
    // Final save on exit
    update_backup_psp(current_rom);
}

int main(int argc, char *argv[])
{
    setup_callbacks();
    
    pspDebugScreenInit();
    
    // Load configuration
    psp_config_load();
    
    // Show options menu at startup
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 10);
    pspDebugScreenSetTextColor(0xFFFFFF00);
    pspDebugScreenPrintf("         gpSP-temp GBA Emulator for PSP\n\n");
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("      Press X to start with current settings\n");
    pspDebugScreenPrintf("      Press Triangle to configure options\n");
    
    sceDisplayWaitVblankStart();
    
    // Wait for button
    int show_options = 0;
    while(1) {
        psp_input_update();
        u32 buttons = psp_get_buttons_pressed();
        
        if(buttons & PSP_CTRL_CROSS) {
            break;  // Start with current settings
        }
        else if(buttons & PSP_CTRL_TRIANGLE) {
            show_options = 1;
            break;  // Show options
        }
        
        sceKernelDelayThread(16666);
    }
    
    if(show_options) {
        if(psp_show_options_menu() != 0) {
            // User cancelled options, use current settings anyway
        }
    }
    
    // Apply configuration
    dynarec_enable = psp_config.dynarec_enable;
    sprite_limit = psp_config.sprite_limit;
    selected_boot_mode = psp_config.boot_mode ? boot_bios : boot_game;
    
    init_gpsp();
    
    // Main loop
    while(1) {
        // Reinit debug screen for file browser (in case we returned from game)
        pspDebugScreenInit();
        pspDebugScreenClear();
        
        // Show file browser
        char selected_rom[512];
        if(psp_file_browser(selected_rom, sizeof(selected_rom)) != 0) {
            // User cancelled or error
            break;
        }
        
        // Load and run ROM
        if(load_rom(selected_rom) == 0) {
            run_emulation();
            
            // Game exited, will return to file browser
            // Reinit debug screen on next loop iteration
        }
    }
    
    deinit_gpsp();
    sceKernelExitGame();
    
    return 0;
}

