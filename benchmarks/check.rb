#!/usr/bin/env ruby

$:.unshift File.dirname(__FILE__) + '/../ext'
$:.unshift File.dirname(__FILE__) + '/../lib'

require 'bundler/setup'
require 'pg'
require 'do_postgres'
require 'swift-db-postgres'
require 'benchmark'

dbs = {
  swift:       Swift::DB::Postgres.new(db: 'swift_test', ssl: {sslmode: 'disable'}),
  do_postgres: DataObjects::Connection.new("postgres://127.0.0.1/swift_test"),
  pg:          PG::Connection.new('dbname=swift_test').tap {|db|
    db.type_map_for_results = PG::BasicTypeMapForResults.new(db)
    db.set_notice_processor {}
  }
}

queries = {
  pg: {
    drop:   'drop table if exists users',
    create: 'create table users(id serial primary key, name text, created_at timestamp with time zone)',
    insert: 'insert into users(name, created_at) values ($1, $2)',
    select: 'select * from users where id > $1'
  },
  swift: {
    drop:   'drop table if exists users',
    create: 'create table users(id serial primary key, name text, created_at timestamp with time zone)',
    insert: 'insert into users(name, created_at) values (?, ?)',
    select: 'select * from users where id > ?'
  },
  do_postgres: {
    drop:   'drop table if exists users',
    create: 'create table users(id serial primary key, name text, created_at timestamp with time zone)',
    insert: 'insert into users(name, created_at) values (?, ?)',
    select: 'select * from users where id > ?'
  }
}

rows = 1000
iter = 100

class PG::Connection
  def execute sql, *args
    exec(sql, args)
  end
end

module DataObjects
  class Connection
    def query sql, *args
      create_command(sql).execute_reader(*args)
    end
    def execute sql, *args
      create_command(sql).execute_non_query(*args)
    end
  end
end

Benchmark.bm(20) do |bm|
  dbs.each do |name, db|
    sql = queries[name]
    db.execute(sql[:drop])
    db.execute(sql[:create])

    bm.report("#{name} insert") do
      rows.times do |n|
        db.execute(sql[:insert], "name #{n}", Time.now.strftime('%FT%T'))
      end
    end

    bm.report("#{name} select") do
      case db
        when DataObjects::Postgres::Connection
          iter.times do
            db.query(sql[:select], 0).entries
          end
        else
          iter.times do
            db.execute(sql[:select], 0).entries
          end
      end
    end
  end
end
