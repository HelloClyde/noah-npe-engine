/* SDL GameController input translated into the existing keyboard actions. */
#ifndef NPE_CONTROLLER_BUTTON_MAP
#define NPE_CONTROLLER_BUTTON_MAP(button) (button)
#endif
static SDL_GameController *game_pad;
static void (*game_pad_show_help)(void);
static SDL_JoystickID game_pad_id=-1;
static uint16_t *game_pad_mode;
static int game_pad_focus=1,game_pad_x,game_pad_y;
static unsigned game_pad_dpad;
static SDL_Keycode game_pad_direction;
static Uint32 game_pad_next;
static void game_pad_reset(void){
    game_pad_x=game_pad_y=0;game_pad_dpad=0;
    game_pad_direction=SDLK_UNKNOWN;game_pad_next=0;
}
static void game_pad_close(void){
    if(game_pad)SDL_GameControllerClose(game_pad);
    game_pad=NULL;game_pad_id=-1;game_pad_reset();
}
static void game_pad_open(void){
    if(game_pad)return;
    for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){
        game_pad=SDL_GameControllerOpen(i);
        if(game_pad){game_pad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(game_pad));break;}
    }
}
static int game_pad_key(SDL_Event *event,SDL_Keycode key,int repeat){
    SDL_zero(*event);event->type=SDL_KEYDOWN;event->key.state=SDL_PRESSED;
    event->key.repeat=(Uint8)repeat;event->key.keysym.sym=key;return 1;
}
static SDL_Keycode game_pad_get_direction(void){
    if(game_pad_dpad&1)return SDLK_UP;
    if(game_pad_dpad&2)return SDLK_DOWN;
    if(game_pad_dpad&4)return SDLK_LEFT;
    if(game_pad_dpad&8)return SDLK_RIGHT;
    if(abs(game_pad_x)<16000 && abs(game_pad_y)<16000)return SDLK_UNKNOWN;
    if(abs(game_pad_x)>abs(game_pad_y))return game_pad_x<0?SDLK_LEFT:SDLK_RIGHT;
    return game_pad_y<0?SDLK_UP:SDLK_DOWN;
}
/* Returns 1 for an event the game should consume; 0 for controller bookkeeping. */
static int game_pad_event(SDL_Event *event){
    if(event->type==SDL_CONTROLLERDEVICEADDED){game_pad_open();return 0;}
    if(event->type==SDL_CONTROLLERDEVICEREMOVED){
        if(event->cdevice.which==game_pad_id){game_pad_close();game_pad_open();}return 0;
    }
    if(event->type==SDL_WINDOWEVENT){
        if(event->window.event==SDL_WINDOWEVENT_FOCUS_LOST){game_pad_focus=0;game_pad_reset();}
        if(event->window.event==SDL_WINDOWEVENT_FOCUS_GAINED){game_pad_focus=1;game_pad_reset();}
        return 1;
    }
    int axis=event->type==SDL_CONTROLLERAXISMOTION;
    int down=event->type==SDL_CONTROLLERBUTTONDOWN;
    if(!axis&&!down&&event->type!=SDL_CONTROLLERBUTTONUP)return 1;
    if(!game_pad_focus || (axis?event->caxis.which:event->cbutton.which)!=game_pad_id)return 0;
    if(axis){
        if(event->caxis.axis==SDL_CONTROLLER_AXIS_LEFTX)game_pad_x=event->caxis.value;
        else if(event->caxis.axis==SDL_CONTROLLER_AXIS_LEFTY)game_pad_y=event->caxis.value;
        else return 0;
    }else{
        event->cbutton.button=NPE_CONTROLLER_BUTTON_MAP(event->cbutton.button);
        unsigned bit=0;
        switch(event->cbutton.button){
        case SDL_CONTROLLER_BUTTON_DPAD_UP:bit=1;break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:bit=2;break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:bit=4;break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:bit=8;break;
        default:
            if(!down)return 0;
            if(event->cbutton.button==SDL_CONTROLLER_BUTTON_START){
                game_pad_reset();
                if(game_pad_show_help)game_pad_show_help();
                return 0;
            }
            if(game_pad_mode && *game_pad_mode==8){
                if(event->cbutton.button==SDL_CONTROLLER_BUTTON_X)return game_pad_key(event,SDLK_F2,0);
                if(event->cbutton.button==SDL_CONTROLLER_BUTTON_Y)return game_pad_key(event,SDLK_BACKSPACE,0);
            }
            switch(event->cbutton.button){
            case SDL_CONTROLLER_BUTTON_A:return game_pad_key(event,
                game_pad_mode&&(*game_pad_mode==2||*game_pad_mode==5)?SDLK_SPACE:SDLK_RETURN,0);
            case SDL_CONTROLLER_BUTTON_B:return game_pad_key(event,SDLK_F4,0);
            case SDL_CONTROLLER_BUTTON_X:return game_pad_key(event,SDLK_SPACE,0);
            case SDL_CONTROLLER_BUTTON_Y:return game_pad_key(event,SDLK_u,0);
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:return game_pad_key(event,SDLK_j,0);
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return game_pad_key(event,SDLK_l,0);
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return game_pad_key(event,SDLK_p,0);
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return game_pad_key(event,SDLK_n,0);
            default:return 0;
            }
        }
        if(down)game_pad_dpad|=bit;else game_pad_dpad&=~bit;
    }
    SDL_Keycode key=game_pad_get_direction();
    if(key==game_pad_direction)return 0;
    game_pad_direction=key;game_pad_next=SDL_GetTicks()+300;
    return key!=SDLK_UNKNOWN?game_pad_key(event,key,0):0;
}
static int game_pad_repeat(SDL_Event *event){
    if(!game_pad_focus||game_pad_direction==SDLK_UNKNOWN||
       !SDL_TICKS_PASSED(SDL_GetTicks(),game_pad_next))return 0;
    game_pad_next=SDL_GetTicks()+100;
    return game_pad_key(event,game_pad_direction,1);
}
