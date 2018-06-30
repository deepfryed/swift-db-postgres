require 'helper'
require 'json'
require 'ipaddr'

describe 'postgres statement' do
  it 'should typecast and return types on #execute' do
    now = Time.now
    assert db.execute('drop table if exists users')
    assert db.execute('create table users (id serial primary key, name text, age integer, created_at timestamp with time zone)')
    assert db.execute('insert into users(name, age, created_at) values(?, ?, ?)', 'test', nil, now)

    result = db.execute('select * from users')

    assert_equal 1, result.selected_rows
    assert_equal 0, result.affected_rows
    assert_equal %w(id name age created_at).map(&:to_sym), result.fields
    assert_equal ['integer', 'text', 'integer', 'timestamp with time zone'], result.types

    row = result.first
    assert_equal 1,      row[:id]
    assert_equal 'test', row[:name]
    assert_nil   row[:age]
    assert_equal now.to_f.round(3), row[:created_at].to_time.to_f.round(3) # millisecs resolution on postgres

    result = db.execute('delete from users where id = 0')
    assert_equal 0, result.selected_rows
    assert_equal 0, result.affected_rows

    assert_equal 1, db.execute('select count(*) as count from users').first[:count]

    result = db.execute('delete from users')
    assert_equal 0, result.selected_rows
    assert_equal 1, result.affected_rows

    # empty result should have field & type information
    result = db.execute('select * from users')
    assert_equal %w(id name age created_at).map(&:to_sym), result.fields
    assert_equal ['integer', 'text', 'integer', 'timestamp with time zone'], result.types
  end

  it 'should allow custom fallback encoder and decoder' do
    db.encoder = proc do |value|
      case value
        when Hash then JSON.dump(value)
        else value.to_s
      end
    end

    db.decoder = proc do |field, oid, value|
      case field
        when :tjson, :bjson then JSON.parse(value)
        when :ip then IPAddr.new(value)
        else nil
      end
    end

    assert db.execute('drop table if exists users')
    assert db.execute(<<-SQL)
      create table users (
        id serial primary key,
        name text,
        age integer,
        tjson json,
        bjson json,
        ip inet
      )
    SQL

    assert db.execute(<<-SQL, 'jdoe', 40, {foo:1}, {bar:1}, '127.0.0.1')
      insert into users(name, age, tjson, bjson, ip) values(?, ?, ?, ?, ?)
    SQL

    result = db.execute('select * from users')
    tuple = result.first
    assert_equal tuple[:tjson], {"foo" => 1}
    assert_equal tuple[:bjson], {"bar" => 1}
    assert_kind_of IPAddr, tuple[:ip]
  end

  it 'should allow fetching result by row and column' do
    assert db.execute('drop table if exists users')
    assert db.execute(<<-SQL)
      create table users (
        id serial primary key,
        name text,
        age int,
        details json
      )
    SQL

    db.encoder = proc do |value|
      case value
        when Hash then JSON.dump(value)
        else value.to_s
      end
    end

    db.decoder = proc do |field, oid, value|
      case field
        when :details then JSON.parse(value)
        else nil
      end
    end

    assert db.execute('insert into users(name, age, details) values(?,?,?)', 'a', 30, {admin: true})
    assert db.execute('insert into users(name, age, details) values(?,?,?)', 'b', 40, {admin: false})

    result = db.execute('select * from users')
    assert_equal result.get(0, 0), 1
    assert_equal result.get(0, 1), 'a'
    assert_equal result.get(0, 2), 30
    assert_equal result.get(0, 3), {"admin" => true}

    assert_equal result.get(1, 0), 2
    assert_equal result.get(1, 1), 'b'
    assert_equal result.get(1, 2), 40
    assert_equal result.get(1, 3), {"admin" => false}

    assert_nil result.get(0, 4)
    assert_nil result.get(1, 4)
    assert_nil result.get(2, 0)
  end
end
