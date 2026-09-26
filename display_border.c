/* display_border.c: Spectrum border display handling
   Copyright (c) 1999-2026 Philip Kendall, Thomas Harte, Witold Filipczyk
                           and Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.
*/

#include "config.h"

#include <string.h>

#include "display.h"
#include "display_internal.h"
#include "peripherals/scld.h"
#include "ui/uidisplay.h"

/* The current border colour */
libspectrum_byte display_lores_border;
libspectrum_byte display_hires_border;
libspectrum_byte display_last_border;

/* The current border colour */
int current_border[ DISPLAY_SCREEN_HEIGHT ][ DISPLAY_SCREEN_WIDTH_COLS ];

/* The border colour changes which have occurred in this frame */
struct border_change_t {
  int x, y;
  int colour;
};

static struct border_change_t border_change_end_sentinel =
{ DISPLAY_SCREEN_WIDTH_COLS, DISPLAY_SCREEN_HEIGHT - 1, 0 };

static int border_changes_last;
static struct border_change_t *border_changes;

static struct border_change_t *
alloc_change( void )
{
  static int border_changes_size;

  if( border_changes_size == border_changes_last ) {
    border_changes_size += 10;
    border_changes = libspectrum_renew( struct border_change_t,
                                        border_changes, border_changes_size );
  }
  return border_changes + border_changes_last++;
}

static int
add_border_sentinel( void )
{
  struct border_change_t *sentinel = alloc_change();

  sentinel->x = sentinel->y = 0;
  sentinel->colour = scld_last_dec.name.hires ?
                     display_hires_border : display_lores_border;

  return 0;
}

int
display_border_init( void )
{
  int error;

  border_changes_last = 0;
  if( border_changes ) libspectrum_free( border_changes );
  border_changes = NULL;

  error = add_border_sentinel();
  if( error ) return error;

  display_last_border = scld_last_dec.name.hires ?
                        display_hires_border : display_lores_border;

  return 0;
}

static void
push_border_change( int colour )
{
  int beam_x, beam_y;
  struct border_change_t *change;

  display_get_beam_position( &beam_x, &beam_y );

  if( beam_y >= DISPLAY_SCREEN_HEIGHT ) return;

  if( beam_x < 0 ) beam_x = 0;
  if( beam_x > DISPLAY_SCREEN_WIDTH_COLS ) beam_x = DISPLAY_SCREEN_WIDTH_COLS;
  if( beam_y < 0 ) beam_y = 0;

  change = alloc_change();
  change->x = beam_x;
  change->y = beam_y;
  change->colour = colour;
}

/* Change border colour if the colour in use changes */
static void
check_border_change( void )
{
  if( scld_last_dec.name.hires &&
      display_hires_border != display_last_border ) {
    push_border_change( display_hires_border );
    display_last_border = display_hires_border;
  } else if( !scld_last_dec.name.hires &&
             display_lores_border != display_last_border ) {
    push_border_change( display_lores_border );
    display_last_border = display_lores_border;
  }
}

void
display_set_lores_border( int colour )
{
  if( display_lores_border != colour ) display_lores_border = colour;
  check_border_change();
}

void
display_set_hires_border( int colour )
{
  if( display_hires_border != colour ) display_hires_border = colour;
  check_border_change();
}

static void
set_border( int y, int start, int end, int colour )
{
  libspectrum_dword chunk_detail = colour << 11;
  int index = start + y * DISPLAY_SCREEN_WIDTH_COLS;

  for( ; start < end; start++ ) {
    /* Draw it if it is different to what was there last time. */
    if( display_last_screen[ index ] != chunk_detail ) {
      uidisplay_plot8( start, y, 0x00, 0, colour );
      display_last_screen[ index ] = chunk_detail;
      display_mark_screen_dirty( start, y );
    }
    index++;
  }
}

static void
border_change_write( int y, int start, int end, int colour )
{
  if( y < DISPLAY_BORDER_HEIGHT ||
      y >= DISPLAY_BORDER_HEIGHT + DISPLAY_HEIGHT ) {
    set_border( y, start, end, colour );
    return;
  }

  if( start < DISPLAY_BORDER_WIDTH_COLS ) {
    int left_end =
      end > DISPLAY_BORDER_WIDTH_COLS ? DISPLAY_BORDER_WIDTH_COLS : end;
    set_border( y, start, left_end, colour );
  }

  if( end > DISPLAY_BORDER_WIDTH_COLS + DISPLAY_WIDTH_COLS ) {
    if( start < DISPLAY_BORDER_WIDTH_COLS + DISPLAY_WIDTH_COLS )
      start = DISPLAY_BORDER_WIDTH_COLS + DISPLAY_WIDTH_COLS;
    set_border( y, start, end, colour );
  }
}

static void
border_change_line_part( int y, int start, int end, int colour )
{
  border_change_write( y, start, end, colour );
}

static void
border_change_line( int y, int colour )
{
  border_change_write( y, 0, DISPLAY_SCREEN_WIDTH_COLS, colour );
}

static void
do_border_change( struct border_change_t *first,
                  struct border_change_t *second )
{
  if( first->x ) {
    if( first->x != DISPLAY_SCREEN_WIDTH_COLS )
      border_change_line_part( first->y, first->x, DISPLAY_SCREEN_WIDTH_COLS,
                               first->colour );
    if( first->y < DISPLAY_SCREEN_HEIGHT - 1 ) first->y++;
  }

  for( ; first->y < second->y; first->y++ )
    border_change_line( first->y, first->colour );

  if( second->x ) {
    if( second->x == DISPLAY_SCREEN_WIDTH_COLS )
      border_change_line( first->y, first->colour );
    else
      border_change_line_part( first->y, 0, second->x, first->colour );
  }
}

void
display_border_frame( void )
{
  int pos;
  int error;
  struct border_change_t *end_sentinel = alloc_change();

  memcpy( end_sentinel, &border_change_end_sentinel,
          sizeof( struct border_change_t ) );

  for( pos = 0; pos < border_changes_last - 1; pos++ )
    do_border_change( border_changes + pos, border_changes + pos + 1 );

  border_changes_last = 0;
  error = add_border_sentinel();
  if( error ) return;
}
