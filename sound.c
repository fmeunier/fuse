/* sound.c: Sound support
   Copyright (c) 2000-2026 Russell Marks, Matan Ziv-Av, Philip Kendall,
                           Fredrick Meunier, Patrik Rak

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

/* The AY white noise RNG algorithm is based on info from MAME's ay8910.c -
 * MAME's licence explicitly permits free use of info (even encourages it).
 */

#include "config.h"

#include "fuse.h"
#include "infrastructure/startup_manager.h"
#include "machine.h"
#include "movie.h"
#include "options.h"
#include "settings.h"
#include "sound.h"
#include "tape.h"
#include "timer/timer.h"
#include "ui/ui.h"
#include "peripherals/sound/sp0256.h"
#include "sound/ay_engine.h"
#include "sound/blipbuffer.h"
#include "sound/output_mixer.h"
#include "sound/source_synths.h"

/* Do we have any of our sound devices available? */

/* configuration */
int sound_enabled = 0;		/* Are we currently using the sound card */

static int sound_enabled_ever = 0; /* whether sound has *ever* been in use; see
				      sound_ay_write() and sound_ay_reset() */
int sound_stereo_ay = SOUND_STEREO_AY_NONE; /* local copy of settings_current.stereo_ay */

int sound_framesiz;

static int sound_channels;

/* The main buffers contain sources which bypass speaker processing. */
Blip_Buffer *left_buf = NULL;
Blip_Buffer *right_buf = NULL;
blip_sample_t *samples = NULL;
static int sound_tv_route = -1;

static void sound_update_source_routes( void );

/* Returns the emulation speed adjusted processor speed */
libspectrum_dword
sound_get_effective_processor_speed( void )
{
  return machine_current->timings.processor_speed / 100 * 
           settings_current.emulation_speed;
}

static int
sound_init_buffer( Blip_Buffer **buf )
{
  *buf = new_Blip_Buffer();
  blip_buffer_set_clock_rate( *buf, sound_get_effective_processor_speed() );
  /* Allow up to 1s of playback buffer - this allows us to cope with slowing
     down to 2% of speed where a single Speccy frame generates just under 1s
     of sound */
  if ( blip_buffer_set_sample_rate( *buf, settings_current.sound_freq, 1000 ) ) {
    sound_end();
    ui_error( UI_ERROR_ERROR, "out of memory at %s:%d", __FILE__, __LINE__ );
    return 0;
  }

  blip_buffer_set_bass_freq( *buf, 0 );
  return 1;
}

#ifndef UI_WIN32
#define MIN_SPEED_PERCENTAGE 2
#define MAX_SPEED_PERCENTAGE 500
#else                        /* #ifndef UI_WIN32 */
/* We are limiting speed until bugs in the DirectSound driver are resolved, see
   [bugs:#364] for more details */
#define MIN_SPEED_PERCENTAGE 50
#define MAX_SPEED_PERCENTAGE 300
#endif                       /* #ifndef UI_WIN32 */

static int
is_in_sound_enabled_range( void )
{
  return settings_current.emulation_speed >= MIN_SPEED_PERCENTAGE &&
    settings_current.emulation_speed <= MAX_SPEED_PERCENTAGE;
}

void
sound_init( const char *device )
{
  float hz;

  /* Allow sound as long as emulation speed is greater than 2%
     (less than that and a single Speccy frame generates more
     than a seconds worth of sound which is bigger than the
     maximum Blip_Buffer of 1 second) */
  if( !( !sound_enabled && settings_current.sound &&
         is_in_sound_enabled_range() ) )
    return;

  /* only try for stereo if we need it */
  sound_stereo_ay = option_enumerate_sound_stereo_ay();
  if( settings_current.sound &&
      sound_lowlevel_init( device, &settings_current.sound_freq,
                           &sound_stereo_ay ) )
    return;
  if( sound_stereo_ay != SOUND_STEREO_AY_NONE &&
      sound_stereo_ay != SOUND_STEREO_AY_ACB &&
      sound_stereo_ay != SOUND_STEREO_AY_ABC ) {
    ui_error( UI_ERROR_ERROR, "unknown AY stereo separation type: %d",
              sound_stereo_ay );
    fuse_abort();
  }

  sound_channels = sound_stereo_ay != SOUND_STEREO_AY_NONE ? 2 : 1;
  hz = ( float )sound_get_effective_processor_speed() /
                machine_current->timings.tstates_per_frame;
  sound_framesiz = ( float )settings_current.sound_freq / hz + 1;

  if( !sound_init_buffer( &left_buf ) ) return;
  if( sound_channels == 2 && !sound_init_buffer( &right_buf ) ) return;
  if( output_mixer_init( sound_get_effective_processor_speed(),
                         settings_current.sound_freq, sound_framesiz,
                         sound_channels, settings_current.volume_beeper ) ) {
    ui_error( UI_ERROR_ERROR, "could not configure Spectrum output mixer" );
    sound_end();
    return;
  }

  ay_engine_init( settings_current.volume_ay, sound_stereo_ay );
  source_synths_init( left_buf, right_buf, sound_channels == 2,
                      settings_current.volume_specdrum,
                      settings_current.volume_covox,
                      settings_current.volume_uspeech );
  samples = libspectrum_new0( blip_sample_t, sound_framesiz * sound_channels );

  sound_enabled = sound_enabled_ever = 1;
  sound_tv_route = -1;
  sound_update_source_routes();
  movie_init_sound( settings_current.sound_freq, sound_stereo_ay );
}

