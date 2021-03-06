# -*- encoding: utf-8 -*-
# stub: swift-db-postgres 0.4.1 ruby lib ext
# stub: ext/swift/db/postgres/extconf.rb

Gem::Specification.new do |s|
  s.name = "swift-db-postgres".freeze
  s.version = "0.4.1"

  s.required_rubygems_version = Gem::Requirement.new(">= 0".freeze) if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib".freeze, "ext".freeze]
  s.authors = ["Bharanee Rathna".freeze]
  s.date = "2018-12-13"
  s.description = "Swift adapter for PostgreSQL database".freeze
  s.email = ["deepfryed@gmail.com".freeze]
  s.extensions = ["ext/swift/db/postgres/extconf.rb".freeze]
  s.files = ["CHANGELOG".freeze, "README.md".freeze, "ext/swift/db/postgres/adapter.c".freeze, "ext/swift/db/postgres/adapter.h".freeze, "ext/swift/db/postgres/common.c".freeze, "ext/swift/db/postgres/common.h".freeze, "ext/swift/db/postgres/datetime.c".freeze, "ext/swift/db/postgres/datetime.h".freeze, "ext/swift/db/postgres/extconf.rb".freeze, "ext/swift/db/postgres/gvl.h".freeze, "ext/swift/db/postgres/main.c".freeze, "ext/swift/db/postgres/result.c".freeze, "ext/swift/db/postgres/result.h".freeze, "ext/swift/db/postgres/statement.c".freeze, "ext/swift/db/postgres/statement.h".freeze, "ext/swift/db/postgres/typecast.c".freeze, "ext/swift/db/postgres/typecast.h".freeze, "lib/swift-db-postgres.rb".freeze, "lib/swift/db/postgres.rb".freeze, "test/helper.rb".freeze, "test/test_adapter.rb".freeze, "test/test_async.rb".freeze, "test/test_encoding.rb".freeze, "test/test_result.rb".freeze, "test/test_ssl.rb".freeze]
  s.homepage = "http://github.com/deepfryed/swift-db-postgres".freeze
  s.rubygems_version = "2.6.13".freeze
  s.summary = "Swift postgres adapter".freeze

  if s.respond_to? :specification_version then
    s.specification_version = 4

    if Gem::Version.new(Gem::VERSION) >= Gem::Version.new('1.2.0') then
      s.add_development_dependency(%q<rake>.freeze, [">= 0"])
    else
      s.add_dependency(%q<rake>.freeze, [">= 0"])
    end
  else
    s.add_dependency(%q<rake>.freeze, [">= 0"])
  end
end
