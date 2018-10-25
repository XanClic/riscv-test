#include <kprintf.h>
#include <platform.h>
#include <platform-virt.h>
#include <string.h>
#include <virt-sound.h>


static bool queue_track(const int16_t *buffer, size_t samples,
                        int sample_rate, int channels,
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
                        int sample_rate, int channels,
                        void (*completed)(void))
{
    (void)buffer;
    (void)samples;
    (void)sample_rate;
    (void)channels;
    (void)completed;
    return false;
}

static void handle_audio(void)
{
}

#else // SERIAL_IS_SOUND

#define BUFFER_MS 10

#define MAX_TRACK_COUNT 16

static struct {
    const int16_t *buffer;
    size_t samples;
    size_t index;

    void (*completed)(void);
} tracks[MAX_TRACK_COUNT];

static int track_count;
static int output_sample_rate, output_channels;


void init_virt_sound(void)
{
    kprintf("[virt-sound] Providing RIFF WAVE over serial\n");

    platform_funcs.queue_audio_track = queue_track;
    platform_funcs.handle_audio = handle_audio;
}

static inline void play_byte(uint8_t b)
{
    *(volatile uint8_t *)VPBA_UART_BASE = b;
}

static void output_wave_header(int rate, int channels, int bytes_per_sample)
{
    play_byte('R');
    play_byte('I');
    play_byte('F');
    play_byte('F');

    // file length - 8
    play_byte(0xf4);
    play_byte(0xff);
    play_byte(0xff);
    play_byte(0xff);

    play_byte('W');
    play_byte('A');
    play_byte('V');
    play_byte('E');

    play_byte('f');
    play_byte('m');
    play_byte('t');
    play_byte(' ');

    // further header length (16 bytes)
    play_byte(0x10);
    play_byte(0x00);
    play_byte(0x00);
    play_byte(0x00);

    // sample format (PCM)
    play_byte(0x01);
    play_byte(0x00);

    // channels
    play_byte(channels);
    play_byte(0x00);

    // sample rate
    play_byte( rate        & 0xff);
    play_byte((rate >>  8) & 0xff);
    play_byte((rate >> 16) & 0xff);
    play_byte((rate >> 24) & 0xff);

    // bytes per second
    int bps = rate * channels * bytes_per_sample;
    play_byte( bps        & 0xff);
    play_byte((bps >>  8) & 0xff);
    play_byte((bps >> 16) & 0xff);
    play_byte((bps >> 24) & 0xff);

    // bytes per frame
    play_byte(channels * bytes_per_sample);
    play_byte(0x00);

    // bits per sample
    play_byte(bytes_per_sample * 8);
    play_byte(0x00);

    play_byte('d');
    play_byte('a');
    play_byte('t');
    play_byte('a');

    // file length - 0x44
    play_byte(0xb8);
    play_byte(0xff);
    play_byte(0xff);
    play_byte(0xff);
}

static bool queue_track(const int16_t *buffer, size_t samples,
                        int sample_rate, int channels,
                        void (*completed)(void))
{
    if (track_count >= MAX_TRACK_COUNT) {
        return false;
    }

    if (output_sample_rate) {
        if (sample_rate != output_sample_rate || channels != output_channels) {
            return false;
        }
    } else {
        output_wave_header(sample_rate, channels, sizeof(int16_t));
        output_sample_rate = sample_rate;
        output_channels = channels;
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
    static int samples_buffered;
    static uint64_t last_gusi; // global micro-sample index

    int real_sample_rate = output_sample_rate * output_channels;

    uint64_t gusi = platform_funcs.elapsed_us() * real_sample_rate;
    size_t samples_played = (gusi - last_gusi) / 1000000;
    last_gusi = gusi - (gusi - last_gusi) % 1000000;

    if (samples_played >= (size_t)samples_buffered) {
        samples_buffered = 0;
    } else {
        samples_buffered -= samples_played;
    }

    if (!track_count) {
        return;
    }

    while (samples_buffered < real_sample_rate / (1000 / BUFFER_MS)) {
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

        play_byte((uint16_t)sample & 0xff);
        play_byte((uint16_t)sample >> 8);
        samples_buffered++;
    }
}

#endif
