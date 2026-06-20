/* sdl2_scaler_state.c: SDL2 fullscreen scaler state helpers
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#include "config.h"

#include "ui/scaler/scaler.h"

#include "sdl2_scaler_state.h"

typedef enum sdl2_scaler_family {
  SDL2_SCALER_FAMILY_NONE = 0,
  SDL2_SCALER_FAMILY_NORMAL,
  SDL2_SCALER_FAMILY_TV,
  SDL2_SCALER_FAMILY_PALTV,
  SDL2_SCALER_FAMILY_ADVMAME,
  SDL2_SCALER_FAMILY_HQ,
  SDL2_SCALER_FAMILY_TIMEX,
  SDL2_SCALER_FAMILY_SPECIAL2X,
  SDL2_SCALER_FAMILY_DOTMATRIX,
  SDL2_SCALER_FAMILY_NTSCTV
} sdl2_scaler_family;

static sdl2_scaler_family scaler_family( scaler_type scaler );
static int scaler_valid( scaler_type scaler, const sdl2_scaler_env *env );
static int scaler_supported( scaler_type scaler, const sdl2_scaler_env *env );
static int scaler_fits( scaler_type scaler, const sdl2_scaler_env *env );
static scaler_type choose_same_family_fit( scaler_type requested,
                                           const sdl2_scaler_env *env );
static scaler_type choose_largest_fit( const sdl2_scaler_env *env );
static scaler_type choose_same_family_supported( scaler_type requested,
                                                 const sdl2_scaler_env *env );
static scaler_type choose_supported_fallback( const sdl2_scaler_env *env );
static scaler_type resolve_fullscreen_scaler( scaler_type requested,
                                              const sdl2_scaler_env *env );
static scaler_type resolve_restore_scaler( scaler_type requested,
                                           const sdl2_scaler_env *env );
static sdl2_scaler_decision keep_scaler( scaler_type scaler,
                                         sdl2_scaler_fullscreen_state state,
                                         scaler_type restore_scaler );

void
sdl2_scaler_state_init( sdl2_scaler_state *state )
{
  state->state = SDL2_SCALER_WINDOWED;
  state->restore_scaler = SCALER_NUM;
}

sdl2_scaler_decision
sdl2_scaler_state_startup( scaler_type requested, int fullscreen,
                           const sdl2_scaler_env *env )
{
  scaler_type actual;

  if( !fullscreen )
    return keep_scaler( requested, SDL2_SCALER_WINDOWED, SCALER_NUM );

  actual = resolve_fullscreen_scaler( requested, env );
  if( actual == requested )
    return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_NATIVE, SCALER_NUM );

  return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_AUTO, requested );
}

sdl2_scaler_decision
sdl2_scaler_state_enter_fullscreen( scaler_type current,
                                    const sdl2_scaler_env *env )
{
  scaler_type actual;

  actual = resolve_fullscreen_scaler( current, env );
  if( actual == current )
    return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_NATIVE, SCALER_NUM );

  return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_AUTO, current );
}

sdl2_scaler_decision
sdl2_scaler_state_leave_fullscreen( const sdl2_scaler_state *state,
                                    scaler_type current,
                                    const sdl2_scaler_env *env )
{
  scaler_type restore_scaler;

  if( state->state != SDL2_SCALER_FULLSCREEN_AUTO ||
      state->restore_scaler == SCALER_NUM )
    return keep_scaler( current, SDL2_SCALER_WINDOWED, SCALER_NUM );

  restore_scaler = resolve_restore_scaler( state->restore_scaler, env );

  return keep_scaler( restore_scaler, SDL2_SCALER_WINDOWED, SCALER_NUM );
}

sdl2_scaler_decision
sdl2_scaler_state_user_select( scaler_type requested, int fullscreen,
                               const sdl2_scaler_env *env )
{
  if( !fullscreen )
    return keep_scaler( requested, SDL2_SCALER_WINDOWED, SCALER_NUM );

  return keep_scaler( resolve_fullscreen_scaler( requested, env ),
                      SDL2_SCALER_FULLSCREEN_NATIVE, SCALER_NUM );
}

sdl2_scaler_decision
sdl2_scaler_state_reevaluate( const sdl2_scaler_state *state,
                              scaler_type current, int fullscreen,
                              const sdl2_scaler_env *env )
{
  scaler_type actual;
  scaler_type restore_scaler;

  if( !fullscreen )
    return keep_scaler( current, SDL2_SCALER_WINDOWED, SCALER_NUM );

  if( scaler_fits( current, env ) )
    return keep_scaler( current, state->state, state->restore_scaler );

  actual = resolve_fullscreen_scaler( current, env );

  if( state->state == SDL2_SCALER_FULLSCREEN_AUTO ) {
    restore_scaler = resolve_restore_scaler( state->restore_scaler, env );
    return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_AUTO,
                        restore_scaler );
  }

  if( actual == current )
    return keep_scaler( current, SDL2_SCALER_FULLSCREEN_NATIVE, SCALER_NUM );

  return keep_scaler( actual, SDL2_SCALER_FULLSCREEN_AUTO, current );
}

sdl2_scaler_decision
sdl2_scaler_state_transition( const sdl2_scaler_state *state,
                              scaler_type current, int fullscreen,
                              int was_fullscreen, int startup,
                              int explicit_scaler_change,
                              const sdl2_scaler_env *env )
{
  if( startup )
    return sdl2_scaler_state_startup( current, fullscreen, env );

  if( was_fullscreen != fullscreen ) {
    if( fullscreen ) return sdl2_scaler_state_enter_fullscreen( current, env );

    return sdl2_scaler_state_leave_fullscreen( state, current, env );
  }

  if( explicit_scaler_change )
    return sdl2_scaler_state_user_select( current, fullscreen, env );

  return sdl2_scaler_state_reevaluate( state, current, fullscreen, env );
}

static sdl2_scaler_family
scaler_family( scaler_type scaler )
{
  switch( scaler ) {
  case SCALER_NORMAL:
  case SCALER_DOUBLESIZE:
  case SCALER_TRIPLESIZE:
  case SCALER_QUADSIZE:
    return SDL2_SCALER_FAMILY_NORMAL;

  case SCALER_TIMEXTV:
  case SCALER_TV2X:
  case SCALER_TV3X:
  case SCALER_TV4X:
    return SDL2_SCALER_FAMILY_TV;

  case SCALER_PALTV2X:
  case SCALER_PALTV3X:
  case SCALER_PALTV4X:
    return SDL2_SCALER_FAMILY_PALTV;

  case SCALER_ADVMAME2X:
  case SCALER_ADVMAME3X:
    return SDL2_SCALER_FAMILY_ADVMAME;

  case SCALER_HQ2X:
  case SCALER_HQ3X:
  case SCALER_HQ4X:
    return SDL2_SCALER_FAMILY_HQ;

  case SCALER_HALF:
  case SCALER_HALFSKIP:
  case SCALER_TIMEX1_5X:
  case SCALER_TIMEX2X:
    return SDL2_SCALER_FAMILY_TIMEX;

  case SCALER_2XSAI:
  case SCALER_SUPER2XSAI:
  case SCALER_SUPEREAGLE:
    return SDL2_SCALER_FAMILY_SPECIAL2X;

  case SCALER_DOTMATRIX:
    return SDL2_SCALER_FAMILY_DOTMATRIX;

  case SCALER_NTSC2X:
  case SCALER_NTSC3X:
  case SCALER_NTSC4X:
    return SDL2_SCALER_FAMILY_NTSCTV;

  default:
    return SDL2_SCALER_FAMILY_NONE;
  }
}

static int
scaler_valid( scaler_type scaler, const sdl2_scaler_env *env )
{
  return scaler >= 0 && scaler < env->scaler_count;
}

static int
scaler_supported( scaler_type scaler, const sdl2_scaler_env *env )
{
  return scaler_valid( scaler, env ) && env->supported[ scaler ];
}

static int
scaler_fits( scaler_type scaler, const sdl2_scaler_env *env )
{
  if( !scaler_supported( scaler, env ) ) return 0;

  if( !env->display_width || !env->display_height ) return 1;

  return env->image_width * env->scales[ scaler ] <= env->display_width &&
         env->image_height * env->scales[ scaler ] <= env->display_height;
}

static scaler_type
choose_same_family_fit( scaler_type requested, const sdl2_scaler_env *env )
{
  sdl2_scaler_family family;
  scaler_type scaler;
  scaler_type best;
  scaler_type fallback;
  float requested_scale;
  float best_scale;
  float fallback_scale;

  family = scaler_family( requested );
  if( family == SDL2_SCALER_FAMILY_NONE ) return SCALER_NUM;

  requested_scale = scaler_valid( requested, env ) ? env->scales[ requested ] :
                    0.0f;
  best = SCALER_NUM;
  fallback = SCALER_NUM;
  best_scale = 0.0f;
  fallback_scale = 0.0f;

  for( scaler = 0; scaler < env->scaler_count; scaler++ ) {
    float scale;

    if( scaler_family( scaler ) != family || !scaler_fits( scaler, env ) )
      continue;

    scale = env->scales[ scaler ];

    if( scale <= requested_scale ) {
      if( best == SCALER_NUM || scale > best_scale ) {
        best = scaler;
        best_scale = scale;
      }
    } else {
      if( fallback == SCALER_NUM || scale < fallback_scale ) {
        fallback = scaler;
        fallback_scale = scale;
      }
    }
  }

  if( best != SCALER_NUM ) return best;

  return fallback;
}

static scaler_type
choose_largest_fit( const sdl2_scaler_env *env )
{
  scaler_type scaler;
  scaler_type best;
  float best_scale;

  best = SCALER_NUM;
  best_scale = 0.0f;

  for( scaler = 0; scaler < env->scaler_count; scaler++ ) {
    if( !scaler_fits( scaler, env ) ) continue;

    if( best == SCALER_NUM || env->scales[ scaler ] > best_scale ) {
      best = scaler;
      best_scale = env->scales[ scaler ];
    }
  }

  return best;
}

static scaler_type
choose_same_family_supported( scaler_type requested, const sdl2_scaler_env *env )
{
  sdl2_scaler_family family;
  scaler_type scaler;
  scaler_type best;
  float requested_scale;
  float best_diff;

  family = scaler_family( requested );
  if( family == SDL2_SCALER_FAMILY_NONE ) return SCALER_NUM;

  requested_scale = scaler_valid( requested, env ) ? env->scales[ requested ] :
                    0.0f;
  best = SCALER_NUM;
  best_diff = 0.0f;

  for( scaler = 0; scaler < env->scaler_count; scaler++ ) {
    float scale;
    float diff;

    if( scaler_family( scaler ) != family || !scaler_supported( scaler, env ) )
      continue;

    scale = env->scales[ scaler ];
    diff = scale - requested_scale;
    if( diff < 0.0f ) diff = -diff;

    if( best == SCALER_NUM || diff < best_diff ||
        ( diff == best_diff && scale < env->scales[ best ] ) ) {
      best = scaler;
      best_diff = diff;
    }
  }

  return best;
}

static scaler_type
choose_supported_fallback( const sdl2_scaler_env *env )
{
  if( scaler_supported( SCALER_NORMAL, env ) ) return SCALER_NORMAL;

  return choose_largest_fit( env );
}

static scaler_type
resolve_fullscreen_scaler( scaler_type requested, const sdl2_scaler_env *env )
{
  scaler_type scaler;

  if( scaler_fits( requested, env ) ) return requested;

  scaler = choose_same_family_fit( requested, env );
  if( scaler != SCALER_NUM ) return scaler;

  scaler = choose_largest_fit( env );
  if( scaler != SCALER_NUM ) return scaler;

  scaler = choose_same_family_supported( requested, env );
  if( scaler != SCALER_NUM ) return scaler;

  scaler = choose_supported_fallback( env );
  if( scaler != SCALER_NUM ) return scaler;

  return requested;
}

static scaler_type
resolve_restore_scaler( scaler_type requested, const sdl2_scaler_env *env )
{
  scaler_type scaler;

  if( requested == SCALER_NUM ) return SCALER_NORMAL;
  if( scaler_supported( requested, env ) ) return requested;

  scaler = choose_same_family_supported( requested, env );
  if( scaler != SCALER_NUM ) return scaler;

  scaler = choose_supported_fallback( env );
  if( scaler != SCALER_NUM ) return scaler;

  return requested;
}

static sdl2_scaler_decision
keep_scaler( scaler_type scaler, sdl2_scaler_fullscreen_state state,
             scaler_type restore_scaler )
{
  sdl2_scaler_decision decision;

  decision.scaler = scaler;
  decision.next.state = state;
  decision.next.restore_scaler = restore_scaler;

  return decision;
}
