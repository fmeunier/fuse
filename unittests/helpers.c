/* helpers.c: shared helpers for Fuse's embedded white-box unit tests
   Copyright (c) 2008-2018 Philip Kendall
   Copyright (c) 2015 Stuart Brady
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

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk
*/

#include "config.h"

#include <stdio.h>

#include "machine.h"
#include "memory_pages.h"
#include "unittests/helpers.h"

#define TEST_ASSERT(x) do { if( !(x) ) { printf("Test assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while( 0 )

static int
assert_page( libspectrum_word base, libspectrum_word length, int source,
             int page )
{
  int base_index = base / MEMORY_PAGE_SIZE;
  int i;

  for( i = 0; i < length / MEMORY_PAGE_SIZE; i++ ) {
    TEST_ASSERT( memory_map_read[ base_index + i ].source == source );
    TEST_ASSERT( memory_map_read[ base_index + i ].page_num == page );
    TEST_ASSERT( memory_map_write[ base_index + i ].source == source );
    TEST_ASSERT( memory_map_write[ base_index + i ].page_num == page );
  }

  return 0;
}

int
unittests_assert_2k_page( libspectrum_word base, int source, int page )
{
  return assert_page( base, 0x0800, source, page );
}

int
unittests_assert_4k_page( libspectrum_word base, int source, int page )
{
  return assert_page( base, 0x1000, source, page );
}

int
unittests_assert_8k_page( libspectrum_word base, int source, int page )
{
  return assert_page( base, 0x2000, source, page );
}

int
unittests_assert_16k_page( libspectrum_word base, int source, int page )
{
  return assert_page( base, 0x4000, source, page );
}

int
unittests_assert_16k_ram_page( libspectrum_word base, int page )
{
  return unittests_assert_16k_page( base, memory_source_ram, page );
}

int
unittests_paging_test_48( int ram8000 )
{
  int r = 0;

  r += unittests_assert_16k_page( 0x0000, memory_source_rom, 0 );
  r += unittests_assert_16k_ram_page( 0x4000, 5 );
  r += unittests_assert_16k_ram_page( 0x8000, ram8000 );
  r += unittests_assert_16k_ram_page( 0xc000, 0 );
  TEST_ASSERT( memory_current_screen == 5 );

  return r;
}
