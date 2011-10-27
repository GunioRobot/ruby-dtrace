#
# Ruby-Dtrace
# (c) 2007 Chris Andrews <chris@nodnol.org>
#

# Represents an aggregation record built from a series of
# DtraceAggData records.
#
# Intended to to built up by calling +add_record+ repeatedly with
# Dtrace::AggData objects until a completed Dtrace::Aggregate is
# returned.  (until a complete record is available, +add_record+
# returns nil).
#
# See consumer.rb for an example of this.
class Dtrace
  class Aggregate
    attr_reader :value, :tuple

    # Create an empty Dtrace::Aggregate: use +add_record+ to add data.
    def initialize
      @tuple = Array.new
    end

    # Add a Dtrace::AggData record to this aggregate. Returns nil until it
    # receives a record of aggtype "last", when it returns the complete
    # Dtrace::Aggregate.
    def add_record(r)
      case r.aggtype
      when "tuple"
        @tuple << r.value
      when "value"
        @value = r.value
      when "last"
        return self
      end
      nil
    end

  end
end
