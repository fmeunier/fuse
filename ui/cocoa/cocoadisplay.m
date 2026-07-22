/* cocoadisplay.m: Routines for dealing with the Cocoa display
   Copyright (c) 2006-2007 Fredrick Meunier

   $Id$

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <libspectrum.h>

#import "EmulationSessionController.h"

#include "cocoadisplay.h"
#include "dirty.h"
#include "display.h"
#include "fuse.h"
#include "machine.h"
#include "scld.h"
#include "screenshot.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/scaler/scaler.h"
#include "ui/uidisplay.h"
#include "utils.h"

/* The current size of the display (in units of DISPLAY_SCREEN_*) */
static float display_current_size = 1.0f;

static int image_width;
static int image_height;

/* Screen texture */
DisplayFramebuffer* screen = NULL;

/* Screen texture in native size */
DisplayFramebuffer unscaled_screen;

/* Screen texture after scaling (only if a transforming scaler is in place) */
DisplayFramebuffer scaled_screen;

/* Screen texture second buffer */
DisplayFramebuffer buffered_screen;
DisplayFramebufferRing buffered_screen_ring;

static unsigned long framebuffer_generation = 0;

#define COLOUR_COUNT 16

static const struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} colour_palette[ COLOUR_COUNT ] = {
  {   0,   0,   0 },
  {   0,   0, 192 },
  { 192,   0,   0 },
  { 192,   0, 192 },
  {   0, 192,   0 },
  {   0, 192, 192 },
  { 192, 192,   0 },
  { 192, 192, 192 },
  {   0,   0,   0 },
  {   0,   0, 255 },
  { 255,   0,   0 },
  { 255,   0, 255 },
  {   0, 255,   0 },
  {   0, 255, 255 },
  { 255, 255,   0 },
  { 255, 255, 255 }
};

static uint16_t colour_values[ COLOUR_COUNT ];
static uint16_t bw_values[ COLOUR_COUNT ];

static int display_updated = 0;

/* This is a rule of thumb for the maximum number of rects that can be updated
   each frame. */
#define MAX_UPDATE_RECT 300

static void
init_scalers( void )
{
  scaler_register_clear();

  scaler_register( SCALER_NORMAL );
  if( machine_current->timex ) {
    scaler_register( SCALER_TIMEXTV );
  } else {
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_TV3X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_PALTV3X );
    scaler_register( SCALER_2XSAI );
    scaler_register( SCALER_SUPER2XSAI );
    scaler_register( SCALER_SUPEREAGLE );
    scaler_register( SCALER_ADVMAME2X );
    scaler_register( SCALER_ADVMAME3X );
    scaler_register( SCALER_DOTMATRIX );
    scaler_register( SCALER_HQ2X );
    scaler_register( SCALER_HQ3X );
    scaler_register( SCALER_NTSC2X );
    scaler_register( SCALER_NTSC3X );
  }
  
  if( scaler_is_supported( current_scaler ) ) {
    scaler_select_scaler( current_scaler );
  } else {
    scaler_select_scaler( SCALER_NORMAL );
  }

  scaler_select_bitformat( 565 );
}

