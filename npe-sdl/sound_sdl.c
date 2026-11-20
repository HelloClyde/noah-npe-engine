#include "sound_sdl.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

enum { SOUND_CONTINUATION_NONE, SOUND_CONTINUATION_MAP=NPE_SOUND_CALLBACK_MAP,
       SOUND_CONTINUATION_BATTLE=NPE_SOUND_CALLBACK_BATTLE };
static int sound_play_range(NpeSoundPlayer *,uint32_t,int32_t,int,unsigned);
static void sound_wait_complete(NpeSoundPlayer *);
static int sound_prime_pcm(NpeSoundPlayer *);
static void sound_callback_return(NpeSoundPlayer *player) {
    npe_audio_eof_callback_end(&player->decoder.input.flags89);
    player->callback_return_pending=0;
}

/* Original signed DAC state transitions. Execute the numeric service now;
   its REP instruction budget is retained separately. Audible ramp scheduling
   still needs an established instruction/output time relation. */
static void sound_dac_ramp(NpeSoundPlayer *player) {
    uint32_t nops;player->dac.repeat_argument=95;player->dac_ramp_nops=0;
    while(npe_audio_dac_ramp_step(&player->dac,&nops)>0)player->dac_ramp_nops+=nops;
}
static void sound_dac_stop_begin(NpeSoundPlayer *player) {
    player->dac_saved[0]=player->dac.current[0];player->dac_saved[1]=player->dac.current[1];
    player->dac.target[0]=player->dac.target[1]=0;player->dac_restore=0;
    sound_dac_ramp(player);player->dac_gpio&=(uint16_t)~0x40u;
}
static void sound_dac_stop_finish(NpeSoundPlayer *player) {
    player->dac_control=(uint16_t)((player->dac_control|0x8000u)&~1u);
}
static unsigned sound_dac_start_begin(NpeSoundPlayer *player,int16_t first_word,int preserve_targets) {
    /*31AC initializes both targets from1784 unless88.4 is set;320E's
      resume path deliberately retains the previous saved targets. */
    if(!preserve_targets)player->dac_saved[0]=player->dac_saved[1]=first_word;
    player->dac.current[0]=player->dac.current[1]=0;
    player->dac.target[0]=player->dac_saved[0];player->dac.target[1]=player->dac_saved[1];
    player->dac_control=(uint16_t)((player->dac_control|1u)&0x7fffu);
    player->dac_ramp_nops=0;player->dac_restore=!(player->decoder.input.flags8a&0x80u);
    if(!player->dac_restore)return 0;
    unsigned ticks=200;
    if(player->dac_gpio&0x4000u){player->dac_gpio&=(uint16_t)~0x4000u;ticks+=1000;}
    return ticks;
}
static void sound_dac_start_finish(NpeSoundPlayer *player) {
    if(player->dac_restore){player->dac_gpio|=0x40u;sound_dac_ramp(player);}
}

/* Original08:475E waits50 Timer2 ticks;08:4702 waits200 after clearing
   the cold GPIO flag. Keep these distinct from DAC ramp instruction time.
   The standalone host has no full-system cold GPIO state. */
static void sound_wait(NpeSoundPlayer *player, NpeSoundPhase phase, unsigned ticks) {
    player->phase=phase;
    if(phase==NPE_SOUND_END_WAIT||phase==NPE_SOUND_CLEANUP_WAIT){
        player->transport_running=0;sound_dac_stop_begin(player);
    }
    player->wait_bytes=(size_t)ticks*NPE_AUDIO_OUTPUT_RATE*sizeof(int16_t)/1000;
}

