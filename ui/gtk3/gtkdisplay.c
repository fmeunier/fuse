/* gtkdisplay.c: GTK routines for dealing with the Speccy screen
   Copyright (c) 2000-2018 Philip Kendall
   Copyright (c) 2026 Alberto Garcia
   Copyright (c) 2026 Fredrick Meunier

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

#include "config.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "display.h"
#include "fuse.h"
#include "gtkinternals.h"
#include "screenshot.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "ui/scaler/scaler.h"
#include "settings.h"

/* The biggest size screen (in units of DISPLAY_ASPECT_WIDTH x
   DISPLAY_SCREEN_HEIGHT ie a Timex screen is size 2) we will be
   creating via the scalers */
#define MAX_SCALE 4

/* The height and width of a 1x1 image in pixels */
int image_width, image_height;

/* A copy of every pixel on the screen, replaceable by plotting directly into
   rgb_image below */
libspectrum_word
  gtkdisplay_image[ 2 * DISPLAY_SCREEN_HEIGHT ][ DISPLAY_SCREEN_WIDTH ];
ptrdiff_t gtkdisplay_pitch = DISPLAY_SCREEN_WIDTH * sizeof( libspectrum_word );

/* An RGB image of the Spectrum screen; slightly bigger than the real
   screen to handle the smoothing filters which read around each pixel */
static guchar rgb_image[ 4 * 2 * ( DISPLAY_SCREEN_HEIGHT + 4 ) *
                                 ( DISPLAY_SCREEN_WIDTH  + 3 )   ];
static const gint rgb_pitch = ( DISPLAY_SCREEN_WIDTH + 3 ) * 4;

/* The scaled image */
static guchar scaled_image[ MAX_SCALE * DISPLAY_SCREEN_HEIGHT *
                            MAX_SCALE * DISPLAY_SCREEN_WIDTH * 2 ];
static const ptrdiff_t scaled_pitch = MAX_SCALE * DISPLAY_SCREEN_WIDTH * 2;

/* The colour palette */
static const guchar rgb_colours[16][3] = {

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
  { 255, 255, 255 },

};

/* And the colours (and black and white 'colours') in 32-bit format */
libspectrum_dword gtkdisplay_colours[16];
static libspectrum_dword bw_colours[16];

/* Colour format for the back buffer in endianess-order */
typedef enum {
  FORMAT_x8r8g8b8,    /* Cairo  (GTK3) */
  FORMAT_x8b8g8r8     /* GdkRGB */
} colour_format_t;

static cairo_surface_t *surface = NULL;

/* The scaler the current cairo surface was created for */
static scaler_type surface_scaler = SCALER_NUM;

/* The size the window has been resized to (in units of DISPLAY_SCREEN_*),
   used to choose the scaler. The cairo surface is sized to the active
   scaler instead, which may be smaller or larger than this when the
   scaler's family has no variant for this size (see scaler_family_table). */
static int gtkdisplay_surface_size=1;

/* Extra height used for menu and status bars */
static int extra_height = 0;

/* If the user resizes the window, switch the scaler only after this
   period of inactivity (i.e. without configure events). */
#define RESIZE_TIMEOUT_MS 250
static guint resize_timeout_id = 0;
static gint64 resize_last_activity = 0;
static int pending_width, pending_height;

static int init_colours( colour_format_t format );
static void gtkdisplay_area(int x, int y, int width, int height);
static void register_scalers( int force_scaler );
static void gtkdisplay_load_gfx_mode( void );
static void cancel_pending_resize( void );

/* Callbacks */

static gboolean gtkdisplay_draw( GtkWidget *widget, cairo_t *cr,
                                 gpointer user_data );

static gint drawing_area_resize_callback( GtkWidget *widget, GdkEvent *event,
                                          gpointer data );