static int
allocate_screen( DisplayFramebuffer* new_screen, int height, int width,
                 float scaling_factor, int allocate_backing_storage )
{
  memset( new_screen, 0, sizeof( *new_screen ) );
  new_screen->pixel_format = DISPLAY_FRAMEBUFFER_PIXEL_FORMAT_RGB565;
  new_screen->width = width * scaling_factor;
  new_screen->height = height * scaling_factor;

  /* Need some extra bytes around when using 2xSaI */
  new_screen->storage_width = new_screen->width+3;
  new_screen->x_offset = 1;
  new_screen->storage_height = new_screen->height+3;
  new_screen->y_offset = 1;

  if( allocate_backing_storage ) {
    new_screen->backing_storage = calloc( new_screen->storage_width *
                                          new_screen->storage_height,
                                          sizeof( uint16_t ) );
    if( !new_screen->backing_storage ) {
      fprintf( stderr, "%s: couldn't allocate screen.backing_storage\n", fuse_progname );
      return 1;
    }

    new_screen->dirty_regions = pig_dirty_open( MAX_UPDATE_RECT );
    if( !new_screen->dirty_regions ) {
      free( new_screen->backing_storage );
      fprintf( stderr, "%s: couldn't allocate screen.dirty_regions\n", fuse_progname );
      return 1;
    }

    new_screen->ownership = DISPLAY_FRAMEBUFFER_OWNS_BACKING_STORAGE |
                            DISPLAY_FRAMEBUFFER_OWNS_DIRTY_REGIONS;
  }

  new_screen->stride = new_screen->storage_width * sizeof( uint16_t );
  new_screen->generation = ++framebuffer_generation;

  return 0;
}

static void
free_screen( DisplayFramebuffer* screen )
{
  if( screen->ownership & DISPLAY_FRAMEBUFFER_OWNS_BACKING_STORAGE ) {
    free( screen->backing_storage );
    screen->backing_storage = NULL;
  }
  if( screen->ownership & DISPLAY_FRAMEBUFFER_OWNS_DIRTY_REGIONS ) {
    pig_dirty_close( screen->dirty_regions );
    screen->dirty_regions = NULL;
  }
  screen->ownership = 0;
}

static int
cocoadisplay_load_gfx_mode( void )
{
  int error;

  display_current_size = scaler_get_scaling_factor( current_scaler );

  error = allocate_screen( &unscaled_screen, image_height, image_width, 1.0f, 1 );
  if( error ) return error;

  screen = &unscaled_screen;

  if( current_scaler != SCALER_NORMAL ) {
    error = allocate_screen( &scaled_screen, image_height, image_width,
                             display_current_size, 1 );
    if( error ) return error;

    screen = &scaled_screen;
  }

  error = allocate_screen( &buffered_screen, screen->height,
                           screen->width, 1.0f, 0 );
  if( error ) return error;

  [[EmulationSessionController instance]
    performSelectorOnMainThread:@selector(applyFramebufferWithValue:)
                     withObject:[NSValue valueWithPointer:&buffered_screen]
                  waitUntilDone:YES];

  for( error = 0; error < DISPLAY_FRAMEBUFFER_SLOT_COUNT; error++ ) {
    PIG_rect area = { 0, 0, screen->width, screen->height };

    if( buffered_screen_ring.slots[ error ] )
      pig_dirty_add( buffered_screen_ring.slots[ error ]->dirty_regions,
                     &area );
  }

  return 0;
}

#define RGB_TO_PIXEL_565( red, green, blue ) \
  ( ( ( (uint16_t)( red ) & 0xf8 ) << 8 ) | \
    ( ( (uint16_t)( green ) & 0xfc ) << 3 ) | \
    ( ( (uint16_t)( blue ) & 0xf8 ) >> 3 ) )

static void
cocoadisplay_allocate_colours( void )
{
  int i;

  for( i = 0; i < COLOUR_COUNT; i++ ) {
    uint8_t red = colour_palette[i].red;
    uint8_t green = colour_palette[i].green;
    uint8_t blue = colour_palette[i].blue;
    uint8_t grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = RGB_TO_PIXEL_565( red, green, blue );
    bw_values[i] = RGB_TO_PIXEL_565( grey, grey, grey );
  }
}

int
uidisplay_init( int width, int height )
{
  cocoadisplay_allocate_colours();

  image_width = width;
  image_height = height;

  init_scalers();

  if ( scaler_select_scaler( current_scaler ) )
    scaler_select_scaler( SCALER_NORMAL );

  cocoadisplay_load_gfx_mode();

  /* We can now output error messages to our output device */
  display_ui_initialised = 1;

  return 0;
}

