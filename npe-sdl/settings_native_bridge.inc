static int settings_native_poll_button(void){
    (void)npe_sound_pump(game_sound_player);
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);return -2;}
        if(game_audio_event(&event))continue;
        if(event.type==SDL_CONTROLLERDEVICEADDED || event.type==SDL_CONTROLLERDEVICEREMOVED){
            (void)game_pad_event(&event);continue;
        }
        if(event.type==SDL_CONTROLLERBUTTONDOWN && event.cbutton.which==game_pad_id)
            return event.cbutton.button;
    }
    return -1;
}
static int settings_native_commit(const NpeSettings *edited,SDL_Window *parent){
    if(settings_save(edited)){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"NPE Settings",
            "Cannot save settings. Previous settings were retained.",parent);return -1;
    }
    settings=*edited;return 0;
}
