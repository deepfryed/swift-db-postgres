# Swift PostgreSQL adapter

PostgreSQL adapter for MRI Ruby.

NOTE: This has nothing to do with Swift programming language (OSX, iOS)

## Features

* Lightweight & fast
* Result typecasting
* Prepared statements
* Asynchronous support using PQsendQuery family of functions
* Nested transactions

## Requirements

* postgresql client deveopment libraries (libpq-dev)
* uuid development libraries (uuid-dev)

## Building

```
git submodule update --init
rake
```

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
    #encoder=
    #decoder=
    #typemap

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
    #clear
    #get(row, column)
```

## Connection options

```
╭────────────────────╥────────────┬─────────────╮
│ Name               ║  Default   │  Optional   │
╞════════════════════╬════════════╪═════════════╡
│ db                 ║  -         │  No         │
│ host               ║  -         │  Yes        │
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

`Swift::DB::Postgres` uses `?` as a bind parameter and replaces them with the `$` equivalents. This causes issues when
you try to use the HStore `?` operator. You can permanently or temporarily disable the replacement strategy as below:

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

### Typecast support

The library understands the following Ruby and Postgres types and will attempt to typecast
values in either direction appropriately. Types not listed below will be stringified on the
way in to the database and returned as a raw string value on the way out.

```
╭────────────────┬─────────────────────╮
│ Ruby           │  PostgreSQL         │
╞════════════════╪═════════════════════╡
│ TrueClass      │  boolean            │
│ FalseClass     │  boolean            │
│ Bignum         │  bigint             │
│ Fixnum         │  int                │
│ Integer        │  int                │
│ Float          │  float, double      │
│ BigDecimal     │  numeric            │
│ Time           │  timestamp          │
│ DateTime       │  timestamp          │
│ String         │  text               │
│ Symbol         │  text               │
│ IO             │  bytea              │
└────────────────┴─────────────────────┘
```

### Custom encoder

`Swift::DB::Postgres#execute` or `Swift::DB::Postgres::Statement#execute` will attempt to encode bind values and
then fallback to stringifying unsupported types using `Object#to_s`. This can be overriden with a customer encoder.

e.g.

```ruby
db = Swift::DB::Postgres.new(db: 'swift_test')
db.encoder = proc do |value|
  case value
    when Hash  then JSON.dump(value)
    when Array then ...
    else nil
  end
end
```

The encoder can be any object that implements `.call` and is passed the bind value. If the encoder returns
`nil` then `#execute` will fallback to stringifying the value.


### Custom decoder

Logical inverse of above. `Result#each` attempts to decode known types but will return a raw string if it cannot
decode a value. A decoder can be provided to extend the typecasting support, the decoder is passed in extra
metadata hints to help including a field name and the Postgres OID.

```ruby
db = Swift::DB::Postgres.new(db: 'swift_test')
db.decoder = proc do |field, oid, value|
  case oid
    when 114 then JSON.parse(value)
    when 142 then Nokogiri::XML(value)
    else nil
  end
end
```

You can get a list of statically generated OIDs with type description using `Swift::DB::Postgres#typemap` but
this method only exists for convenience and the type map OIDs may not include all the supported types in your
PostgreSQL instance.

### Asynchronous

There are several approaches to handling IO wait and concurrency but all of them require creating a connection
pool and checking out a connection to run queries in parallel.

1. Create a connection pool with separate connections to the database.
2. Use `Swift::DB::Postgres#query` to execute a SQL in a non-blocking fashion.
3. Use one of the polling schemes below to retrieve results.

#### Threads

Call `Swift::DB::Postgres::Result#each` in a thread to handle IO wait

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
p rows #=> [3, 2, 1]
```

#### IO select

Use `Swift::DB::Postgres#fileno` to get the unix file descriptor and create an IO object. You can
then use `Kernel#select` to poll the IO object.

```ruby
require 'swift/db/postgres'

rows = []
pool = 3.times.map {Swift::DB::Postgres.new(db: 'swift_test')}

io_list = pool.map(&:fileno).map {|fd| IO.for_fd(fd) }
io_lookup = Hash[io_list.map(&:fileno).zip(pool)]

3.times do |n|
  pool[n].query("select pg_sleep(#{(3 - n) / 10.0}), #{n + 1} as query_id")
end

loop do
  ready = IO.select(io_list, nil, nil, 0.5)
  break if ready.nil?
  ready.first.each do |io|
    io_lookup[io.fileno].result.each do |tuple|
      rows << tuple[:query_id]
    end
  end
end

p rows #=> [3, 2, 1]
```

#### EventMachine.watch

Similar to above example, you can use `EventMachine.watch` on the file descriptor returned by `Swift::DB::Postgres#fileno`

See https://www.rubydoc.info/github/eventmachine/eventmachine/EventMachine.watch

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

Tests:
* Insert 1000 rows
* Read them back 100 times with typecast enabled

Caveats:
* This is just a raw performance benchmark, each library has its advantages and disadvantages.

```
$ ruby -v
ruby 2.1.5p273 (2014-11-13 revision 48405) [x86_64-linux]

$ ruby check.rb
                           user     system      total        real
swift insert           0.020000   0.000000   0.020000 (  0.090353)
swift select           0.170000   0.000000   0.170000 (  0.268109)

do_postgres insert     0.020000   0.020000   0.040000 (  0.096619)
do_postgres select     0.550000   0.000000   0.550000 (  0.659646)

pg insert              0.020000   0.000000   0.020000 (  0.069376)
pg select              0.680000   0.030000   0.710000 (  0.819120)
```

## License

MIT
