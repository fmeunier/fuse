/* sdl2scalerstatetest.c: Tests for SDL2 scaler state helpers
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "ui/scaler/scaler.h"
#include "ui/sdl2/sdl2_scaler_state.h"

static void
fill_test_scales( float *scales )
{
  int i;

  for( i = 0; i < SCALER_NUM; i++ ) scales[i] = 0.0f;

  scales[ SCALER_HALF ] = 0.5f;
  scales[ SCALER_HALFSKIP ] = 0.5f;
  scales[ SCALER_NORMAL ] = 1.0f;
  scales[ SCALER_TIMEXTV ] = 1.0f;
  scales[ SCALER_TIMEX1_5X ] = 1.5f;
  scales[ SCALER_DOUBLESIZE ] = 2.0f;
  scales[ SCALER_2XSAI ] = 2.0f;
  scales[ SCALER_SUPER2XSAI ] = 2.0f;
  scales[ SCALER_SUPEREAGLE ] = 2.0f;
  scales[ SCALER_ADVMAME2X ] = 2.0f;
  scales[ SCALER_TV2X ] = 2.0f;
  scales[ SCALER_DOTMATRIX ] = 2.0f;
  scales[ SCALER_TIMEX2X ] = 2.0f;
  scales[ SCALER_PALTV2X ] = 2.0f;
  scales[ SCALER_HQ2X ] = 2.0f;
  scales[ SCALER_TRIPLESIZE ] = 3.0f;
  scales[ SCALER_ADVMAME3X ] = 3.0f;
  scales[ SCALER_TV3X ] = 3.0f;
  scales[ SCALER_PALTV3X ] = 3.0f;
  scales[ SCALER_HQ3X ] = 3.0f;
  scales[ SCALER_QUADSIZE ] = 4.0f;
  scales[ SCALER_TV4X ] = 4.0f;
  scales[ SCALER_PALTV4X ] = 4.0f;
  scales[ SCALER_HQ4X ] = 4.0f;
}

static void
fill_supported( unsigned char *supported )
{
  memset( supported, 1, SCALER_NUM * sizeof( *supported ) );
}

static void
fill_env( sdl2_scaler_env *env, unsigned char *supported, float *scales,
          int display_width, int display_height )
{
  env->image_width = 320;
  env->image_height = 240;
  env->display_width = display_width;
  env->display_height = display_height;
  env->supported = supported;
  env->scales = scales;
  env->scaler_count = SCALER_NUM;
}

static int
startup_fullscreen_auto_restores_on_exit( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_QUADSIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  decision = sdl2_scaler_state_startup( SCALER_QUADSIZE, 1, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_QUADSIZE ) {
    fprintf( stderr, "startup auto: expected normal/fullscreen-auto restore 4x\n" );
    return 1;
  }

  decision = sdl2_scaler_state_leave_fullscreen( &decision.next,
                                                 decision.scaler, &env );
  if( decision.scaler != SCALER_QUADSIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr, "startup leave: expected restore 4x and clear state\n" );
    return 1;
  }

  return 0;
}

static int
startup_fullscreen_keeps_requested_when_it_fits( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  decision = sdl2_scaler_state_startup( SCALER_QUADSIZE, 1, &env );
  if( decision.scaler != SCALER_QUADSIZE ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_NATIVE ) {
    fprintf( stderr, "startup native: expected keep 4x fullscreen-native\n" );
    return 1;
  }

  return 0;
}

static int
enter_fullscreen_prefers_same_family( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_TV2X ] = 1;
  supported[ SCALER_TV3X ] = 1;
  supported[ SCALER_TV4X ] = 1;
  fill_env( &env, supported, scales, 900, 700 );

  decision = sdl2_scaler_state_enter_fullscreen( SCALER_TV4X, &env );
  if( decision.scaler != SCALER_TV2X ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_TV4X ) {
    fprintf( stderr, "enter fullscreen: expected TV4x -> TV2x auto fallback\n" );
    return 1;
  }

  return 0;
}

static int
tv_family_includes_timextv( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_TIMEXTV ] = 1;
  supported[ SCALER_TV4X ] = 1;
  fill_env( &env, supported, scales, 500, 350 );

  decision = sdl2_scaler_state_enter_fullscreen( SCALER_TV4X, &env );
  if( decision.scaler != SCALER_TIMEXTV ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ) {
    fprintf( stderr, "tv family: expected TV4x -> Timex TV fallback\n" );
    return 1;
  }

  return 0;
}

static int
user_select_clears_auto_restore_state( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_QUADSIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  decision = sdl2_scaler_state_startup( SCALER_QUADSIZE, 1, &env );
  decision = sdl2_scaler_state_user_select( SCALER_NORMAL, 1, &env );

  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_NATIVE ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr, "user select: expected native scaler with cleared restore\n" );
    return 1;
  }

  return 0;
}

static int
reevaluate_native_becomes_auto_when_current_no_longer_fits( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_DOUBLESIZE ] = 1;
  fill_env( &env, supported, scales, 500, 350 );

  state.state = SDL2_SCALER_FULLSCREEN_NATIVE;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_reevaluate( &state, SCALER_DOUBLESIZE, 1,
                                           &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_DOUBLESIZE ) {
    fprintf( stderr, "reevaluate native: expected 2x -> normal auto fallback\n" );
    return 1;
  }

  return 0;
}

static int
reevaluate_auto_preserves_supported_restore_scaler( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_DOUBLESIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  state.state = SDL2_SCALER_FULLSCREEN_AUTO;
  state.restore_scaler = SCALER_DOUBLESIZE;

  decision = sdl2_scaler_state_reevaluate( &state, SCALER_NORMAL, 1, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_DOUBLESIZE ) {
    fprintf( stderr, "reevaluate auto: expected preserve restore scaler\n" );
    return 1;
  }

  return 0;
}

static int
leave_fullscreen_restores_supported_equivalent_when_original_invalid( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_TRIPLESIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  state.state = SDL2_SCALER_FULLSCREEN_AUTO;
  state.restore_scaler = SCALER_QUADSIZE;

  decision = sdl2_scaler_state_leave_fullscreen( &state, SCALER_NORMAL, &env );
  if( decision.scaler != SCALER_TRIPLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ) {
    fprintf( stderr, "leave fullscreen: expected restore equivalent 3x scaler\n" );
    return 1;
  }

  return 0;
}

static int
transition_dispatches_startup_branch( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_QUADSIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  state.state = SDL2_SCALER_WINDOWED;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_transition( &state, SCALER_QUADSIZE, 1, 0, 1,
                                           0, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ) {
    fprintf( stderr, "transition startup: expected startup branch\n" );
    return 1;
  }

  return 0;
}

static int
transition_dispatches_enter_branch( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_DOUBLESIZE ] = 1;
  fill_env( &env, supported, scales, 500, 350 );

  state.state = SDL2_SCALER_WINDOWED;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_transition( &state, SCALER_DOUBLESIZE, 1, 0, 0,
                                           0, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_DOUBLESIZE ) {
    fprintf( stderr, "transition enter: expected enter branch\n" );
    return 1;
  }

  return 0;
}

static int
transition_dispatches_leave_branch( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_TRIPLESIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  state.state = SDL2_SCALER_FULLSCREEN_AUTO;
  state.restore_scaler = SCALER_QUADSIZE;

  decision = sdl2_scaler_state_transition( &state, SCALER_NORMAL, 0, 1, 0, 0,
                                           &env );
  if( decision.scaler != SCALER_TRIPLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ) {
    fprintf( stderr, "transition leave: expected leave branch\n" );
    return 1;
  }

  return 0;
}

static int
transition_dispatches_explicit_selection_branch( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_QUADSIZE ] = 1;
  fill_env( &env, supported, scales, 400, 300 );

  state.state = SDL2_SCALER_FULLSCREEN_AUTO;
  state.restore_scaler = SCALER_QUADSIZE;

  decision = sdl2_scaler_state_transition( &state, SCALER_QUADSIZE, 1, 1, 0, 1,
                                           &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_NATIVE ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr, "transition explicit: expected explicit branch\n" );
    return 1;
  }

  return 0;
}

static int
transition_dispatches_reevaluate_branch( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  memset( supported, 0, sizeof( supported ) );
  supported[ SCALER_NORMAL ] = 1;
  supported[ SCALER_DOUBLESIZE ] = 1;
  fill_env( &env, supported, scales, 500, 350 );

  state.state = SDL2_SCALER_FULLSCREEN_NATIVE;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_transition( &state, SCALER_DOUBLESIZE, 1, 1, 0,
                                           0, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_AUTO ||
      decision.next.restore_scaler != SCALER_DOUBLESIZE ) {
    fprintf( stderr, "transition reevaluate: expected reevaluate branch\n" );
    return 1;
  }

  return 0;
}

static int
fullscreen_unknown_display_keeps_current_scaler( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 0, 0 );

  decision = sdl2_scaler_state_enter_fullscreen( SCALER_DOUBLESIZE, &env );
  if( decision.scaler != SCALER_DOUBLESIZE ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_NATIVE ) {
    fprintf( stderr, "unknown display: expected keep current scaler\n" );
    return 1;
  }

  return 0;
}

static int
startup_windowed_returns_requested_as_windowed( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  decision = sdl2_scaler_state_startup( SCALER_DOUBLESIZE, 0, &env );
  if( decision.scaler != SCALER_DOUBLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "startup windowed: expected keep 2x windowed, no restore\n" );
    return 1;
  }

  return 0;
}

static int
leave_fullscreen_from_native_state_keeps_current( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  state.state = SDL2_SCALER_FULLSCREEN_NATIVE;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_leave_fullscreen( &state, SCALER_DOUBLESIZE,
                                                 &env );
  if( decision.scaler != SCALER_DOUBLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "leave native: expected keep current and transition to windowed\n" );
    return 1;
  }

  return 0;
}

static int
leave_fullscreen_auto_with_no_restore_keeps_current( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  state.state = SDL2_SCALER_FULLSCREEN_AUTO;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_leave_fullscreen( &state, SCALER_NORMAL, &env );
  if( decision.scaler != SCALER_NORMAL ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "leave auto no restore: expected keep current and windowed\n" );
    return 1;
  }

  return 0;
}

static int
user_select_windowed_returns_requested_as_windowed( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  decision = sdl2_scaler_state_user_select( SCALER_TRIPLESIZE, 0, &env );
  if( decision.scaler != SCALER_TRIPLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "user select windowed: expected keep 3x windowed, no restore\n" );
    return 1;
  }

  return 0;
}

static int
reevaluate_windowed_keeps_current( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  state.state = SDL2_SCALER_WINDOWED;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_reevaluate( &state, SCALER_TRIPLESIZE, 0,
                                           &env );
  if( decision.scaler != SCALER_TRIPLESIZE ||
      decision.next.state != SDL2_SCALER_WINDOWED ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "reevaluate windowed: expected keep current scaler unchanged\n" );
    return 1;
  }

  return 0;
}

static int
reevaluate_native_with_fitting_scaler_keeps_state( void )
{
  unsigned char supported[ SCALER_NUM ];
  float scales[ SCALER_NUM ];
  sdl2_scaler_env env;
  sdl2_scaler_state state;
  sdl2_scaler_decision decision;

  fill_test_scales( scales );
  fill_supported( supported );
  fill_env( &env, supported, scales, 1920, 1080 );

  state.state = SDL2_SCALER_FULLSCREEN_NATIVE;
  state.restore_scaler = SCALER_NUM;

  decision = sdl2_scaler_state_reevaluate( &state, SCALER_DOUBLESIZE, 1,
                                           &env );
  if( decision.scaler != SCALER_DOUBLESIZE ||
      decision.next.state != SDL2_SCALER_FULLSCREEN_NATIVE ||
      decision.next.restore_scaler != SCALER_NUM ) {
    fprintf( stderr,
             "reevaluate native fits: expected keep 2x native unchanged\n" );
    return 1;
  }

  return 0;
}

typedef int (*test_fn_t)( void );

struct test_t {
  const char *name;
  test_fn_t fn;
};

static const struct test_t tests[] = {
  { "startup_fullscreen_auto_restores_on_exit",
    startup_fullscreen_auto_restores_on_exit },
  { "startup_fullscreen_keeps_requested_when_it_fits",
    startup_fullscreen_keeps_requested_when_it_fits },
  { "startup_windowed_returns_requested_as_windowed",
    startup_windowed_returns_requested_as_windowed },
  { "enter_fullscreen_prefers_same_family",
    enter_fullscreen_prefers_same_family },
  { "tv_family_includes_timextv", tv_family_includes_timextv },
  { "user_select_clears_auto_restore_state",
    user_select_clears_auto_restore_state },
  { "user_select_windowed_returns_requested_as_windowed",
    user_select_windowed_returns_requested_as_windowed },
  { "reevaluate_native_becomes_auto_when_current_no_longer_fits",
    reevaluate_native_becomes_auto_when_current_no_longer_fits },
  { "reevaluate_auto_preserves_supported_restore_scaler",
    reevaluate_auto_preserves_supported_restore_scaler },
  { "reevaluate_native_with_fitting_scaler_keeps_state",
    reevaluate_native_with_fitting_scaler_keeps_state },
  { "reevaluate_windowed_keeps_current",
    reevaluate_windowed_keeps_current },
  { "leave_fullscreen_restores_supported_equivalent_when_original_invalid",
    leave_fullscreen_restores_supported_equivalent_when_original_invalid },
  { "leave_fullscreen_from_native_state_keeps_current",
    leave_fullscreen_from_native_state_keeps_current },
  { "leave_fullscreen_auto_with_no_restore_keeps_current",
    leave_fullscreen_auto_with_no_restore_keeps_current },
  { "transition_dispatches_startup_branch",
    transition_dispatches_startup_branch },
  { "transition_dispatches_enter_branch",
    transition_dispatches_enter_branch },
  { "transition_dispatches_leave_branch",
    transition_dispatches_leave_branch },
  { "transition_dispatches_explicit_selection_branch",
    transition_dispatches_explicit_selection_branch },
  { "transition_dispatches_reevaluate_branch",
    transition_dispatches_reevaluate_branch },
  { "fullscreen_unknown_display_keeps_current_scaler",
    fullscreen_unknown_display_keeps_current_scaler },
  { NULL, NULL }
};

#ifdef main
/* SDL headers redefine main on Windows, but this test needs a normal entry point. */
#undef main
#endif

int
main( void )
{
  const struct test_t *test;

  for( test = tests; test->fn; test++ ) {
    if( test->fn() ) {
      fprintf( stderr, "Test failed: %s\n", test->name );
      return 1;
    }
  }

  return 0;
}