static void sound_start_wait(NpeSoundPlayer *player) {
    /* The original reopen path calls B6DE again; prediction, excitation
       and synthesis history belong to this playback, not the resource. */
    uint16_t mute=(uint16_t)(player->decoder.input.flags8a&0x80u);
    uint16_t callback_active=(uint16_t)(player->decoder.input.flags89&8u);
    npe_audio_mode5_init(&player->decoder);
    /*25:DF62 registers2F:66F2 in slot0, leaves slot1 null, and
       registers2B:2894 in slot2. Input-request5 is distinct from a null
       slot1 registration and remains the PCM window-refill condition. */
    player->decoder.input.flags89=(uint16_t)(0x5050u|(player->loop?0x8000u:0)|callback_active);
    player->decoder.input.flags8a=(uint16_t)(3u|mute);
    player->output=(NpeAudioOutput){2,0,0};
    player->pcm_service=(NpeAudioPcmService){0};
    player->transport_controls[0]=player->transport_controls[1]=0x8000;
    player->cursor=0;player->sample_pending=0;player->repeat_byte=0;
    player->frame_valid=0;player->pcm_halves_pending=0;player->decode_failed=0;
    player->driver_configured=1;
    /*24:97AC publishes both demands;25:E24C calls2F:66F2 before31AC.
      Each half advances gain and the service clock before the start wait. */
    if(sound_prime_pcm(player)){
        player->decode_failed=1;player->finished=1;player->loop=0;
        player->pcm_halves_pending=0;
        sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);return;
    }
    /*25:E2FA prefills before08:31AC. Its08:31BE clears a provisional
      EOF raised by the initial cursor/length check;89.3 remains live. */
    player->decoder.input.flags89&=(uint16_t)~0x800u;
    mute=(uint16_t)(player->decoder.input.flags8a&0x80u);
    /* Original08:471C skips GPIO, Timer2 wait and DAC restoration when
       the current driver's mute bit is set, retaining it across cleanup. */
    unsigned ticks=sound_dac_start_begin(player,player->half[0][0],0);
    sound_wait(player,NPE_SOUND_START_WAIT,ticks);
    if(mute)sound_wait_complete(player);
}

static void sound_wait_complete(NpeSoundPlayer *player) {
    switch(player->phase){
    case NPE_SOUND_END_WAIT: {
        sound_dac_stop_finish(player);
        /*08:39B6 stops output, but does not call08:3292. A loop callback
          reopens through25:0000, whose08:3262 cleanup stops it again. */
        int callback=npe_audio_eof_callback_begin(&player->decoder.input.flags89);
        if(callback==1 && player->continuation){
            /* Original31:A3F4 reads mutable guest state. Notify the game
               thread; never read World/runtime RAM on the audio thread. */
            player->finished=1;player->phase=NPE_SOUND_STOPPED;player->end_pending=player->continuation;
            if(player->end_event!=(Uint32)-1){
                SDL_Event event;SDL_zero(event);event.type=player->end_event;
                event.user.data1=player;(void)SDL_PushEvent(&event);
            }
        }else if(callback==1&&player->loop){
            /*08:3292 retains89.3;08:3B3E clears it only after the callback
              returns. Cached asynchronous reopen has the same boundary. */
            player->callback_return_pending=1;
            player->finished=0;sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);
        }else{
            sound_callback_return(player);
            player->finished=1;player->phase=NPE_SOUND_STOPPED;
        }
        break;
    }
    case NPE_SOUND_CLEANUP_WAIT:
        sound_dac_stop_finish(player);
        player->dac.current[0]=player->dac.current[1]=0; /*3292 clears both DAC words. */
        player->driver_configured=0;
        /*08:3292 clears all five clock words after the cleanup wait. */
        player->clock=(NpeAudioClock){0};
        npe_audio_cleanup_flags(&player->decoder.input.flags89,&player->decoder.input.flags8a);
        if(player->active>=0&&!player->finished)sound_start_wait(player);
        else{player->phase=NPE_SOUND_STOPPED;player->request_complete=player->request_generation;}
        break;
    case NPE_SOUND_START_WAIT:
        sound_dac_start_finish(player);
        player->cursor=0;player->sample_pending=0;player->repeat_byte=0;
        player->phase=NPE_SOUND_PLAYING;player->transport_running=1;
        player->request_complete=player->request_generation;
        break;
    default: break;
    }
    if(player->callback_return_pending &&
       (player->phase==NPE_SOUND_PLAYING||player->phase==NPE_SOUND_STOPPED))
        sound_callback_return(player);
    if(player->request_waiting && player->request_ready)
        (void)SDL_SemPost(player->request_ready);
}

