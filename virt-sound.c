#include <kprintf.h>
#include <platform.h>
#include <platform-virt.h>
#include <string.h>
#include <virt-sound.h>


static bool queue_track(const int16_t *buffer, size_t samples,
                        void (*completed)(void));
static void handle_audio(void);


#ifndef SERIAL_IS_SOUND

void init_virt_sound(void)
{
    puts("[virt-sound] (Disabled at compile time)");

    platform_funcs.queue_audio_track = queue_track;
    platform_funcs.handle_audio = handle_audio;
}

static bool queue_track(const int16_t *buffer, size_t samples,
                        void (*completed)(void))
{
    (void)buffer;
    (void)samples;
    (void)completed;
    return false;
}

static void handle_audio(void)
{
}

#else // SERIAL_IS_SOUND

// mono, 16 bit signed
#define SAMPLE_RATE 8000

#define BUFFER_MS 10

#define MAX_TRACK_COUNT 16

static struct {
    const int16_t *buffer;
    size_t samples;
    size_t index;

    void (*completed)(void);
} tracks[MAX_TRACK_COUNT];

static int track_count;

void init_virt_sound(void)
{
    kprintf("[virt-sound] Providing raw PCM over serial (%i Hz s16le mono)\n",
            SERIAL_IS_SOUND);

    platform_funcs.queue_audio_track = queue_track;
    platform_funcs.handle_audio = handle_audio;
}

static bool queue_track(const int16_t *buffer, size_t samples,
                        void (*completed)(void))
{
    if (track_count >= MAX_TRACK_COUNT) {
        return false;
    }

    tracks[track_count].buffer = buffer;
    tracks[track_count].samples = samples;
    tracks[track_count].index = 0;
    tracks[track_count].completed = completed;

    track_count++;

    return true;
}

static void handle_audio(void)
{
    static size_t samples_buffered;
    static uint64_t last_gusi; // global micro-sample index

    uint64_t gusi = platform_funcs.elapsed_us() * SAMPLE_RATE;
    size_t samples_played = (gusi - last_gusi) / 1000000;
    last_gusi = gusi - (gusi - last_gusi) % 1000000;

    if (samples_played >= samples_buffered) {
        samples_buffered = 0;
    } else {
        samples_buffered -= samples_played;
    }

    if (!track_count) {
        return;
    }

    while (samples_buffered < SAMPLE_RATE / (1000 / BUFFER_MS)) {
        int16_t sample = 0;
        for (int i = 0; i < track_count; i++) {
            sample += tracks[i].buffer[tracks[i].index++];
            if (tracks[i].index >= tracks[i].samples) {
                if (tracks[i].completed) {
                    tracks[i].completed();
                }

                memmove(&tracks[i], &tracks[i + 1],
                        (track_count - i - 1) * sizeof(tracks[0]));
                tracks[--track_count].buffer = NULL;
                i--;
            }
        }

        *(volatile uint8_t *)VPBA_UART_BASE = (uint16_t)sample & 0xff;
        *(volatile uint8_t *)VPBA_UART_BASE = (uint16_t)sample >> 8;
        samples_buffered++;
    }
}

#endif
