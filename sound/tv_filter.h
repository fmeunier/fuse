/* tv_filter.h: TV speaker response
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

#ifndef FUSE_TV_FILTER_H
#define FUSE_TV_FILTER_H

#define TV_FILTER_HIGH_PASS_FREQUENCY 100.0
#define TV_FILTER_LOW_PASS_FREQUENCY 10000.0

typedef struct tv_filter_tag {
  double high_pass_decay;
  double low_pass_alpha;
  double previous_input;
  double high_pass_state;
  double low_pass_state;
  int initialized;
} tv_filter_t;

int tv_filter_configure( tv_filter_t *filter, int sample_rate );
void tv_filter_reset( tv_filter_t *filter );
double tv_filter_apply( tv_filter_t *filter, double input );

#endif                  /* #ifndef FUSE_TV_FILTER_H */
