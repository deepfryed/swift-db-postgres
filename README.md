# Swift PostgreSQL adapter

MRI adapter for PostgreSQL

## Features

* Lightweight & fast
* Result typecasting
* Prepared statements
* Asynchronous support using PQsendQuery family of functions
* Nested transactions

## API

```
  Swift::DB::Postgres
    .new(options)
    #execute(sql, *bind)
    #prepare(sql)
    #begin(savepoint = nil)
    #commit(savepoint = nil)
    #rollback(savepoint = nil)
    #transaction(savepoint = nil, &block)
    #native_bind_format(&block)
    #native_bind_format=(value)
    #ping
    #close
    #closed?
    #escape(text)
    #query(sql, *bind)
    #fileno
    #result
    #write(table = nil, fields = nil, io_or_string)
    #read(table = nil, fields = nil, io = nil, &block)

  Swift::DB::Postgres::Statement
    .new(Swift::DB::Postgres, sql)
    #execute(*bind)
    #release

  Swift::DB::Postgres::Result
    #selected_rows
    #affected_rows
    #fields
    #types
    #each
    #insert_id
```

## Connection options

```
╭────────────────────╥────────────┬─────────────╮
│ Name               ║  Default   │  Optional   │
╞════════════════════╬════════════╪═════════════╡
│ db                 ║  -         │  No         │
│ host               ║  127.0.0.1 │  Yes        │
│ port               ║  5432      │  Yes        │
│ user               ║  Etc.login │  Yes        │
│ password           ║  nil       │  Yes        │
│ encoding           ║  utf8      │  Yes        │
│ ssl[:sslmode]      ║  allow     │  Yes        │
│ ssl[:sslcert]      ║  nil       │  Yes        │
│ ssl[:sslkey]       ║  nil       │  Yes        │
│ ssl[:sslrootcert]  ║  nil       │  Yes        │
│ ssl[:sslcrl]       ║  nil       │  Yes        │
└────────────────────╨────────────┴─────────────┘
```

## Bind parameters and hstore operators

Swift::DB::Postgres uses '?' as a bind parameter and replaces them with the '$' equivalents. This causes issues when
you try to use the HStore '?' operator. You can permanently or temporarily disable the replacement strategy as below:

```ruby

db.native_bind_format = true
db.execute("select * from users where tags ? $1", 'mayor')
db.native_bind_format = false

db.native_bind_format do
  db.execute("select * from users where tags ? $1", 'mayor')
end
```

## Example

```ruby
require 'swift/db/postgres'

db = Swift::DB::Postgres.new(db: 'swift_test', encoding: 'utf8')

db.execute('drop table if exists users')
db.execute('create table users (id serial, name text, age integer, created_at timestamp)')
db.execute('insert into users(name, age, created_at) values(?, ?, ?)', 'test', 30, Time.now.utc)

row = db.execute('select * from users').first
p row #=> {:id => 1, :name => 'test', :age => 30, :created_at=> #<Swift::DateTime>}
```

### Asynchronous

Hint: You can use `Adapter#fileno` and `EventMachine.watch` if you need to use this with EventMachine.

```ruby
require 'swift/db/postgres'

rows = []
pool = 3.times.map {Swift::DB::Postgres.new(db: 'swift_test')}

3.times do |n|
  Thread.new do
    pool[n].query("select pg_sleep(#{(3 - n) / 10.0}), #{n + 1} as query_id") do |row|
      rows << row[:query_id]
    end
  end
end

Thread.list.reject {|thread| Thread.current == thread}.each(&:join)
rows #=> [3, 2, 1]
```

### Data I/O

The adapter supports data read and write via COPY command.

```ruby
require 'swift/db/postgres'

db = Swift::DB::Postgres.new(db: 'swift_test')
db.execute('drop table if exists users')
db.execute('create table users (id serial, name text)')

db.write('users', %w{name}, "foo\nbar\nbaz\n")
db.write('users', %w{name}, StringIO.new("foo\nbar\nbaz\n"))
db.write('users', %w{name}, File.open("users.dat"))


db.read('users', %w{name}) do |data|
  p data
end

csv = File.open('users.csv', 'w')
db.execute('copy users to stdout with csv')
db.read(csv)
```

## Performance

Don't read too much into it. Each library has its advantages and disadvantages.

* insert 1000 rows and read them back 100 times with typecast enabled
* pg uses the pg_typecast extension

```
$ ruby -v

ruby 1.9.3p125 (2012-02-16 revision 34643) [x86_64-linux]

$ ruby check.rb
                          user     system      total        real
do_postgres insert     0.190000   0.080000   0.270000 (  0.587877)
do_postgres select     1.440000   0.020000   1.460000 (  2.081172)

pg insert              0.100000   0.030000   0.130000 (  0.395280)
pg select              0.630000   0.220000   0.850000 (  1.284905)

swift insert           0.070000   0.040000   0.110000 (  0.348211)
swift select           0.640000   0.030000   0.670000 (  1.111561)
```

## License

MIT
