// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include <stdio.h>
#include "adapter.h"
#include "typecast.h"
#include "gvl.h"

/* declaration */
VALUE cDPA, sUser;
VALUE db_postgres_result_each(VALUE);
VALUE db_postgres_result_load(VALUE, PGresult *);
VALUE db_postgres_result_allocate(VALUE);
VALUE db_postgres_statement_allocate(VALUE);
VALUE db_postgres_statement_initialize(VALUE, VALUE, VALUE);

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
    memset(a, 0, sizeof(Adapter));
    return Data_Wrap_Struct(klass, 0, db_postgres_adapter_deallocate, a);
}

/* TODO: log messages */
VALUE db_postgres_adapter_notice(VALUE self, char *message) {
    return Qtrue;
}

void append_ssl_option(char *buffer, int size, VALUE ssl, char *key, char *fallback) {
    int offset = strlen(buffer);
    VALUE option = rb_hash_aref(ssl, ID2SYM(rb_intern(key)));

    if (NIL_P(option) && fallback)
        snprintf(buffer + offset, size - offset, " %s='%s'", key, fallback);

    if (!NIL_P(option))
        snprintf(buffer + offset, size - offset, " %s='%s'", key, CSTRING(option));
}

VALUE db_postgres_adapter_initialize(VALUE self, VALUE options) {
    char *connection_info;
    VALUE db, user, pass, host, port, ssl;
    Adapter *a = db_postgres_adapter_handle(self);

    if (TYPE(options) != T_HASH)
        rb_raise(eSwiftArgumentError, "options needs to be a hash");

    db   = rb_hash_aref(options, ID2SYM(rb_intern("db")));
    user = rb_hash_aref(options, ID2SYM(rb_intern("user")));
    pass = rb_hash_aref(options, ID2SYM(rb_intern("password")));
    host = rb_hash_aref(options, ID2SYM(rb_intern("host")));
    port = rb_hash_aref(options, ID2SYM(rb_intern("port")));
    ssl  = rb_hash_aref(options, ID2SYM(rb_intern("ssl")));

    if (NIL_P(db))
        rb_raise(eSwiftConnectionError, "Invalid db name");
    if (NIL_P(host))
        host = rb_str_new2("127.0.0.1");
    if (NIL_P(port))
        port = rb_str_new2("5432");
    if (NIL_P(user))
        user = sUser;

    if (!NIL_P(ssl) && TYPE(ssl) != T_HASH)
            rb_raise(eSwiftArgumentError, "ssl options needs to be a hash");

    connection_info = (char *)malloc(4096);
    snprintf(connection_info, 4096, "dbname='%s' user='%s' password='%s' host='%s' port='%s'",
        CSTRING(db), CSTRING(user), CSTRING(pass), CSTRING(host), CSTRING(port));

    if (!NIL_P(ssl)) {
        append_ssl_option(connection_info, 4096, ssl, "sslmode",      "allow");
        append_ssl_option(connection_info, 4096, ssl, "sslcert",      0);
        append_ssl_option(connection_info, 4096, ssl, "sslkey",       0);
        append_ssl_option(connection_info, 4096, ssl, "sslrootcert",  0);
        append_ssl_option(connection_info, 4096, ssl, "sslcrl",       0);
    }

    a->connection = PQconnectdb(connection_info);
    free(connection_info);

    if (!a->connection)
        rb_raise(eSwiftRuntimeError, "unable to allocate database handle");
    if (PQstatus(a->connection) == CONNECTION_BAD)
        rb_raise(eSwiftConnectionError, PQerrorMessage(a->connection));

    PQsetNoticeProcessor(a->connection, (PQnoticeProcessor)db_postgres_adapter_notice, (void*)self);
    PQsetClientEncoding(a->connection, "utf8");
    return self;
}

GVL_NOLOCK_RETURN_TYPE nogvl_pq_exec(void *ptr) {
    Query *q = (Query *)ptr;
    return (GVL_NOLOCK_RETURN_TYPE)PQexec(q->connection, q->command);
}

