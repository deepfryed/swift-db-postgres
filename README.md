# Swift PostgreSQL adapter

MRI adapter for PostgreSQL

## Features

* Lightweight & fast
* Result typecasting
* Prepared statements
* Asynchronous support using PQsendQuery family of functions

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

### Asynchronous API

```
  Swift::DB::Postgres
    #query(sql, *bind)
    #fileno
    #result
```

### Data I/O API

```
  Swift::DB::Postgres
    #write(table = nil, fields = nil, io_or_string)
    #read(table = nil, fields = nil, io = nil, &block)
```

## Example


### Synchronous

```ruby
require 'swift/db/postgres'

db = Swift::DB::Postgres.new(db: 'swift_test')

db.execute('drop table if exists users')
db.execute('create table users (id serial, name text, age integer, created_at timestamp)')
db.execute('insert into users(name, age, created_at) values(?, ?, ?)', 'test', 30, Time.now.utc)

p db.execute('select * from users').first #=> {:id => 1, :name => 'test', :age => 30, :created_at=> #<Swift::DateTime>}
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
## License

MIT
