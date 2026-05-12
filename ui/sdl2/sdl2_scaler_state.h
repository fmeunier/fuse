/* sdl2_scaler_state.h: SDL2 fullscreen scaler state helpers
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#ifndef FUSE_SDL2_SCALER_STATE_H
#define FUSE_SDL2_SCALER_STATE_H

#include "config.h"

#include "ui/scaler/scaler.h"

typedef enum sdl2_scaler_fullscreen_state {
  SDL2_SCALER_WINDOWED = 0,
  SDL2_SCALER_FULLSCREEN_NATIVE,
  SDL2_SCALER_FULLSCREEN_AUTO
} sdl2_scaler_fullscreen_state;

typedef struct sdl2_scaler_state {
  sdl2_scaler_fullscreen_state state;
  scaler_type restore_scaler;
} sdl2_scaler_state;

typedef struct sdl2_scaler_env {
  int image_width;
  int image_height;
  int display_width;
  int display_height;
  const unsigned char *supported;
  const float *scales;
  int scaler_count;
} sdl2_scaler_env;

typedef struct sdl2_scaler_decision {
  scaler_type scaler;
  sdl2_scaler_state next;
} sdl2_scaler_decision;

void sdl2_scaler_state_init( sdl2_scaler_state *state );

sdl2_scaler_decision sdl2_scaler_state_startup( scaler_type requested,
                                                int fullscreen,
                                                const sdl2_scaler_env *env );

sdl2_scaler_decision sdl2_scaler_state_enter_fullscreen(
  scaler_type current, const sdl2_scaler_env *env );

sdl2_scaler_decision sdl2_scaler_state_leave_fullscreen(
  const sdl2_scaler_state *state, scaler_type current,
  const sdl2_scaler_env *env );

sdl2_scaler_decision sdl2_scaler_state_user_select( scaler_type requested,
                                                    int fullscreen,
                                                    const sdl2_scaler_env *env );

sdl2_scaler_decision sdl2_scaler_state_reevaluate(
  const sdl2_scaler_state *state, scaler_type current, int fullscreen,
  const sdl2_scaler_env *env );

sdl2_scaler_decision sdl2_scaler_state_transition(
  const sdl2_scaler_state *state, scaler_type current, int fullscreen,
  int was_fullscreen, int startup, int explicit_scaler_change,
  const sdl2_scaler_env *env );

#endif
