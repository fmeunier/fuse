/* dc_filter.h: First-order DC-blocking filter
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef FUSE_DC_FILTER_H
#define FUSE_DC_FILTER_H

typedef struct dc_filter_tag {
  double decay;
  double previous_input;
  double state;
  int initialized;
} dc_filter_t;

int dc_filter_configure( dc_filter_t *filter, int sample_rate );
void dc_filter_reset( dc_filter_t *filter );
double dc_filter_apply( dc_filter_t *filter, double input );

#endif                  /* #ifndef FUSE_DC_FILTER_H */