GVL_NOLOCK_RETURN_TYPE nogvl_pq_exec_params(void *ptr) {
    Query *q = (Query *)ptr;
    PGresult * r = PQexecParams(q->connection, q->command, q->n_args, 0, (const char * const *)q->data, q->size, q->format, 0);
    return (GVL_NOLOCK_RETURN_TYPE)r;
}

VALUE db_postgres_adapter_execute(int argc, VALUE *argv, VALUE self) {
    char **bind_args_data = 0;
    int n, *bind_args_size = 0, *bind_args_fmt = 0;
    PGresult *result;
    VALUE sql, bind, data;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    rb_scan_args(argc, argv, "10*", &sql, &bind);
    if (!a->native)
        sql = db_postgres_normalized_sql(sql);

    if (RARRAY_LEN(bind) > 0) {
        bind_args_size = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_fmt  = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_data = (char **) malloc(sizeof(char *) * RARRAY_LEN(bind));

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
            .connection = a->connection,
            .command    = CSTRING(sql),
            .n_args     = RARRAY_LEN(bind),
            .data       = bind_args_data,
            .size       = bind_args_size,
            .format     = bind_args_fmt
        };

        result = (PGresult *)GVL_NOLOCK(nogvl_pq_exec_params, &q, RUBY_UBF_IO, 0);
        rb_gc_unregister_address(&bind);
        free(bind_args_size);
        free(bind_args_data);
        free(bind_args_fmt);
    }
    else {
        Query q = {.connection = a->connection, .command = CSTRING(sql)};
        result = (PGresult *)GVL_NOLOCK(nogvl_pq_exec, &q, RUBY_UBF_IO, 0);
    }

    db_postgres_check_result(result);
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), result);
}

VALUE db_postgres_adapter_begin(int argc, VALUE *argv, VALUE self) {
    char command[256];
    VALUE savepoint;
    PGresult *result;

    Adapter *a = db_postgres_adapter_handle_safe(self);
    rb_scan_args(argc, argv, "01", &savepoint);

    if (a->t_nesting == 0) {
        result = PQexec(a->connection, "begin");
        db_postgres_check_result(result);
        PQclear(result);
        a->t_nesting++;
        if (NIL_P(savepoint))
            return Qtrue;
    }

    if (NIL_P(savepoint))
        savepoint = rb_uuid_string();

    snprintf(command, 256, "savepoint %s", CSTRING(savepoint));
    result = PQexec(a->connection, command);
    db_postgres_check_result(result);
    PQclear(result);

    a->t_nesting++;
    return savepoint;
}

VALUE db_postgres_adapter_commit(int argc, VALUE *argv, VALUE self) {
    VALUE savepoint;
    char command[256];
    PGresult *result;

    Adapter *a = db_postgres_adapter_handle_safe(self);
    rb_scan_args(argc, argv, "01", &savepoint);

    if (a->t_nesting == 0)
        return Qfalse;

    if (NIL_P(savepoint)) {
        result = PQexec(a->connection, "commit");
        db_postgres_check_result(result);
        PQclear(result);
        a->t_nesting--;
    }
    else {
        snprintf(command, 256, "release savepoint %s", CSTRING(savepoint));
        result = PQexec(a->connection, command);
        db_postgres_check_result(result);
        PQclear(result);
        a->t_nesting--;
    }
    return Qtrue;
}

VALUE db_postgres_adapter_rollback(int argc, VALUE *argv, VALUE self) {
    VALUE savepoint;
    char command[256];
    PGresult *result;

    Adapter *a = db_postgres_adapter_handle_safe(self);
    rb_scan_args(argc, argv, "01", &savepoint);

    if (a->t_nesting == 0)
        return Qfalse;

    if (NIL_P(savepoint)) {
        result = PQexec(a->connection, "rollback");
        db_postgres_check_result(result);
        PQclear(result);
        a->t_nesting--;
    }
    else {
        snprintf(command, 256, "rollback to savepoint %s", CSTRING(savepoint));
        result = PQexec(a->connection, command);
        db_postgres_check_result(result);
        PQclear(result);
        a->t_nesting--;
    }
    return Qtrue;
}