int
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Free the old surfaces */
  free_screen( &unscaled_screen );
  free_screen( &scaled_screen );
  free_screen( &buffered_screen );

  /* Setup the new GFX mode */
  cocoadisplay_load_gfx_mode();

  fuse_emulation_unpause();
  
  return 0;
}

/* Set one pixel in the display */
void
uidisplay_putpixel( int x, int y, int colour )
{
  uint16_t *dest_base, *dest;
  uint16_t *palette_values = settings_current.bw_tv ? bw_values : colour_values;

  uint16_t palette_colour = palette_values[ colour ];

  if( machine_current->timex ) {
    x <<= 1; y <<= 1;
    dest_base = dest = (uint16_t*)( (uint8_t*)unscaled_screen.backing_storage +
                                    (x+unscaled_screen.x_offset) * sizeof(uint16_t) +
                                    (y+unscaled_screen.y_offset) * unscaled_screen.stride );

    *(dest++) = palette_colour;
    *(dest  ) = palette_colour;
    dest = (uint16_t*)( (uint8_t*)dest_base + unscaled_screen.stride );
    *(dest++) = palette_colour;
    *(dest  ) = palette_colour;
  } else {
    dest = (uint16_t*)( (uint8_t*)unscaled_screen.backing_storage +
                        (x+unscaled_screen.x_offset) * sizeof(uint16_t) +
                        (y+unscaled_screen.y_offset) * unscaled_screen.stride );

    *dest = palette_colour;
  }
}

/* Print the 8 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (8*x) , y ) */
void
uidisplay_plot8( int x, int y, libspectrum_byte data,
	         libspectrum_byte ink, libspectrum_byte paper )
{
  uint16_t *dest;
  uint16_t *palette_values = settings_current.bw_tv ? bw_values : colour_values;

  uint16_t palette_ink = palette_values[ ink ];
  uint16_t palette_paper = palette_values[ paper ];

  if( machine_current->timex ) {
    int i;
    uint16_t *dest_base;

    x <<= 4; y <<= 1;

    dest_base = (uint16_t*)( (uint8_t*)unscaled_screen.backing_storage +
                             (x+unscaled_screen.x_offset) * sizeof(uint16_t) +
                             (y+unscaled_screen.y_offset) * unscaled_screen.stride );

    for( i=0; i<2; i++ ) {
      dest = dest_base;

      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x01 ) ? palette_ink : palette_paper;
      *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;

      dest_base = (uint16_t*)( (uint8_t*)dest_base + unscaled_screen.stride );
    }
  } else {
    x <<= 3;
    dest = (uint16_t*)( (uint8_t*)unscaled_screen.backing_storage +
                        (x+unscaled_screen.x_offset) * sizeof(uint16_t) +
                        (y+unscaled_screen.y_offset) * unscaled_screen.stride );

    *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;
  }
}

/* Print the 16 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (16*x) , y ) */
void
uidisplay_plot16( int x, int y, libspectrum_word data,
		  libspectrum_byte ink, libspectrum_byte paper )
{
  uint16_t *dest_base, *dest;
  int i; 
  uint16_t *palette_values = settings_current.bw_tv ? bw_values : colour_values;
  uint16_t palette_ink = palette_values[ ink ];
  uint16_t palette_paper = palette_values[ paper ];
  x <<= 4; y <<= 1;

  dest_base = (uint16_t*)( (uint8_t*)unscaled_screen.backing_storage + (x+unscaled_screen.x_offset) *
                           sizeof(uint16_t) + (y+unscaled_screen.y_offset) *
                           unscaled_screen.stride );

  for( i=0; i<2; i++ ) {
    dest = dest_base;

    *(dest++) = ( data & 0x8000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x4000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x2000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x1000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0800 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0400 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0200 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0100 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0080 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0040 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0020 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0010 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0008 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0004 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0002 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x0001 ) ? palette_ink : palette_paper;

    dest_base = (uint16_t*)( (uint8_t*)dest_base + unscaled_screen.stride );
  }
}

