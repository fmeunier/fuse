/* tape_record.c: tape recording routines
   Copyright (c) 1999-2017 Philip Kendall, Darren Salt, Witold Filipczyk
   Copyright (c) 2015-2018 UB880D
   Copyright (c) 2016-2021 Fredrick Meunier

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

#include "libspectrum.h"

#include "event.h"
#include "fuse.h"
#include "machine.h"
#include "peripherals/ula.h"
#include "tape.h"
#include "tape_internals.h"
#include "ui/ui.h"

/* Sample rate and initial buffer size for tape recording */
#define TAPE_RECORDING_SAMPLE_RATE 44100
#define TAPE_RECORDING_BUFFER_SIZE 8192

typedef struct
{
  libspectrum_byte *tape_buffer;
  libspectrum_dword tape_buffer_size;
  libspectrum_dword tape_buffer_used;
  int tstates_per_sample;
  int last_level;
  int last_level_count;
} tape_rec_state;

int tape_recording = 0;

static libspectrum_tape *recording_tape;
static int record_event;
static tape_rec_state rec_state;

static void
tape_event_record_sample( libspectrum_dword last_tstates, int type,
                          void *user_data );

void
tape_record_ensure_capacity( libspectrum_byte **buffer,
                             libspectrum_dword *size,
                             libspectrum_dword used )
{
  if( used + 5 < *size ) return;

  *size *= 2;
  *buffer = libspectrum_renew( libspectrum_byte, *buffer, *size );
}

void
tape_record_set_tape( libspectrum_tape *current_tape )
{
  recording_tape = current_tape;
}

void
tape_record_init( libspectrum_tape *current_tape )
{
  tape_record_set_tape( current_tape );
  record_event = event_register( tape_event_record_sample,
                                 "Tape sample record" );
}

void
tape_record_start( void )
{
  /* sample rate will be 44.1KHz */
  rec_state.tstates_per_sample =
    machine_current->timings.processor_speed / TAPE_RECORDING_SAMPLE_RATE;

  rec_state.tape_buffer_size = TAPE_RECORDING_BUFFER_SIZE;
  rec_state.tape_buffer = libspectrum_new( libspectrum_byte,
                                           rec_state.tape_buffer_size );
  rec_state.tape_buffer_used = 0;

  /* start scheduling events that record into a buffer that we
     start allocating here */
  event_add( tstates + rec_state.tstates_per_sample, record_event );

  rec_state.last_level = ula_tape_level();
  rec_state.last_level_count = 1;

  tape_recording = 1;

  /* Also want to disable other tape actions */
  ui_menu_activate( UI_MENU_ITEM_TAPE_RECORDING, 1 );
}

int
tape_record_encode( libspectrum_byte *tape_buffer,
                  libspectrum_dword tape_buffer_used,
                  int last_level_count )
{
  if( last_level_count <= 0xff ) {
    tape_buffer[ tape_buffer_used++ ] = last_level_count;
  } else {
    tape_buffer[ tape_buffer_used++ ] = 0;
    tape_buffer[ tape_buffer_used++ ] = last_level_count & 0x000000ff;
    tape_buffer[ tape_buffer_used++ ] =
      ( last_level_count & 0x0000ff00 ) >> 8;
    tape_buffer[ tape_buffer_used++ ] =
      ( last_level_count & 0x00ff0000 ) >> 16;
    tape_buffer[ tape_buffer_used++ ] =
      ( last_level_count & 0xff000000 ) >> 24;
  }

  return tape_buffer_used;
}

static void
tape_event_record_sample( libspectrum_dword last_tstates, int type,
                          void *user_data )
{
  if( rec_state.last_level != ula_tape_level() ) {
    /* put a sample into the recording buffer */
    rec_state.tape_buffer_used =
      tape_record_encode( rec_state.tape_buffer,
                        rec_state.tape_buffer_used,
                        rec_state.last_level_count );

    rec_state.last_level_count = 0;
    rec_state.last_level = ula_tape_level();
    /* Make sure we can still fit a dword and a flag byte in the buffer. */
    tape_record_ensure_capacity( &rec_state.tape_buffer,
                                  &rec_state.tape_buffer_size,
                                  rec_state.tape_buffer_used );
  }

  rec_state.last_level_count++;

  /* schedule next timer */
  event_add( last_tstates + rec_state.tstates_per_sample, record_event );
}

int
tape_record_stop( void )
{
  libspectrum_tape_block *block;
  libspectrum_error error;

  /* put last sample into the recording buffer */
  rec_state.tape_buffer_used =
    tape_record_encode( rec_state.tape_buffer, rec_state.tape_buffer_used,
                      rec_state.last_level_count );

  /* stop scheduling events and turn buffer into a block and
     pop into the current tape */
  event_remove_type( record_event );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_RLE_PULSE );

  libspectrum_tape_block_set_scale( block, rec_state.tstates_per_sample );
  libspectrum_tape_block_set_data_length( block, rec_state.tape_buffer_used );
  libspectrum_tape_block_set_data( block, rec_state.tape_buffer );

  error = libspectrum_tape_append_block( recording_tape, block );
  if( error ) libspectrum_tape_block_free( block );

  rec_state.tape_buffer = NULL;
  rec_state.tape_buffer_size = 0;
  rec_state.tape_buffer_used = 0;

  if( !error ) {
    tape_modified = 1;
    ui_tape_browser_update( UI_TAPE_BROWSER_NEW_BLOCK, block );
  }

  tape_recording = 0;

  /* Also want to reenable other tape actions */
  ui_menu_activate( UI_MENU_ITEM_TAPE_RECORDING, 0 );

  return error;
}