VALUE db_postgres_adapter_transaction(int argc, VALUE *argv, VALUE self) {
    int status;
    VALUE savepoint, block, block_result;

    Adapter *a = db_postgres_adapter_handle_safe(self);
    rb_scan_args(argc, argv, "01&", &savepoint, &block);

    if (NIL_P(block))
        rb_raise(eSwiftRuntimeError, "postgres transaction requires a block");

    if (a->t_nesting == 0) {
        db_postgres_adapter_begin(1, &savepoint, self);
        block_result = rb_protect(rb_yield, self, &status);
        if (!status) {
            db_postgres_adapter_commit(1, &savepoint, self);
            if (!NIL_P(savepoint))
                db_postgres_adapter_commit(0, 0, self);
        }
        else {
            db_postgres_adapter_rollback(1, &savepoint, self);
            if (!NIL_P(savepoint))
                db_postgres_adapter_rollback(0, 0, self);
            rb_jump_tag(status);
        }
    }
    else {
        if (NIL_P(savepoint))
            savepoint = rb_uuid_string();
        db_postgres_adapter_begin(1, &savepoint, self);
        block_result = rb_protect(rb_yield, self, &status);
        if (!status)
            db_postgres_adapter_commit(1, &savepoint, self);
        else {
            db_postgres_adapter_rollback(1, &savepoint, self);
            rb_jump_tag(status);
        }
    }

    return block_result;
}

VALUE db_postgres_adapter_close(VALUE self) {
    Adapter *a = db_postgres_adapter_handle(self);
    if (a->connection) {
        PQfinish(a->connection);
        a->connection = 0;
        return Qtrue;
    }
    return Qfalse;
}

VALUE db_postgres_adapter_closed_q(VALUE self) {
    Adapter *a = db_postgres_adapter_handle(self);
    return a->connection ? Qfalse : Qtrue;
}

VALUE db_postgres_adapter_ping(VALUE self) {
    Adapter *a = db_postgres_adapter_handle(self);
    return a->connection && PQstatus(a->connection) == CONNECTION_OK ? Qtrue : Qfalse;
}

VALUE db_postgres_adapter_prepare(VALUE self, VALUE sql) {
    return db_postgres_statement_initialize(db_postgres_statement_allocate(cDPS), self, sql);
}

VALUE db_postgres_adapter_escape(VALUE self, VALUE fragment) {
    int error;
    VALUE text = TO_S(fragment);
    char pg_escaped[RSTRING_LEN(text) * 2 + 1];
    Adapter *a = db_postgres_adapter_handle_safe(self);
    PQescapeStringConn(a->connection, pg_escaped, RSTRING_PTR(text), RSTRING_LEN(text), &error);

    if (error)
        rb_raise(eSwiftArgumentError, "invalid escape string: %s\n", PQerrorMessage(a->connection));

    return rb_str_new2(pg_escaped);
}

VALUE db_postgres_adapter_fileno(VALUE self) {
    Adapter *a = db_postgres_adapter_handle_safe(self);
    return INT2NUM(PQsocket(a->connection));
}

VALUE db_postgres_adapter_result(VALUE self) {
    PGresult *result, *rest;
    Adapter *a = db_postgres_adapter_handle_safe(self);
    while (1) {
        PQconsumeInput(a->connection);
        if (!PQisBusy(a->connection))
            break;
    }
    result = PQgetResult(a->connection);
    while ((rest = PQgetResult(a->connection))) PQclear(rest);
    db_postgres_check_result(result);
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), result);
}

VALUE db_postgres_adapter_native(VALUE self) {
    int status, native;
    VALUE result;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    native    = a->native;
    a->native = 1;
    result    = rb_protect(rb_yield, Qnil, &status);
    a->native = native;
    if (status)
        rb_jump_tag(status);
    return result;
}

VALUE db_postgres_adapter_native_set(VALUE self, VALUE flag) {
    Adapter *a = db_postgres_adapter_handle_safe(self);
    a->native = Qtrue == flag ? 1 : 0;
    return flag;
}

