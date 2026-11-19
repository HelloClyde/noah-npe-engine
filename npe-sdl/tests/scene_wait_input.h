#include <string.h>
/* Older menu/route probes do not measure playback time. Supply an accepted
   frame-wait input so the new opening renders fully without a 12-second wait.
   Main-loop key sequences remain in SDL's real event queue. */
static unsigned game_input_busy;
static int probe_scene_key_down;
static int probe_scene_wait_input(SDL_Event *event) {
    if(game_input_busy)return 0;
    memset(event, 0, sizeof(*event));
    event->type = probe_scene_key_down?SDL_KEYUP:SDL_KEYDOWN;
    probe_scene_key_down=!probe_scene_key_down;
    event->key.keysym.sym = SDLK_RETURN;
    return 1;
}
#define SDL_PollEvent probe_scene_wait_input
