/* cocoadisplay.h: Routines for dealing with the Cocoa display
   Copyright (c) 2000-2003 Philip Kendall, Fredrick Meunier

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

#ifndef FUSE_COCOADISPLAY_H
#define FUSE_COCOADISPLAY_H

#include "ui/ui.h"
#include "dirty.h"

typedef enum display_framebuffer_pixel_format {
  DISPLAY_FRAMEBUFFER_PIXEL_FORMAT_RGB565,
  DISPLAY_FRAMEBUFFER_PIXEL_FORMAT_BGRA8888
} display_framebuffer_pixel_format;

typedef enum display_framebuffer_ownership {
  DISPLAY_FRAMEBUFFER_OWNS_BACKING_STORAGE = 1,
  DISPLAY_FRAMEBUFFER_OWNS_DIRTY_REGIONS = 2
} display_framebuffer_ownership;

/* This C-compatible state is shared by the emulation and presentation layers.
   `synchronization` is an opaque lock owned by the display view. */
#define DISPLAY_FRAMEBUFFER_SLOT_COUNT 3

typedef struct DisplayFramebuffer {
  display_framebuffer_pixel_format pixel_format;
  int storage_height;
  int storage_width;
  int height;
  int width;
  int x_offset;
  int y_offset;
  int stride;
  void *backing_storage;
  PIG_dirtytable *dirty_regions;
  unsigned long generation;
  void *synchronization;
  unsigned int ownership;
} DisplayFramebuffer;

typedef struct DisplayFramebufferRing {
  DisplayFramebuffer *slots[ DISPLAY_FRAMEBUFFER_SLOT_COUNT ];
} DisplayFramebufferRing;

extern DisplayFramebuffer *screen;
extern DisplayFramebuffer buffered_screen;
extern DisplayFramebufferRing buffered_screen_ring;

void copy_area( DisplayFramebuffer *dest_screen, DisplayFramebuffer *src_screen,
                PIG_rect *r );

#endif			/* #ifndef FUSE_COCOADISPLAY_H */