static int
init_colours( colour_format_t format )
{
  size_t i;

  for( i = 0; i < 16; i++ ) {


    guchar red, green, blue, grey;

    red   = rgb_colours[i][0];
    green = rgb_colours[i][1];
    blue  = rgb_colours[i][2];

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

#ifdef WORDS_BIGENDIAN

    switch( format ) {
    case FORMAT_x8b8g8r8:
      gtkdisplay_colours[i] =  red << 24 | green << 16 | blue << 8;
      break;
    case FORMAT_x8r8g8b8:
      gtkdisplay_colours[i] = blue << 24 | green << 16 |  red << 8;
      break;
    }

              bw_colours[i] = grey << 24 |  grey << 16 | grey << 8;

#else                           /* #ifdef WORDS_BIGENDIAN */

    switch( format ) {
    case FORMAT_x8b8g8r8:
      gtkdisplay_colours[i] =  red | green << 8 | blue << 16;
      break;
    case FORMAT_x8r8g8b8:
      gtkdisplay_colours[i] = blue | green << 8 |  red << 16;
      break;
    }

              bw_colours[i] = grey |  grey << 8 | grey << 16;

#endif                          /* #ifdef WORDS_BIGENDIAN */

  }

  return 0;
}

int
uidisplay_init( int width, int height )
{
  int x, y, error;
  libspectrum_dword black;
  const char *machine_name;
  colour_format_t colour_format;

  g_signal_connect( G_OBJECT( gtkui_drawing_area ), "draw",
                    G_CALLBACK( gtkdisplay_draw ), NULL );

  colour_format = FORMAT_x8r8g8b8;

  g_signal_connect( G_OBJECT( gtkui_window ), "configure_event",
                    G_CALLBACK( drawing_area_resize_callback ), NULL );

  error = init_colours( colour_format ); if( error ) return error;
  error = scaler_select_bitformat( BITFORMAT_X8R8G8B8 );
  if( error ) return error;

  black = settings_current.bw_tv ? bw_colours[0] : gtkdisplay_colours[0];

  for( y = 0; y < DISPLAY_SCREEN_HEIGHT + 4; y++ )
    for( x = 0; x < DISPLAY_SCREEN_WIDTH + 3; x++ )
      *(libspectrum_dword*)( rgb_image + y * rgb_pitch + 4 * x ) = black;

  image_width = width; image_height = height;

  register_scalers( 0 );

  display_refresh_all();

  if ( scaler_select_scaler( current_scaler ) )
        scaler_select_scaler( SCALER_NORMAL );

  gtkdisplay_load_gfx_mode();

  machine_name = libspectrum_machine_name( machine_current->machine );
  gtkstatusbar_update_machine( machine_name );

  display_ui_initialised = 1;

  return 0;
}

/* Ensure that an appropriate Cairo surface exists */
static void
ensure_appropriate_surface( void )
{
  /* Recreate the cairo surface to match the active scaler */
  float scale = scaler_get_scaling_factor( current_scaler );
  if( surface ) cairo_surface_destroy( surface );

  surface =
      cairo_image_surface_create_for_data( scaled_image,
                                           CAIRO_FORMAT_RGB24,
                                           scale * image_width,
                                           scale * image_height,
                                           scaled_pitch );

  surface_scaler = current_scaler;
}

static int
drawing_area_resize( int width, int height, int force_scaler )
{
  int size;

  size = width / DISPLAY_ASPECT_WIDTH;
  if( size > height / DISPLAY_SCREEN_HEIGHT )
    size = height / DISPLAY_SCREEN_HEIGHT;

  if( size > MAX_SCALE ) size = MAX_SCALE;
  if( size < 1 ) size = 1;

  /* If we're the same size and scaler as before, no need to do anything else */
  if( size == gtkdisplay_surface_size && current_scaler == surface_scaler )
    return 0;

  gtkdisplay_surface_size = size;

  register_scalers( force_scaler );

  memset( scaled_image, 0, sizeof( scaled_image ) );

  ensure_appropriate_surface();

  display_refresh_all();

  return 0;
}

