/* result_settings.c: automation effective settings serialization
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

#include "machine.h"
#include "options.h"
#include "result_settings.h"
#include "settings.h"
#include "sound.h"
#include "sound/output_mixer.h"
#include "ui/scaler/scaler.h"

static const char *
stereo_name( int stereo )
{
  return stereo == SOUND_STEREO_AY_ACB ? "acb" :
         stereo == SOUND_STEREO_AY_ABC ? "abc" : "none";
}

static const char *
speaker_name( int speaker )
{
  return speaker == SOUND_SPEAKER_TYPE_AUTOMATIC ? "automatic" :
         speaker == SOUND_SPEAKER_TYPE_TV ? "tv" :
         speaker == SOUND_SPEAKER_TYPE_BEEPER ? "beeper" : "unfiltered";
}

static int
clamp_percent( int value )
{
  return value < 0 ? 0 : value > 100 ? 100 : value;
}

static void
write_audio_settings( automation_json *json )
{
  int speaker = output_mixer_speaker_type();
  unsigned int routes = sound_tv_source_routes(
                          speaker, machine_current->capabilities );

  automation_json_object_begin( json, "audio" );
  automation_json_ulong( json, "emulation_speed_percent",
                         settings_current.emulation_speed );
  automation_json_ulong( json, "effective_processor_speed_hz",
                         sound_get_effective_processor_speed() );
  automation_json_ulong( json, "sample_rate", settings_current.sound_freq );
  automation_json_string( json, "channel_mode",
                          sound_stereo_ay == SOUND_STEREO_AY_NONE ?
                          "mono" : "stereo" );
  automation_json_string( json, "ay_channel_arrangement",
                          stereo_name( sound_stereo_ay ) );
  automation_json_ulong( json, "ay_gain_percent",
                         clamp_percent( settings_current.volume_ay ) );
  automation_json_ulong( json, "beeper_gain_percent",
                         clamp_percent( settings_current.volume_beeper ) );
  automation_json_string( json, "speaker_mode",
                          speaker_name(
                            option_enumerate_sound_speaker_type() ) );
  automation_json_string( json, "effective_speaker_model",
                          speaker_name( speaker ) );
  automation_json_boolean( json, "route_ula_mic_to_tv",
                           routes & SOUND_ROUTE_ULA_MIC );
  automation_json_boolean( json, "route_builtin_ay_to_tv",
                           routes & SOUND_ROUTE_BUILTIN_AY );
  automation_json_boolean( json, "route_uspeech_to_tv",
                           routes & SOUND_ROUTE_USPEECH );
  automation_json_boolean( json, "loading_sound",
                           settings_current.sound_load );
  automation_json_boolean( json, "specdrum_enabled",
                           settings_current.specdrum );
  automation_json_ulong( json, "specdrum_gain_percent",
                         clamp_percent( settings_current.volume_specdrum ) );
  automation_json_boolean( json, "uspeech_enabled",
                           settings_current.uspeech );
  automation_json_ulong( json, "uspeech_gain_percent",
                         clamp_percent( settings_current.volume_uspeech ) );
  automation_json_ulong( json, "covox_gain_percent",
                         clamp_percent( settings_current.volume_covox ) );
  automation_json_end( json );
}

static void
write_display_settings( automation_json *json )
{
  automation_json_object_begin( json, "display" );
  automation_json_string( json, "colour_mode",
                          settings_current.bw_tv ? "greyscale" : "colour" );
  automation_json_string( json, "palette", "spectrum-rgb" );
  automation_json_string( json, "effective_scaler",
                          scaler_name( current_scaler == SCALER_NUM ?
                                       SCALER_NORMAL : current_scaler ) );
  automation_json_end( json );
}

void
automation_result_settings_write_json( automation_json *json,
                                       int capture_audio, int capture_screen )
{
  automation_json_object_begin( json, "settings" );
  automation_json_boolean( json, "autoload", settings_current.auto_load );
  automation_json_boolean( json, "fastload", settings_current.fastload );
  automation_json_boolean( json, "tape_traps", settings_current.tape_traps );
  automation_json_boolean( json, "loader_acceleration",
                           settings_current.accelerate_loader );
  automation_json_string( json, "phantom_typist_mode",
                          settings_current.phantom_typist_mode );

  if( capture_audio ) write_audio_settings( json );
  if( capture_screen ) write_display_settings( json );

  automation_json_end( json );
}

#endif
