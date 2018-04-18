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

    def emit level = nil
      unless level
        members.each do |m|
          m.emit('')
        end
        return
      end

      self_assignment = members.detect{|m| Assignment === m && m.parent == self }
      others = members-[self_assignment]
      zuper_name = @zuper ? (@zuper.parent.parent.parent == nil && @zuper.name == 'Object' ? ':' : ': '+@zuper.name) : nil
      print "#{level}#{@name}#{zuper_name && zuper_name}#{others.empty? ? nil : (zuper_name ? ' ' : '')+"{\n"}"
      others.each do |m|
        m.emit(level+"\t")
      end
      print "#{level}}" unless others.empty?
      print "#{self_assignment && self_assignment.inline}" unless members.empty?
      puts((others.empty? || self_assignment) ? ';' : '')
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

    def inline level = ''
      %Q{#{@is_final ? ' =' : ' ~='} #{
        if ADLObject === @value
          @value.pathname
        elsif Array === @value
          "[\n" +
          @value.map do |v|
            level+"\t"+
              if ADLObject === @value
                @value.pathname
              else
                @value.inspect
              end
          end* ",\n" +
          "#{level}]"
        else
          @value.inspect
        end
      }}
    end

    def emit level = ''
      puts "#{level}#{@variable.name}#{inline level};"
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
    @is_sterile = ADLObject.new(@object, 'Is Sterile', @boolean, nil)
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

  def emit
    @top.emit
  end

  # print an indent for displaying the parse
  def indent
    print "\t"*(@stack.size-1)
  end

  # A place to dis/enable parse progress messages.
  def show stuff
#    indent
#    puts stuff
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

    # Ascend the parent chain until we fail or find our first name:
    # REVISIT: If we descend a supertype's child, this may become contextual!
    until no_ascend or path_name.empty? or m = o.member?(path_name[0])
      o = o.parent    # Ascend
      error("Failed to find #{path_name[0].inspect} from #{@stack.last.name}") unless o
    end
    o = m
    path_name.shift
    return o if path_name.empty?

    # Now descend from the current position down the named children
    # REVISIT: If we descend a supertype's child, this becomes contextual!
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
      error "Parse terminated at #{peek.inspect}" if peek
    end

    def object
      return nil if peek('close') or !peek
      return true if expect('semi')   # Empty definition
      object_name = path_name
      definition(object_name)
    end

    def definition(object_name)
      # print "In #{@adl.stack.last.inspect} defining #{object_name} and looking at "; p peek; debugger
      unless defining = reference(object_name) || alias_from(object_name)
        inheriting = peek('inherits')
        supertype_name = type

        # We only want to start a new object if there's a supertype, or a block, or an array indicator
        if supertype_name || peek('open') || peek('lbrack') || inheriting
          defining = @adl.start_object(object_name, supertype_name)
          is_block = block object_name
          is_array = array_indicator
          @adl.end_object
          assignment(defining)
        elsif object_name
          # print "In #{@adl.stack.last.inspect} defining #{object_name} and looking at "; p peek
          variable = @adl.resolve_name(object_name)
          assigned = assignment(variable)
        end
        if !is_block || is_array || assigned
          peek 'close' or require 'semi'
        end
      end
      opt_white
      true
    end

    def type
      if expect('inherits')
        opt_white
        supertype_name = path_name
        error('Expected supertype, body or ;') unless supertype_name or peek('semi') or peek('open') or peek('approx') or peek('equals')
        supertype_name || ['Object']
      end
    end

    def block object_name
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

        # Create the reference
        # An eponymous reference uses reference_to for object_name
        defining = @adl.start_object(object_name||reference_to, ['Reference'])

        # Find what this is a reference to, and add a final assignment (to itself) for its type:
        reference_object = @adl.resolve_name(reference_to)
        defining.assign(defining, reference_object, true)

        is_block = block object_name
        @adl.end_object

        # The following assignment has the same parent as the Reference itself
        assignment(defining)
        peek('rbrack') or require 'semi' if !is_block || is_assigned
        opt_white
        defining
      end
    end

    def alias_from object_name
      return nil unless expect('rename')
      defining = @adl.start_object(object_name, ['Alias'])
      defining.assign(defining, @alias_for, false)
      @adl.end_object
      defining
    end

    def assignment variable
      if operator = ((is_final = expect('equals')) || expect('approx'))
        opt_white

        # REVISIT: Detect the required value type here, including arrays, and deal with it
        val = value

        parent = @adl.stack.last
        parent.assign variable, val, is_final
      end
    end

    def value
      integer or string or (p = path_name and @adl.resolve_name(p)) or array or expect('regexp')
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
      @adl.error "Parse failed at #{peek.inspect}: #{message}"
    end

    # Consume the current token, returning the value
    def consume
      # puts "consuming #{peek.inspect}" unless @token[0][0] == 'white'
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

    # Without consuming it, return or match the next token
    def peek token = nil
      @token[0] && (token ? @token[0][0] == token : @token[0])
    end
  end
end

adl = ADL.new
adl.parse(File.read(ARGV[0]))
adl.emit
