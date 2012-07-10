// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "common.h"
#include <ruby/re.h>
#include <uuid/uuid.h>

#define BEG(no) (regs->beg[(no)])
#define END(no) (regs->end[(no)])

extern VALUE oRegex;
VALUE rb_uuid_string() {
    size_t n;
    uuid_t uuid;
    char uuid_hex[sizeof(uuid_t) * 2 + 1];

    uuid_generate(uuid);
    for (n = 0; n < sizeof(uuid_t); n++)
        sprintf(uuid_hex + n * 2 + 1, "%02x", uuid[n]);

    uuid_hex[0] = 'u';
    return rb_str_new(uuid_hex, sizeof(uuid_t) * 2 + 1);
}

/* TODO: is this slower than rb_funcall(sql, rb_intern("sub!"), ...) */
VALUE db_postgres_normalized_sql(VALUE sql) {
    char buffer[256];
    int n = 1, begin, end;
    struct re_registers *regs;
    VALUE match, repl;

    sql = rb_obj_dup(TO_S(sql));
    while (rb_reg_search(oRegex, sql, 0, 0) >= 0) {
        snprintf(buffer, 256, "$%d", n++);
        match = rb_backref_get();
        regs  = RMATCH_REGS(match);
        begin = BEG(0);
        end   = END(0);
        repl  = rb_str_new(RSTRING_PTR(sql), begin);
        rb_str_concat(repl, rb_str_new2(buffer));
        rb_str_concat(repl, rb_str_new(RSTRING_PTR(sql) + end, RSTRING_LEN(sql) - end + 1));
        sql   = repl;
    }
    return sql;
}

void db_postgres_check_result(PGresult *result) {
    VALUE error;
    switch (PQresultStatus(result)) {
        case PGRES_TUPLES_OK:
        case PGRES_COPY_OUT:
        case PGRES_COPY_IN:
        case PGRES_EMPTY_QUERY:
        case PGRES_COMMAND_OK:
            break;
        case PGRES_BAD_RESPONSE:
        case PGRES_FATAL_ERROR:
        case PGRES_NONFATAL_ERROR:
            error = rb_str_new2(PQresultErrorMessage(result));
            PQclear(result);
            rb_raise(eSwiftRuntimeError, "%s", CSTRING(error));
            break;
        default:
            PQclear(result);
            rb_raise(eSwiftRuntimeError, "unknown error, check logs");
    }
}
