/* gpSP PSP Standalone
 * PSP audio implementation
 */

#include <pspkernel.h>
#include <pspaudio.h>
#include <string.h>
#include <stdlib.h>

#include "psp_audio.h"
#include "../common.h"
#include "../sound.h"

#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES  800  // TempGBA uses 800 samples
#define AUDIO_SAMPLE_RATE 48000  // TempGBA uses 48000 Hz, not 44100!

static int audio_channel = -1;
static s16 audio_buffer[AUDIO_SAMPLES * AUDIO_CHANNELS];
static int audio_thread_running = 0;
static SceUID audio_thread_id = -1;

static int audio_thread(SceSize args, void *argp)
{
    audio_thread_running = 1;
    
    while(audio_thread_running) {
        // This will be called from the main loop
        sceKernelDelayThread(1000);
    }
    
    return 0;
}

void psp_audio_init(void)
{
    // GBA now generates at 48000 Hz (via -DGBA_SOUND_FREQUENCY=48000)
    // This matches TempGBA's approach - generate at supported SRC rate
    // SRC with same input/output does minimal processing
    audio_channel = sceAudioSRCChReserve(AUDIO_SAMPLES, AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);
    
    if(audio_channel < 0) {
        audio_channel = -1;
        return;
    }
    
    memset(audio_buffer, 0, sizeof(audio_buffer));
}

void psp_audio_deinit(void)
{
    audio_thread_running = 0;
    
    if(audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL);
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
    }
    
    if(audio_channel >= 0) {
        sceAudioSRCChRelease();
        audio_channel = -1;
    }
}

void psp_audio_update(void)
{
    if(audio_channel < 0)
        return;
    
    // GBA now generates at 48000 Hz (matching PSP)
    // At 59.73 FPS: need ~803 samples/frame (we use 800)
    
    u32 samples_got = sound_read_samples(audio_buffer, AUDIO_SAMPLES);
    
    if(samples_got == 0) {
        return;  // Skip if no samples
    }
    
    // Pad with silence if needed
    if(samples_got < AUDIO_SAMPLES) {
        memset(&audio_buffer[samples_got * 2], 0, (AUDIO_SAMPLES - samples_got) * 2 * sizeof(s16));
    }
    
    // Output at 48000 Hz (same as generation rate)
    sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, audio_buffer);
}

