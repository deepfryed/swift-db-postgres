// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "statement.h"
#include "adapter.h"
#include "typecast.h"

/* declaration */

VALUE cDPS;

VALUE    db_postgres_result_allocate(VALUE);
VALUE    db_postgres_result_load(VALUE, PGresult*);
Adapter* db_postgres_adapter_handle_safe(VALUE);

typedef struct Statement {
    char id[64];
    VALUE adapter;
} Statement;

/* definition */

Statement* db_postgres_statement_handle(VALUE self) {
    Statement *s;
    Data_Get_Struct(self, Statement, s);
    if (!s)
        rb_raise(eSwiftRuntimeError, "Invalid postgres statement");
    return s;
}

void db_postgres_statement_mark(Statement *s) {
    if (s && s->adapter)
        rb_gc_mark_maybe(s->adapter);
}

VALUE db_postgres_statement_deallocate(Statement *s) {
    if (s)
        free(s);
}

VALUE db_postgres_statement_allocate(VALUE klass) {
    Statement *s = (Statement*)malloc(sizeof(Statement));
    return Data_Wrap_Struct(klass, db_postgres_statement_mark, db_postgres_statement_deallocate, s);
}

VALUE db_postgres_statement_initialize(VALUE self, VALUE adapter, VALUE sql) {
    PGresult *result;
    PGconn *connection;
    Statement *s = db_postgres_statement_handle(self);

    snprintf(s->id, 64, "s%s", CSTRING(rb_uuid_string()));
    s->adapter = adapter;
    rb_gc_mark(s->adapter);

    connection = db_postgres_adapter_handle_safe(adapter)->connection;
    result     = PQprepare(connection, s->id, CSTRING(db_postgres_normalized_sql(sql)), 0, 0);

    db_postgres_check_result(result);
    PQclear(result);
    return self;
}

VALUE db_postgres_statement_release(VALUE self) {
    char command[128];
    PGresult *result;
    PGconn *connection;

    Statement *s = db_postgres_statement_handle(self);
    connection   = db_postgres_adapter_handle_safe(s->adapter)->connection;

    if (connection && PQstatus(connection) == CONNECTION_OK) {
        snprintf(command, 128, "deallocate %s", s->id);
        result = PQexec(connection, command);
        db_postgres_check_result(result);
        PQclear(result);
        return Qtrue;
    }

    return Qfalse;
}

VALUE db_postgres_statement_execute(int argc, VALUE *argv, VALUE self) {
    PGresult *result;
    PGconn *connection;
    char **bind_args_data = 0;
    int n, *bind_args_size = 0;
    VALUE bind, data;

    Statement *s = db_postgres_statement_handle(self);
    connection   = db_postgres_adapter_handle_safe(s->adapter)->connection;

    rb_scan_args(argc, argv, "00*", &bind);

    if (RARRAY_LEN(bind) > 0) {
        bind_args_size = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_data = (char **) malloc(sizeof(char *) * RARRAY_LEN(bind));

        rb_gc_disable();
        for (n = 0; n < RARRAY_LEN(bind); n++) {
            data = rb_ary_entry(bind, n);
            if (NIL_P(data)) {
                bind_args_size[n] = 0;
                bind_args_data[n] = 0;
            }
            else {
                data = typecast_to_string(data);
                bind_args_size[n] = RSTRING_LEN(data);
                bind_args_data[n] = RSTRING_PTR(data);
            }
        }

        result = PQexecPrepared(connection, s->id, RARRAY_LEN(bind), (const char* const *)bind_args_data, bind_args_size, 0, 0);

        rb_gc_enable();
        free(bind_args_size);
        free(bind_args_data);
    }
    else {
        result = PQexecPrepared(connection, s->id, RARRAY_LEN(bind), 0, 0, 0, 0);
    }

    db_postgres_check_result(result);
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), result);
}

void init_swift_db_postgres_statement() {
    cDPS = rb_define_class_under(cDPA, "Statement", rb_cObject);
    rb_define_alloc_func(cDPS, db_postgres_statement_allocate);
    rb_define_method(cDPS, "initialize", db_postgres_statement_initialize, 2);
    rb_define_method(cDPS, "execute",    db_postgres_statement_execute,   -1);
    rb_define_method(cDPS, "release",    db_postgres_statement_release,    0);
}


