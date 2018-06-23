// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#pragma once

#include "common.h"

typedef struct Adapter {
    PGconn *connection;
    int t_nesting;
    int native;
    VALUE encoder;
    VALUE decoder;
} Adapter;

void init_swift_db_postgres_adapter();
