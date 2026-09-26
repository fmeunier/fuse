/* automation_json.c: tests for the automation streaming JSON writer
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
#include <stdio.h>
#include <string.h>
#include "automation/json.h"

int
main( void )
{
  automation_json json;
  char output[256];
  FILE *file = tmpfile();
  if( !file ) return 1;
  automation_json_init( &json, file );
  automation_json_object_begin( &json, NULL );
  automation_json_string( &json, "escaped", "quote\" slash\\ line\n\001" );
  automation_json_array_begin( &json, "items" );
  automation_json_object_begin( &json, NULL );
  automation_json_ulong( &json, "number", 7 );
  automation_json_boolean( &json, "flag", 1 );
  automation_json_end( &json );
  automation_json_end( &json );
  /* An optional member is represented by simply not emitting it. */
  automation_json_end( &json );
  if( automation_json_error( &json ) ) return 1;
  rewind( file );
  if( !fgets( output, sizeof( output ), file ) ) return 1;
  fclose( file );
  return strcmp( output,
                 "{\"escaped\":\"quote\\\" slash\\\\ line\\n\\u0001\","
                 "\"items\":[{\"number\":7,\"flag\":true}]}" ) != 0;
}
