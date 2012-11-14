// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "statement.h"
#include "adapter.h"
#include "typecast.h"
#include "gvl.h"

/* declaration */

VALUE cDPS;

VALUE    db_postgres_result_allocate(VALUE);
VALUE    db_postgres_result_load(VALUE, PGresult*);
Adapter* db_postgres_adapter_handle_safe(VALUE);

typedef struct Statement {
    char id[128];
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

Statement* db_postgres_statement_handle_safe(VALUE self) {
    Statement *s = db_postgres_statement_handle(self);
    if (!s->adapter)
        rb_raise(eSwiftRuntimeError, "Invalid postgres statement: no associated adapter");
    return s;
}

void db_postgres_statement_mark(Statement *s) {
    if (s && s->adapter)
        rb_gc_mark(s->adapter);
}

VALUE db_postgres_statement_deallocate(Statement *s) {
    if (s)
        free(s);
}

VALUE db_postgres_statement_allocate(VALUE klass) {
    Statement *s = (Statement*)malloc(sizeof(Statement));
    memset(s, 0, sizeof(Statement));
    return Data_Wrap_Struct(klass, db_postgres_statement_mark, db_postgres_statement_deallocate, s);
}

VALUE db_postgres_statement_initialize(VALUE self, VALUE adapter, VALUE sql) {
    PGresult *result;
    Statement *s = db_postgres_statement_handle(self);
    Adapter *a   = db_postgres_adapter_handle_safe(adapter);

    snprintf(s->id, 128, "s%s", CSTRING(rb_uuid_string()));
    s->adapter = adapter;

    if (!a->native)
        sql = db_postgres_normalized_sql(sql);

    result = PQprepare(a->connection, s->id, CSTRING(sql), 0, 0);
    db_postgres_check_result(result);
    PQclear(result);
    return self;
}

VALUE db_postgres_statement_release(VALUE self) {
    char command[256];
    PGresult *result;
    PGconn *connection;

    Statement *s = db_postgres_statement_handle_safe(self);
    connection   = db_postgres_adapter_handle_safe(s->adapter)->connection;

    if (connection && PQstatus(connection) == CONNECTION_OK) {
        snprintf(command, 256, "deallocate %s", s->id);
        result = PQexec(connection, command);
        db_postgres_check_result(result);
        PQclear(result);
        return Qtrue;
    }

    return Qfalse;
}

 GVL_NOLOCK_RETURN_TYPE nogvl_pq_exec_prepared(void *ptr) {
    Query *q = (Query *)ptr;
    PGresult *r  = PQexecPrepared(q->connection, q->command, q->n_args, (const char * const *)q->data, q->size, q->format, 0);
    return (GVL_NOLOCK_RETURN_TYPE)r;
}

VALUE db_postgres_statement_execute(int argc, VALUE *argv, VALUE self) {
    PGresult *result;
    PGconn *connection;
    char **bind_args_data = 0;
    int n, *bind_args_size = 0, *bind_args_fmt = 0;
    VALUE bind, data;

    Statement *s = db_postgres_statement_handle_safe(self);
    connection   = db_postgres_adapter_handle_safe(s->adapter)->connection;

    rb_scan_args(argc, argv, "00*", &bind);

    if (RARRAY_LEN(bind) > 0) {
        bind_args_size = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_fmt  = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_data = (char **) malloc(sizeof(char *) * RARRAY_LEN(bind));

        rb_gc_disable();
        rb_gc_register_address(&bind);
        for (n = 0; n < RARRAY_LEN(bind); n++) {
            data = rb_ary_entry(bind, n);
            if (NIL_P(data)) {
                bind_args_size[n] = 0;
                bind_args_data[n] = 0;
                bind_args_fmt[n]  = 0;
            }
            else {
                if (rb_obj_is_kind_of(data, rb_cIO) || rb_obj_is_kind_of(data, cStringIO))
                    bind_args_fmt[n] = 1;
                else
                    bind_args_fmt[n] = 0;
                data = typecast_to_string(data);
                bind_args_size[n] = RSTRING_LEN(data);
                bind_args_data[n] = RSTRING_PTR(data);
            }
        }

        Query q = {
            .connection = connection,
            .command    = s->id,
            .n_args     = RARRAY_LEN(bind),
            .data       = bind_args_data,
            .size       = bind_args_size,
            .format     = bind_args_fmt
        };

        result = (PGresult *)GVL_NOLOCK(nogvl_pq_exec_prepared, &q, RUBY_UBF_IO, 0);
        rb_gc_unregister_address(&bind);
        rb_gc_enable();
        free(bind_args_fmt);
        free(bind_args_size);
        free(bind_args_data);
    }
    else {
        Query q = {
            .connection = connection,
            .command    = s->id,
            .n_args     = 0,
            .data       = 0,
            .size       = 0,
            .format     = 0
        };
        result = (PGresult *)GVL_NOLOCK(nogvl_pq_exec_prepared, &q, RUBY_UBF_IO, 0);
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