static void sound_end(NpeSoundPlayer *player) {
    /* The candidate frame count supplies EOF; it is not original codec
       exhaustion proof. Original guards now consume the live driver word. */
    player->decoder.input.flags89|=0x800u;
    if(npe_audio_eof_gate(&player->decoder.input.flags89,player->irq_flags8d,
        player->irq_stack_word)==NPE_AUDIO_EOF_READY){
        player->finished=!(player->decoder.input.flags89&0x8000u);
        sound_wait(player,NPE_SOUND_END_WAIT,50);
    }else{
        player->finished=0;player->phase=NPE_SOUND_EOF_BLOCKED;
    }
}

static int sound_decode_frame(NpeSoundPlayer *player) {
    unsigned index=(unsigned)(player->cursor/(NPE_AUDIO_MODE5_PCM_SAMPLES*sizeof(int16_t)));
    if(player->frame_valid&&player->frame_index==index)return 0;
    /* Decode only the next compressed frame, never rebuild a track in the
       real-time callback or reuse another playback's predictor history. */
    uint16_t control=(uint16_t)(player->decoder.input.flags89&0x907fu);
    uint16_t mute=(uint16_t)(player->decoder.input.flags8a&0x80u);
    if(index!=player->decoder.next_frame||npe_audio_mode5_decode_deferred(&player->decoder,
            &player->resources[player->active],index,player->frame))return -1;
    /* Candidate open owns its compression state, not the driver's
       registration/busy/callback flags retained around the codec boundary. */
    player->decoder.input.flags89|=control;
    player->decoder.input.flags8a|=mute;
    player->frame_index=index;player->frame_valid=1;
    return 0;
}

typedef struct {NpeSoundPlayer *player;int restored;} SoundPcmContext;
static int sound_prepare_half(void *context,unsigned half,uint32_t destination) {
    NpeSoundPlayer *player=((SoundPcmContext *)context)->player;
    (void)destination; /* SDL retains both host halves, no physical DTU. */
    if(half>1)return -1;
    if(sound_decode_frame(player))return -1;
    uint16_t previous_words=player->output.output_words;
    if(npe_audio_output_half(&player->output,&player->gain,
        &player->decoder.input.flags8a,player->frame,NPE_AUDIO_MODE5_PCM_SAMPLES,
        NPE_AUDIO_MODE5_PCM_SAMPLES,player->half[half],NPE_AUDIO_GAIN_BLOCK_SAMPLES)!=1)return -1;
    if(player->output.output_words!=previous_words){
        if(npe_audio_clock_configure(&player->clock,NPE_AUDIO_OUTPUT_RATE,player->output.output_words))return -1;
        player->pcm_service.transfer_words=player->output.output_words;
    }
    return 0;
}
static int sound_codec_cursor(void *context,NpeAudioCursor *cursor) {
    NpeSoundPlayer *player=((SoundPcmContext *)context)->player;
    return npe_audio_input_candidate_cursor(&player->decoder.input,cursor);
}
static int sound_codec_restore(void *context,NpeAudioCursor cursor) {
    SoundPcmContext *pcm=context;
    int result=npe_audio_input_candidate_restore(&pcm->player->decoder.input,cursor);
    if(!result)pcm->restored=1;
    return result;
}
static const NpeAudioPcmBackend sound_pcm_backend={
    sound_prepare_half,sound_codec_cursor,sound_codec_restore
};
static int sound_prime_pcm(NpeSoundPlayer *player) {
    SoundPcmContext pcm={player,0};
    player->pcm_service.flags8e|=0xc000u;
    if(npe_audio_pcm_service(&player->pcm_service,&player->decoder.input,
        &player->output,&player->clock,&sound_pcm_backend,&pcm)!=2)return -1;
    player->pcm_halves_pending=3u;
    player->transport_controls[0]=player->transport_controls[1]=
        (uint16_t)(0x8000u|player->output.output_words);
    return 0;
}

