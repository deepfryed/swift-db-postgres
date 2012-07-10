// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "adapter.h"
#include "typecast.h"

/* declaration */
VALUE cDPA, sUser;
VALUE db_postgres_result_load(VALUE, PGresult *);
VALUE db_postgres_result_allocate(VALUE);

typedef struct Adapter {
    PGconn *connection;
    int t_nesting;
} Adapter;

/* definition */
Adapter* db_postgres_adapter_handle(VALUE self) {
    Adapter *a;
    Data_Get_Struct(self, Adapter, a);
    if (!a)
        rb_raise(eSwiftRuntimeError, "Invalid postgres adapter");
    return a;
}

Adapter* db_postgres_adapter_handle_safe(VALUE self) {
    Adapter *a = db_postgres_adapter_handle(self);
    if (!a->connection)
        rb_raise(eSwiftConnectionError, "postgres database is not open");
    return a;
}

VALUE db_postgres_adapter_deallocate(Adapter *a) {
    if (a && a->connection)
        PQfinish(a->connection);
    if (a)
        free(a);
}

VALUE db_postgres_adapter_allocate(VALUE klass) {
    Adapter *a = (Adapter*)malloc(sizeof(Adapter));

    a->connection = 0;
    a->t_nesting  = 0;
    return Data_Wrap_Struct(klass, 0, db_postgres_adapter_deallocate, a);
}

/* TODO: log messages */
VALUE db_postgres_adapter_notice(VALUE self, char *message) {
    return Qtrue;
}

VALUE db_postgres_adapter_initialize(VALUE self, VALUE options) {
    char connection_info[1024];
    VALUE db, user, pass, host, port;
    Adapter *a = db_postgres_adapter_handle(self);

    if (TYPE(options) != T_HASH)
        rb_raise(eSwiftArgumentError, "options needs to be a hash");

    db   = rb_hash_aref(options, ID2SYM(rb_intern("db")));
    user = rb_hash_aref(options, ID2SYM(rb_intern("user")));
    pass = rb_hash_aref(options, ID2SYM(rb_intern("pass")));
    host = rb_hash_aref(options, ID2SYM(rb_intern("host")));
    port = rb_hash_aref(options, ID2SYM(rb_intern("port")));

    if (NIL_P(db))
        rb_raise(eSwiftConnectionError, "Invalid db name");
    if (NIL_P(host))
        host = rb_str_new2("127.0.0.1");
    if (NIL_P(port))
        port = rb_str_new2("5432");
    if (NIL_P(user))
        user = sUser;

    snprintf(connection_info, 1024, "dbname='%s' user='%s' password='%s' host='%s' port='%s' sslmode='allow'",
        CSTRING(db), CSTRING(user), CSTRING(pass), CSTRING(host), CSTRING(port));

    a->connection = PQconnectdb(connection_info);

    if (!a->connection)
        rb_raise(eSwiftRuntimeError, "unable to allocate database handle");
    if (PQstatus(a->connection) == CONNECTION_BAD)
        rb_raise(eSwiftConnectionError, PQerrorMessage(a->connection));

    PQsetNoticeProcessor(a->connection, (PQnoticeProcessor)db_postgres_adapter_notice, (void*)self);
    PQsetClientEncoding(a->connection, "utf8");
    return self;
}

void db_postgres_adapter_check_result(PGresult *result) {
}

VALUE db_postgres_adapter_execute(int argc, VALUE *argv, VALUE self) {
    char buffer[256];
    char **bind_args_data = 0;
    int n, *bind_args_size = 0;
    PGresult *pg_result;
    VALUE sql, bind, data;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    rb_scan_args(argc, argv, "10*", &sql, &bind);
    sql = db_postgres_normalized_sql(sql);

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

        pg_result = PQexecParams(a->connection, CSTRING(sql), RARRAY_LEN(bind), 0,
            (const char * const *)bind_args_data, bind_args_size, 0, 0);

        rb_gc_enable();
        free(bind_args_size);
        free(bind_args_data);
    }
    else {
        pg_result = PQexec(a->connection, CSTRING(sql));
    }

    db_postgres_adapter_check_result(pg_result);
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), pg_result);
}

void init_swift_db_postgres_adapter() {
    rb_require("etc");
    sUser  = rb_funcall(CONST_GET(rb_mKernel, "Etc"), rb_intern("getlogin"), 0);
    cDPA   = rb_define_class_under(mDB, "Postgres", rb_cObject);

    rb_define_alloc_func(cDPA, db_postgres_adapter_allocate);

    rb_define_method(cDPA, "initialize",  db_postgres_adapter_initialize,   1);
    rb_define_method(cDPA, "execute",     db_postgres_adapter_execute,     -1);
/*
    rb_define_method(cDPA, "prepare",     db_postgres_adapter_prepare,      1);
    rb_define_method(cDPA, "begin",       db_postgres_adapter_begin,       -1);
    rb_define_method(cDPA, "commit",      db_postgres_adapter_commit,      -1);
    rb_define_method(cDPA, "rollback",    db_postgres_adapter_rollback,    -1);
    rb_define_method(cDPA, "transaction", db_postgres_adapter_transaction, -1);
    rb_define_method(cDPA, "close",       db_postgres_adapter_close,        0);
    rb_define_method(cDPA, "closed?",     db_postgres_adapter_closed_q,     0);
*/

    rb_global_variable(&sUser);
}
