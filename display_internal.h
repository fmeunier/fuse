/* display_internal.h: Internal interfaces shared by display modules
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef FUSE_DISPLAY_INTERNAL_H
#define FUSE_DISPLAY_INTERNAL_H

#include "libspectrum.h"

void display_dirty_init( void );
void display_dirty_frame_begin( void );
void display_dirty_frame_end( void );
void display_dirty_refresh_all( void );
void display_get_beam_position( int *x, int *y );
void display_mark_screen_dirty( int x, int y );
void display_dirty8( libspectrum_word address );
void display_dirty64( libspectrum_word address );

void display_render_init( void );
void display_render_frame( void );

int display_border_init( void );
void display_border_frame( void );

#endif /* #ifndef FUSE_DISPLAY_INTERNAL_H */