static void
register_scalers( int force_scaler )
{
  scaler_type scaler;

  scaler_register_clear();

  if( machine_current->timex ) {
    scaler_register( SCALER_HALF );
    scaler_register( SCALER_HALFSKIP );
    scaler_register( SCALER_TIMEXTV );
    scaler_register( SCALER_TIMEX1_5X );
    scaler_register( SCALER_TIMEX2X );
  } else {
    scaler_register( SCALER_DOUBLESIZE );
    scaler_register( SCALER_TRIPLESIZE );
    scaler_register( SCALER_QUADSIZE );
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_TV3X );
    scaler_register( SCALER_TV4X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_PALTV3X );
    scaler_register( SCALER_PALTV4X );
    scaler_register( SCALER_HQ2X );
    scaler_register( SCALER_HQ3X );
    scaler_register( SCALER_HQ4X );
    scaler_register( SCALER_ADVMAME2X );
    scaler_register( SCALER_ADVMAME3X );
    scaler_register( SCALER_2XSAI );
    scaler_register( SCALER_SUPER2XSAI );
    scaler_register( SCALER_SUPEREAGLE );
    scaler_register( SCALER_DOTMATRIX );
    scaler_register( SCALER_NTSC2X );
    scaler_register( SCALER_NTSC3X );
    scaler_register( SCALER_NTSC4X );
  }
  scaler_register( SCALER_NORMAL );

  scaler =
    scaler_is_supported( current_scaler ) ? current_scaler : SCALER_NORMAL;

  /* When the window is resized switch to the scaler of the same family
     (e.g. PAL TV, HQ) that fits the new size */
  if( force_scaler )
    scaler = scaler_for_size( scaler, gtkdisplay_surface_size );

  /* Activate the scaler without trying to resize the GTK window */
  scaler_activate_scaler( scaler );
}

void
uidisplay_frame_end( void )
{
  if( scaler_flags & SCALER_FLAGS_FULL_REFRESH ) {
    uidisplay_area( 0, 0, image_width, image_height );
  }

  /* If the user changed the full screen option, apply it now */
  gtkui_fullscreen_apply();
}

void
uidisplay_area( int x, int y, int w, int h )
{
  float scale = scaler_get_scaling_factor( current_scaler );
  int scaled_x, scaled_y, i, yy;
  libspectrum_dword *palette;

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &w, &h, image_width, image_height );

  scaled_x = scale * x; scaled_y = scale * y;

  palette = settings_current.bw_tv ? bw_colours : gtkdisplay_colours;

  /* Create the RGB image */
  for( yy = y; yy < y + h; yy++ ) {

    libspectrum_dword *rgb; libspectrum_word *display;

    rgb = (libspectrum_dword*)( rgb_image + ( yy + 2 ) * rgb_pitch );
    rgb += x + 1;

    display = &gtkdisplay_image[yy][x];

    for( i = 0; i < w; i++, rgb++, display++ ) *rgb = palette[ *display ];
  }

  /* Create scaled image */
  scaler_proc32( &rgb_image[ ( y + 2 ) * rgb_pitch + 4 * ( x + 1 ) ],
                 rgb_pitch,
                 &scaled_image[ scaled_y * scaled_pitch + 4 * scaled_x ],
                 scaled_pitch, w, h );

  w *= scale; h *= scale;

  /* Blit to the real screen */
  gtkdisplay_area( scaled_x, scaled_y, w, h );
}

/* Map the cairo surface onto the drawing area.
   'scale' is set so the contents fit the drawing area.
   'offset_x' and 'offset_y' are set so the contents are centred. */
static void
get_surface_placement( double *scale, int *offset_x, int *offset_y )
{
  int surface_width, surface_height, avail_width, avail_height, origin_y;
  double scale_x, scale_y, s;
  GtkAllocation alloc;

  surface_width = cairo_image_surface_get_width( surface );
  surface_height = cairo_image_surface_get_height( surface );

  /* By default fit the image into the drawing area */
  gtk_widget_get_allocation( gtkui_drawing_area, &alloc );
  avail_width = alloc.width;
  avail_height = alloc.height;
  origin_y = 0;

  /* In fullscreen fit the image into the whole window instead.
     If the menu and status bars are hidden then we see the whole content.
     If they are visible then those bars cover the edges of the image. */
  if( settings_current.full_screen ) {
    avail_height = gtk_widget_get_allocated_height( gtkui_window );
    origin_y = -alloc.y;
  }

  scale_x = (double)avail_width  / surface_width;
  scale_y = (double)avail_height / surface_height;
  s = scale_x < scale_y ? scale_x : scale_y;

  *offset_x =            ( avail_width  - (int)( surface_width  * s ) ) / 2;
  *offset_y = origin_y + ( avail_height - (int)( surface_height * s ) ) / 2;
  *scale = s;
}

