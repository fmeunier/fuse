/* wait.c: macOS 14.4 public address notification
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

#include <errno.h>
#include <mach/mach_time.h>
#include <os/os_sync_wait_on_address.h>

#include "compat/wait.h"

/* Address waits own no native handle. Storage belongs to the caller and must
   remain aligned and alive until both waiter and notifier are quiescent. */
struct compat_wait_t { int unused; };
static compat_wait_t address_wait;
const int compat_wait_uses_address = 1;
_Static_assert( sizeof( _Atomic uint32_t ) == 4,
                "Darwin address wait requires four-byte atomic storage" );

compat_wait_t *
compat_wait_alloc( void )
{
  return &address_wait;
}

void
compat_wait_free( compat_wait_t *wait )
{
  (void)wait;
}

compat_wait_deadline
compat_wait_until( unsigned int milliseconds )
{
  mach_timebase_info_data_t timebase;
  mach_timebase_info( &timebase );
  return mach_absolute_time() +
         (uint64_t)milliseconds * 1000000 * timebase.denom / timebase.numer;
}

int
compat_wait_expired( compat_wait_deadline end )
{
  return mach_absolute_time() >= end;
}

int
compat_wait_discard( compat_wait_t *wait )
{
  (void)wait;
  return 0;
}

int
compat_wait_wake( compat_wait_t *wait, _Atomic uint32_t *address )
{
  (void)wait;
  if( os_sync_wake_by_address_any( address, 4,
                                   OS_SYNC_WAKE_BY_ADDRESS_NONE ) < 0 )
    return errno == ENOENT ? 0 : errno;
  return 0;
}

int
compat_wait_park( compat_wait_t *wait, _Atomic uint32_t *address,
                  uint32_t expected, compat_wait_deadline end )
{
  (void)wait;
  if( os_sync_wait_on_address_with_deadline( address, expected, 4,
                                             OS_SYNC_WAIT_ON_ADDRESS_NONE,
                                             OS_CLOCK_MACH_ABSOLUTE_TIME,
                                             end ) < 0 )
    return errno;
  return 0;
}
