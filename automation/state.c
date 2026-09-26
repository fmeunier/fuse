/* state.c: fixed automation state summary
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
*/

#include "config.h"
#ifdef ENABLE_AUTOMATION

#include "state.h"
#include "display.h"
#include "machine.h"
#include "rzx.h"
#include "spectrum.h"
#include "tape.h"
#include "z80/z80.h"

void
automation_state_write_json( automation_json *json )
{
  automation_json_object_begin( json, "state" );
  automation_json_object_begin( json, "cpu" );
#define WORD_STATE( n, v ) automation_json_ulong( json, n, ( v ).w )
  WORD_STATE( "af", z80.af );
  WORD_STATE( "bc", z80.bc );
  WORD_STATE( "de", z80.de );
  WORD_STATE( "hl", z80.hl );
  WORD_STATE( "af_alt", z80.af_ );
  WORD_STATE( "bc_alt", z80.bc_ );
  WORD_STATE( "de_alt", z80.de_ );
  WORD_STATE( "hl_alt", z80.hl_ );
  WORD_STATE( "ix", z80.ix );
  WORD_STATE( "iy", z80.iy );
  WORD_STATE( "sp", z80.sp );
  WORD_STATE( "pc", z80.pc );
#undef WORD_STATE
  automation_json_ulong( json, "i", z80.i );
  automation_json_ulong( json, "r", ( z80.r & 0x7f ) | z80.r7 );
  automation_json_boolean( json, "iff1", z80.iff1 );
  automation_json_boolean( json, "iff2", z80.iff2 );
  automation_json_ulong( json, "interrupt_mode", z80.im );
  automation_json_boolean( json, "halted", z80.halted );
  automation_json_ulong( json, "frame_tstate", tstates );
  automation_json_end( json );

  automation_json_object_begin( json, "machine" );
  automation_json_ulong( json, "screen_page", memory_current_screen );
  automation_json_ulong( json, "border", display_lores_border );
  automation_json_boolean( json, "tape_playing", tape_playing );
  automation_json_boolean( json, "rzx_playback", rzx_playback );
  automation_json_object_begin( json, "paging" );
  automation_json_ulong( json, "ram_page", machine_current->ram.current_page );
  automation_json_ulong( json, "rom_page", machine_current->ram.current_rom );
  automation_json_boolean( json, "locked", machine_current->ram.locked );
  automation_json_boolean( json, "special", machine_current->ram.special );
  automation_json_boolean( json, "romcs", machine_current->ram.romcs );
  automation_json_end( json );
  automation_json_end( json );
  automation_json_end( json );
}

#endif
