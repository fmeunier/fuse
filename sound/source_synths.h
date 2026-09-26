/* source_synths.h: Peripheral sound source synthesis */

#ifndef FUSE_SOURCE_SYNTHS_H
#define FUSE_SOURCE_SYNTHS_H

#include "sound/blipbuffer.h"

/* Map a percentage volume setting to a Blip_Synth gain factor, clamping
 * out-of-range values: <0% -> silence, >100% -> full gain. */
double source_volume( int volume );

void source_synths_init( Blip_Buffer *left, Blip_Buffer *right, int stereo,
                         int specdrum_volume, int covox_volume,
                         int uspeech_volume );
void source_synths_end( void );
void source_synths_set_speech_output( Blip_Buffer *left, Blip_Buffer *right );
void source_synths_sp0256_write( libspectrum_dword at_tstates,
                                 libspectrum_signed_word val );

#endif
