// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "common.h"
#include "typecast.h"
#include "datetime.h"

const static struct {
    int oid;
    const char *type;
} oid_types[] = {
    {16, "boolean"},
    {17, "bytea"},
    {18, "char"},
    {19, "name"},
    {20, "bigint"},
    {21, "smallint"},
    {23, "integer"},
    {25, "text"},
    {114, "json"},
    {142, "xml"},
    {600, "point"},
    {601, "lseg"},
    {602, "path"},
    {603, "box"},
    {604, "polygon"},
    {628, "line"},
    {650, "cidr"},
    {700, "real"},
    {701, "double precision"},
    {702, "abstime"},
    {703, "reltime"},
    {704, "tinterval"},
    {718, "circle"},
    {774, "macaddr8"},
    {790, "money"},
    {829, "macaddr"},
    {869, "inet"},
    {1007, "int array"},
    {1009, "text array"},
    {1042, "character"},
    {1043, "character varying"},
    {1082, "date"},
    {1083, "time without time zone"},
    {1114, "timestamp without time zone"},
    {1184, "timestamp with time zone"},
    {1186, "interval"},
    {1266, "time with time zone"},
    {1560, "bit"},
    {1562, "bit varying"},
    {1700, "numeric"},
    {2277, "anyarray"},
    {2283, "anyelement"},
    {2776, "anynonarray"},
    {2950, "uuid"},
    {3500, "anyenum"},
    {3614, "tsvector"},
    {3615, "tsquery"},
    {3642, "gtsvector"},
    {3802, "jsonb"},
    {3831, "anyrange"},
    {3904, "int4range"},
    {3906, "numrange"},
    {3908, "tsrange"},
    {3910, "tstzrange"},
    {3912, "daterange"},
    {3926, "int8range"}
};

#define date_parse(klass, data,len) rb_funcall(datetime_parse(klass, data, len), fto_date, 0)

ID fnew, fto_date, fstrftime;
VALUE cBigDecimal, cStringIO;
VALUE dtformat;
VALUE cDateTime;

#define TO_UTF8(value) rb_str_encode(value, rb_str_new2("UTF-8"), 0, Qnil)
#define UTF8_STRING(value) strcmp(rb_enc_get(value)->name, "UTF-8") ? TO_UTF8(value) : value

VALUE typecast_to_str(VALUE value) {
    return UTF8_STRING(rb_funcall(value, rb_intern("to_s"), 0));
}

VALUE typecast_encode(VALUE value) {
    switch (TYPE(value)) {
        case T_STRING:
            return UTF8_STRING(value);
        case T_TRUE:
            return rb_str_new2("1");
        case T_FALSE:
            return rb_str_new2("0");
        case T_FLOAT:
        case T_BIGNUM:
        case T_FIXNUM:
        case T_SYMBOL:
        case T_RATIONAL:
            return typecast_to_str(value);
        default:
            if (rb_obj_is_kind_of(value, rb_cTime) || rb_obj_is_kind_of(value, cDateTime))
                return rb_funcall(value, fstrftime, 1, dtformat);
            else if (rb_obj_is_kind_of(value, rb_cIO) || rb_obj_is_kind_of(value, cStringIO))
                return rb_funcall(value, rb_intern("read"), 0);
            else
                return Qnil;
    }
}

VALUE typecast_decode(const char *data, size_t size, int oid) {
    VALUE value;
    unsigned char *bytea;
    size_t bytea_len;
    switch (oid) {
        case 16:
            return (data && (data[0] =='t' || data[0] == '1')) ? Qtrue : Qfalse;
        case 17:
            bytea = PQunescapeBytea((const unsigned char*)data, &bytea_len);
            value = rb_str_new((const char*)bytea, bytea_len);
            PQfreemem(bytea);
            return rb_funcall(cStringIO, fnew, 1, value);
        case 20:
        case 21:
        case 23:
            return rb_cstr2inum(data, 10);
        case 18:
        case 19:
        case 25:
            return rb_enc_str_new(data, size, rb_utf8_encoding());
        case 700:
        case 701:
            return rb_float_new(atof(data));
        case 1700:
            return rb_funcall(cBigDecimal, fnew, 1, rb_str_new(data, size));
        case 1114:
        case 1184:
            return datetime_parse(cSwiftDateTime, data, size);
        case 1082:
            return date_parse(cSwiftDateTime, data, size);
        default:
            return Qnil;
    }
}

VALUE typecast_description(VALUE types) {
    VALUE strings = rb_ary_new();
    for (size_t i = 0; i < RARRAY_LEN(types); i++) {
        VALUE type = Qnil;
        int oid = NUM2INT(rb_ary_entry(types, i));
        for (size_t j = 0; j < sizeof(oid_types) / sizeof(oid_types[0]); j++) {
            if (oid_types[j].oid == oid) {
                type = rb_str_new2(oid_types[j].type);
                break;
            }
        }
        rb_ary_push(strings, type);
    }
    return strings;
}

VALUE typecast_typemap(void) {
    VALUE typemap = rb_hash_new();
    for (size_t i = 0; i < sizeof(oid_types) / sizeof(oid_types[0]); i++)
        rb_hash_aset(typemap, INT2FIX(oid_types[i].oid), rb_str_new2(oid_types[i].type));
    return typemap;
}

void init_swift_db_postgres_typecast() {
    rb_require("bigdecimal");
    rb_require("stringio");
    rb_require("date");

    cStringIO   = CONST_GET(rb_mKernel, "StringIO");
    cBigDecimal = CONST_GET(rb_mKernel, "BigDecimal");
    cDateTime   = CONST_GET(rb_mKernel, "DateTime");
    fnew        = rb_intern("new");
    fto_date    = rb_intern("to_date");
    fstrftime   = rb_intern("strftime");
    dtformat    = rb_str_new2("%F %T.%N %z");

    rb_global_variable(&dtformat);
}
