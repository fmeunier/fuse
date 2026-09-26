/* null.c: conditional virtual timer for development automation
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

#include "automation/automation.h"
#include "compat.h"
#include "timer/timer.h"

static double virtual_time;
static int was_active;

double
timer_get_time( void )
{
  if( automation_active() ) {
    if( !was_active ) {
      virtual_time = 0.0;
      was_active = 1;
    }
    return virtual_time;
  }
  was_active = 0;
  return compat_timer_get_time();
}

void
timer_sleep( int ms )
{
  if( automation_active() ) {
    if( !was_active ) {
      virtual_time = 0.0;
      was_active = 1;
    }
    virtual_time += ms / 1000.0;
  } else {
    was_active = 0;
    compat_timer_sleep( ms );
  }
}
