# Swift PostgreSQL adapter

MRI adapter for postgres for use in Swift ORM.

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

  Swift::DB::Postgres::Statement
    .new(Swift::DB::Postgres, sql)
    #execute(*bind)

  Swift::DB::Postgres::Result
    #selected_rows
    #affected_rows
    #fields
    #types
    #each
    #insert_id
```

## License

MIT
