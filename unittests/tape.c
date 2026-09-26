/* tape.c: tape unit tests
   Copyright (c) 1999-2017 Philip Kendall, Darren Salt, Witold Filipczyk
   Copyright (c) 2015-2018 UB880D
   Copyright (c) 2016-2021 Fredrick Meunier
   Copyright (c) 2026 Alberto Garcia

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "libspectrum.h"

#include "event.h"
#include "machine.h"
#include "memory_pages.h"
#include "rzx.h"
#include "settings.h"
#include "tape.h"
#include "tape_internals.h"
#include "z80/z80.h"
#include "z80/z80_macros.h"

typedef struct tape_test_fixture {
  libspectrum_tape *saved_tape;
  libspectrum_tape *test_tape;
  libspectrum_tape_block *rom;
  libspectrum_tape_block *following;
  libspectrum_byte *data;
  int saved_microphone;
} tape_test_fixture;

static int
tape_test_create_rom_pause_tape( tape_test_fixture *fixture )
{
  fixture->test_tape = libspectrum_tape_alloc();
  if( !fixture->test_tape ) return 1;

  fixture->rom =
    libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_ROM );
  fixture->following =
    libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PAUSE );
  fixture->data = libspectrum_new( libspectrum_byte, 2 );
  if( !fixture->rom || !fixture->following || !fixture->data ) return 1;

  fixture->data[0] = 0x80;
  fixture->data[1] = 0x80;
  libspectrum_tape_block_set_data_length( fixture->rom, 2 );
  libspectrum_tape_block_set_data( fixture->rom, fixture->data );
  fixture->data = NULL;
  libspectrum_tape_block_set_pause_tstates( fixture->rom, 3500000 );
  libspectrum_tape_block_set_pause_tstates( fixture->following, 1 );

  if( libspectrum_tape_append_block( fixture->test_tape, fixture->rom ) )
    return 1;
  fixture->rom = NULL;
  if( libspectrum_tape_append_block( fixture->test_tape,
                                     fixture->following ) )
    return 1;
  fixture->following = NULL;
  return 0;
}

static int
check_trap_pause_position( void )
{
  libspectrum_tape_signal_level level;
  int position;

  return tape_trap_finish_rom_block() ||
         libspectrum_tape_state( tape ) != LIBSPECTRUM_TAPE_STATE_PAUSE ||
         libspectrum_tape_position( &position, tape ) || position != 0 ||
         libspectrum_tape_signal_level_get( &level, tape ) ||
         level != LIBSPECTRUM_TAPE_SIGNAL_LOW || tape_microphone != 1;
}

static int
check_pause_playback( void )
{
  libspectrum_tape_edge edge;
  int position;

  /* Previewing must not consume the pause: normal playback returns the same
     high pause interval and only then selects the following block. */
  return libspectrum_tape_get_next_edge( &edge, tape ) ||
         edge.tstates != 3500000 ||
         edge.level != LIBSPECTRUM_TAPE_SIGNAL_HIGH ||
         !( edge.flags & LIBSPECTRUM_TAPE_FLAGS_BLOCK ) ||
         libspectrum_tape_position( &position, tape ) || position != 1;
}

static void
tape_test_cleanup( tape_test_fixture *fixture )
{
  tape = fixture->saved_tape;
  tape_microphone = fixture->saved_microphone;
  if( fixture->data ) libspectrum_free( fixture->data );
  if( fixture->rom ) libspectrum_tape_block_free( fixture->rom );
  if( fixture->following )
    libspectrum_tape_block_free( fixture->following );
  libspectrum_tape_free( fixture->test_tape );
}

static int
check_block_details( libspectrum_tape_block *block, const char *expected )
{
  char buffer[128];

  tape_block_details( buffer, sizeof( buffer ), block );
  if( strcmp( buffer, expected ) ) {
    printf( "tape block detail: expected '%s', got '%s'\n", expected, buffer );
    return 1;
  }
  return 0;
}

