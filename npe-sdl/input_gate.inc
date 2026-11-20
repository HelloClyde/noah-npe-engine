/* Consume gameplay input while a synchronous battle animation owns the UI.
   Window/quit/audio events stay queued. Held controls require a release. */
static unsigned game_input_busy;
static int game_input_quit_pending;
static Uint8 game_input_keys[SDL_NUM_SCANCODES],game_input_key_blocked[SDL_NUM_SCANCODES];
static Uint8 game_input_buttons[SDL_CONTROLLER_BUTTON_MAX],game_input_button_blocked[SDL_CONTROLLER_BUTTON_MAX];
static Uint8 game_input_mobile[12],game_input_mobile_blocked[12];
static Sint16 game_input_axes[2];
static Uint8 game_input_axis_blocked[2];
static SDL_JoystickID game_input_pad_id=-1;
static int game_title_input_fence_pending;
static void game_input_pad_sync(void){
    if(game_input_pad_id==game_pad_id)return;
    game_input_pad_id=game_pad_id;
    memset(game_input_buttons,0,sizeof(game_input_buttons));memset(game_input_button_blocked,0,sizeof(game_input_button_blocked));
    memset(game_input_axes,0,sizeof(game_input_axes));memset(game_input_axis_blocked,0,sizeof(game_input_axis_blocked));
}
static int game_input_gate_event(SDL_Event *event){
    if(event->type==SDL_QUIT){game_input_quit_pending=1;return 1;}
    if(event->type==SDL_KEYDOWN || event->type==SDL_KEYUP){
        SDL_Scancode scan=event->key.keysym.scancode;
        if(scan==SDL_SCANCODE_UNKNOWN)scan=SDL_GetScancodeFromKey(event->key.keysym.sym);
        if(scan>SDL_SCANCODE_UNKNOWN && scan<SDL_NUM_SCANCODES){
            int blocked=game_input_key_blocked[scan];
            game_input_keys[scan]=event->type==SDL_KEYDOWN;
            if(event->type==SDL_KEYUP)game_input_key_blocked[scan]=0;
            else if(game_input_busy)game_input_key_blocked[scan]=1;
            if(blocked || game_input_busy)return 0;
        }
        return !game_input_busy;
    }
    if(event->type==SDL_CONTROLLERBUTTONDOWN || event->type==SDL_CONTROLLERBUTTONUP){
        game_input_pad_sync();unsigned button=event->cbutton.button;
        if(event->cbutton.which==game_pad_id && button<SDL_CONTROLLER_BUTTON_MAX){
            int blocked=game_input_button_blocked[button];
            game_input_buttons[button]=event->type==SDL_CONTROLLERBUTTONDOWN;
            if(event->type==SDL_CONTROLLERBUTTONUP)game_input_button_blocked[button]=0;
            else if(game_input_busy)game_input_button_blocked[button]=1;
            if(blocked || game_input_busy)return 0;
        }
        return !game_input_busy;
    }
    if(event->type==SDL_CONTROLLERAXISMOTION){
        game_input_pad_sync();
        int axis=event->caxis.axis==SDL_CONTROLLER_AXIS_LEFTX?0:event->caxis.axis==SDL_CONTROLLER_AXIS_LEFTY?1:-1;
        if(event->caxis.which==game_pad_id && axis>=0){
            game_input_axes[axis]=event->caxis.value;
            if(game_input_busy && abs(event->caxis.value)>=16000)game_input_axis_blocked[axis]=1;
            if(game_input_axis_blocked[axis]){
                if(abs(event->caxis.value)<16000)game_input_axis_blocked[axis]=0;
                return 0;
            }
        }
        return !game_input_busy;
    }
    if(event->type==SDL_USEREVENT && event->user.code==0x4e50454b){
        unsigned action=(unsigned)(uintptr_t)event->user.data1;
        if(action<sizeof(game_input_mobile)){
            int down=event->user.data2!=NULL,blocked=game_input_mobile_blocked[action];
            game_input_mobile[action]=(Uint8)down;
            if(!down)game_input_mobile_blocked[action]=0;
            else if(game_input_busy)game_input_mobile_blocked[action]=1;
            if(blocked || game_input_busy)return 0;
        }
        return !game_input_busy;
    }
    if(game_input_busy && (event->type==SDL_JOYAXISMOTION || event->type==SDL_JOYHATMOTION ||
        event->type==SDL_JOYBUTTONDOWN || event->type==SDL_JOYBUTTONUP ||
        event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP ||
        event->type==SDL_FINGERDOWN || event->type==SDL_FINGERUP || event->type==SDL_FINGERMOTION))return 0;
    return 1;
}
static int SDLCALL game_input_queue_filter(void *unused,SDL_Event *event){
    (void)unused;return game_input_gate_event(event);
}
static void game_input_drain_busy(void){
    if(!game_input_busy)return;
    SDL_PumpEvents();SDL_FilterEvents(game_input_queue_filter,NULL);
}
static void game_input_begin(void){
    if(!game_input_busy){
        game_input_pad_sync();int count=0;const Uint8 *keys=SDL_GetKeyboardState(&count);
        for(int i=1;i<SDL_NUM_SCANCODES;++i)
            if(game_input_keys[i] || (i<count && keys[i]))game_input_key_blocked[i]=1;
        for(unsigned i=0;i<SDL_CONTROLLER_BUTTON_MAX;++i)
            if(game_input_buttons[i] || (game_pad && SDL_GameControllerGetButton(game_pad,(SDL_GameControllerButton)i)))
                game_input_button_blocked[i]=1;
        for(unsigned i=0;i<2;++i){
            int value=game_pad?SDL_GameControllerGetAxis(game_pad,i?SDL_CONTROLLER_AXIS_LEFTY:SDL_CONTROLLER_AXIS_LEFTX):0;
            if(abs(value)>=16000 || abs(game_input_axes[i])>=16000)game_input_axis_blocked[i]=1;
        }
        for(unsigned i=0;i<sizeof(game_input_mobile);++i)if(game_input_mobile[i])game_input_mobile_blocked[i]=1;
        game_pad_reset();
    }
    ++game_input_busy;game_input_drain_busy();
}
static void game_input_end(void){
    if(!game_input_busy)return;
    game_input_drain_busy();if(!--game_input_busy)game_pad_reset();
}
static void game_input_fence(void){game_input_begin();game_input_end();}
static void game_input_reset(void){
    game_input_busy=0;game_input_quit_pending=0;game_title_input_fence_pending=0;game_input_pad_id=-2;
    memset(game_input_keys,0,sizeof(game_input_keys));memset(game_input_key_blocked,0,sizeof(game_input_key_blocked));
    memset(game_input_mobile,0,sizeof(game_input_mobile));memset(game_input_mobile_blocked,0,sizeof(game_input_mobile_blocked));
    game_input_pad_sync();
}
