require 'helper'

describe 'postgres adapter' do
  it 'should initialize' do
    assert db
  end

  it 'should execute sql' do
    assert db.execute("select * from pg_tables")
  end

  it 'should expect the correct number of bind args' do
    assert_raises(Swift::ArgumentError) { db.execute("select * from pg_tables where tablename = ?", 1, 2) }
  end

  it 'should return result on #execute' do
    now = Time.now
    assert db.execute('drop table if exists users')
    assert db.execute('create table users (id serial primary key, name text, age integer, created_at timestamp with time zone)')
    assert db.execute('insert into users(name, age, created_at) values(?, ?, ?)', 'test', nil, now)

    result = db.execute('select * from users')

    assert_equal 1, result.selected_rows
    assert_equal 0, result.affected_rows
    assert_equal %w(id name age created_at).map(&:to_sym), result.fields
    assert_equal %w(integer text integer timestamp), result.types

    row = result.first
    assert_equal 1,      row[:id]
    assert_equal 'test', row[:name]
    assert_equal nil,    row[:age]
    assert_equal now.to_f.round(3), row[:created_at].to_time.to_f.round(3) # millisecs resolution on postgres

    result = db.execute('delete from users where id = 0')
    assert_equal 0, result.selected_rows
    assert_equal 0, result.affected_rows

    assert_equal 1, db.execute('select count(*) as count from users').first[:count]

    result = db.execute('delete from users')
    assert_equal 0, result.selected_rows
    assert_equal 1, result.affected_rows
  end

  it 'should close handle' do
    assert !db.closed?
    assert db.close
    assert db.closed?

    assert_raises(Swift::ConnectionError) { db.execute("select * from users") }
  end

  it 'should prepare & release statement' do
    assert db.execute('drop table if exists users')
    assert db.execute("create table users(id serial primary key, name text)")
    assert db.execute("insert into users (name) values (?)", "test")
    assert s = db.prepare("select * from users where id > ?")

    assert_equal 1, s.execute(0).selected_rows
    assert_equal 0, s.execute(1).selected_rows

    assert s.release
    assert_raises(Swift::RuntimeError) { s.execute(1) }
  end

  it 'should escape whatever' do
    assert_equal "foo''bar", db.escape("foo'bar")
  end
end
