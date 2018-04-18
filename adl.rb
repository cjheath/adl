require 'byebug'

class ADL
  attr_reader :stack

  class ADLObject
    attr_reader :name, :parent, :zuper
    attr_accessor :syntax
    attr_accessor :has_block
    attr_accessor :zuper_placeholder
    attr_writer :zuper  # Reference objects get created then changed

    def initialize parent, name, zuper, aspect = nil
      @parent, @name, @zuper, @aspect = parent, name, zuper, aspect || parent

      @parent.members << self if @parent
    end

    def assign variable, value, is_final
      # REVISIT: Special-case the built-in Object variables.
      Assignment.new(self, variable, value, is_final)
    end

    def members
      @members ||= []
    end

    def member? name
      members.detect{|m| m.name == name}
    end

    def pathname
      (@parent && !@parent.top? ? @parent.pathname+'.' : '')+(@name ? Array(@name)*' ' : '<anonymous>')
    end

    def top?
      !@parent
    end

    def inspect
      pathname +
        (@zuper_placeholder ? ' : '+@zuper_placeholder : 
          (@zuper != @object ? ':'+@zuper.pathname : '')
        ) +
        (has_block ? '{}' : '')
    end
  end

  class Assignment < ADLObject
    def initialize parent, variable, value, is_final
      super parent, nil, @assignment
      @variable, @value, @is_final = variable, value, is_final
    end

    def inspect
      @parent.pathname+'.'+@variable.name+(@is_final ? '=' : '~=') + @value.inspect
    end
  end

  def initialize
    make_built_in
  end

  def make_built_in
    @top = ADLObject.new(nil, 'TOP', nil, nil)
    @builtin = ADLObject.new(@top, 'ADL', nil, nil)
    @object = ADLObject.new(@builtin, 'Object', nil, nil)
    @reference = ADLObject.new(@builtin, 'Reference', @object, nil)
    @regexp = ADLObject.new(@builtin, 'Regular Expression', @object, nil)
    @assignment = ADLObject.new(@builtin, 'Assignment', @object, nil)
    @string = ADLObject.new(@builtin, 'String', @object, nil)
    @string.syntax = /'(\\[befntr']|\\[0-7][0-7][0-7]|\\0|\\x[0-9A-F][0-9A-F]|\\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F]|[^\\'])*'/;
    @enumeration = ADLObject.new(@builtin, 'Enumeration', @object, nil)
    @boolean = ADLObject.new(@builtin, 'Boolean', @enumeration, nil)
    @false = ADLObject.new(@builtin, 'False', @boolean, nil)
    @true = ADLObject.new(@builtin, 'True', @boolean, nil)

    @alias = ADLObject.new(@builtin, 'Alias', @object, nil)
    @alias_for = ADLObject.new(@alias, 'For', @reference, nil)
  end

  def parse io
    @scanner = Scanner.new(self, io)
    @stack = [@top]
    @scanner.parse
  end

  # print an indent for displaying the parse
  def indent
    print "\t"*(@stack.size-1)
  end

  # A place to dis/enable parse progress messages.
  def show stuff
    indent
    puts stuff
  end

  # Report a parse failure
  def error message
    puts message
    exit 1
  end

  def resolve_name path_name, levels_up = 1
    o = @stack.last
    levels_up.times { o = o.parent }
    return o if path_name.empty?
    path_name = []+path_name
    if path_name[0] == '.'      # Do not implicitly ascend
      no_ascend = true
      path_name.shift
      while path_name[0] == '.' # just ascend explicitly
        path_name.shift
        o = o.parent
      end
    end
    return o if path_name.empty?

    # Ascend the parent chain until we fail or find our name:
    until no_ascend or path_name.empty? or m = o.member?(path_name[0])
      o = o.parent    # Ascend
      error("Failed to find #{path_name[0].inspect} from #{@stack.last.name}") unless o
    end
    o = m
    path_name.shift
    return o if path_name.empty?

    # Now descend from the 
    path_name.each do |n|
      m = o.member?(n)
      error("Failed to find #{n.inspect} in #{o.pathname}") unless m
      o = m   # Descend
    end
    o
  end

  # Called when the name and type have been parsed
  def start_object(object_name, supertype_name)
    # Resolve the object_name prefix to find the parent and local name:
    if object_name
      parent = resolve_name(object_name[0..-2], 0)
    else
      parent = @stack.last
    end
    local_name = object_name ? object_name[-1] : nil
    # Resolve the supertype_name to find the zuper:
    zuper = supertype_name ? resolve_name(supertype_name, 0) : @object
    o = parent.member?(local_name) ||
      ADLObject.new(parent, local_name, zuper)

    show("#{o.inspect}")
    o.zuper_placeholder = supertype_name*' ' if supertype_name
    @stack.push o
    o
  end

  def end_object
    unless @stack.last.has_block
      # debugger  # Check things.
    end
    @stack.pop
  end

  def start_block
    @stack.last.has_block = true
    show(" {")
  end

  def end_block
    show("}")
  end

  class Scanner
    Tokens = {
      white: %r{(\s|//.*)+},
      symbol: %r{[_[:alpha:]][_[:alnum:]]*},
      integer: %r{-?([1-9][0-9]*|0)},
      string: %r{
        '
          (
                                #
            \\[0befntr\\']      # A control-character escape
            | \\[0-3][0-7][0-7] # An octal constant
            | \\x[0-9A-F][0-9A-F]       # A hexadecimal character
            | \\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F]   # A Unicode character
            | [^\\']            # Anything else except \ and end of string
          )*
        '
      }x,
      open: /{/,
      close: /}/,
      lbrack: /\[/,
      rbrack: /\]/,
      inherits: %r{:},
      semi: %r{;},
      arrow: %r{->},
      darrow: %r{=>},
      rename: %r{!},
      comma: %r{,},
      equals: %r{=},
      approx: %r{~=},
      scope: %r{\.},
      # The following recursive regexp defines our regular expression features:
      regexp: %r{
        /                                       # A / to lead off
        (?<sequence>                            # A sequence of one or more alternates
          (?<alternate>                         # Anything that can go between vertical bars (alternates)
            (?<atom>                            # Anything that may be repeated
              ( (?<char>                        # Any normal regexp char
                  \\s                           # Single whitespace
                  | \\[0-3][0-7][0-7]           # An octal character
                  | \\x[0-9A-F][0-9A-F]         # A hexadecimal character
                  | \\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F] # A Unicode character
                  | \\[0befntr\\*+?()|/\[]      # A control character or escaped regexp char
                  | [^*+?()/|\[ ]               # Anything else except these
                )
              | \(                              # A parenthesised group
                  (\?(<[_[:alnum:]]*>|!))?      # Optional capture name or negative lookahead
                  \g<sequence>*
                \)
              | \[                              # Regexp range starts with [
                  ^?                            # Optional character class negation
                  -?                            # Optional initial hyphen
                  ( (?![-\]])                   # Not a closing ] or hyphen
                    ( (\g<char> | [+*?()/|] )   # A character including some special chars
                      (-(?![-\]])((\g<char>)|[+*?()/|]))?       # or a range of them
                      | \[:(alpha|upper|lower|digit|alnum):\]   # POSIX character class
                    )
                  )*                            # zero or more range elements
                \]
              )[*+?]?                           # Optional repeat specifier
            )*                                  # Any number of atoms in an alternate
          )
          (\|\g<alternate>)*                    # More alternates in the sequence
        )
        /                                       # And a slash to finish off
      }x
    }

    def initialize adl, io
      @adl = adl
      @io = io
      @token = []
      @io.scan(parse_re) do |a|
        match = $~
        @token << match.names.map{|n| match[n] ? [n, match[n]] : nil}.compact[0]
      end
      # puts @token.map(&:inspect)*"\n"
    end

    def parse
      # Any rule that consumes a token that may be followed by white space should skip it before returning
      opt_white
      while object
      end
      error "Parse terminated at #{@token[0].inspect}" if @token[0]
    end

    def object
      return nil if peek('close') or !@token[0]
      return true if expect('semi')   # Empty definition
      object_name = path_name
      definition(object_name)
    end

    def definition(object_name)
      # print "In #{@adl.stack.last.inspect} looking at "; p peek; debugger
      unless defining = reference(object_name) || alias_from(object_name)
        supertype_name = type
        defining = @adl.start_object(object_name, supertype_name)
        is_block = block
        is_array = array_indicator
        is_assign = assignment(defining.parent, defining) # REVISIT: Check parameters
        if !is_block || is_array || is_assign
          peek 'close' or require 'semi'
        end
      end
      opt_white
      @adl.end_object
      true
    end

    def type
      if expect('inherits')
        opt_white
        supertype_name = path_name
        error('Expected supertype, body or ;') unless supertype_name or peek('semi') or peek('open') or peek('approx') or peek('equals')
        supertype_name
      end
    end

    def block
      if has_block = expect('open')
        opt_white
        @adl.start_block
        while object
        end
        require 'close'
        opt_white
        @adl.end_block
        true
      end
    end

    def array_indicator
      if expect 'lbrack'
        opt_white
        require 'rbrack'
        opt_white
        true
      end
    end

    def reference object_name
      if operator = (expect('arrow') or expect('darrow'))
        opt_white
        reference_to = path_name
        error('expected path name for Reference') unless reference_to

        # An eponymous reference used reference_to for object_name
        defining = @adl.start_object(object_name||reference_to, ['Reference'])

        # Create the reference:
        reference_object = @adl.resolve_name(reference_to)
        defining.assign(defining, reference_object, true)
        is_block = block
        # REVISIT: The following tentative assignment has the same parent as the final assignment above. Perhaps that assignment should be on `defining` not its parent?
        is_assigned = assignment(defining.parent, defining)
        peek('rbrack') or require 'semi' if !is_block || is_assigned
        opt_white
        defining
      end
    end

    def alias_from object_name
      return nil unless expect('rename')
      defining = @adl.start_object(object_name, ['Alias'])
      defining.assign(defining, @alias_for, false)
      defining
    end

    def assignment parent, variable
      if operator = (expect('equals') || expect('approx'))
        opt_white
        val = value
      end
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
      while expect('scope')
        opt_white
        names << '.'
      end
      while n = (expect('symbol') or expect('integer'))
        opt_white
        names << [n]
        while n = (expect('symbol') or expect('integer'))
          opt_white
          names[-1] << n  # Compound name - this is a following word
        end
        # Make the name array into a multi-word string:
        names[-1] = names[-1]*' '
        if expect('scope')
          opt_white
        else
          break
        end
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
      @adl.error "Parse failed at #{@token[0].inspect}: #{message}"
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

    def require token
      error "Expected #{token}" unless t = expect(token)
      t
    end

    # Return true if token is next
    def peek token
      @token[0] && @token[0][0] == token
    end
  end
end

ADL.new.parse(File.read(ARGV[0]))
