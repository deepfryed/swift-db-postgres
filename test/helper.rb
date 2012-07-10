require 'minitest/autorun'
require 'swift/db/postgres'

class MiniTest::Spec
  def db
    @db ||= Swift::DB::Postgres.new(db: 'swift_test')
  end
end
