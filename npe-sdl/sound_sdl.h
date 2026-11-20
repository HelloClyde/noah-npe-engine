#ifndef NPE_SOUND_SDL_H
#define NPE_SOUND_SDL_H

#include "npe.h"
#include "audio.h"
#include <SDL.h>

enum { NPE_SOUND_FIRST_SECTION = 16, NPE_SOUND_TRACKS = 22 };

typedef enum {
    NPE_SOUND_STOPPED,
    NPE_SOUND_START_WAIT,
    NPE_SOUND_PLAYING,
    NPE_SOUND_END_WAIT,
    NPE_SOUND_CLEANUP_WAIT,
    NPE_SOUND_EOF_BLOCKED
} NpeSoundPhase;

typedef struct {
    SDL_AudioDeviceID device;
    SDL_sem *request_ready;
    int request_waiting; /* Audio-lock protected foreground return boundary. */
    uint32_t request_generation, request_complete;
    const NpeFile *file; /* Borrow the live current NPE for range requests. */
    NpeAudioResource resources[NPE_SOUND_TRACKS+1]; /* Last slot is a live range. */
    size_t byte_count[NPE_SOUND_TRACKS+1];
    NpeAudioMode5Decoder decoder;
    int16_t frame[NPE_AUDIO_MODE5_PCM_SAMPLES];
    NpeAudioOutput output;
    NpeAudioPcmService pcm_service; /* Original half-service orchestration state. */
    NpeAudioClock clock; /* Driver service clock, not a claim about audible position. */
    int16_t half[2][NPE_AUDIO_GAIN_BLOCK_SAMPLES];
    unsigned frame_index;
    int frame_valid;
    uint16_t transport_controls[2]; /* Host delivery word counters, not physical DTU. */
    int transport_running;
    unsigned pcm_halves_pending; /* Bit0/1: prepared halves retained across service yields. */
    int decode_failed;
    size_t cursor;
    int active;
    int loop;
    int finished;
    int selected_section;
    NpeAudioGain gain; /* Resident4A64 high=current,low=target; output-stage state. */
    NpeAudioDacRamp dac; /* Logical mono DAC words; no physical transfer clock. */
    int16_t dac_saved[2]; /* Original4A68/4A6A; fresh start selects the first PCM word. */
    uint16_t dac_control,dac_gpio; /* Original REG72 and GPIO31 output bits. */
    uint64_t dac_ramp_nops; /* Last ramp REP budget, not SDL samples or milliseconds. */
    int dac_restore; /* Startup mute skips GPIO enable and restoration. */
    unsigned host_attenuation;
    int16_t host_sample;
    int16_t repeated_sample;
    unsigned repeat_byte; /* Byte0..7 in the four copies of a pending word. */
    int sample_pending;
    NpeSoundPhase phase;
    size_t wait_bytes; /* Nominal Timer2 waits advanced by SDL byte delivery, not clock.value. */
    int driver_configured; /* Original BIT[88].2 clear; EOF alone does not reset it. */
    uint16_t irq_flags8d,irq_stack_word; /* F01A and the EOF guard's virtual SP. */
    unsigned continuation,end_pending; /* Audio-lock protected; no guest RAM in callback. */
    int callback_return_pending; /* Keep BIT89.3 until an asynchronous reopen completes. */
    Uint32 end_event;
    NpeWorld *world; /* Bound and accessed only by the game thread. */
} NpeSoundPlayer;

/* Keep file bytes alive until npe_sound_close; no whole-track PCM cache. */
void npe_sound_open(NpeSoundPlayer *player, const NpeFile *file);
/* Bind the world request observer. Rendering the same world does not replay. */
void npe_sound_select(NpeSoundPlayer *player, NpeWorld *world);
/* Dispatch pending engine callbacks on the game thread, including menus. */
int npe_sound_pump(NpeSoundPlayer *player);
/* Consume only this player's SDL end notification; no keyboard event changes. */
int npe_sound_handle_event(NpeSoundPlayer *player,const SDL_Event *event);
/* An explicit firmware playback request restarts the current-NPE resource.
   Output follows the Timer2 cleanup/start waits; cursor metadata resets now.
   A missing/unsupported resource stops playback; it never substitutes a track. */
int npe_sound_play_section(NpeSoundPlayer *player, unsigned section, int loop);
int npe_sound_play_range(NpeSoundPlayer *player,uint32_t offset,int32_t length);
int npe_sound_play_range_loop(NpeSoundPlayer *player,uint32_t offset,int32_t length,int loop);
/* Game-thread observer calls wait for original cleanup/start completion.
   Low-level play requests remain asynchronous for backend/diagnostic use.
   No audio/paused output returns -1; it cannot advance Timer2 byte delivery.
   Only one foreground caller owns a player. No World access from callback. */
int npe_sound_wait_request(NpeSoundPlayer *player);
void npe_sound_set_volume(NpeSoundPlayer *player,unsigned percent);
void npe_sound_close(NpeSoundPlayer *player);
/* Diagnostic read; synchronized with the SDL audio callback. */
size_t npe_sound_cursor(NpeSoundPlayer *player);

#endif