static int
tape_block_details_unittest( void )
{
  libspectrum_tape_block *block;
  libspectrum_byte *data;
  libspectrum_dword *lengths;
  size_t *repeats;
  char *text;
  int error = 0;

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_ROM );
  if( !block ) return 1;
  data = libspectrum_new( libspectrum_byte, 19 );
  if( !data ) { libspectrum_tape_block_free( block ); return 1; }
  memset( data, ' ', 19 );
  data[0] = 0x00; data[1] = 0x03;
  memcpy( &data[2], "TEST      ", 10 );
  libspectrum_tape_block_set_data_length( block, 19 );
  libspectrum_tape_block_set_data( block, data );
  error |= check_block_details( block, "Bytes: \"TEST\"" );
  data[0] = 0xff;
  error |= check_block_details( block, "19 bytes" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_TURBO );
  libspectrum_tape_block_set_data_length( block, 42 );
  error |= check_block_details( block, "42 bytes" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PURE_TONE );
  libspectrum_tape_block_set_pulse_length( block, 2168 );
  error |= check_block_details( block, "2168 tstates" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PULSES );
  libspectrum_tape_block_set_count( block, 7 );
  error |= check_block_details( block, "7 pulses" );
  libspectrum_tape_block_free( block );

  block =
    libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PULSE_SEQUENCE );
  if( !block ) return 1;
  lengths = libspectrum_new( libspectrum_dword, 2 );
  repeats = libspectrum_new( size_t, 2 );
  if( !lengths || !repeats ) {
    if( lengths ) libspectrum_free( lengths );
    if( repeats ) libspectrum_free( repeats );
    libspectrum_tape_block_free( block );
    return 1;
  }
  lengths[0] = 100; lengths[1] = 200;
  repeats[0] = 2; repeats[1] = 3;
  libspectrum_tape_block_set_count( block, 2 );
  libspectrum_tape_block_set_pulse_lengths( block, lengths );
  libspectrum_tape_block_set_pulse_repeats( block, repeats );
  error |= check_block_details( block, "5 pulses" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PAUSE );
  libspectrum_tape_block_set_pause( block, 1000 );
  error |= check_block_details( block, "1000 ms" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_COMMENT );
  if( !block ) return 1;
  text = libspectrum_new( char, 8 );
  if( !text ) { libspectrum_tape_block_free( block ); return 1; }
  strcpy( text, "comment" );
  libspectrum_tape_block_set_text( block, text );
  error |= check_block_details( block, "comment" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_JUMP );
  libspectrum_tape_block_set_offset( block, 3 );
  error |= check_block_details( block, "Forward 3 blocks" );
  libspectrum_tape_block_set_offset( block, -2 );
  error |= check_block_details( block, "Backward 2 blocks" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_LOOP_START );
  libspectrum_tape_block_set_count( block, 4 );
  error |= check_block_details( block, "4 iterations" );
  libspectrum_tape_block_free( block );

  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_SELECT );
  libspectrum_tape_block_set_count( block, 6 );
  error |= check_block_details( block, "6 options" );
  /* No option arrays are needed for formatting, so keep block cleanup empty. */
  libspectrum_tape_block_set_count( block, 0 );
  libspectrum_tape_block_free( block );

  /* RLE pulse data counts pulses using the CSW/TZX RLE encoding: a pulse of
     n samples is one byte if n <= 255, otherwise a zero marker followed by a
     little-endian dword (here 0x0000012c = 300 samples). */
  block = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_RLE_PULSE );
  if( !block ) return 1;
  data = libspectrum_new( libspectrum_byte, 6 );
  if( !data ) { libspectrum_tape_block_free( block ); return 1; }
  memset( data, 0, 6 );
  data[0] = 100;
  data[2] = 0x2c; data[3] = 1;
  libspectrum_tape_block_set_data_length( block, 6 );
  libspectrum_tape_block_set_data( block, data );
  error |= check_block_details( block, "2 pulses" );
  libspectrum_tape_block_free( block );

  return error;
}

static libspectrum_tape_block *
make_rom_block( const libspectrum_byte *source, size_t length )
{
  libspectrum_tape_block *block =
    libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_ROM );
  libspectrum_byte *data;

  if( !block ) return NULL;
  if( !length ) return block;
  data = libspectrum_new( libspectrum_byte, length );
  memcpy( data, source, length );
  libspectrum_tape_block_set_data_length( block, length );
  libspectrum_tape_block_set_data( block, data );
  return block;
}

static int
run_trap_load( const libspectrum_byte *data, size_t length, int verify,
               libspectrum_word requested, libspectrum_word count,
               size_t *consumed )
{
  libspectrum_tape_block *block = make_rom_block( data, length );
  int error;

  if( !block ) return 1;
  A_ = requested;
  F_ = verify ? 0 : FLAG_C;
  DE = count;
  IX = 0x8000;
  error = tape_trap_load_block( block, consumed );
  libspectrum_tape_block_free( block );
  return error;
}

