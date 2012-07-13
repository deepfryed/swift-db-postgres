# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = "swift-db-postgres"
  s.version = "0.1.0"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.authors = ["Bharanee Rathna"]
  s.date = "2012-07-14"
  s.description = "Swift adapter for PostgreSQL database"
  s.email = ["deepfryed@gmail.com"]
  s.extensions = ["ext/swift/db/postgres/extconf.rb"]
  s.files = ["ext/swift/db/postgres/typecast.c", "ext/swift/db/postgres/datetime.c", "ext/swift/db/postgres/result.c", "ext/swift/db/postgres/statement.c", "ext/swift/db/postgres/common.c", "ext/swift/db/postgres/adapter.c", "ext/swift/db/postgres/main.c", "ext/swift/db/postgres/typecast.h", "ext/swift/db/postgres/datetime.h", "ext/swift/db/postgres/result.h", "ext/swift/db/postgres/common.h", "ext/swift/db/postgres/adapter.h", "ext/swift/db/postgres/statement.h", "ext/swift/db/postgres/extconf.rb", "test/helper.rb", "test/test_adapter.rb", "lib/swift/db/postgres.rb", "lib/swift-db-postgres.rb", "README.md", "CHANGELOG"]
  s.homepage = "http://github.com/deepfryed/swift-db-postgres"
  s.require_paths = ["lib", "ext"]
  s.rubygems_version = "1.8.24"
  s.summary = "Swift postgres adapter"

  if s.respond_to? :specification_version then
    s.specification_version = 3

    if Gem::Version.new(Gem::VERSION) >= Gem::Version.new('1.2.0') then
      s.add_development_dependency(%q<rake>, [">= 0"])
    else
      s.add_dependency(%q<rake>, [">= 0"])
    end
  else
    s.add_dependency(%q<rake>, [">= 0"])
  end
end
