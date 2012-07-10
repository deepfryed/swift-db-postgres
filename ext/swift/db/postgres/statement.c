// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "statement.h"
#include "typecast.h"

/* declaration */

VALUE cDPS;

VALUE db_postgres_result_allocate(VALUE);
VALUE db_postgres_result_load(VALUE, PGresult*);

typedef struct Statement {
    char id[64];
    PGconn *connection;
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
}

VALUE db_postgres_statement_deallocate(Statement *s) {
    char command[128];
    if (s) {
        if (s->connection && PQstatus(s->connection) == CONNECTION_OK) {
            snprintf(command, 128, "deallocate %s", s->id);
            PQclear(PQexec(s->connection, command));
        }
        free(s);
    }
}

VALUE db_postgres_statement_allocate(VALUE klass) {
    Statement *s = (Statement*)malloc(sizeof(Statement));
    return Data_Wrap_Struct(klass, db_postgres_statement_mark, db_postgres_statement_deallocate, s);
}

/*

VALUE db_postgres_statement_initialize(VALUE self, VALUE adapter, VALUE sql) {
    Statement *s = db_postgres_statement_handle(self);

    s->s       = 0;
    s->c       = db_postgres_adapter_handle_safe(adapter)->connection;
    s->adapter = adapter;

    if (postgres_prepare_v2(s->c, RSTRING_PTR(sql), RSTRING_LEN(sql), &(s->s), 0) != SQLITE_OK)
        rb_raise(eSwiftRuntimeError, "%s\nSQL: %s", postgres_errmsg(s->c), RSTRING_PTR(sql));

    return self;
}

VALUE db_postgres_statement_insert_id(VALUE self) {
    Statement *s = db_postgres_statement_handle(self);
    return SIZET2NUM(postgres_last_insert_rowid(s->c));
}

VALUE db_postgres_statement_execute(int argc, VALUE *argv, VALUE self) {
    int expect, n;
    VALUE bind, result;

    Statement *s = db_postgres_statement_handle(self);

    rb_scan_args(argc, argv, "00*", &bind);
    expect = postgres_bind_parameter_count(s->s);
    if (expect != RARRAY_LEN(bind))
        rb_raise(eSwiftArgumentError, "expected %d bind values got %d", expect, RARRAY_LEN(bind));

    for (n = 0; n < expect; n++) {
        VALUE value = rb_ary_entry(bind, n);
        if (NIL_P(value))
            postgres_bind_null(s->s, n + 1);
        else {
            VALUE text = typecast_to_string(value);
            postgres_bind_text(s->s, n + 1, RSTRING_PTR(text), RSTRING_LEN(text), 0);
        }
    }

    result = db_postgres_result_allocate(cDSR);
    db_postgres_result_initialize(result, self);
    db_postgres_result_consume(result);
    return result;
}
*/

void init_swift_db_postgres_statement() {
    cDPS = rb_define_class_under(cDPA, "Statement", rb_cObject);
    rb_define_alloc_func(cDPS, db_postgres_statement_allocate);
    /*
    rb_define_method(cDPS, "initialize", db_postgres_statement_initialize, 2);
    rb_define_method(cDPS, "execute",    db_postgres_statement_execute,   -1);
    rb_define_method(cDPS, "insert_id",  db_postgres_statement_insert_id,  0);
    */
}