static int
trap_load_special_cases_unittest( const libspectrum_byte *valid,
                                   size_t valid_length )
{
  static const libspectrum_byte bad_parity[] = { 0xff, 0x12, 0x34, 0xda };
  size_t consumed;
  int error = 0;

  error |= run_trap_load( bad_parity, sizeof( bad_parity ), 0, 0xff, 2,
                          &consumed );
  error |= consumed != 4 || ( F & FLAG_C );

  error |= run_trap_load( valid, valid_length, 0, 0xff, 1, &consumed );
  error |= consumed != 3 || DE != 0 || IX != 0x8001;

  error |= run_trap_load( NULL, 0, 0, 0xff, 2, &consumed );
  error |= consumed != 0 || L != 1 || F_ != 1 || ( F & FLAG_C );

  error |= run_trap_load( valid, valid_length, 0, 0xff, 0, &consumed );
  error |= consumed != 1 || DE != 0 || IX != 0x8000 || B != 0xb0;

  /* A one-byte ROM block contains only the flag and no data bytes. */
  static const libspectrum_byte single[] = { 0xff };
  error |= run_trap_load( single, sizeof( single ), 0, 0xff, 2, &consumed );
  error |= consumed != 1 || DE != 2 || IX != 0x8000 || L != 1 ||
           ( F & FLAG_C );
  return error;
}

static int
trap_load_unittest( void )
{
  static const libspectrum_byte valid[] = { 0xff, 0x12, 0x34, 0xd9 };
  libspectrum_byte saved[2];
  libspectrum_word saved_af = AF, saved_af_ = AF_, saved_bc = BC;
  libspectrum_word saved_de = DE, saved_hl = HL, saved_ix = IX;
  size_t consumed;
  int error = 0;

  saved[0] = readbyte_internal( 0x8000 );
  saved[1] = readbyte_internal( 0x8001 );

  error |= run_trap_load( valid, sizeof( valid ), 0, 0xff, 2, &consumed );
  error |= consumed != 4 || DE != 0 || IX != 0x8002 || !( F & FLAG_C ) ||
           readbyte_internal( 0x8000 ) != 0x12 ||
           readbyte_internal( 0x8001 ) != 0x34;

  error |= run_trap_load( valid, sizeof( valid ), 1, 0xff, 2, &consumed );
  error |= consumed != 4 || DE != 0 || IX != 0x8002 || !( F & FLAG_C );

  writebyte_internal( 0x8000, 0x00 );
  error |= run_trap_load( valid, sizeof( valid ), 1, 0xff, 2, &consumed );
  error |= consumed != 2 || DE != 2 || IX != 0x8000 || ( F & FLAG_C );

  error |= run_trap_load( valid, sizeof( valid ), 0, 0x00, 2, &consumed );
  error |= consumed != 1 || DE != 2 || IX != 0x8000 || ( F & FLAG_C );

  error |= trap_load_special_cases_unittest( valid, sizeof( valid ) );

  writebyte_internal( 0x8000, saved[0] );
  writebyte_internal( 0x8001, saved[1] );
  AF = saved_af; AF_ = saved_af_; BC = saved_bc;
  DE = saved_de; HL = saved_hl; IX = saved_ix;
  return error;
}

static int
tape_record_unittest( void )
{
  libspectrum_byte encoded[5];
  libspectrum_byte *buffer;
  libspectrum_dword size = 8;
  libspectrum_tape *test_tape;
  libspectrum_tape_block *block;
  int saved_modified = tape_modified;
  int error = 0;

  error |= tape_record_encode( encoded, 0, 0xff ) != 1 ||
           encoded[0] != 0xff;
  error |= tape_record_encode( encoded, 0, 0x100 ) != 5 ||
           encoded[0] != 0 || encoded[1] != 0 || encoded[2] != 1 ||
           encoded[3] != 0 || encoded[4] != 0;
  error |= tape_record_encode( encoded, 0, 0x12345678 ) != 5 ||
           encoded[1] != 0x78 || encoded[2] != 0x56 ||
           encoded[3] != 0x34 || encoded[4] != 0x12;

  buffer = libspectrum_new( libspectrum_byte, size );
  if( !buffer ) return 1;
  tape_record_ensure_capacity( &buffer, &size, 3 );
  error |= size != 16;
  libspectrum_free( buffer );

  test_tape = libspectrum_tape_alloc();
  if( !test_tape ) return 1;
  tape_record_set_tape( test_tape );
  tape_record_start();
  error |= tape_record_stop();
  block = libspectrum_tape_current_block( test_tape );
  error |= !block || libspectrum_tape_block_data_length( block ) != 1 ||
           libspectrum_tape_block_data( block )[0] != 1;
  libspectrum_tape_free( test_tape );
  tape_record_set_tape( tape );
  tape_modified = saved_modified;
  return error;
}

