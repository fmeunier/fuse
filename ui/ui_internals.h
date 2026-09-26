/* ui/ui_internals.h: Interface between shared UI code and UI backends
   Copyright (c) 2026 Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef FUSE_UI_INTERNALS_H
#define FUSE_UI_INTERNALS_H

#include "ui/ui.h"

/* Backend implementations called by the formatting wrappers in ui.c. */
int ui_error_specific( ui_error_level severity, const char *message );
ui_confirm_save_t ui_confirm_save_specific( const char *message );
int ui_query_message( const char *message );

#endif				/* #ifndef FUSE_UI_INTERNALS_H */