static void gtkdisplay_area(int x, int y, int width, int height)
{
  int max_width, max_height;
  int offset_x, offset_y;
  int wx, wy, ww, wh;
  double scale;

  if( width <= 0 || height <= 0 ) return;

  if( !surface ) {
    gtk_widget_queue_draw_area( gtkui_drawing_area, x, y, width, height );
    return;
  }

  max_width = cairo_image_surface_get_width( surface );
  max_height = cairo_image_surface_get_height( surface );

  /* Expand the invalidated area slightly to avoid thin seams on scaled GTK
     redraws where Cairo clips right on a dirty-rect edge. */
  if( x > 0 ) { x--; width++; }
  if( y > 0 ) { y--; height++; }
  if( x + width < max_width ) width++;
  if( y + height < max_height ) height++;

  /* Map the surface onto the drawing area */
  get_surface_placement( &scale, &offset_x, &offset_y );

  /* Adjust the values according to the scale factor */
  wx = (int)( x * scale ) + offset_x;
  wy = (int)( y * scale ) + offset_y;
  ww = (int)( ceil( ( x + width  ) * scale ) ) - (int)( x * scale );
  wh = (int)( ceil( ( y + height ) * scale ) ) - (int)( y * scale );

  gtk_widget_queue_draw_area( gtkui_drawing_area, wx, wy, ww, wh );
}

int
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Setup the new GFX mode */
  gtkdisplay_load_gfx_mode();

  fuse_emulation_unpause();

  return 0;
}

int
uidisplay_end( void )
{
  cancel_pending_resize();

  return 0;
}

/* Set one pixel in the display */
void
uidisplay_putpixel( int x, int y, int colour )
{
  if( machine_current->timex ) {
    x <<= 1; y <<= 1;
    gtkdisplay_image[y  ][x  ] = colour;
    gtkdisplay_image[y  ][x+1] = colour;
    gtkdisplay_image[y+1][x  ] = colour;
    gtkdisplay_image[y+1][x+1] = colour;
  } else {
    gtkdisplay_image[y][x] = colour;
  }
}

