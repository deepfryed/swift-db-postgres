# Swift PostgreSQL adapter

MRI adapter for postgres.

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

    Asynchronous API (see test/test_async.rb)

    #query(sql, *bind)
    #fileno
    #result

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

## Example


### Synchronous

```ruby
db = Swift::DB::Postgres.new(db: 'swift_test')

db.execute('drop table if exists users')
db.execute('create table users (id serial primary key, name text, age integer, created_at timestamp)')
db.execute('insert into users(name, age, created_at) values(?, ?, ?)', 'test', 30, Time.now.utc)

db.execute('select * from users').first #=> {:id => 1, :name => 'test', :age => 30, :created_at=> #<Swift::DateTime>}
```

### Asynchronous

Hint: You can use `Adapter#fileno` and `EventMachine.watch` if you need to use this with EventMachine.

```ruby
rows = []
pool = 3.times.map {Swift::DB::Postgres.new(db: 'swift_test')}

3.times do |n|
  Thread.new do
    pool[n].query("select pg_sleep(#{(3 - n) / 10.0}), #{n + 1} as query_id") {|row| rows << row[:query_id]}
  end
end

Thread.list.reject {|thread| Thread.current == thread}.each(&:join)
rows #=> [3, 2, 1]
```
## License

MIT
