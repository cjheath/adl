require 'byebug'

class ADL

  def initialize
  end

  def parse io
    @scanner = Scanner.new(self, io)
    @stack = []
    @scanner.parse
  end

  # Called when the name and type have been parsed
  def object(ascend, object_name, supertype_name)
    @stack.push([ascend, object_name, supertype_name, false])
  end

  # Called when we open the object's namespace
  def open
    @stack.last[3] = true
    ascend, object_name, supertype_name = @stack.last
    puts("#{ascend ? '.' : ''}#{object_name ? (object_name*' ').inspect : ''}#{supertype_name ? ' : '+(supertype_name*' ').inspect : ''} {")
  end

  # Called when we close the object's namespace
  def close
  end

  # Called when we finalise the object, perhaps with an operator and value
  def object_end operator, value
    ascend, object_name, supertype_name, has_block = @stack.pop
    indent
    if has_block
      puts("#{operator ? " #{operator} #{value}" : ''}}")
    else
      puts("#{ascend ? '.' : ''}#{object_name ? (object_name*' ').inspect : ''}#{supertype_name ? ' : '+(supertype_name*' ').inspect : ''}#{operator ? " #{operator} #{value}" : ''}")
    end
  end

  # print an indent for displaying the parse
  def indent
    print "\t"*@stack.size
  end

  class Scanner
    Tokens = {
      white: %r{\s+},
      symbol: %r{[_a-z][_a-z0-9]*},
      integer: %r{-?([1-9][0-9]*|0)},
      string: %r{'(\\[befntr']|\\[0-3][0-7][0-7]|\\0|\\x[0-9a-f][0-9a-f]|\\u[0-9a-f][0-9a-f][0-9a-f][0-9a-f]|[^\\'])*'},
      open: /{/,
      close: /}/,
      lbrack: /\[/,
      rbrack: /\]/,
      inherits: %r{:},
      semi: %r{;},
      comma: %r{,},
      equals: %r{=},
      approx: %r{~=},
      descend: %r{\.},
      # The following recursive regexp follows the CQL definition, but has a modified definition of <char>.
      regexp: %r{/(?<sequence>(?<alternate>(?<atom>((?<char>(\\[0-3][0-7][0-7]|\\x[0-9a-f][0-9a-f]|\\u[0-9a-f][0-9a-f][0-9a-f][0-9a-f]|\\[0befntr\\*+?()|/\[]|[^*+?()|/\[]))|\(\g<sequence>*\)|\[((\g<char>|[+*?()/|])(-(\g<char>)|[+*?()/|])?)*\])[*+?]?)*)(\|\g<alternate>)*)/}
    }

    def initialize adl, io
      @adl = adl
      @io = io
      @token = []
      @io.scan(parse_re) do |a|
        match = $~
        @token << match.names.map{|n| match[n] ? [n, match[n]] : nil}.compact[0]
      end
    end

    def parse
      # Any rule that consumes a token that may be followed by white space should skip it before returning
      opt_white
      while object
      end
    end

    def object
      ascend = expect('descend')
      opt_white
      object_name = path_name
      if expect('inherits')
        supertype_name = path_name
        error('Expected supertype, body or ;') unless supertype_name or peek('semi') or peek('open')
      end
      return nil unless ascend || object_name || supertype_name
      @adl.object(ascend, object_name, supertype_name)
      if has_block = expect('open')
        @adl.open
        while object
        end
        error('expected closing }') unless expect('close')
        @adl.close
      end
      if operator = (expect('equals') or expect('approx') or expect('arrow'))
        val = value
        error('expected closing ;') unless expect('semi') or peek('close')
      elsif !has_block
        expect('semi')
      end

      @adl.object_end(operator, val)
      true
    end

    def value
      integer or string or path_name or array or expect('regexp')
    end

    def integer
      s = expect('integer') and eval(s)
    end

    def string
      s = expect('string') and eval(s)
    end

    def array
      return nil unless expect('lbrack')
      array_value = []
      while val = value
        array_value << val
        break unless expect('comma')
      end
      error("Array elements must separated by , and end in ]") unless expect('rbrack')
      array_value
    end

    def path_name
      names = []
      while n = (expect('symbol') or expect('integer'))
        names << [n]
        while n = (expect('symbol') or expect('integer'))
          names[-1] << n  # Compound name - this is a following word
        end
        break unless expect('descend')
      end
      names.empty? ? nil : names
    end

    def opt_white
      white or true
    end

    def white
      expect('white')
    end

  private
    def parse_re
      @parse_re ||=
      Regexp.new(
        '(?:' +
        (
          Tokens.map do |tag, re|
            re_text = re.to_s
            # In general, we don't want captures, but this breaks the regexp regexp:
            re_text.gsub!(/\((\?-mix:)?(?![\?)])/,'(?:') unless tag == :regexp
            "(?<#{tag}>#{re_text})"
          end +
          ['(?<waste>.)'] # Must be last
        ) * '|' +
        ')',
        Regexp::IGNORECASE
      )
    end

    # Report a parse failure
    def error message
      puts "Parse failed at #{@token[0].inspect}: #{message}"
      exit 1
    end

    # Consume the current token
    def consume
      # puts "consuming #{@token[0].inspect}" unless @token[0][0] == 'white'
      @token.shift[1]
    end

    # If token is next, consume it and retrun the value, else return nil
    def expect token
      return nil unless @token[0] && @token[0][0] == token
      [consume, opt_white][0]
    end

    # Return true if token is next
    def peek token
      @token[0] && @token[0][0] == token
    end
  end
end

ADL.new.parse(File.read(ARGV[0]))