VALUE db_postgres_adapter_query(int argc, VALUE *argv, VALUE self) {
    VALUE sql, bind, data;
    char **bind_args_data = 0;
    int n, ok = 1, *bind_args_size = 0, *bind_args_fmt = 0;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    rb_scan_args(argc, argv, "10*", &sql, &bind);
    if (!a->native)
        sql = db_postgres_normalized_sql(sql);

    if (RARRAY_LEN(bind) > 0) {
        bind_args_size = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_fmt  = (int   *) malloc(sizeof(int)    * RARRAY_LEN(bind));
        bind_args_data = (char **) malloc(sizeof(char *) * RARRAY_LEN(bind));

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

        ok = PQsendQueryParams(a->connection, RSTRING_PTR(sql), RARRAY_LEN(bind), 0,
            (const char* const *)bind_args_data, bind_args_size, bind_args_fmt, 0);

        rb_gc_unregister_address(&bind);
        free(bind_args_size);
        free(bind_args_data);
        free(bind_args_fmt);
    }
    else
        ok = PQsendQuery(a->connection, RSTRING_PTR(sql));

    if (!ok)
        rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));

    if (rb_block_given_p()) {
        rb_thread_wait_fd(PQsocket(a->connection));
        return db_postgres_result_each(db_postgres_adapter_result(self));
    }
    else
        return Qtrue;
}

VALUE db_postgres_adapter_write(int argc, VALUE *argv, VALUE self) {
    char *sql;
    VALUE table, fields, io, data;
    PGresult *result;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    if (argc < 1 || argc > 3)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 1..3)", argc);

    table = io = fields = Qnil;
    switch (argc) {
        case 1:
            io = argv[0];
            break;
        case 2:
            table = argv[0];
            io    = argv[1];
            break;
        case 3:
            table  = argv[0];
            fields = argv[1];
            io     = argv[2];

            if (TYPE(fields) != T_ARRAY)
                rb_raise(eSwiftArgumentError, "fields needs to be an array");
            if (RARRAY_LEN(fields) < 1)
                fields = Qnil;
    }

    if (argc > 1) {
        sql = (char *)malloc(4096);
        if (NIL_P(fields))
            snprintf(sql, 4096, "copy %s from stdin", CSTRING(table));
        else
            snprintf(sql, 4096, "copy %s(%s) from stdin", CSTRING(table), CSTRING(rb_ary_join(fields, rb_str_new2(", "))));

        result = PQexec(a->connection, sql);
        free(sql);

        db_postgres_check_result(result);
        PQclear(result);
    }

    if (rb_respond_to(io, rb_intern("read"))) {
        while (!NIL_P((data = rb_funcall(io, rb_intern("read"), 1, INT2NUM(4096))))) {
            data = TO_S(data);
            if (PQputCopyData(a->connection, RSTRING_PTR(data), RSTRING_LEN(data)) != 1)
                rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));
        }
        if (PQputCopyEnd(a->connection, 0) != 1)
            rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));
    }
    else {
        io = TO_S(io);
        if (PQputCopyData(a->connection, RSTRING_PTR(io), RSTRING_LEN(io)) != 1)
            rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));
        if (PQputCopyEnd(a->connection, 0) != 1)
            rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));
    }

    result = PQgetResult(a->connection);
    db_postgres_check_result(result);
    if (!result)
        rb_raise(eSwiftRuntimeError, "invalid result at the end of COPY command");
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), result);
}

