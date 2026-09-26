/* artifacts.h: automation screen and audio artifacts
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

#ifndef FUSE_AUTOMATION_ARTIFACTS_H
#define FUSE_AUTOMATION_ARTIFACTS_H

#include <stddef.h>
#include "libspectrum.h"
#include "json.h"

/* Stable semantic observation stages in the result contract. */
#define AUTOMATION_ARTIFACT_STAGE_SCREEN "logical-display"
#define AUTOMATION_ARTIFACT_STAGE_AUDIO  "final-mix"

void automation_artifacts_capture_screen( const libspectrum_byte *pixels,
                                          size_t width, size_t height );
void automation_artifacts_capture_pcm( const libspectrum_signed_word *samples,
                                       int count, int sample_rate,
                                       int channels );
void automation_artifacts_audio_initialized( int sample_rate, int channels );
int automation_artifacts_write( const char *directory, int capture_screen,
                                int capture_audio );
void automation_artifacts_write_json( automation_json *json );
void automation_artifacts_end( void );

#endif