/* Print the 8 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (8*x) , y ) */
void
uidisplay_plot8( int x, int y, libspectrum_byte data,
                 libspectrum_byte ink, libspectrum_byte paper )
{
  x <<= 3;

  if( machine_current->timex ) {
    int i;

    x <<= 1; y <<= 1;
    for( i=0; i<2; i++,y++ ) {
      gtkdisplay_image[y][x+ 0] = ( data & 0x80 ) ? ink : paper;
      gtkdisplay_image[y][x+ 1] = ( data & 0x80 ) ? ink : paper;
      gtkdisplay_image[y][x+ 2] = ( data & 0x40 ) ? ink : paper;
      gtkdisplay_image[y][x+ 3] = ( data & 0x40 ) ? ink : paper;
      gtkdisplay_image[y][x+ 4] = ( data & 0x20 ) ? ink : paper;
      gtkdisplay_image[y][x+ 5] = ( data & 0x20 ) ? ink : paper;
      gtkdisplay_image[y][x+ 6] = ( data & 0x10 ) ? ink : paper;
      gtkdisplay_image[y][x+ 7] = ( data & 0x10 ) ? ink : paper;
      gtkdisplay_image[y][x+ 8] = ( data & 0x08 ) ? ink : paper;
      gtkdisplay_image[y][x+ 9] = ( data & 0x08 ) ? ink : paper;
      gtkdisplay_image[y][x+10] = ( data & 0x04 ) ? ink : paper;
      gtkdisplay_image[y][x+11] = ( data & 0x04 ) ? ink : paper;
      gtkdisplay_image[y][x+12] = ( data & 0x02 ) ? ink : paper;
      gtkdisplay_image[y][x+13] = ( data & 0x02 ) ? ink : paper;
      gtkdisplay_image[y][x+14] = ( data & 0x01 ) ? ink : paper;
      gtkdisplay_image[y][x+15] = ( data & 0x01 ) ? ink : paper;
    }
  } else {
    gtkdisplay_image[y][x+ 0] = ( data & 0x80 ) ? ink : paper;
    gtkdisplay_image[y][x+ 1] = ( data & 0x40 ) ? ink : paper;
    gtkdisplay_image[y][x+ 2] = ( data & 0x20 ) ? ink : paper;
    gtkdisplay_image[y][x+ 3] = ( data & 0x10 ) ? ink : paper;
    gtkdisplay_image[y][x+ 4] = ( data & 0x08 ) ? ink : paper;
    gtkdisplay_image[y][x+ 5] = ( data & 0x04 ) ? ink : paper;
    gtkdisplay_image[y][x+ 6] = ( data & 0x02 ) ? ink : paper;
    gtkdisplay_image[y][x+ 7] = ( data & 0x01 ) ? ink : paper;
  }
}

/* Print the 16 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (16*x) , y ) */
void
uidisplay_plot16( int x, int y, libspectrum_word data,
                 libspectrum_byte ink, libspectrum_byte paper )
{
  int i;
  x <<= 4; y <<= 1;

  for( i=0; i<2; i++,y++ ) {
    gtkdisplay_image[y][x+ 0] = ( data & 0x8000 ) ? ink : paper;
    gtkdisplay_image[y][x+ 1] = ( data & 0x4000 ) ? ink : paper;
    gtkdisplay_image[y][x+ 2] = ( data & 0x2000 ) ? ink : paper;
    gtkdisplay_image[y][x+ 3] = ( data & 0x1000 ) ? ink : paper;
    gtkdisplay_image[y][x+ 4] = ( data & 0x0800 ) ? ink : paper;
    gtkdisplay_image[y][x+ 5] = ( data & 0x0400 ) ? ink : paper;
    gtkdisplay_image[y][x+ 6] = ( data & 0x0200 ) ? ink : paper;
    gtkdisplay_image[y][x+ 7] = ( data & 0x0100 ) ? ink : paper;
    gtkdisplay_image[y][x+ 8] = ( data & 0x0080 ) ? ink : paper;
    gtkdisplay_image[y][x+ 9] = ( data & 0x0040 ) ? ink : paper;
    gtkdisplay_image[y][x+10] = ( data & 0x0020 ) ? ink : paper;
    gtkdisplay_image[y][x+11] = ( data & 0x0010 ) ? ink : paper;
    gtkdisplay_image[y][x+12] = ( data & 0x0008 ) ? ink : paper;
    gtkdisplay_image[y][x+13] = ( data & 0x0004 ) ? ink : paper;
    gtkdisplay_image[y][x+14] = ( data & 0x0002 ) ? ink : paper;
    gtkdisplay_image[y][x+15] = ( data & 0x0001 ) ? ink : paper;
  }
}

/* Callbacks */

/* Called by gtkui_drawing_area on "draw" event */
static gboolean
gtkdisplay_draw( GtkWidget *widget GCC_UNUSED, cairo_t *cr,
                 gpointer user_data )
{
  int offset_x, offset_y;
  double scale;

  /* Create a new surface for this gfx mode */
  if( !surface ) ensure_appropriate_surface();

  /* Map the surface onto the drawing area */
  get_surface_placement( &scale, &offset_x, &offset_y );

  /* Fill the drawing area with black. This clears the margins around
     the source if the drawing area is larger */
  cairo_set_source_rgb( cr, 0, 0, 0 );
  cairo_paint( cr );

  /* Repaint the surface on top */
  cairo_translate( cr, offset_x, offset_y );
  cairo_scale( cr, scale, scale );
  cairo_set_source_surface( cr, surface, 0, 0 );

  /* Paint the image over its own area and not the whole drawing area.
     Otherwise it can bleed into the surrounding margins when the
     scaling factor is not an integer, leaving a stale thin border. */
  cairo_rectangle( cr, 0, 0,
                   cairo_image_surface_get_width( surface ),
                   cairo_image_surface_get_height( surface ) );
  cairo_fill( cr );

  return FALSE;
}

