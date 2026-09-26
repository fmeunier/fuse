/* source_synths.c: Peripheral sound source synthesis
   Copyright (c) 2000-2026 Russell Marks, Matan Ziv-Av, Philip Kendall,
                           Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "config.h"

#include "fuse.h"
#include "machine.h"
#include "periph.h"
#include "sound.h"
#include "sound/source_synths.h"

static Blip_Synth *left_specdrum, *right_specdrum;
static Blip_Synth *left_covox, *right_covox;
static Blip_Synth *left_sp0256, *right_sp0256;

double
source_volume( int volume )
{
  if( volume < 0 ) return 0.0;
  if( volume > 100 ) return 1.0;
  return volume / 100.0;
}

static Blip_Synth *
source_synth_init( Blip_Buffer *output, int volume )
{
  Blip_Synth *synth = new_Blip_Synth();
  blip_synth_set_volume( synth, source_volume( volume ) );
  blip_synth_set_output( synth, output );
  blip_synth_set_treble_eq( synth, 0.0 );
  return synth;
}

void
source_synths_init( Blip_Buffer *left, Blip_Buffer *right, int stereo,
                    int specdrum_volume, int covox_volume,
                    int uspeech_volume )
{
  left_specdrum = source_synth_init( left, specdrum_volume );
  left_covox = source_synth_init( left, covox_volume );
  left_sp0256 = source_synth_init( left, uspeech_volume );

  right_specdrum = right_covox = right_sp0256 = NULL;
  if( !stereo ) return;
  right_specdrum = source_synth_init( right, specdrum_volume );
  right_covox = source_synth_init( right, covox_volume );
  right_sp0256 = source_synth_init( right, uspeech_volume );
}

void
source_synths_end( void )
{
  delete_Blip_Synth( &left_specdrum );
  delete_Blip_Synth( &right_specdrum );
  delete_Blip_Synth( &left_covox );
  delete_Blip_Synth( &right_covox );
  delete_Blip_Synth( &left_sp0256 );
  delete_Blip_Synth( &right_sp0256 );
}

void
source_synths_set_speech_output( Blip_Buffer *left, Blip_Buffer *right )
{
  blip_synth_set_output( left_sp0256, left );
  if( right_sp0256 ) blip_synth_set_output( right_sp0256, right );
}

/* SpecDrum, Covox and SP0256 provide already digitized waveforms; their
 * writes update simple level synths without an additional source model. */
void
sound_specdrum_write( libspectrum_word port GCC_UNUSED, libspectrum_byte val )
{
  if( periph_is_active( PERIPH_TYPE_SPECDRUM ) ) {
    blip_synth_update( left_specdrum, tstates, ( val - 128 ) * 128 );
    if( right_specdrum )
      blip_synth_update( right_specdrum, tstates, ( val - 128 ) * 128 );
    machine_current->specdrum.specdrum_dac = val - 128;
  }
}

void
sound_covox_write( libspectrum_word port GCC_UNUSED, libspectrum_byte val )
{
  if( periph_is_active( PERIPH_TYPE_COVOX_FB ) ||
      periph_is_active( PERIPH_TYPE_COVOX_DD ) ) {
    blip_synth_update( left_covox, tstates, val * 128 );
    if( right_covox ) blip_synth_update( right_covox, tstates, val * 128 );
    machine_current->covox.covox_dac = val;
  }
}

void
source_synths_sp0256_write( libspectrum_dword at_tstates,
                            libspectrum_signed_word val )
{
  blip_synth_update( left_sp0256, at_tstates, val );
  if( right_sp0256 ) blip_synth_update( right_sp0256, at_tstates, val );
}
