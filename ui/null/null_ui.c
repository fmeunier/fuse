/* null_ui.c: Routines for dealing with the null user interface
   Copyright (c) 2017 Philip Kendall

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

#ifdef ENABLE_AUTOMATION
#include "automation/automation.h"
#endif
#include <string.h>

#include "display.h"
#include "keyboard.h"
#include "machine.h"
#include "spectrum.h"
#include "utils.h"
#include "ui/scaler/scaler.h"
#include "ui/ui.h"
#include "ui/ui_internals.h"

#include "../uijoystick.c"

static libspectrum_byte *framebuffer;
static int framebuffer_width, framebuffer_height;

static void
set_pixel( int x, int y, libspectrum_byte colour )
{
  if( framebuffer && x >= 0 && y >= 0 && x < framebuffer_width &&
      y < framebuffer_height ) framebuffer[y * framebuffer_width + x] = colour;
}

keysyms_map_t keysyms_map[] = {
  { 0, 0 } /* End marker */
};

scaler_type
menu_get_scaler( scaler_available_fn selector )
{
  /* No scaler selected */
  return SCALER_NUM;
}

int
menu_select_roms_with_title( const char *title, size_t start, size_t count,
                             int is_peripheral )
{
  /* No error */
  return 0;
}

void
ui_breakpoints_updated( void )
{
  /* Do nothing */
}

ui_confirm_save_t
ui_confirm_save_specific( const char *message )
{
  return UI_CONFIRM_SAVE_DONTSAVE;
}

ui_confirm_joystick_t
ui_confirm_joystick( libspectrum_joystick libspectrum_type, int inputs )
{
  return UI_CONFIRM_JOYSTICK_NONE;
}

int
ui_debugger_activate( void )
{
  /* No error */
  return 0;
}

int
ui_debugger_deactivate( int interruptable )
{
  /* No error */
  return 0;
}

int
ui_debugger_disassemble( libspectrum_word addr )
{
  /* No error */
  return 0;
}

int
ui_debugger_update( void )
{
  /* No error */
  return 0;
}

int
ui_end( void )
{
  /* No error */
  return 0;
}

int
ui_error_specific( ui_error_level severity, const char *message )
{
#ifdef ENABLE_AUTOMATION
  automation_diagnostic( severity, message );
#endif
  return 0;
}

int
ui_event( void )
{
  /* No error */
  return 0;
}

char *
ui_get_open_filename( const char *title )
{
  /* No filename */
  return NULL;
}

int
ui_get_rollback_point( GSList *points )
{
  /* No rollback point */
  return -1;
}

char *
ui_get_save_filename( const char *title )
{
  /* No filename */
  return NULL;
}

int
ui_init( int *argc, char ***argv )
{
  /* No error */
  return 0;
}

int
ui_menu_item_set_active( const char *path, int active )
{
  /* No error */
  return 0;
}

int
ui_mouse_grab( int startup )
{
  /* Successful grab */
  return 1;
}

int
ui_mouse_release( int suspend )
{
  /* No error */
  return 0;
}

void
ui_pokemem_selector( const char *filename )
{
  /* Do nothing */
}

int
ui_query_message( const char *message )
{
  /* Query confirmed */
  return 1;
}

int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
#ifdef ENABLE_AUTOMATION
  if( automation_active() && item == UI_STATUSBAR_ITEM_DISK &&
      ( state == UI_STATUSBAR_STATE_ACTIVE ||
        state == UI_STATUSBAR_STATE_INACTIVE ) )
    automation_disk_motor_changed( state == UI_STATUSBAR_STATE_ACTIVE,
                                   spectrum_get_frame_count() );
#endif
  /* No error */
  return 0;
}

int
ui_statusbar_update_speed( float speed )
{
  /* No error */
  return 0;
}

int
ui_tape_browser_update( ui_tape_browser_update_type change,
                        libspectrum_tape_block *block )
{
  /* No error */
  return 0;
}

int
ui_widgets_reset( void )
{
  /* No error */
  return 0;
}

void
uidisplay_area( int x, int y, int w, int h )
{
  /* Do nothing */
}

int
uidisplay_end( void )
{
  libspectrum_free( framebuffer );
  framebuffer = NULL;
  return 0;
}

void
uidisplay_frame_end( void )
{
#ifdef ENABLE_AUTOMATION
  if( automation_capture_screen_enabled() )
    automation_capture_screen( framebuffer, framebuffer_width,
                               framebuffer_height );
#endif
}

int
uidisplay_hotswap_gfx_mode( void )
{
  /* No error */
  return 0;
}

int
uidisplay_init( int width, int height )
{
#ifdef ENABLE_AUTOMATION
  if( automation_capture_screen_enabled() ) {
    for( scaler_type scaler = 0; scaler < SCALER_NUM; scaler++ )
      scaler_register( scaler );
    framebuffer_width = width;
    framebuffer_height = height;
    framebuffer = libspectrum_new( libspectrum_byte, (size_t)width * height );
    memset( framebuffer, 0, (size_t)width * height );
  }
#endif
  return 0;
}

void
uidisplay_plot16( int x, int y, libspectrum_word data,
                  libspectrum_byte ink, libspectrum_byte paper )
{
  int i, j;
  x <<= 4;
  y <<= 1;
  for( j = 0; j < 2; j++ ) for( i = 0; i < 16; i++ )
      set_pixel( x + i, y + j, data & ( 0x8000 >> i ) ? ink : paper );
}

void
uidisplay_plot8( int x, int y, libspectrum_byte data,
                 libspectrum_byte ink, libspectrum_byte paper )
{
  int i, j, scale = machine_current->timex ? 2 : 1;
  x *= 8 * scale;
  y *= scale;
  for( j = 0; j < scale; j++ ) for( i = 0; i < 8; i++ ) {
      libspectrum_byte colour = data & ( 0x80 >> i ) ? ink : paper;
      for( int k = 0; k < scale;
           k++ ) set_pixel( x + i * scale + k, y + j, colour );
    }
}

void
uidisplay_putpixel( int x, int y, int colour )
{
  int i, j, scale = machine_current->timex ? 2 : 1;
  x *= scale;
  y *= scale;
  for( j = 0; j < scale; j++ ) for( i = 0; i < scale; i++ )
      set_pixel( x + i, y + j, colour );
}
