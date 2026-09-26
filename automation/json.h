/* json.h: streaming JSON writer for development automation
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
#ifndef FUSE_AUTOMATION_JSON_H
#define FUSE_AUTOMATION_JSON_H

#include <stdio.h>

typedef struct automation_json {
  FILE *file;
  unsigned char first[16];
  unsigned char object[16];
  unsigned int depth;
  int error;
} automation_json;

void automation_json_init( automation_json *json, FILE *file );
void automation_json_object_begin( automation_json *json, const char *name );
void automation_json_array_begin( automation_json *json, const char *name );
void automation_json_end( automation_json *json );
void automation_json_string( automation_json *json, const char *name,
                             const char *value );
void automation_json_ulong( automation_json *json, const char *name,
                            unsigned long value );
void automation_json_boolean( automation_json *json, const char *name,
                              int value );
int automation_json_error( const automation_json *json );

#endif
