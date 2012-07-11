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

/* NOTE: very naive, no regex etc. */
/* TODO: a better ragel based replace thingamajigy */
VALUE db_postgres_normalized_sql(VALUE sql) {
    int i = 0, j = 0, n = 1;
    char normalized[RSTRING_LEN(sql) * 2], *ptr = RSTRING_PTR(sql);

    while (i < RSTRING_LEN(sql)) {
        if (*ptr == '?')
            j += snprintf(normalized + j, 4, "$%d", n++);
        else
            normalized[j++] = *ptr;
        ptr++;
        i++;
    }

    return rb_str_new(normalized, j);
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
            rb_raise(strstr(CSTRING(error), "bind message") ? eSwiftArgumentError : eSwiftRuntimeError, "%s", CSTRING(error));
            break;
        default:
            PQclear(result);
            rb_raise(eSwiftRuntimeError, "unknown error, check logs");
    }
}
