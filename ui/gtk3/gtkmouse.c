/* gtkmouse.c: GTK routines for emulating Spectrum mice
   Copyright (c) 2004 Darren Salt
   Copyright (c) 2015-2021 Sergio Baldoví

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

   E-mail: linux@youmustbejoking.demon.co.uk

*/

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkinternals.h"
#include "settings.h"
#include "ui/ui.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
/* For XWarpPointer *only* - see below */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif

static GdkCursor *nullpointer = NULL;

/* Auto-hide the mouse pointer (and, in fullscreen, the menu and status bars)
   after this many seconds of inactivity over the drawing area */
#define POINTER_HIDE_TIMEOUT 2
static guint pointer_timeout_id = 0;
static gint64 pointer_last_activity = 0;
static gboolean pointer_hidden = FALSE;
static gboolean bars_hidden = FALSE;
static gboolean pointer_over_drawing_area = FALSE;

/* The widget we base our events, grabs, warping etc on */
static GtkWidget *mouse_widget = NULL;

/* Translate absolute pointer coordinate to relative movement */
static void (*mouse_motion_fn)( gdouble x, gdouble y, int *rel_x, int *rel_y );

#if defined GDK_WINDOWING_WAYLAND || defined GDK_WINDOWING_WIN32

/* On Wayland we can't warp the pointer so we keep the last position */
static gdouble last_pos_x = 0;
static gdouble last_pos_y = 0;
static int have_last_position = 0;

static void
mouse_motion_relative( gdouble x, gdouble y, int *rel_x, int *rel_y )
{
   if( have_last_position ) {
     *rel_x = x - last_pos_x;
     *rel_y = y - last_pos_y;
   } else {
     *rel_x = 0;
     *rel_y = 0;
     have_last_position = 1;
   }

   last_pos_x = x;
   last_pos_y = y;
}

#endif                /* if defined GDK_WINDOWING_WAYLAND ||
                            defined GDK_WINDOWING_WIN32 */

#ifdef GDK_WINDOWING_X11

static void
mouse_motion_x11( gdouble x, gdouble y, int *rel_x, int *rel_y )
{
  *rel_x = x - 128;
  *rel_y = y - 128;

  if( x != 128 || y != 128 ) {
    GdkWindow *window = gtk_widget_get_window( mouse_widget );
    XWarpPointer( GDK_WINDOW_XDISPLAY( window ), None,
                  GDK_WINDOW_XID( window ), 0, 0, 0, 0, 128, 128 );
  }
}

#endif                /* #ifdef GDK_WINDOWING_X11 */

#ifdef GDK_WINDOWING_WIN32

static void
mouse_motion_win32( gdouble x, gdouble y, int *rel_x, int *rel_y )
{
  mouse_motion_relative( x, y, rel_x, rel_y );

  /* Keep pointer hidden */
  SetCursor( NULL );
}

#endif                /* #ifdef GDK_WINDOWING_WIN32 */

static void
gtkmouse_reset_pointer( void )
{
  /* Ugh. GDK doesn't have its own move-pointer function :-|

     The logic here is a bit hairy:

     * On GTK 2.x, we warp relative to the drawing area
     * On GTK 3.x on X11, we warp relative to the top-level window
     * On GTK 3.x on Wayland, we don't warp at all because it causes a
       segfault (see bug #435)
     * On GTK 3.x on win32, we don't warp at all
   */

#ifdef GDK_WINDOWING_WAYLAND

  GdkDisplay *display = gdk_display_get_default();
  if( GDK_IS_WAYLAND_DISPLAY( display ) ) {
    mouse_motion_fn = mouse_motion_relative;
    have_last_position = 0;
    return;
  }

#endif                /* #ifdef GDK_WINDOWING_WAYLAND */

#ifdef GDK_WINDOWING_X11

  mouse_motion_fn = mouse_motion_x11;

  /* Force initial position */
  GdkWindow *window = gtk_widget_get_window( mouse_widget );
  XWarpPointer( GDK_WINDOW_XDISPLAY( window ), None, 
                GDK_WINDOW_XID( window ), 0, 0, 0, 0, 128, 128 );
  return;

#endif                /* #ifdef GDK_WINDOWING_WAYLAND */

#ifdef GDK_WINDOWING_WIN32

  mouse_motion_fn = mouse_motion_win32;
  have_last_position = 0;
  return;

#endif                /* #ifdef GDK_WINDOWING_WIN32 */

}

