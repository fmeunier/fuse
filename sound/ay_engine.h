/* ay_engine.h: AY sound generation */

#ifndef FUSE_AY_ENGINE_H
#define FUSE_AY_ENGINE_H

#include "libspectrum.h"
#include "sound/blipbuffer.h"

int ay_engine_init( int volume, int stereo );
void ay_engine_end( void );
void ay_engine_set_outputs( Blip_Buffer *left, Blip_Buffer *right, int stereo );
void ay_engine_render( libspectrum_dword tstates_per_frame );
void ay_engine_end_frame( void );
void ay_engine_write( int reg, int val, libspectrum_dword now );
void ay_engine_reset( void );

#endif
