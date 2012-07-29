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

    # empty result should have field & type information
    result = db.execute('select * from users')
    assert_equal %w(id name age created_at).map(&:to_sym), result.fields
    assert_equal %w(integer text integer timestamp), result.types

    # like query
    assert db.execute('select * from users where name like ?', '%foo%')
  end

  it 'should close handle' do
    assert db.ping
    assert !db.closed?
    assert db.close
    assert db.closed?
    assert !db.ping

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

  it 'should support #write and #read' do
    assert db.execute('drop table if exists users')
    assert db.execute("create table users(id serial primary key, name text)")

    assert_equal 3, db.write('users', %w(name), "foo\nbar\nbaz\n").affected_rows
    assert_equal 3, db.execute('select count(*) as count from users').first[:count]

    assert db.execute('copy users(name) from stdin')
    assert_equal 3, db.write("foo\nbar\nbaz\n").affected_rows
    assert_equal 6, db.execute('select count(*) as count from users').first[:count]

    assert_equal 3, db.write('users', StringIO.new("7\tfoo\n8\tbar\n9\tbaz\n")).affected_rows
    assert_equal 9, db.execute('select count(*) as count from users').first[:count]

    io = StringIO.new
    db.execute('copy (select * from users order by id limit 1) to stdout with csv')
    assert_equal 1, db.read(io).affected_rows

    assert_match %r{1,foo\n}, io.tap(&:rewind).read

    rows = []
    db.read('users') {|row| rows << row}

    expect = (%w(foo bar baz)*3).zip(1..9).map {|r| r.reverse.join("\t") + "\n"}
    assert_equal expect, rows

    assert_raises(Swift::RuntimeError) { db.write("foo") }
    assert_raises(Swift::RuntimeError) { db.write("users", "bar") }
    assert_raises(Swift::RuntimeError) { db.write("users", %w(name), "bar") }
  end

  it 'should not change the hstore ? operator' do
    assert db.execute('create extension if not exists hstore')
    assert db.execute('drop table if exists hstore_test')
    assert db.execute('create table hstore_test(id int, payload hstore)')
    assert db.execute('insert into hstore_test values(1, ?)', 'a => 1, b => 2')

    db.native_bind_format = true
    assert_equal 1, db.execute('select * from hstore_test where payload ? $1', 'a').selected_rows
    assert_equal 0, db.execute('select * from hstore_test where payload ? $1', 'c').selected_rows
    db.native_bind_format = false

    db.native_bind_format do
      assert_equal 1, db.execute('select * from hstore_test where payload ?| ARRAY[$1, $2]', 'a', 'b').selected_rows
      assert_equal 1, db.execute('select * from hstore_test where payload ?& ARRAY[$1, $2]', 'a', 'b').selected_rows
    end
  end
end