/* SDL consumes one logical word on each pair of delivered bytes. The
   counters expose4070's software decision; they do not emulate physical
   dual-channel DTU progress, timers or a silicon transfer request source. */
static int sound_transport_pause(void *context) {
    ((NpeSoundPlayer *)context)->transport_running=0;return 0;
}
static int sound_transport_refill(void *context) {
    NpeSoundPlayer *player=context;
    unsigned half=(unsigned)((player->cursor/(NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))&1u);
    if(!(player->pcm_halves_pending&(1u<<half))){
        player->pcm_service.flags8e|=(uint16_t)(2u|(0x4000u<<half));
    }
    return 0;
}
static int sound_transport_start(void *context) {
    ((NpeSoundPlayer *)context)->transport_running=1;return 0;
}
static const NpeAudioTransportBackend sound_transport_backend={
    NULL,sound_transport_pause,sound_transport_refill,sound_transport_start
};
static int sound_transport_restore(NpeSoundPlayer *player) {
    return npe_audio_transport_resume(&player->decoder.input.flags89,&player->decoder.input.flags8a,
        &player->pcm_service.flags8e,player->transport_controls,&sound_transport_backend,player);
}

/* Original3EC2 clears producer demand before2B:2894, preserving later
   demands raised during the call. The NPE file adapter is synchronous; this
   uses the software405A/4070 controller, with explicit host transport
   hooks; shared filesystem and physical SPI remain separate. */
static int sound_service_producer(NpeSoundPlayer *player) {
    NpeAudioInput *input=&player->decoder.input;
    /* The first candidate open performs the synchronous prime boundary. */
    if(!input->initialized)return NPE_AUDIO_SERVICE_NEXT;
    int gate=npe_audio_service_begin(&input->flags89,input->flags8a,
        player->irq_flags8d,NULL,NPE_AUDIO_SERVICE_PRODUCER);
    if(gate==NPE_AUDIO_SERVICE_YIELD){
        /*25:DF62 selects NPE mode8040, whose optional rate-adjust bit4 is
           clear. Other mode-specific16:EDFA behavior has no host adapter. */
        if(npe_audio_transport_suspend(&input->flags89,0x8040,&sound_transport_backend,player))return -1;
    }
    if(gate!=NPE_AUDIO_SERVICE_READY)return gate;
    int result=npe_audio_input_candidate_produce(input);
    npe_audio_service_end(&input->flags89,NPE_AUDIO_SERVICE_PRODUCER);
    return result?-1:NPE_AUDIO_SERVICE_READY;
}

