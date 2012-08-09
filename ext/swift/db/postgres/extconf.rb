#!/usr/bin/env ruby

require 'mkmf'

$CFLAGS = '-std=c99 -Os'

inc_paths = %w(
  /usr/include/postgresql
  /usr/local/include/postgresql
  /opt/local/include
  /opt/local/include/postgresql90
  /opt/local/include/postgresql85
  /opt/local/include/postgresql84
  /sw/include
)

lib_paths = %w(
  /usr/lib
  /usr/local/lib
  /opt/lib
  /opt/local/lib
  /sw/lib
)

uuid_inc,  uuid_lib  = dir_config('uuid',  '/usr/include/uuid', '/usr/lib')
libpq_inc, libpq_lib = dir_config('postgresql')

find_header 'uuid/uuid.h', *inc_paths.dup.unshift(uuid_inc).compact
find_header 'libpq-fe.h',  *inc_paths.dup.unshift(libpq_inc).compact

find_library 'uuid', 'main', *lib_paths.dup.unshift(uuid_lib).compact
find_library 'pq',   'main', *lib_paths.dup.unshift(libpq_lib).compact

create_makefile('swift_db_postgres_ext')
