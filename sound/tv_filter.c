/* tv_filter.c: TV speaker response
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

#include <math.h>

#include "sound/tv_filter.h"

#define TV_FILTER_PI 3.14159265358979323846
#define TV_FILTER_DENORMAL_LIMIT 1.0e-20

/* The Spectrum 128K normally reproduced audio through an external television
 * or other consumer equipment, as do Fuse machines without a built-in beeper.
 * There is no single physical response to model, so this deliberately uses a
 * mild, representative 100 Hz-10 kHz response. */

int
tv_filter_configure( tv_filter_t *filter, int sample_rate )
{
  if( sample_rate <= 0 ||
      TV_FILTER_LOW_PASS_FREQUENCY >= 0.5 * sample_rate )
    return 1;

  filter->high_pass_decay =
    exp( -2.0 * TV_FILTER_PI * TV_FILTER_HIGH_PASS_FREQUENCY /
         sample_rate );
  filter->low_pass_alpha =
    -expm1( -2.0 * TV_FILTER_PI * TV_FILTER_LOW_PASS_FREQUENCY /
            sample_rate );
  tv_filter_reset( filter );

  return 0;
}

void
tv_filter_reset( tv_filter_t *filter )
{
  filter->previous_input = 0.0;
  filter->high_pass_state = 0.0;
  filter->low_pass_state = 0.0;
  filter->initialized = 0;
}

double
tv_filter_apply( tv_filter_t *filter, double input )
{
  double output;

  if( !filter->initialized ) {
    /* Begin a newly selected steady level without an artificial transient. */
    filter->previous_input = input;
    filter->initialized = 1;
    return 0.0;
  }

  filter->high_pass_state = filter->high_pass_decay *
                            ( filter->high_pass_state + input -
                              filter->previous_input );
  filter->previous_input = input;
  filter->low_pass_state += filter->low_pass_alpha *
                            ( filter->high_pass_state -
                              filter->low_pass_state );

  output = filter->low_pass_state;
  if( fabs( filter->high_pass_state ) < TV_FILTER_DENORMAL_LIMIT )
    filter->high_pass_state = 0.0;
  if( fabs( filter->low_pass_state ) < TV_FILTER_DENORMAL_LIMIT )
    filter->low_pass_state = 0.0;

  return output;
}