static void
cancel_pending_resize( void )
{
  if( resize_timeout_id ) {
    g_source_remove( resize_timeout_id );
    resize_timeout_id = 0;
  }
}

/* Resize the drawing area when the user has finished resizing the window */
static gboolean
drawing_area_resize_timeout( gpointer data GCC_UNUSED )
{
  gint64 timeout = (gint64)RESIZE_TIMEOUT_MS * 1000;
  gint64 idle = g_get_monotonic_time() - resize_last_activity;

  /* Rearm the timer if more configure events arrived in the meantime */
  if( idle < timeout ) {
    resize_timeout_id = g_timeout_add( ( timeout - idle ) / 1000,
                                       drawing_area_resize_timeout, NULL );
    return G_SOURCE_REMOVE;
  }

  resize_timeout_id = 0;

  drawing_area_resize( pending_width, pending_height, 1 );

  return G_SOURCE_REMOVE;
}

/* Called by gtkui_window on "configure_event".
   On GTK 3 the window determines the size of the drawing area.

   Wait for RESIZE_TIMEOUT_MS before changing the scaler to prevent
   the window from flickering while it is being resized. */
static gint
drawing_area_resize_callback( GtkWidget *widget GCC_UNUSED, GdkEvent *event,
                              gpointer data GCC_UNUSED )
{
  pending_width  = event->configure.width;
  pending_height = event->configure.height - extra_height;
  resize_last_activity = g_get_monotonic_time();

  if( !resize_timeout_id )
    resize_timeout_id =
      g_timeout_add( RESIZE_TIMEOUT_MS, drawing_area_resize_timeout, NULL );

  return FALSE;
}

void
gtkdisplay_update_geometry( void )
{
  GdkGeometry geometry;
  GdkWindowHints hints;
  GtkWidget *geometry_widget;

  if( !scalers_registered ) return;

  hints = GDK_HINT_MIN_SIZE;

  /* Since GTK 3.20 it is intended that gtk_window_set_geometry_hints
     don't set geometry of widgets. See [bugs:#344] */
  geometry_widget = NULL;

  /* Add extra space for menu bar */
  extra_height = gtkui_menubar_get_height();

  /* Add extra space for status bar + padding */
  if( settings_current.statusbar ) {
    extra_height += gtkstatusbar_get_height();
  }

  geometry.min_width = DISPLAY_ASPECT_WIDTH;
  geometry.min_height = DISPLAY_SCREEN_HEIGHT + extra_height;

  gtk_window_set_geometry_hints( GTK_WINDOW( gtkui_window ),
                                 geometry_widget,
                                 &geometry, hints );
}

static void
gtkdisplay_load_gfx_mode( void )
{
  float scale;

  scale = scaler_get_scaling_factor( current_scaler );

  gtkdisplay_update_geometry();

  /* This is a programmatic resize for a new scaler so apply it right away */
  cancel_pending_resize();
  drawing_area_resize( scale * image_width, scale * image_height, 0 );

  gtk_window_resize( GTK_WINDOW( gtkui_window ), scale * image_width,
                     scale * image_height + extra_height );

  /* Redraw the entire screen... */
  display_refresh_all();
}

/* The window's natural size for the current scaler */
void
gtkdisplay_get_window_size( int *width, int *height )
{
  float scale = scaler_get_scaling_factor( current_scaler );

  *width = scale * image_width;
  *height = scale * image_height + extra_height;
}