static void sound_callback(void *userdata, Uint8 *stream, int length) {
    NpeSoundPlayer *player = userdata;
    size_t remaining = length > 0 ? (size_t)length : 0;
    Uint8 *output = stream;
    SDL_memset(stream, 0, remaining);
    while (remaining) {
        if(player->wait_bytes){
            size_t count=remaining<player->wait_bytes?remaining:player->wait_bytes;
            output+=count;remaining-=count;player->wait_bytes-=count;
            if(!player->wait_bytes)sound_wait_complete(player);
            continue;
        }
        if(player->phase==NPE_SOUND_EOF_BLOCKED){
            /* A new host audio service opportunity retries a blocked EOF.
               No wait, PCM or callback advances while its guard remains. */
            sound_end(player);
            if(player->phase==NPE_SOUND_EOF_BLOCKED)return;
            continue;
        }
        if(player->phase!=NPE_SOUND_PLAYING||player->finished||player->active<0||
           player->active>NPE_SOUND_TRACKS||!player->resources[player->active].bytes||
           !player->byte_count[player->active])return;
        if(!player->sample_pending){
            if(player->cursor==player->byte_count[player->active]){
                sound_end(player);continue;
            }
            /* Host delivery requests one half at a time; the original
               two-bit service handles its state changes and call order.
               Candidate file production remains a synchronous host adapter
               after restore, not an emulated resident producer/IRQ. */
            if(!(player->cursor%(NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))){
                unsigned half=(unsigned)((player->cursor/
                    (NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))&1u);
                if(!(player->pcm_halves_pending&(1u<<half)))
                    player->pcm_service.flags8e|=(uint16_t)(0x4000u<<half);
                int result=0,route=-1;
                if(player->decoder.input.flags89&0x400u){
                    route=sound_transport_restore(player);
                    if(route<0){player->decode_failed=1;player->finished=1;
                        sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);continue;}
                    if(route==NPE_AUDIO_TRANSPORT_BLOCKED)return;
                }
                /* The original chain runs PCM before the producer. When
                   output is unavailable it still reaches the producer gate.
                   A prepared half survives a later yield; retrying that
                   producer must not decode/advance the same half twice. */
                if((route<0||route==NPE_AUDIO_TRANSPORT_PCM)&&!(player->pcm_halves_pending&(1u<<half))&&
                   (player->decoder.input.flags89&0x1010u)==0x1010u){
                    int gate=npe_audio_service_begin(&player->decoder.input.flags89,
                        player->decoder.input.flags8a,player->irq_flags8d,
                        &player->pcm_service.flags8e,NPE_AUDIO_SERVICE_PCM);
                    if(gate==NPE_AUDIO_SERVICE_READY){
                        SoundPcmContext pcm={player,0};
                        result=npe_audio_pcm_service(&player->pcm_service,&player->decoder.input,
                            &player->output,&player->clock,&sound_pcm_backend,&pcm);
                        npe_audio_service_end(&player->decoder.input.flags89,NPE_AUDIO_SERVICE_PCM);
                        if(result==1){
                            player->pcm_halves_pending|=1u<<half;
                            player->transport_controls[0]=player->transport_controls[1]=
                                (uint16_t)(0x8000u|player->output.output_words);
                        }
                        else result=-1;
                    }
                }
                int producer=route==NPE_AUDIO_TRANSPORT_RETURN?NPE_AUDIO_SERVICE_NEXT:
                    sound_service_producer(player);
                if(result<0||producer<0){
                    player->decode_failed=1;player->finished=1;player->loop=0;
                    sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);continue;
                }
                if(producer==NPE_AUDIO_SERVICE_YIELD||producer==NPE_AUDIO_SERVICE_RESUME||
                   !(player->pcm_halves_pending&(1u<<half))||!player->transport_running)return;
                /* The second initially primed half has not run another
                   PCM service; arm its host counters when delivery begins. */
                player->transport_controls[0]=player->transport_controls[1]=
                    (uint16_t)(0x8000u|player->output.output_words);
            }
            unsigned half=(unsigned)((player->cursor/(NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))&1u);
            player->repeated_sample=player->half[half][(player->cursor/sizeof(int16_t))%NPE_AUDIO_GAIN_BLOCK_SAMPLES];
            player->cursor+=sizeof(int16_t);player->repeat_byte=0;player->sample_pending=1;
        }
        unsigned byte=player->repeat_byte%sizeof(int16_t);
        if(!byte)player->host_sample=(int16_t)((int32_t)player->repeated_sample*
            (int)(100-player->host_attenuation)/100);
        *output++=((const uint8_t *)&player->host_sample)[byte];
        --remaining;
        if((player->repeat_byte&1u)&&player->transport_running){
            /* The mono delivery adapter latches a complete signed word;
               an odd first byte cannot replace the retained DAC value. */
            player->dac.current[0]=player->dac.current[1]=player->repeated_sample;
            for(unsigned channel=0;channel<2;++channel)
                if(player->transport_controls[channel]&0x7fffu)--player->transport_controls[channel];
        }
        if(++player->repeat_byte==NPE_AUDIO_OUTPUT_REPEAT*sizeof(int16_t)){
            player->sample_pending=0;
            if(!(player->cursor%(NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))){
                unsigned consumed=(unsigned)(((player->cursor/(NPE_AUDIO_GAIN_BLOCK_SAMPLES*sizeof(int16_t)))-1u)&1u);
                player->pcm_halves_pending&=~(1u<<consumed);
            }
            if(player->cursor==player->byte_count[player->active]){
                sound_end(player);
            }
        }
    }
}

