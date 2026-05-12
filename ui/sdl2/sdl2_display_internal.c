/* sdl2_display_internal.c: SDL2 presentation helpers
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#include "config.h"

#include "ui/scaler/scaler.h"

#include "sdl2_display_internal.h"

scaler_type
sdl2display_choose_fullscreen_scaler( scaler_type current,
                                      float current_scale,
                                      int image_width,
                                      int image_height,
                                      int display_width,
                                      int display_height,
                                      const unsigned char *supported,
                                      const float *scales,
                                      int scaler_count,
                                      int *preserve_windowed )
{
  scaler_type i;
  scaler_type best = current;
  float best_scale = 0.0f;
  int found = 0;

  *preserve_windowed = 0;

  if( !display_width || !display_height ) return current;

  if( current < scaler_count && supported[current] &&
      image_width * current_scale <= display_width &&
      image_height * current_scale <= display_height )
    return current;

  for( i = 0; i < scaler_count; i++ ) {
    if( !supported[i] ) continue;

    if( image_width * scales[i] <= display_width &&
        image_height * scales[i] <= display_height &&
        scales[i] > best_scale ) {
      best = i;
      best_scale = scales[i];
      found = 1;
    }
  }

  if( !found || ( best == current && best_scale == current_scale ) )
    return current;

  *preserve_windowed = 1;

  return best;
}