void
sound_pause( void )
{
  if( sound_enabled )
    sound_end();
}

void
sound_unpause( void )
{
  /* No sound if fastloading in progress */
  if( settings_current.fastload && timer_fastloading_active() )
    return;

  sound_init( settings_current.sound_device );
}

void
sound_end( void )
{
  if( sound_enabled ) {
    ay_engine_end();
    source_synths_end();
    output_mixer_end();
    delete_Blip_Buffer( &left_buf );
    delete_Blip_Buffer( &right_buf );

    if( settings_current.sound ) sound_lowlevel_end();
    libspectrum_free( samples );
    sound_enabled = 0;
  }
}

void
sound_register_startup( void )
{
  startup_manager_module dependencies[] = { STARTUP_MANAGER_MODULE_SETUID };
  startup_manager_register( STARTUP_MANAGER_MODULE_SOUND, dependencies,
                            ARRAY_SIZE( dependencies ), NULL, NULL, sound_end );
}

void
sound_ay_write( int reg, int val, libspectrum_dword now )
{
  ay_engine_write( reg, val, now );
}

void
sound_ay_reset( void )
{
  ay_engine_reset();
  output_mixer_reset();
  sound_tv_route = -1;
}

void
sound_sp0256_write( libspectrum_dword at_tstates, libspectrum_signed_word val )
{
  if( !sound_enabled ) return;
  sound_update_source_routes();
  source_synths_sp0256_write( at_tstates, val );
}

int
sound_resolve_speaker_type( int selected_type, int machine_capabilities,
                            int uspeech_enabled )
{
  if( selected_type != SOUND_SPEAKER_TYPE_AUTOMATIC ) return selected_type;

  /* A connected Currah routes its speech and the Spectrum MIC lead through
   * the television, including on machines which normally use a beeper. */
  if( uspeech_enabled ) return SOUND_SPEAKER_TYPE_TV;

  return machine_capabilities & LIBSPECTRUM_MACHINE_CAPABILITY_BEEPER ?
           SOUND_SPEAKER_TYPE_BEEPER : SOUND_SPEAKER_TYPE_TV;
}

unsigned int
sound_tv_source_routes( int speaker_type, int machine_capabilities )
{
  if( speaker_type != SOUND_SPEAKER_TYPE_TV ) return 0;

  return SOUND_ROUTE_ULA_MIC | SOUND_ROUTE_USPEECH |
         ( ( machine_capabilities & LIBSPECTRUM_MACHINE_CAPABILITY_AY ) ?
             SOUND_ROUTE_BUILTIN_AY : 0 );
}

static void
sound_update_source_routes( void )
{
  int speaker_type = output_mixer_speaker_type();
  unsigned int routes = sound_tv_source_routes(
                          speaker_type, machine_current->capabilities );
  int tv_route = speaker_type == SOUND_SPEAKER_TYPE_TV;
  int ay_to_tv = routes & SOUND_ROUTE_BUILTIN_AY;
  Blip_Buffer *ay_left = ay_to_tv ? output_mixer_tv_left() : left_buf;
  Blip_Buffer *ay_right = ay_to_tv ? output_mixer_tv_right() : right_buf;
  Blip_Buffer *speech_left = tv_route ? output_mixer_tv_left() : left_buf;
  Blip_Buffer *speech_right = tv_route ? output_mixer_tv_right() : right_buf;

  if( sound_tv_route == tv_route ) return;

  ay_engine_set_outputs( ay_left, ay_right, sound_stereo_ay );

  source_synths_set_speech_output( speech_left, speech_right );

  output_mixer_route_changed();
  sound_tv_route = tv_route;
}

void
sound_frame( void )
{
  long count;

  if( !sound_enabled )
    return;

  sound_update_source_routes();
  sp0256_do_frame();

  /* overlay AY sound */
  ay_engine_render( machine_current->timings.tstates_per_frame );

  blip_buffer_end_frame( left_buf, machine_current->timings.tstates_per_frame );

  if( sound_stereo_ay != SOUND_STEREO_AY_NONE ) {
    blip_buffer_end_frame( right_buf,
                           machine_current->timings.tstates_per_frame );

    /* Read left channel into even samples, right channel into odd samples:
       LRLRLRLRLR... */
    count = blip_buffer_read_samples( left_buf, samples, sound_framesiz, 1 );
    blip_buffer_read_samples( right_buf, samples + 1, count, 1 );
    count <<= 1;
  } else {
    count = blip_buffer_read_samples( left_buf, samples, sound_framesiz,
                                      BLIP_BUFFER_DEF_STEREO );
  }

  output_mixer_end_frame( machine_current->timings.tstates_per_frame,
                          samples, count );

  if( settings_current.sound )
    sound_lowlevel_frame( samples, count );

  if( movie_recording )
      movie_add_sound( samples, count );
  ay_engine_end_frame();
}

