#!/usr/bin/env ruby

$:.unshift File.dirname(__FILE__) + '/../ext'
$:.unshift File.dirname(__FILE__) + '/../lib'

require 'bundler/setup'
require 'pg'
require 'pg_typecast'
require 'swift-db-postgres'
require 'benchmark'

dbs = {
  pg:      PG::Connection.new('dbname=swift_test').tap {|db| db.set_notice_processor {}},
  swift:   Swift::DB::Postgres.new(db: 'swift_test')
}

queries = {
  pg: {
    drop:   'drop table if exists users',
    create: 'create table users(id serial primary key, name text, created_at timestamp with time zone)',
    insert: 'insert into users(name, created_at) values ($1, $2)',
    update: 'update users set name = $1, created_at = $2 where id = $3',
    select: 'select * from users where id > $1'
  },
  swift: {
    drop:   'drop table if exists users',
    create: 'create table users(id serial primary key, name text, created_at timestamp with time zone)',
    insert: 'insert into users(name, created_at) values (?, ?)',
    update: 'update users set name = ?, created_at = ? where id = ?',
    select: 'select * from users where id > ?'
  },
}

rows = 1000
iter = 100

class PG::Connection
  def execute sql, *args
    exec(sql, args)
  end
end

Benchmark.bm(15) do |bm|
  dbs.each do |name, db|
    sql = queries[name]
    db.execute(sql[:drop])
    db.execute(sql[:create])

    bm.report("#{name} insert") do
      rows.times do |n|
        db.execute(sql[:insert], "name #{n}", Time.now.to_s)
      end
    end

    bm.report("#{name} select") do
      iter.times do
        db.execute(sql[:select], 0).entries
      end
    end
  end
end