void
copy_area( DisplayFramebuffer *dest_screen, DisplayFramebuffer *src_screen, PIG_rect *r )
{
  int y;

  for( y = r->y; y <= r->y + r->h; y++ ) {
    int src_offset = (y + src_screen->y_offset) * src_screen->stride +
                     sizeof(uint16_t) * ( r->x + src_screen->x_offset);
    int dest_offset = (y + dest_screen->y_offset) * dest_screen->stride +
                      sizeof(uint16_t) * ( r->x + dest_screen->x_offset);
    memcpy( dest_screen->backing_storage + dest_offset, src_screen->backing_storage + src_offset,
            r->w * sizeof(uint16_t) );
  }
}

void
uidisplay_frame_end( void )
{
  DisplayFramebuffer *presentation_framebuffer;
  int i;

  if( scaler_flags & SCALER_FLAGS_FULL_REFRESH ) {
    uidisplay_area( 0, 0, image_width, image_height );
  }

  if( display_updated ) {
    for( i = 0; i < DISPLAY_FRAMEBUFFER_SLOT_COUNT; i++ ) {
      if( buffered_screen_ring.slots[i] )
        pig_dirty_merge( buffered_screen_ring.slots[i]->dirty_regions,
                         screen->dirty_regions );
    }

    presentation_framebuffer = NULL;
    /* The slot state is protected by MetalDisplayView's lock, so this must
       not synchronously dispatch to the main thread. The main thread may be
       waiting for this emulation thread to handle a command. */
    [[EmulationSessionController instance]
      acquireFramebufferWithValue:[NSValue valueWithPointer:&presentation_framebuffer]];
    if( presentation_framebuffer ) {
      for( i = 0; i < presentation_framebuffer->dirty_regions->count; i++ )
        copy_area( presentation_framebuffer, screen,
                   presentation_framebuffer->dirty_regions->rects + i );
      presentation_framebuffer->dirty_regions->count = 0;
      [[EmulationSessionController instance]
        publishFramebufferWithValue:[NSValue valueWithPointer:presentation_framebuffer]];
    }

    display_updated = 0;
    unscaled_screen.dirty_regions->count = 0;
    if( current_scaler != SCALER_NORMAL ) scaled_screen.dirty_regions->count = 0;
  }
}

void
uidisplay_area( int x, int y, int width, int height )
{
  PIG_rect r = { x, y, width, height };

  display_updated = 1;

  if( current_scaler == SCALER_NORMAL ) {
    pig_dirty_add( unscaled_screen.dirty_regions, &r );
    return;
  }

  /* Extend the dirty region by 1 pixel for scalers that "smear" the screen,
     e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &width, &height, image_width, image_height );

  r.x = display_current_size * x;
  r.y = display_current_size * y;
  r.w = display_current_size * width;
  r.h = display_current_size * height;
  pig_dirty_add( scaled_screen.dirty_regions, &r );

  /* Create scaled image */
  scaler_proc16( unscaled_screen.backing_storage + ( y + unscaled_screen.y_offset ) *
                   unscaled_screen.stride + sizeof(uint16_t) *
                   ( x + unscaled_screen.x_offset ),
                 unscaled_screen.stride,
                 scaled_screen.backing_storage + ( r.y + scaled_screen.y_offset ) *
                   scaled_screen.stride + sizeof(uint16_t) *
                   ( r.x + scaled_screen.x_offset ),
                 scaled_screen.stride, width, height );
}

int
uidisplay_end( void )
{
  if( screen && screen->backing_storage ) {
    [[EmulationSessionController instance]
      performSelectorOnMainThread:@selector(removeFramebuffer)
                       withObject:nil
                    waitUntilDone:YES];
  }

  free_screen( &unscaled_screen );
  free_screen( &scaled_screen );
  free_screen( &buffered_screen );
  memset( &buffered_screen_ring, 0, sizeof( buffered_screen_ring ) );

  return 0;
}
