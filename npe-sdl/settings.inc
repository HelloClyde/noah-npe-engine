/* Host preferences, independent of NPE data and save slots. */
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "platform/settings_types.h"
static const SDL_Keycode settings_default_keys[SETTINGS_KEYS]={SDLK_UP,SDLK_DOWN,
    SDLK_LEFT,SDLK_RIGHT,SDLK_RETURN,SDLK_F4,SDLK_SPACE,SDLK_u,SDLK_p,SDLK_n,SDLK_j,SDLK_l};
static const Uint8 settings_default_buttons[SETTINGS_BUTTONS]={
    SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,SDL_CONTROLLER_BUTTON_RIGHTSTICK};
static NpeSettings settings;
static int settings_loaded;
static int settings_last_saved;
static void settings_defaults(NpeSettings *value){
    memset(value,0,sizeof(*value));
    value->volume=100;
    memcpy(value->keys,settings_default_keys,sizeof(value->keys));
    memcpy(value->buttons,settings_default_buttons,sizeof(value->buttons));
}
static int settings_valid(const NpeSettings *value){
    if(value->sound_muted>1 || value->volume>100)return 0;
    if((value->rows||value->cols) &&
       (value->rows<7 || value->rows>40 || value->cols<15 || value->cols>60))return 0;
    for(unsigned i=0;i<SETTINGS_KEYS;++i){
        if(!value->keys[i] || value->keys[i]==SDLK_F10 || !*SDL_GetKeyName(value->keys[i]))return 0;
        for(unsigned j=0;j<i;++j)if(value->keys[j]==value->keys[i])return 0;
    }
    for(unsigned i=0;i<SETTINGS_BUTTONS;++i){
        if(value->buttons[i]>=SDL_CONTROLLER_BUTTON_MAX)return 0;
        for(unsigned j=0;j<i;++j)if(value->buttons[j]==value->buttons[i])return 0;
    }
    return 1;
}
static int settings_decode(const char *text,NpeSettings *value){
    NpeSettings next;settings_defaults(&next);int consumed=0;
    unsigned version;
    if(sscanf(text,"NPE_SETTINGS %u%n",&version,&consumed)!=1 || !consumed || (version!=1&&version!=2))return -1;
    unsigned keys=version==1?10:SETTINGS_KEYS,buttons=version==1?11:SETTINGS_BUTTONS;
    text+=consumed;
    for(unsigned i=0;i<keys;++i){
        int key;consumed=0;if(sscanf(text," %d%n",&key,&consumed)!=1 || !consumed)return -1;
        next.keys[i]=(SDL_Keycode)key;text+=consumed;
    }
    for(unsigned i=0;i<buttons;++i){
        unsigned button;consumed=0;if(sscanf(text," %u%n",&button,&consumed)!=1 || button>=SDL_CONTROLLER_BUTTON_MAX)return -1;
        next.buttons[i]=(Uint8)button;text+=consumed;
    }
    if(sscanf(text," %u %u%n",&next.rows,&next.cols,&consumed)!=2)return -1;
    text+=consumed;while(*text==' '||*text=='\r'||*text=='\n'||*text=='\t')++text;
    if(version==2 && *text){
        if(sscanf(text,"sound %u %u%n",&next.sound_muted,&next.volume,&consumed)!=2)return -1;
        text+=consumed;while(*text==' '||*text=='\r'||*text=='\n'||*text=='\t')++text;
    }
    if(version==1){
        for(unsigned i=10;i<SETTINGS_KEYS;++i){
            int duplicate=0;for(unsigned j=0;j<i;++j)duplicate|=next.keys[j]==next.keys[i];
            if(duplicate){SDL_Keycode candidate=i==10?SDLK_q:SDLK_e;
                for(;;){int used=0;for(unsigned j=0;j<i;++j)used|=next.keys[j]==candidate;if(!used)break;candidate=candidate==SDLK_z?SDLK_a:candidate+1;}
                next.keys[i]=candidate;
            }
        }
        for(unsigned i=11;i<SETTINGS_BUTTONS;++i){
            int duplicate=0;for(unsigned j=0;j<i;++j)duplicate|=next.buttons[j]==next.buttons[i];
            if(duplicate)for(unsigned candidate=0;candidate<SDL_CONTROLLER_BUTTON_MAX;++candidate){
                if(candidate==SDL_CONTROLLER_BUTTON_GUIDE)continue;
                int used=0;for(unsigned j=0;j<i;++j)used|=next.buttons[j]==candidate;
                if(!used){next.buttons[i]=(Uint8)candidate;break;}
            }
        }
    }
    if(*text || !settings_valid(&next))return -1;
    *value=next;return 0;
}
static int settings_encode(const NpeSettings *value,char *text,size_t size){
    if(!settings_valid(value))return -1;
    int used=snprintf(text,size,"NPE_SETTINGS 2\n");
    for(unsigned i=0;i<SETTINGS_KEYS+SETTINGS_BUTTONS;++i){
        if(used<0 || (size_t)used>=size)return -1;
        int number=i<SETTINGS_KEYS?(int)value->keys[i]:(int)value->buttons[i-SETTINGS_KEYS];
        int n=snprintf(text+used,size-(size_t)used,"%d ",number);if(n<0)return -1;used+=n;
    }
    if(used<0 || (size_t)used>=size)return -1;
    int n=snprintf(text+used,size-(size_t)used,"\n%u %u\n",value->rows,value->cols);
    if(n<0 || (size_t)n>=size-(size_t)used)return -1;
    used+=n;
    if(value->sound_muted||value->volume!=100){
        n=snprintf(text+used,size-(size_t)used,"sound %u %u\n",value->sound_muted,value->volume);
        if(n<0||(size_t)n>=size-(size_t)used)return -1;
        used+=n;
    }
    return used;
}
static char *settings_path(const char *name){
    char *base=SDL_GetPrefPath("NPE","npe-sdl");if(!base)return NULL;
    size_t n=strlen(base)+strlen(name)+1;char *path=malloc(n);
    if(path)snprintf(path,n,"%s%s",base,name);
    SDL_free(base);return path;
}
static void settings_load(void){
    if(settings_loaded)return;
    settings_defaults(&settings);settings_loaded=1;
    char *path=settings_path("settings.cfg");if(!path)return;
    SDL_RWops *rw=SDL_RWFromFile(path,"rb");free(path);if(!rw)return;
    Sint64 size=SDL_RWsize(rw);char text[1024];
    if(size>0 && size<(Sint64)sizeof(text) && SDL_RWread(rw,text,1,(size_t)size)==(size_t)size){
        text[size]=0;NpeSettings parsed;if(!settings_decode(text,&parsed))settings=parsed;
    }
    SDL_RWclose(rw);
}
static int settings_replace(const char *from,const char *to){
#ifdef _WIN32
    wchar_t a[32768],b[32768];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,from,-1,a,32768) ||
       !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,to,-1,b,32768))return -1;
    return MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
    return rename(from,to);
