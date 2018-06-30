// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "result.h"
#include <stdlib.h>

/* declaration */

typedef struct Result {
    PGresult *result;
    VALUE fields;
    VALUE types;
    VALUE decoder;
    size_t selected;
    size_t affected;
    size_t insert_id;
} Result;

VALUE cDPR;

/* definition */

Result* db_postgres_result_handle(VALUE self) {
    Result *r;
    Data_Get_Struct(self, Result, r);
    if (!r)
        rb_raise(eSwiftRuntimeError, "Invalid postgres result");
    return r;
}

void db_postgres_result_mark(Result *r) {
    if (r) {
        if (r->fields)
            rb_gc_mark(r->fields);
        if (r->types)
            rb_gc_mark(r->types);
    }
}

VALUE db_postgres_result_deallocate(Result *r) {
    if (r) {
        if (r->result)
            PQclear(r->result);
        free(r);
    }
    return Qtrue;
}

VALUE db_postgres_result_allocate(VALUE klass) {
    Result *r = (Result*)malloc(sizeof(Result));
    memset(r, 0, sizeof(Result));
    return Data_Wrap_Struct(klass, db_postgres_result_mark, db_postgres_result_deallocate, r);
}

VALUE db_postgres_result_load(VALUE self, PGresult *result, VALUE decoder) {
    size_t n, rows, cols;
    const char *type, *data;

    Result *r    = db_postgres_result_handle(self);
    r->fields    = rb_ary_new();
    r->types     = rb_ary_new();
    r->result    = result;
    r->affected  = atol(PQcmdTuples(result));
    r->selected  = PQntuples(result);
    r->insert_id = 0;
    r->decoder   = decoder;

    rows = PQntuples(result);
    cols = PQnfields(result);
    if (rows > 0)
        r->insert_id = PQgetisnull(result, 0, 0) ? 0 : atol(PQgetvalue(result, 0, 0));

    for (n = 0; n < cols; n++) {
        /* this must be a command execution result without field information */
        if (!(data = PQfname(result, n)))
            break;
        rb_ary_push(r->fields, ID2SYM(rb_intern(data)));
        rb_ary_push(r->types, INT2NUM(PQftype(result, n)));
    }

    return self;
}

VALUE db_postgres_result_each(VALUE self) {
    VALUE tuple;
    int row, col;
    Result *r = db_postgres_result_handle(self);

    if (!r->result)
        return Qnil;

    for (row = 0; row < PQntuples(r->result); row++) {
        tuple = rb_hash_new();
        for (col = 0; col < PQnfields(r->result); col++) {
            if (PQgetisnull(r->result, row, col))
                rb_hash_aset(tuple, rb_ary_entry(r->fields, col), Qnil);
            else {
                size_t csize = PQgetlength(r->result, row, col);
                const char *cvalue = PQgetvalue(r->result, row, col);
                VALUE value = typecast_decode(cvalue, csize, NUM2INT(rb_ary_entry(r->types, col)));
                if (NIL_P(value)) {
                    if (r->decoder) {
                        value = rb_funcall(
                            r->decoder,
                            rb_intern("call"),
                            3,
                            rb_ary_entry(r->fields, col),
                            rb_ary_entry(r->types, col),
                            rb_str_new(cvalue, csize)
                        );
                    }
                    else {
                        value = rb_str_new(cvalue, csize);
                    }
                }

                rb_hash_aset(tuple, rb_ary_entry(r->fields, col), value);
            }
        }
        rb_yield(tuple);
    }
    return Qtrue;
}

VALUE db_postgres_result_get(VALUE self, VALUE g_row, VALUE g_col) {
    VALUE tuple;
    int row = NUM2INT(g_row), col = NUM2INT(g_col);
    Result *r = db_postgres_result_handle(self);

    if (!r->result)
        return Qnil;

    if (row >= PQntuples(r->result) || col >= PQnfields(r->result))
        return Qnil;

    if (PQgetisnull(r->result, row, col))
        return Qnil;

    size_t csize = PQgetlength(r->result, row, col);
    const char *cvalue = PQgetvalue(r->result, row, col);
    VALUE value = typecast_decode(cvalue, csize, NUM2INT(rb_ary_entry(r->types, col)));
    if (NIL_P(value)) {
        if (r->decoder) {
            value = rb_funcall(
                r->decoder,
                rb_intern("call"),
                3,
                rb_ary_entry(r->fields, col),
                rb_ary_entry(r->types, col),
                rb_str_new(cvalue, csize)
            );
        }
        else {
            value = rb_str_new(cvalue, csize);
        }
    }

    return value;
}

VALUE db_postgres_result_selected_rows(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return SIZET2NUM(r->selected);
}

VALUE db_postgres_result_affected_rows(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return SIZET2NUM(r->selected > 0 ? 0 : r->affected);
}

VALUE db_postgres_result_fields(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return r->fields ? r->fields : rb_ary_new();
}

VALUE db_postgres_result_types(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return r->types ? typecast_description(r->types) : rb_ary_new();
}

VALUE db_postgres_result_insert_id(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return SIZET2NUM(r->insert_id);
}

VALUE db_postgres_result_clear(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    if (r->result) {
        PQclear(r->result);
        r->result = NULL;
    }
    return Qtrue;
}

void init_swift_db_postgres_result() {
    rb_require("bigdecimal");
    rb_require("stringio");
    rb_require("date");

    cDPR = rb_define_class_under(cDPA, "Result", rb_cObject);

    rb_include_module(cDPR, rb_mEnumerable);
    rb_define_alloc_func(cDPR, db_postgres_result_allocate);
    rb_define_method(cDPR, "each",          db_postgres_result_each,          0);
    rb_define_method(cDPR, "get",           db_postgres_result_get,           2);
    rb_define_method(cDPR, "selected_rows", db_postgres_result_selected_rows, 0);
    rb_define_method(cDPR, "affected_rows", db_postgres_result_affected_rows, 0);
    rb_define_method(cDPR, "fields",        db_postgres_result_fields,        0);
    rb_define_method(cDPR, "types",         db_postgres_result_types,         0);
    rb_define_method(cDPR, "insert_id",     db_postgres_result_insert_id,     0);
    rb_define_method(cDPR, "clear",         db_postgres_result_clear,         0);
}