/* Show the mouse pointer (and the bars) again if we hid them */
static void
show_pointer_and_bars( void )
{
  GdkWindow *window = gtk_widget_get_window( gtkui_drawing_area );

  if( pointer_hidden && window ) {
    gdk_window_set_cursor( window, NULL );
    pointer_hidden = FALSE;
  }

  if( bars_hidden ) {
    gtkui_set_bars_visible( 1 );
    bars_hidden = FALSE;
  }
}

/* Inactivity timer: hide the pointer once it has been idle long enough */
static gboolean
hide_pointer( gpointer data GCC_UNUSED )
{
  GdkWindow *window;
  gint64 timeout = (gint64)POINTER_HIDE_TIMEOUT * G_USEC_PER_SEC;
  gint64 idle = g_get_monotonic_time() - pointer_last_activity;

  /* Rearm the timer if the pointer hasn't been idle long enough */
  if( idle < timeout ) {
    pointer_timeout_id = g_timeout_add( ( timeout - idle ) / 1000,
                                        hide_pointer, NULL );
    return G_SOURCE_REMOVE;
  }

  pointer_timeout_id = 0;

  /* If the pointer is idle, hide it */
  window = gtk_widget_get_window( gtkui_drawing_area );
  if( window ) {
    if( !nullpointer )
      nullpointer = gdk_cursor_new_for_display( gdk_window_get_display( window ),
                                                GDK_BLANK_CURSOR );
    gdk_window_set_cursor( window, nullpointer );
    pointer_hidden = TRUE;
  }

  /* In fullscreen also hide the menu and status bars */
  if( settings_current.full_screen ) {
    gtkui_set_bars_visible( 0 );
    bars_hidden = TRUE;
  }

  return G_SOURCE_REMOVE;
}

/* Pointer over the drawing area: track its idle status */
static void
pointer_activity( void )
{
  pointer_last_activity = g_get_monotonic_time();

  if( !pointer_timeout_id )
    pointer_timeout_id =
      g_timeout_add( POINTER_HIDE_TIMEOUT * 1000, hide_pointer, NULL );

  show_pointer_and_bars();
}

/* Pointer left the drawing area: stop tracking its idle status */
static void
pointer_reset( void )
{
  if( pointer_timeout_id ) {
    g_source_remove( pointer_timeout_id );
    pointer_timeout_id = 0;
  }

  show_pointer_and_bars();
}

static gboolean
gtkmouse_window_state ( GtkWidget *widget, GdkEventWindowState *event,
                        gpointer data GCC_UNUSED )
{
  /* Show the pointer and the bars and restart the inactivity timer
     after switching fullscreen mode */
  if( event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN ) {
    if( !ui_mouse_grabbed && pointer_over_drawing_area )
      pointer_activity();
  }

  return FALSE;
}

static gboolean
motion_event( GtkWidget *widget GCC_UNUSED, GdkEventMotion *event,
              gpointer data GCC_UNUSED )
{
  int rel_x, rel_y;

  /* When the mouse isn't grabbed, keep the pointer visible and track its
     inactivity */
  if( !ui_mouse_grabbed ) {
    if( pointer_over_drawing_area ) pointer_activity();
    return FALSE;
  }

  /* Get relative movement from last position */
  (*mouse_motion_fn)( event->x, event->y, &rel_x, &rel_y );
  ui_mouse_motion( rel_x, rel_y );

  return FALSE;
}

/* The pointer entered the drawing area: start tracking inactivity */
static gboolean
enter_event( GtkWidget *widget GCC_UNUSED, GdkEventCrossing *event GCC_UNUSED,
             gpointer data GCC_UNUSED )
{
  pointer_over_drawing_area = TRUE;
  if( !ui_mouse_grabbed ) pointer_activity();

  return FALSE;
}

/* The pointer left the drawing area: make it visible */
static gboolean
leave_event( GtkWidget *widget GCC_UNUSED, GdkEventCrossing *event GCC_UNUSED,
             gpointer data GCC_UNUSED )
{
  pointer_over_drawing_area = FALSE;
  pointer_reset();

  return FALSE;
}

