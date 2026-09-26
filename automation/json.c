/* json.c: small dependency-free streaming JSON writer
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
#ifdef ENABLE_AUTOMATION

#include <string.h>
#include "json.h"

static void
string( automation_json *json, const char *text )
{
  const unsigned char *p = (const unsigned char *)text;
  if( fputc( '"', json->file ) == EOF ) json->error = 1;
  for( ; *p; p++ ) {
    switch( *p ) {
    case '"': fputs( "\\\"", json->file ); break;
    case '\\': fputs( "\\\\", json->file ); break;
    case '\b': fputs( "\\b", json->file ); break;
    case '\f': fputs( "\\f", json->file ); break;
    case '\n': fputs( "\\n", json->file ); break;
    case '\r': fputs( "\\r", json->file ); break;
    case '\t': fputs( "\\t", json->file ); break;
    default:
      if( *p < 0x20 ) fprintf( json->file, "\\u%04x", *p );
      else fputc( *p, json->file );
    }
  }
  if( fputc( '"', json->file ) == EOF || ferror( json->file ) ) json->error = 1;
}

static void
member( automation_json *json, const char *name )
{
  if( json->depth ) {
    if( !json->first[json->depth - 1] ) fputc( ',', json->file );
    json->first[json->depth - 1] = 0;
    if( json->object[json->depth - 1] ) {
      string( json, name ); fputc( ':', json->file );
    }
  }
}

static void
begin( automation_json *json, const char *name, int object )
{
  member( json, name );
  if( json->depth >= sizeof( json->first ) ) {
    json->error = 1; return;
  }
  fputc( object ? '{' : '[', json->file );
  json->first[json->depth] = 1;
  json->object[json->depth++] = object;
}

void
automation_json_init( automation_json *json, FILE *file )
{
  memset( json, 0, sizeof( *json ) ); json->file = file;
}

void
automation_json_object_begin( automation_json *json, const char *name )
{
  begin( json, name, 1 );
}

void
automation_json_array_begin( automation_json *json, const char *name )
{
  begin( json, name, 0 );
}

void
automation_json_end( automation_json *json )
{
  if( !json->depth ) {
    json->error = 1; return;
  }
  json->depth--;
  fputc( json->object[json->depth] ? '}' : ']', json->file );
}

void
automation_json_string( automation_json *json, const char *name,
                        const char *value )
{
  member( json, name ); string( json, value );
}

void
automation_json_ulong( automation_json *json, const char *name,
                       unsigned long value )
{
  member( json, name );
  if( fprintf( json->file, "%lu", value ) < 0 ) json->error = 1;
}

void
automation_json_boolean( automation_json *json, const char *name, int value )
{
  member( json, name );
  if( fputs( value ? "true" : "false", json->file ) == EOF ) json->error = 1;
}

int
automation_json_error( const automation_json *json )
{
  return json->error || ferror( json->file ) || json->depth != 0;
}

#endif
