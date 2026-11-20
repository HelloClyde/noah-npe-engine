#ifndef NPE_SETTINGS_TYPES_H
#define NPE_SETTINGS_TYPES_H
#include <SDL.h>
enum { SETTINGS_KEYS=12, SETTINGS_BUTTONS=13 };
typedef struct {
    SDL_Keycode keys[SETTINGS_KEYS];
    Uint8 buttons[SETTINGS_BUTTONS];
    unsigned rows,cols;
    unsigned sound_muted,volume;
} NpeSettings;
typedef struct {
    void (*defaults)(NpeSettings *);
    void (*bind_key)(NpeSettings *,unsigned,SDL_Keycode);
    void (*bind_button)(NpeSettings *,unsigned,Uint8);
    int (*poll_button)(void); /* -1 no input, -2 quit, otherwise physical button. */
} NpeSettingsHooks;
int npe_macos_settings_dialog(NpeSettings *,unsigned,const NpeSettingsHooks *);
#endif