static gboolean
button_event( GtkWidget *widget GCC_UNUSED, GdkEventButton *event,
	      gpointer data GCC_UNUSED )
{
  if( event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS
      || event->type == GDK_3BUTTON_PRESS )
    ui_mouse_button( event->button, 1 );
  else
    ui_mouse_button( event->button, 0 );

  /* Stop other handlers only if we've grabbed the mouse */
  return ui_mouse_grabbed? TRUE : FALSE;
}

void
gtkmouse_init( void )
{
  mouse_widget = gtkui_window;

  gtk_widget_add_events( GTK_WIDGET( mouse_widget ),
    GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
  g_signal_connect( G_OBJECT( mouse_widget ), "motion-notify-event",
		    G_CALLBACK( motion_event ), NULL );
  g_signal_connect( G_OBJECT( mouse_widget ), "button-press-event",
		    G_CALLBACK( button_event ), NULL );
  g_signal_connect( G_OBJECT( mouse_widget ), "button-release-event",
		    G_CALLBACK( button_event ), NULL );

  /* Track when the pointer is over the drawing area so we can hide it */
  gtk_widget_add_events( gtkui_drawing_area,
                         GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK );
  g_signal_connect( G_OBJECT( gtkui_drawing_area ), "enter-notify-event",
		    G_CALLBACK( enter_event ), NULL );
  g_signal_connect( G_OBJECT( gtkui_drawing_area ), "leave-notify-event",
		    G_CALLBACK( leave_event ), NULL );

  /* Restart the mouse inactivity timer after switching fullscreen mode */
  g_signal_connect( G_OBJECT( gtkui_window ), "window-state-event",
		    G_CALLBACK( gtkmouse_window_state ), NULL );
}

void
gtkmouse_init( void )
{
#if GTK_CHECK_VERSION( 3, 0, 0 )
  mouse_widget = gtkui_window;
#else                 /* #if GTK_CHECK_VERSION( 3, 0, 0 ) */
  mouse_widget = gtkui_drawing_area;
#endif                /* #if GTK_CHECK_VERSION( 3, 0, 0 ) */

  gtk_widget_add_events( GTK_WIDGET( mouse_widget ),
    GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
  g_signal_connect( G_OBJECT( mouse_widget ), "motion-notify-event",
		    G_CALLBACK( motion_event ), NULL );
  g_signal_connect( G_OBJECT( mouse_widget ), "button-press-event",
		    G_CALLBACK( button_event ), NULL );
  g_signal_connect( G_OBJECT( mouse_widget ), "button-release-event",
		    G_CALLBACK( button_event ), NULL );
}

int
ui_mouse_grab( int startup )
{
  GdkWindow *window;
  GdkGrabStatus status;

  if( startup ) return 0;

  window = gtk_widget_get_window( mouse_widget );

  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_window_get_display( window );

  if( !nullpointer ) {
    nullpointer = gdk_cursor_new_for_display( display, GDK_BLANK_CURSOR );
  }

  seat = gdk_display_get_default_seat( display );
  status = gdk_seat_grab( seat, window, GDK_SEAT_CAPABILITY_ALL_POINTING,
                          FALSE, nullpointer, NULL, NULL, NULL );

  if( status == GDK_GRAB_SUCCESS ) {
    /* The grab hides the pointer, so stop tracking its activity */
    pointer_reset();
    gtkmouse_reset_pointer();
    ui_statusbar_update( UI_STATUSBAR_ITEM_MOUSE, UI_STATUSBAR_STATE_ACTIVE );
    return 1;
  }

  ui_error( UI_ERROR_WARNING, "Mouse grab failed" );
  return 0;
}

int
ui_mouse_release( int suspend GCC_UNUSED )
{
  GdkDisplay *display;
  GdkSeat *seat;

  display = gtk_widget_get_display( mouse_widget );
  seat = gdk_display_get_default_seat( display );
  gdk_seat_ungrab( seat );

  /* The grab finished, show the pointer again */
  pointer_reset();

  ui_statusbar_update( UI_STATUSBAR_ITEM_MOUSE, UI_STATUSBAR_STATE_INACTIVE );
  return 0;
}
