/* nullsound.c: dummy sound routines
   Copyright (c) 2003-2007 Philip Kendall

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
#include "sound.h"

#ifdef ENABLE_AUTOMATION
#include "automation/artifacts.h"
#include "automation/automation.h"
#endif

static int sample_rate, channels;

int
sound_lowlevel_init( const char *device, int *freqptr, int *stereoptr )
{
#ifdef ENABLE_AUTOMATION
  if( automation_capture_audio_enabled() ) {
    sample_rate = *freqptr;
    channels = *stereoptr == SOUND_STEREO_AY_NONE ? 1 : 2;
    automation_artifacts_audio_initialized( sample_rate, channels );
    return 0;
  }
#endif
  return 1;
}

void
sound_lowlevel_end( void )
{
}

void
sound_lowlevel_frame( libspectrum_signed_word *data, int len )
{
#ifdef ENABLE_AUTOMATION
  automation_capture_pcm( data, len, sample_rate, channels );
#endif
}