static libspectrum_dword edge_test_tstates;

static void
capture_tape_edge_event( gpointer data, gpointer user_data )
{
  event_t *event = data;

  if( event->type == tape_edge_event ) edge_test_tstates = event->tstates;
}

static int
tape_edge_unittest( void )
{
  libspectrum_tape_edge edge = { 0 };
  libspectrum_tape_block *rom, *pause;
  libspectrum_machine saved_machine = machine_current->machine;
  int saved_pending = tape_stop_pending;
  int saved_blocked = tape_autoplay_blocked;
  int saved_autoplay = tape_autoplay;
  int saved_traps = settings_current.tape_traps;
  int saved_recording = rzx_recording;
  int saved_microphone = tape_microphone;
  int error = 0;

  edge.flags = LIBSPECTRUM_TAPE_FLAGS_STOP48;
  machine_current->machine = LIBSPECTRUM_MACHINE_48;
  error |= !tape_edge_requests_stop( &edge );
  machine_current->machine = LIBSPECTRUM_MACHINE_128;
  error |= tape_edge_requests_stop( &edge );

  tape_stop_pending = tape_autoplay_blocked = 0;
  edge.flags = LIBSPECTRUM_TAPE_FLAGS_STOP;
  tape_handle_stop_request( &edge );
  error |= !tape_stop_pending || tape_autoplay_blocked;
  tape_stop_pending = tape_autoplay_blocked = 0;
  edge.flags = LIBSPECTRUM_TAPE_FLAGS_STOP |
               LIBSPECTRUM_TAPE_FLAGS_BLOCK;
  tape_handle_stop_request( &edge );
  error |= !tape_stop_pending || !tape_autoplay_blocked;

  rom = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_ROM );
  pause = libspectrum_tape_block_alloc( LIBSPECTRUM_TAPE_BLOCK_PAUSE );
  if( !rom || !pause ) {
    if( rom ) libspectrum_tape_block_free( rom );
    if( pause ) libspectrum_tape_block_free( pause );
    error = 1;
    goto done;
  }
  tape_autoplay = settings_current.tape_traps = 1;
  rzx_recording = 0;
  error |= !tape_should_stop_for_rom_block( rom );
  error |= tape_should_stop_for_rom_block( pause );
  rzx_recording = 1;
  error |= tape_should_stop_for_rom_block( rom );
  libspectrum_tape_block_free( rom );
  libspectrum_tape_block_free( pause );

  edge.level = LIBSPECTRUM_TAPE_SIGNAL_HIGH;
  tape_update_microphone( &edge );
  error |= tape_microphone != LIBSPECTRUM_TAPE_SIGNAL_HIGH;

  event_remove_type( tape_edge_event );
  edge.tstates = 123;
  edge.flags = LIBSPECTRUM_TAPE_FLAGS_LENGTH_SHORT;
  edge_test_tstates = 0;
  tape_schedule_edge( 1000, &edge, 0 );
  event_foreach( capture_tape_edge_event, NULL );
  error |= edge_test_tstates != 1123;
  event_remove_type( tape_edge_event );

done:
  machine_current->machine = saved_machine;
  tape_stop_pending = saved_pending;
  tape_autoplay_blocked = saved_blocked;
  tape_autoplay = saved_autoplay;
  settings_current.tape_traps = saved_traps;
  rzx_recording = saved_recording;
  tape_microphone = saved_microphone;
  return error;
}

int
tape_unittest( void )
{
  tape_test_fixture fixture = { 0 };
  int error;

  fixture.saved_tape = tape;
  fixture.saved_microphone = tape_microphone;
  error = tape_test_create_rom_pause_tape( &fixture );

  if( !error ) {
    tape = fixture.test_tape;
    tape_microphone = 0;
    error = check_trap_pause_position();
  }
  if( !error ) error = check_pause_playback();
  if( !error ) error = tape_block_details_unittest();
  if( !error ) error = trap_load_unittest();
  if( !error ) error = tape_record_unittest();
  if( !error ) error = tape_edge_unittest();

  tape_test_cleanup( &fixture );
  if( error ) printf( "tape_unittest failed\n" );
  return error;
}