void npe_sound_set_volume(NpeSoundPlayer *player,unsigned percent) {
    if(!player)return;
    if(player->device)SDL_LockAudioDevice(player->device);
    player->host_attenuation=percent>100?0:100-percent;
    if(player->device)SDL_UnlockAudioDevice(player->device);
}

void npe_sound_open(NpeSoundPlayer *player, const NpeFile *file) {
    SDL_AudioSpec wanted, obtained;
    unsigned track;
    memset(player, 0, sizeof(*player));
    player->file = file;
    player->active = -1;
    player->selected_section = -1;
    /* Standalone C has no DMCU IRQ frame. This is a safe virtual service
       value, not a claim about the physical host stack or original SP. */
    player->irq_stack_word=0x6000;
    player->end_event=SDL_RegisterEvents(1);
    /* Standalone host output starts at the observed audible level7/7.
       This is not a claim about the missing full system's cold volume.
       Requests retain the levels; tail-fade scheduling is a separate
       service contract, not inferred from SDL buffer sizes. */
    player->gain.current=player->gain.target=7;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL audio unavailable: %s\n", SDL_GetError());
        return;
    }
    SDL_zero(wanted);
    wanted.freq = NPE_AUDIO_OUTPUT_RATE;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = 1024;
    wanted.callback = sound_callback;
    wanted.userdata = player;
    player->device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (!player->device) {
        fprintf(stderr, "SDL audio unavailable: %s\n", SDL_GetError());
        return;
    }
    player->request_ready=SDL_CreateSemaphore(0);
    if(!player->request_ready){
        fprintf(stderr,"SDL audio unavailable: %s\n",SDL_GetError());
        SDL_CloseAudioDevice(player->device);player->device=0;
        return;
    }
    for (track = 0; track < NPE_SOUND_TRACKS; ++track) {
        NpeAudioResource resource;
        unsigned section = NPE_SOUND_FIRST_SECTION + track;
        if (npe_audio_resource(file, section, &resource)) continue;
        player->byte_count[track] =
            (size_t)resource.header_count * NPE_AUDIO_MODE5_PCM_SAMPLES *
            sizeof(int16_t);
        player->resources[track]=resource;
    }
    SDL_PauseAudioDevice(player->device, 0);
}

static void sound_request(void *context, unsigned section, int loop) {
    npe_sound_play_section(context, section, loop);
    (void)npe_sound_wait_request(context);
}

static void sound_range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    (void)sound_play_range(context,offset,length,callback!=0,callback);
    (void)npe_sound_wait_request(context);
}

void npe_sound_select(NpeSoundPlayer *player, NpeWorld *world) {
    if (!player || !world) return;
    if(player->world!=world){
        if(player->device)SDL_LockAudioDevice(player->device);
        player->end_pending=0;
        sound_callback_return(player);
        if(player->continuation)player->loop=0;
        player->continuation=SOUND_CONTINUATION_NONE;
        if(player->device)SDL_UnlockAudioDevice(player->device);
        player->world=world;
    }
    /* Requests originate in the C engine, including while SDL waits in
       nested menus. No game-mode track guess and no hardware-dependent RNG. */
    if (world->sound_observer != sound_request || world->sound_context != player)
        npe_world_sound_bind(world, sound_request, player);
    world->sound_range_observer=sound_range_request;
    (void)npe_sound_pump(player);
}

