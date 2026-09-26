/* speaker_filter.h: Built-in Spectrum speaker acoustic response
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

#ifndef FUSE_SPEAKER_FILTER_H
#define FUSE_SPEAKER_FILTER_H

typedef struct speaker_filter_tag {
  double b0, b1, b2;
  double a1, a2;
  double z1, z2;
  int initialized;
} speaker_filter_t;

int speaker_filter_configure( speaker_filter_t *filter, int sample_rate );
void speaker_filter_reset( speaker_filter_t *filter );
double speaker_filter_apply( speaker_filter_t *filter, double input );

#endif                  /* #ifndef FUSE_SPEAKER_FILTER_H */
