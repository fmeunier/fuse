/* dc_filter.c: First-order DC-blocking filter
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "config.h"

#include <math.h>

#include "sound/dc_filter.h"

#define DC_FILTER_PI 3.14159265358979323846
#define DC_FILTER_CUTOFF_HZ 16.0
#define DC_FILTER_DENORMAL_LIMIT 1.0e-20

int
dc_filter_configure( dc_filter_t *filter, int sample_rate )
{
  if( sample_rate <= 0 || DC_FILTER_CUTOFF_HZ >= sample_rate / 2.0 ) return 1;

  filter->decay = exp( -2.0 * DC_FILTER_PI * DC_FILTER_CUTOFF_HZ /
                       sample_rate );
  dc_filter_reset( filter );
  return 0;
}

void
dc_filter_reset( dc_filter_t *filter )
{
  filter->previous_input = 0.0;
  filter->state = 0.0;
  filter->initialized = 0;
}

double
dc_filter_apply( dc_filter_t *filter, double input )
{
  if( !filter->initialized ) {
    /* Begin a newly selected steady level without an artificial transient. */
    filter->previous_input = input;
    filter->initialized = 1;
    return 0.0;
  }

  filter->state = filter->decay *
                  ( filter->state + input - filter->previous_input );
  filter->previous_input = input;
  if( fabs( filter->state ) < DC_FILTER_DENORMAL_LIMIT ) filter->state = 0.0;

  return filter->state;
}
