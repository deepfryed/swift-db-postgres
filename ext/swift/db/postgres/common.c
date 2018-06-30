// vim:ts=4:sts=4:sw=4:expandtab

// (c) Bharanee Rathna 2012

#include "common.h"
#include <math.h>
#include <uuid/uuid.h>

VALUE rb_uuid_string() {
    size_t n;
    uuid_t uuid;
    char uuid_hex[sizeof(uuid_t) * 2 + 1];

    uuid_generate(uuid);

    memset(uuid_hex, 0, sizeof(uuid_hex));
    for (n = 0; n < sizeof(uuid_t); n++)
        sprintf(uuid_hex + n * 2, "%02x", uuid[n]);

    return rb_str_new(uuid_hex, sizeof(uuid_t) * 2);
}

/* NOTE: very naive, no regex etc. */
VALUE db_postgres_normalized_sql(VALUE sql) {
    VALUE result;
    int i = 0, j = 0, n = 1, size = RSTRING_LEN(sql) * 2, digits;
    char *ptr = RSTRING_PTR(sql), *normalized = malloc(size);
    if (!normalized)
        rb_raise(rb_eNoMemError, "normalize sql");

    while (i < RSTRING_LEN(sql)) {
        if (*ptr == '?')
            j += sprintf(normalized + j, "$%d", n++);
        else
            normalized[j++] = *ptr;

        digits = (int)floor(log10(n)) + 2;
        if (j + digits >= size)
            normalized = realloc(normalized, size += 4096);

        ptr++;
        i++;
    }

    result = rb_str_new(normalized, j);
    free(normalized);
    return result;
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
