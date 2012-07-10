#pragma once

#define DLL_PRIVATE __attribute__ ((visibility ("hidden")))
#define CONST_GET(scope, constant) rb_funcall(scope, rb_intern("const_get"), 1, rb_str_new2(constant))
#define TO_S(v)    rb_funcall(v, rb_intern("to_s"), 0)
#define CSTRING(v) RSTRING_PTR(TO_S(v))

#include <ruby/ruby.h>
#include <ruby/encoding.h>

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern VALUE mSwift, mDB;
extern VALUE cDPA, cDPS, cDPR;
extern VALUE eSwiftError, eSwiftArgumentError, eSwiftRuntimeError, eSwiftConnectionError;

DLL_PRIVATE VALUE rb_uuid_string();
DLL_PRIVATE VALUE db_postgres_normalized_sql(VALUE);