VALUE db_postgres_adapter_read(int argc, VALUE *argv, VALUE self) {
    int n, done = 0;
    char *sql, *data;
    PGresult *result;
    VALUE table, fields, io;
    Adapter *a = db_postgres_adapter_handle_safe(self);

    if (argc > 3)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 0..3)", argc);

    table = fields = io = Qnil;
    switch (argc) {
        case 0:
            if (!rb_block_given_p())
                rb_raise(eSwiftArgumentError, "#read needs an IO object to write to or a block to call");
            break;
        case 1:
            if (rb_respond_to(argv[0], rb_intern("write")))
                io = argv[0];
            else
                table = argv[0];
            break;
        case 2:
            table = argv[0];
            io    = argv[1];
            if (!rb_respond_to(io, rb_intern("write")))
                rb_raise(eSwiftArgumentError, "#read needs an IO object that responds to #write");
            break;
        case 3:
            table  = argv[0];
            fields = argv[1];
            io     = argv[2];
            if (!rb_respond_to(io, rb_intern("write")))
                rb_raise(eSwiftArgumentError, "#read needs an IO object that responds to #write");
            if (TYPE(fields) != T_ARRAY)
                rb_raise(eSwiftArgumentError, "fields needs to be an array");
            if (RARRAY_LEN(fields) < 1)
                fields = Qnil;
    }


    if (!NIL_P(table)) {
        sql = (char *)malloc(4096);
        if (NIL_P(fields))
            snprintf(sql, 4096, "copy %s to stdout", CSTRING(table));
        else
            snprintf(sql, 4096, "copy %s(%s) to stdout", CSTRING(table), CSTRING(rb_ary_join(fields, rb_str_new2(", "))));

        result = PQexec(a->connection, sql);
        free(sql);

        db_postgres_check_result(result);
        PQclear(result);
    }

    while (!done) {
        switch ((n = PQgetCopyData(a->connection, &data, 0))) {
            case -1: done = 1; break;
            case -2: rb_raise(eSwiftRuntimeError, "%s", PQerrorMessage(a->connection));
            default:
                if (n > 0) {
                    if (NIL_P(io))
                        rb_yield(rb_str_new(data, n));
                    else
                        rb_funcall(io, rb_intern("write"), 1, rb_str_new(data, n));
                    PQfreemem(data);
                }
        }
    }

    result = PQgetResult(a->connection);
    db_postgres_check_result(result);
    if (!result)
        rb_raise(eSwiftRuntimeError, "invalid result at the end of COPY command");
    return db_postgres_result_load(db_postgres_result_allocate(cDPR), result);
}

void init_swift_db_postgres_adapter() {
    rb_require("etc");
    sUser  = rb_funcall(CONST_GET(rb_mKernel, "Etc"), rb_intern("getlogin"), 0);
    cDPA   = rb_define_class_under(mDB, "Postgres", rb_cObject);

    rb_define_alloc_func(cDPA, db_postgres_adapter_allocate);

    rb_define_method(cDPA, "initialize",  db_postgres_adapter_initialize,   1);
    rb_define_method(cDPA, "execute",     db_postgres_adapter_execute,     -1);
    rb_define_method(cDPA, "prepare",     db_postgres_adapter_prepare,      1);
    rb_define_method(cDPA, "begin",       db_postgres_adapter_begin,       -1);
    rb_define_method(cDPA, "commit",      db_postgres_adapter_commit,      -1);
    rb_define_method(cDPA, "rollback",    db_postgres_adapter_rollback,    -1);
    rb_define_method(cDPA, "transaction", db_postgres_adapter_transaction, -1);
    rb_define_method(cDPA, "close",       db_postgres_adapter_close,        0);
    rb_define_method(cDPA, "closed?",     db_postgres_adapter_closed_q,     0);
    rb_define_method(cDPA, "ping",        db_postgres_adapter_ping,         0);
    rb_define_method(cDPA, "escape",      db_postgres_adapter_escape,       1);
    rb_define_method(cDPA, "fileno",      db_postgres_adapter_fileno,       0);
    rb_define_method(cDPA, "query",       db_postgres_adapter_query,       -1);
    rb_define_method(cDPA, "result",      db_postgres_adapter_result,       0);
    rb_define_method(cDPA, "write",       db_postgres_adapter_write,       -1);
    rb_define_method(cDPA, "read",        db_postgres_adapter_read,        -1);

    rb_define_method(cDPA, "native_bind_format",  db_postgres_adapter_native,     0);
    rb_define_method(cDPA, "native_bind_format=", db_postgres_adapter_native_set, 1);

    rb_global_variable(&sUser);
}
