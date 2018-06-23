require 'helper'

describe 'adapter ssl' do
  it 'should connect to database with ssl options' do
    skip 'skipping in macos' if %r{darwin|mac os}i.match(RbConfig::CONFIG['host_os'] )
    db.execute('create extension if not exists sslinfo')
    key  = File.join(File.dirname(__FILE__), 'server.key')
    cert = File.join(File.dirname(__FILE__), 'server.crt')

    assert db = Swift::DB::Postgres.new(db: 'swift_test', ssl: {sslmode: 'require', sslcert: cert, sslkey: key})
    assert db.execute('select ssl_is_used()').first[:ssl_is_used]

    assert db = Swift::DB::Postgres.new(db: 'swift_test', ssl: {sslmode: 'disable'})
    assert !db.execute('select ssl_is_used()').first[:ssl_is_used]
  end
end
