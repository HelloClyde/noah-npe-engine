#if (defined(_WIN32) || defined(__APPLE__)) && !defined(NPE_SETTINGS_SDL_TEST)
#include "settings_native_bridge.inc"
#ifdef _WIN32
#include "settings_windows.inc"
#endif
static void show_settings_page(unsigned page){
    settings_last_saved=0;settings_load();NpeSettings edited=settings;SDL_Window *parent=SDL_GetKeyboardFocus();
    game_pad_reset();int accepted;
#ifdef _WIN32
    accepted=settings_windows_dialog(&edited,page);
#else
    NpeSettingsHooks hooks={settings_defaults,settings_bind_key,settings_bind_button,settings_native_poll_button};
    accepted=npe_macos_settings_dialog(&edited,page,&hooks);
#endif
    if(accepted && !settings_native_commit(&edited,parent))settings_last_saved=1;
    npe_sound_set_volume(game_sound_player,settings.sound_muted?0:settings.volume);
    game_pad_reset();game_input_fence();
    if(parent){SDL_RaiseWindow(parent);SDL_Event event;SDL_zero(event);event.type=SDL_WINDOWEVENT;
        event.window.windowID=SDL_GetWindowID(parent);event.window.event=SDL_WINDOWEVENT_EXPOSED;SDL_PushEvent(&event);}
}
#else
/* A separate SDL window leaves the game's original LCD/frame caches intact. */
static void settings_ui_text(SDL_Renderer *renderer,SDL_Texture *texture,
    const NpeFont *font,const char *text,int x,int y){
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]={0};uint32_t rgba[NPE_WIDTH*16];
    (void)npe_draw_gbk_text(font,(const uint8_t *)text,strlen(text),0,0,pixels);
    for(unsigned i=0;i<NPE_WIDTH*16;++i)rgba[i]=pixels[i]?0xffeeeeee:0x0020242a;
    SDL_UpdateTexture(texture,NULL,rgba,NPE_WIDTH*4);
    SDL_Rect source={0,0,NPE_WIDTH,16},dest={x,y,NPE_WIDTH*2,32};
    SDL_RenderCopy(renderer,texture,&source,&dest);
}
static void show_settings_page(unsigned tab){
#ifdef __ANDROID__
    (void)tab;return;
#else
    NpeFont font={0};if(load_native_fonts(&font))return;
    SDL_Window *parent=SDL_GetKeyboardFocus();
    SDL_Rect usable;int initial_width=960,initial_height=840;
    int display=parent?SDL_GetWindowDisplayIndex(parent):0;if(display<0)display=0;
    if(!SDL_GetDisplayUsableBounds(display,&usable)){
        if(initial_width>usable.w-40)initial_width=usable.w-40;
        if(initial_height>usable.h-60)initial_height=usable.h-60;
    }
    SDL_Window *window=SDL_CreateWindow("Noah NPE - Settings",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,initial_width,initial_height,SDL_WINDOW_RESIZABLE);
    if(!window){npe_font_free(&font);return;}
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    SDL_Texture *texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,16):NULL;
    if(!texture){if(renderer)SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);return;}
    SDL_RenderSetLogicalSize(renderer,640,624);
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);settings_load();
    NpeSettings edited=settings;unsigned selected=0;int capture=0,running=1;char number[5]="";
    game_pad_reset();
    while(running){
        const char *labels[]={"\xc9\xcf","\xcf\xc2","\xd7\xf3","\xd3\xd2","\xc8\xb7\xc8\xcf","\xb7\xb5\xbb\xd8\x2f\xb2\xcb\xb5\xa5",
            "\xb6\xd4\xbb\xb0\x2f\xb7\xad\xd2\xb3","\xd0\xb6\xd7\xb0","\xcb\xb5\xc3\xf7\xc9\xcf\xd2\xbb\xd2\xb3","\xcb\xb5\xc3\xf7\xcf\xc2\xd2\xbb\xd2\xb3","\xb0\xb4\xbc\xfc\xcb\xb5\xc3\xf7","\xd1\xa1\xcf\xee\xd7\xf3\xa3\xa8\x4a\xa3\xa9","\xd1\xa1\xcf\xee\xd3\xd2\xa3\xa8\x4c\xa3\xa9"};
        SDL_SetRenderDrawColor(renderer,32,36,42,255);SDL_RenderClear(renderer);
        settings_ui_text(renderer,texture,&font,"\xc9\xe8\xd6\xc3\xa3\xba\x20\x5b\x31\x5d\xbc\xfc\xc5\xcc\x20\x20\x5b\x32\x5d\xca\xd6\xb1\xfa\x20\x20\x5b\x33\x5d\xb5\xd8\xcd\xbc",16,8);
        settings_ui_text(renderer,texture,&font,capture==2?"\xca\xe4\xc8\xeb\xca\xfd\xd7\xd6\xba\xf3\xb0\xb4\x45\x6e\x74\x65\x72\xc8\xb7\xc8\xcf\xa3\xac\x45\x73\x63\xc8\xa1\xcf\xfb":capture?"\xc7\xeb\xb0\xb4\xd0\xc2\xb0\xb4\xbc\xfc\xa3\xac\x45\x73\x63\xc8\xa1\xcf\xfb\xa3\xbb\xd6\xd8\xb8\xb4\xb0\xf3\xb6\xa8\xbb\xe1\xbd\xbb\xbb\xbb":
            "\xb5\xe3\xbb\xf7\xcf\xee\xc4\xbf\xbb\xf2\xc9\xcf\xcf\xc2\xd1\xa1\xd4\xf1\xa3\xac\x45\x6e\x74\x65\x72\xd0\xde\xb8\xc4\xa3\xac\x54\x61\x62\xbb\xbb\xd2\xb3",16,42);
        unsigned count=tab==0?SETTINGS_KEYS:tab==1?SETTINGS_BUTTONS:3;
        for(unsigned i=0;i<count;++i){
            SDL_Rect row={12,84+(int)i*32,610,30};
            SDL_SetRenderDrawColor(renderer,selected==i?56:38,selected==i?76:43,selected==i?100:50,255);
            SDL_RenderFillRect(renderer,&row);
            char line[240];
            if(tab<2){
                const char *name=tab==0?SDL_GetKeyName(edited.keys[i]):
                    SDL_GameControllerGetStringForButton((SDL_GameControllerButton)edited.buttons[i]);
                snprintf(line,sizeof(line),"%s %s: %s",selected==i?">":" ",labels[tab==0&&i>=10?i+1:i],name?name:"?");
            }else if(i==0)snprintf(line,sizeof(line),"%s %s: %s",selected==i?">":" ","\xd4\xad\xca\xbc\xca\xd3\xd2\xb0",edited.rows?"\xb9\xd8\xb1\xd5":"\xbf\xaa\xc6\xf4");
            else snprintf(line,sizeof(line),"%s %s: %u",selected==i?">":" ",
                i==1?"\xcf\xd4\xca\xbe\xd0\xd0\xca\xfd":"\xcf\xd4\xca\xbe\xc1\xd0\xca\xfd",i==1?edited.rows:edited.cols);
            settings_ui_text(renderer,texture,&font,line,16,86+(int)i*32);
            if(tab==2 && i){
                if(capture==2 && selected==i)settings_ui_text(renderer,texture,&font,number,280,86+(int)i*32);
                SDL_Rect minus={420,86+(int)i*32,60,28},plus={500,86+(int)i*32,60,28};
                SDL_SetRenderDrawColor(renderer,70,80,90,255);SDL_RenderFillRect(renderer,&minus);SDL_RenderFillRect(renderer,&plus);
                settings_ui_text(renderer,texture,&font,"-",442,86+(int)i*32);
                settings_ui_text(renderer,texture,&font,"+",522,86+(int)i*32);
            }
        }
        if(tab==2){
            settings_ui_text(renderer,texture,&font,"\xd7\xf3\xd3\xd2\xbc\xfc\xb5\xf7\xd5\xfb\xa3\xac\xd0\xd0\x37\x2d\x34\x30\xa3\xac\xc1\xd0\x31\x35\x2d\x36\x30",16,230);
            settings_ui_text(renderer,texture,&font,"\xd6\xbb\xc0\xa9\xd5\xb9\xb5\xd8\xcd\xbc\xa3\xac\xd5\xbd\xb6\xb7\xd3\xeb\xb2\xcb\xb5\xa5\xb1\xa3\xb3\xd6\xd4\xad\xb2\xbc\xbe\xd6",16,270);
        }
        settings_ui_text(renderer,texture,&font,"\x5b\x52\x5d\x20\xbb\xd6\xb8\xb4\xc8\xab\xb2\xbf\xc4\xac\xc8\xcf\xc9\xe8\xd6\xc3",16,534);
        settings_ui_text(renderer,texture,&font,"\x5b\x45\x73\x63\x2f\x46\x31\x30\x5d\x20\xb1\xa3\xb4\xe6\xb2\xa2\xb7\xb5\xbb\xd8",16,574);
        SDL_RenderPresent(renderer);
        SDL_Event event;(void)npe_sound_pump(game_sound_player);
        if(!SDL_WaitEventTimeout(&event,20))continue;
        if(game_audio_event(&event))continue;
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);break;}
        if(event.type==SDL_CONTROLLERDEVICEADDED || event.type==SDL_CONTROLLERDEVICEREMOVED){game_pad_event(&event);continue;}
        if(event.type==SDL_WINDOWEVENT && event.window.windowID==SDL_GetWindowID(window) &&
           event.window.event==SDL_WINDOWEVENT_CLOSE)break;
        SDL_Keycode key=SDLK_UNKNOWN;int activate=0,delta=0;
        if(event.type==SDL_KEYDOWN && !event.key.repeat){
            key=event.key.keysym.sym;
            if(capture==2){
                size_t n=strlen(number);
                if(key==SDLK_ESCAPE)capture=0;
                else if(key==SDLK_BACKSPACE && n)number[n-1]=0;
                else if(key>=SDLK_0 && key<=SDLK_9 && n<3){number[n]=(char)key;number[n+1]=0;}
                else if(key==SDLK_RETURN || key==SDLK_KP_ENTER){
                    unsigned value=(unsigned)atoi(number),min=selected==1?7:15,max=selected==1?40:60;
                    if(value>=min && value<=max){if(!edited.rows){edited.rows=14;edited.cols=25;}
                        if(selected==1)edited.rows=value;else edited.cols=value;capture=0;}
                }
                continue;
            }
            if(capture){
                if(key==SDLK_ESCAPE)capture=0;
                else if(tab==0 && key!=SDLK_F10 && *SDL_GetKeyName(key)){
                    settings_bind_key(&edited,selected,key);capture=0;
                }
                continue;
            }
        }else if(event.type==SDL_CONTROLLERBUTTONDOWN && event.cbutton.which==game_pad_id){
            if(capture){if(tab==1){settings_bind_button(&edited,selected,event.cbutton.button);capture=0;}continue;}
            switch(settings_pad_button(event.cbutton.button)){
                case SDL_CONTROLLER_BUTTON_DPAD_UP:key=SDLK_UP;break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:key=SDLK_DOWN;break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:key=SDLK_LEFT;break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:key=SDLK_RIGHT;break;
                case SDL_CONTROLLER_BUTTON_A:key=SDLK_RETURN;break;
                case SDL_CONTROLLER_BUTTON_B:case SDL_CONTROLLER_BUTTON_START:key=SDLK_ESCAPE;break;
                default:break;
            }
        }else if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT){
            float x,y;SDL_RenderWindowToLogical(renderer,event.button.x,event.button.y,&x,&y);
            if(capture)continue;
            if(y<40){tab=x<180?0:x<360?1:2;selected=0;continue;}
            if(y>=574){running=0;continue;}
            if(y>=534 && y<569){settings_defaults(&edited);continue;}
            if(y>=86 && y<86+32*(int)count){selected=(unsigned)(y-86)/32;activate=1;
                if(tab==2 && selected && x>=420){delta=x>=500?1:-1;activate=0;}}
        }
        if(key==SDLK_ESCAPE || key==SDLK_F10){running=0;continue;}
        if(key==SDLK_TAB || (key>=SDLK_1 && key<=SDLK_3)){
            tab=key==SDLK_TAB?(tab+1)%3:(unsigned)(key-SDLK_1);selected=0;continue;
        }
        if(key==SDLK_r){settings_defaults(&edited);continue;}
        if(key==SDLK_UP)selected=selected?selected-1:count-1;
        if(key==SDLK_DOWN)selected=(selected+1)%count;
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER)activate=1;
        if(tab==2){
            if(key==SDLK_LEFT || key==SDLK_MINUS)delta=-1;
            if(key==SDLK_RIGHT || key==SDLK_PLUS || key==SDLK_EQUALS)delta=1;
            if(activate && selected==0){if(edited.rows)edited.rows=edited.cols=0;else{edited.rows=14;edited.cols=25;}}
            if(activate && selected){capture=2;number[0]=0;continue;}
            if(delta && selected){
                if(!edited.rows){edited.rows=14;edited.cols=25;}
                unsigned *value=selected==1?&edited.rows:&edited.cols;
                unsigned min=selected==1?7:15,max=selected==1?40:60;
                int next=(int)*value+delta;if(next<(int)min)next=(int)min;if(next>(int)max)next=(int)max;*value=(unsigned)next;
            }
        }else if(activate)capture=1;
    }
    if(settings_save(&edited))SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
        "Noah NPE", "Cannot save settings. Previous settings were retained.",window);
    else {settings=edited;settings_last_saved=1;}
    game_pad_reset();SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);npe_font_free(&font);
    if(parent){SDL_RaiseWindow(parent);SDL_Event event;SDL_zero(event);event.type=SDL_WINDOWEVENT;
        event.window.windowID=SDL_GetWindowID(parent);event.window.event=SDL_WINDOWEVENT_EXPOSED;SDL_PushEvent(&event);}
#endif
}


#endif
static void show_settings(void){show_settings_page(0);}
