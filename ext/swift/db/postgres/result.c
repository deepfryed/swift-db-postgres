// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "result.h"
#include <stdlib.h>

/* declaration */

typedef struct Result {
    PGresult *result;
    VALUE fields;
    VALUE types;
    VALUE rows;
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
        if (!NIL_P(r->fields))
            rb_gc_mark_maybe(r->fields);
        if (!NIL_P(r->types))
            rb_gc_mark_maybe(r->types);
        if (!NIL_P(r->rows))
            rb_gc_mark_maybe(r->rows);
    }
}

VALUE db_postgres_result_deallocate(Result *r) {
    if (r) {
        if (r->result)
            PQclear(r->result);
        free(r);
    }
}

VALUE db_postgres_result_allocate(VALUE klass) {
    Result *r = (Result*)malloc(sizeof(Result));
    return Data_Wrap_Struct(klass, db_postgres_result_mark, db_postgres_result_deallocate, r);
}

VALUE db_postgres_result_load(VALUE self, PGresult *result) {
    size_t n, rows, cols;
    const char *type, *data;

    Result *r    = db_postgres_result_handle(self);
    r->fields    = rb_ary_new();
    r->types     = rb_ary_new();
    r->rows      = rb_ary_new();
    r->result    = result;
    r->affected  = atol(PQcmdTuples(result));
    r->selected  = PQntuples(result);
    r->insert_id = 0;

    rows = PQntuples(result);
    cols = PQnfields(result);
    if (rows > 0)
        r->insert_id = PQgetisnull(result, 0, 0) ? 0 : atol(PQgetvalue(result, 0, 0));

    for (n = 0; n < cols; n++) {
        rb_ary_push(r->fields, ID2SYM(rb_intern(PQfname(result, n))));
        switch (PQftype(result, n)) {
            case   16: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_BOOLEAN)); break;
            case   17: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_BLOB)); break;
            case   20:
            case   21:
            case   23: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_INT)); break;
            case   25: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_TEXT)); break;
            case  700:
            case  701: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_FLOAT)); break;
            case 1082: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_DATE)); break;
            case 1114:
            case 1184: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_TIMESTAMP)); break;
            case 1700: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_NUMERIC)); break;
            case 1083:
            case 1266: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_TIME)); break;
              default: rb_ary_push(r->types, INT2NUM(SWIFT_TYPE_TEXT));
        }
    }

    return self;
}

VALUE db_postgres_result_each(VALUE self) {
    int row, col;
    Result *r = db_postgres_result_handle(self);

    for (row = 0; row < PQntuples(r->result); row++) {
        VALUE tuple = rb_hash_new();
        for (col = 0; col < PQnfields(r->result); col++) {
            if (PQgetisnull(r->result, row, col))
                rb_hash_aset(tuple, rb_ary_entry(r->fields, col), Qnil);
            else {
                rb_hash_aset(
                    tuple,
                    rb_ary_entry(r->fields, col),
                    typecast_detect(
                        PQgetvalue(r->result, row, col),
                        PQgetlength(r->result, row, col),
                        NUM2INT(rb_ary_entry(r->types, col))
                    )
                );
            }
        }
        rb_yield(tuple);
    }
    return Qtrue;
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
    return r->fields;
}

VALUE db_postgres_result_types(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return typecast_description(r->types);
}

VALUE db_postgres_result_insert_id(VALUE self) {
    Result *r = db_postgres_result_handle(self);
    return SIZET2NUM(r->insert_id);
}

void init_swift_db_postgres_result() {
    rb_require("bigdecimal");
    rb_require("stringio");
    rb_require("date");

    cDPR = rb_define_class_under(cDPA, "Result", rb_cObject);

    rb_include_module(cDPR, rb_mEnumerable);
    rb_define_alloc_func(cDPR, db_postgres_result_allocate);
    rb_define_method(cDPR, "each",          db_postgres_result_each,          0);
    rb_define_method(cDPR, "selected_rows", db_postgres_result_selected_rows, 0);
    rb_define_method(cDPR, "affected_rows", db_postgres_result_affected_rows, 0);
    rb_define_method(cDPR, "fields",        db_postgres_result_fields,        0);
    rb_define_method(cDPR, "types",         db_postgres_result_types,         0);
    rb_define_method(cDPR, "insert_id",     db_postgres_result_insert_id,     0);
}
