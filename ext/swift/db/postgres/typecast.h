// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#pragma once

#include "common.h"

DLL_PRIVATE VALUE typecast_to_str(VALUE);
DLL_PRIVATE VALUE typecast_encode(VALUE);
DLL_PRIVATE VALUE typecast_decode(const char *, size_t, int);
DLL_PRIVATE VALUE typecast_description(VALUE types);
DLL_PRIVATE VALUE typecast_typemap(void);
DLL_PRIVATE void  init_swift_db_postgres_typecast();
