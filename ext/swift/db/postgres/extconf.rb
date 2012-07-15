#!/usr/bin/env ruby

require 'mkmf'

$CFLAGS = '-std=c99'

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

(inc_paths << ENV['SPG_INCLUDE_DIRS']).compact!
(lib_paths << ENV['SPG_LIBRARY_DIRS']).compact!

find_header('libpq-fe.h',  *inc_paths) or raise 'unable to locate postgresql headers set SPG_INCLUDE_DIRS'
find_header('uuid/uuid.h', *inc_paths) or raise 'unable to locate uuid headers set SPG_INCLUDE_DIRS'

find_library('pq',  'main', *lib_paths) or raise 'unable to locate postgresql lib set SPG_LIBRARY_DIRS'
find_library('uuid','main', *lib_paths) or raise 'unable to locate uuid lib set SPG_LIBRARY_DIRS'
create_makefile('swift_db_postgres_ext')