static void sound_producer_poll(NpeSoundPlayer *player) {
    if(!player->decoder.input.initialized||
       (player->phase!=NPE_SOUND_PLAYING&&player->phase!=NPE_SOUND_EOF_BLOCKED))return;
    int ready=npe_audio_producer_poll_begin(&player->decoder.input.flags89,player->irq_flags8d);
    if(ready!=1)return;
    int result=npe_audio_input_candidate_produce(&player->decoder.input);
    npe_audio_service_end(&player->decoder.input.flags89,NPE_AUDIO_SERVICE_PRODUCER);
    if(result){player->decode_failed=1;player->finished=1;player->loop=0;
        sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);return;}
    /* Original3FB0/404E returns through4070 if suspension is pending.
       Host refill queues PCM for the audio thread rather than calling the
       candidate decoder while a game-thread poll holds the audio lock. */
    if(player->decoder.input.flags89&0x400u){
        int route=sound_transport_restore(player);
        if(route<0){player->decode_failed=1;player->finished=1;player->loop=0;
            sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);}
    }
}

int npe_sound_pump(NpeSoundPlayer *player){
    if(!player || !player->device)return 0;
    SDL_LockAudioDevice(player->device);
    sound_producer_poll(player);
    unsigned pending=player->end_pending;
    uint32_t generation=player->request_generation;
    player->end_pending=0;
    SDL_UnlockAudioDevice(player->device);
    if(!pending)return 0;
    NpeWorld *world=player->world;
    int result=0;
    if(world && world->sound_context==player &&
       world->sound_range_observer==sound_range_request && world->battle_file==player->file){
        if(pending==SOUND_CONTINUATION_MAP)result=npe_world_sound_map_replay(world);
        else if(pending==SOUND_CONTINUATION_BATTLE)result=npe_world_sound_battle_replay(world);
    }
    SDL_LockAudioDevice(player->device);
    /* A suspended SDL backend cannot satisfy the observer's foreground
       wait. Retain original callback activity until delivery completes
       the new request, rather than treating its asynchronous queue as a
       completed original call. Normal foreground calls already finished. */
    if(player->request_generation!=generation &&
       player->request_complete!=player->request_generation)
        player->callback_return_pending=1;
    else sound_callback_return(player);
    SDL_UnlockAudioDevice(player->device);
    return result;
}

int npe_sound_handle_event(NpeSoundPlayer *player,const SDL_Event *event){
    if(!player || !event || event->type!=player->end_event || event->user.data1!=player)return 0;
    (void)npe_sound_pump(player);return 1;
}

/* Caller holds the audio lock while changing both the range and playback. */
static int sound_play_locked(NpeSoundPlayer *player,int track,int section,int loop){
    /* Repeated stop while its cleanup is pending does not postpone it. */
    int stopping=track<0 || (track>=0&&(!player->resources[track].bytes||!player->byte_count[track]));
    int retain_stop=stopping&&player->active<0&&player->phase==NPE_SOUND_CLEANUP_WAIT;
    ++player->request_generation;
    player->selected_section = section;
    if (track >= 0 && (!player->resources[track].bytes || !player->byte_count[track]))
        track = -1;
    player->active = track;
    player->loop = track>=0 && loop != 0;
    if(player->end_pending||player->callback_return_pending)sound_callback_return(player);
    player->continuation=SOUND_CONTINUATION_NONE;player->end_pending=0;
    player->finished = track < 0;
    player->cursor = 0;
    player->sample_pending=0;player->repeat_byte=0;player->pcm_halves_pending=0;
    if(!retain_stop){
        if(player->driver_configured)sound_wait(player,NPE_SOUND_CLEANUP_WAIT,50);
        else if(track>=0)sound_start_wait(player);
        else{player->phase=NPE_SOUND_STOPPED;player->wait_bytes=0;}
    }
    if(player->phase==NPE_SOUND_STOPPED)
        player->request_complete=player->request_generation;
    return track < 0 ? -1 : 0;
}

