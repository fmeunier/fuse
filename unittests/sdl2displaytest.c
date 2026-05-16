/* sdl2displaytest.c: Tests for SDL2 display helpers
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#include <config.h>

#include <stdio.h>

#include "ui/sdl2/sdl2_display_internal.h"

static int
scale_rect_applies_offset( void )
{
  sdl2_display_rect src, dst;

  src.x = 10;
  src.y = 20;
  src.w = 30;
  src.h = 40;

  sdl2display_scale_rect( &src, 2.0f, 100, 50, &dst );

  if( dst.x != 120 || dst.y != 90 || dst.w != 60 || dst.h != 80 ) {
    fprintf( stderr, "scaled rect: expected 120,90 60x80 got %d,%d %dx%d\n",
             dst.x, dst.y, dst.w, dst.h );
    return 1;
  }

  return 0;
}

static int
icon_rect_matches_scaled_rect( void )
{
  sdl2_display_rect dst;

  sdl2display_icon_rect( 243, 218, 8, 6, 2.0f, 100, 50, &dst );

  if( dst.x != 586 || dst.y != 486 || dst.w != 16 || dst.h != 12 ) {
    fprintf( stderr, "icon rect: expected 586,486 16x12 got %d,%d %dx%d\n",
             dst.x, dst.y, dst.w, dst.h );
    return 1;
  }

  return 0;
}

static int
update_rect_windowed( void )
{
  sdl2_display_rect dst;

  sdl2display_update_rect( 12, 34, 56, 78, 2.0f, 0, 0, &dst );

  if( dst.x != 24 || dst.y != 68 || dst.w != 112 || dst.h != 156 ) {
    fprintf( stderr,
             "update rect windowed: expected 24,68 112x156 got %d,%d %dx%d\n",
             dst.x, dst.y, dst.w, dst.h );
    return 1;
  }

  return 0;
}

static int
update_rect_fullscreen_offset( void )
{
  sdl2_display_rect dst;

  sdl2display_update_rect( 12, 34, 56, 78, 2.0f, 100, 50, &dst );

  if( dst.x != 124 || dst.y != 118 || dst.w != 112 || dst.h != 156 ) {
    fprintf( stderr,
             "update rect fullscreen: expected 124,118 112x156 got %d,%d %dx%d\n",
             dst.x, dst.y, dst.w, dst.h );
    return 1;
  }

  return 0;
}

static int
surface_view_initialises_fields( void )
{
  sdl2_surface_view view;
  int pixels;

  sdl2display_surface_view_init( &view, &pixels, 320, 240, 640, 2 );

  if( view.pixels != &pixels || view.width != 320 || view.height != 240 ||
      view.pitch != 640 || view.bytes_per_pixel != 2 ) {
    fprintf( stderr, "surface view init: unexpected field values\n" );
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
  { "scale_rect_applies_offset", scale_rect_applies_offset },
  { "icon_rect_matches_scaled_rect", icon_rect_matches_scaled_rect },
  { "update_rect_windowed", update_rect_windowed },
  { "update_rect_fullscreen_offset", update_rect_fullscreen_offset },
  { "surface_view_initialises_fields", surface_view_initialises_fields },
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