#endif
}
static int settings_save(const NpeSettings *value){
    char text[1024];int size=settings_encode(value,text,sizeof(text));if(size<0)return -1;
    char *path=settings_path("settings.cfg"),*temp=settings_path("settings.cfg.tmp");
    if(!path || !temp){free(path);free(temp);return -1;}
    SDL_RWops *rw=SDL_RWFromFile(temp,"wb");int result=-1;
    if(rw){int complete=SDL_RWwrite(rw,text,1,(size_t)size)==(size_t)size;
        int closed=SDL_RWclose(rw);if(complete&&!closed)result=settings_replace(temp,path);}
    free(path);free(temp);return result;
}
static void settings_bind_key(NpeSettings *value,unsigned action,SDL_Keycode key){
    SDL_Keycode old=value->keys[action];
    for(unsigned i=0;i<SETTINGS_KEYS;++i)if(value->keys[i]==key)value->keys[i]=old;
    value->keys[action]=key;
}
static void settings_bind_button(NpeSettings *value,unsigned action,Uint8 button){
    Uint8 old=value->buttons[action];
    for(unsigned i=0;i<SETTINGS_BUTTONS;++i)if(value->buttons[i]==button)value->buttons[i]=old;
    value->buttons[action]=button;
}
static SDL_Keycode settings_keyboard(SDL_Keycode key,unsigned mode){
    if(!settings_loaded || mode==8)return key; /* Answer text stays literal. */
    for(unsigned i=0;i<SETTINGS_KEYS;++i)if(settings.keys[i]==key)return settings_default_keys[i];
    for(unsigned i=0;i<SETTINGS_KEYS;++i)if(settings_default_keys[i]==key)return SDLK_UNKNOWN;
    return key;
}
static Uint8 settings_pad_button(Uint8 button){
    if(!settings_loaded)return button;
    for(unsigned i=0;i<SETTINGS_BUTTONS;++i)if(settings.buttons[i]==button)return settings_default_buttons[i];
    return SDL_CONTROLLER_BUTTON_INVALID;
}