int npe_sound_play_section(NpeSoundPlayer *player, unsigned section, int loop) {
    int track = -1;
    if (!player || !player->device) return -1;
    if (section >= NPE_SOUND_FIRST_SECTION &&
        section < NPE_SOUND_FIRST_SECTION + NPE_SOUND_TRACKS)
        track = (int)(section - NPE_SOUND_FIRST_SECTION);
    SDL_LockAudioDevice(player->device);
    int result=sound_play_locked(player,track,section<NPE_MAX_SECTIONS?(int)section:-1,loop);
    SDL_UnlockAudioDevice(player->device);
    return result;
}

int npe_sound_play_range(NpeSoundPlayer *player,uint32_t offset,int32_t length){
    return npe_sound_play_range_loop(player,offset,length,0);
}

int npe_sound_play_range_loop(NpeSoundPlayer *player,uint32_t offset,int32_t length,int loop){
    return sound_play_range(player,offset,length,loop,SOUND_CONTINUATION_NONE);
}

static int sound_play_range(NpeSoundPlayer *player,uint32_t offset,int32_t length,int loop,unsigned continuation){
    if(!player||!player->device)return -1;
    NpeAudioResource resource;int section=-1;
    int valid=!npe_audio_resource_range(player->file,offset,length,&resource);
    if(valid)for(unsigned i=0;i<player->file->section_count;++i)
        if(player->file->sections[i].offset==offset &&
           player->file->sections[i].size==(uint32_t)length){section=(int)i;break;}
    SDL_LockAudioDevice(player->device);
    if(valid){
        player->resources[NPE_SOUND_TRACKS]=resource;
        player->byte_count[NPE_SOUND_TRACKS]=(size_t)resource.header_count*NPE_AUDIO_MODE5_PCM_SAMPLES*sizeof(int16_t);
    }
    /* Action requests have no loop callback; map requests retain one.
      Invalid ranges never choose another section or unsigned length. */
    int result=sound_play_locked(player,valid?NPE_SOUND_TRACKS:-1,section,loop);
    if(!result)player->continuation=continuation;
    SDL_UnlockAudioDevice(player->device);
    return result;
}

int npe_sound_wait_request(NpeSoundPlayer *player) {
    if(!player || !player->device || !player->request_ready ||
       SDL_GetAudioDeviceStatus(player->device)!=SDL_AUDIO_PLAYING)return -1;
    SDL_LockAudioDevice(player->device);
    while(SDL_SemTryWait(player->request_ready)==0){}
    uint32_t generation=player->request_generation;
    int pending=player->request_complete!=generation;
    player->request_waiting=pending;
    SDL_UnlockAudioDevice(player->device);
    while(pending){
        /* Timer2 counts are still advanced by audio delivery. Timeout only
           observes device suspension/errors; it never consumes wait bytes. */
        int result=SDL_SemWaitTimeout(player->request_ready,50);
        int unavailable=result<0||SDL_GetAudioDeviceStatus(player->device)!=SDL_AUDIO_PLAYING;
        SDL_LockAudioDevice(player->device);
        pending=!unavailable&&player->request_complete!=generation;
        if(!pending)player->request_waiting=0;
        SDL_UnlockAudioDevice(player->device);
        if(unavailable)return -1;
    }
    return 0;
}

void npe_sound_close(NpeSoundPlayer *player) {
    if (player->device) SDL_CloseAudioDevice(player->device);
    if (player->request_ready) SDL_DestroySemaphore(player->request_ready);
    memset(player, 0, sizeof(*player));
    player->active = player->selected_section = -1;
}

size_t npe_sound_cursor(NpeSoundPlayer *player) {
    size_t cursor;
    if (!player || !player->device) return 0;
    SDL_LockAudioDevice(player->device);
    cursor = player->cursor;
    SDL_UnlockAudioDevice(player->device);
    return cursor;
}
