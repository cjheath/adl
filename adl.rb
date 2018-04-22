class ADL
  attr_reader :stack

  class ADLObject
    attr_reader :name, :parent, :zuper
    attr_accessor :is_array, :is_sterile, :is_complete

    def initialize parent, name, zuper, aspect = nil
      @parent, @name, @zuper, @aspect = parent, name, zuper, aspect || parent

      @parent.adopt(self) if @parent
    end

    def members
      @members ||= []
    end

    def adopt child
      if child.name && member?(child.name)
        raise "Cannot have two children called #{child.name}"
      end
      members << child
    end

    def ancestry
      @ancestry ||= begin
        ((@parent ? @parent.ancestry : []) + [self]).freeze
      end
    end

    def supertypes
      @supertypes ||= begin
        ([self]+(@zuper ? @zuper.supertypes : [])).freeze
      end
    end

    def assigned variable
      if variable.parent == (o = supertypes[-1])  # supertypes[-1] is always Object
        case variable.name
        when 'Name'; return [@name, o, true]
        when 'Parent'; return [@parent, o, true]
        when 'Super'; return [@zuper, o, true]
        when 'Aspect'; return [@aspect, o, true]
        when 'Syntax'; return [@syntax, o, true]
        when 'Is Array'; return [@is_array, o, true]
        when 'Is Sterile'; return [@is_sterile, o, true]
        when 'Is Complete'; return [@is_complete, o, true]
        end
      end
      existing = members.detect{|m| Assignment === m && m.variable == variable}
      existing && [existing.value, existing.parent, existing.is_final]
    end

    def assigned_transitive variable
      assigned(variable) or @zuper && @zuper.assigned_transitive(variable)
    end

    def assign variable, value, is_final
      a, p, f = assigned(variable)
      if a and a != value and variable != self   # Reference variable Super allows self-assignment
        raise "#{inspect} cannot have two assignments to #{variable.inspect}"
      end
      if variable.name == 'Syntax' && variable.parent.name == 'Object'  # Check namespace of Syntax properly here
        @syntax = Regexp.new('\A'+value[1..-2])
      else
        Assignment.new(self, variable, value, is_final)
      end
    end

    def is_reference
      supertypes[-2].name == 'Reference'
    end

    def is_syntax
      s = supertypes[-3] and s.name == 'Syntax'
    end

    def member? name
      name && members.detect{|m| m.name == name}
    end

    def member_transitive? name
      members.detect{|m| m.name == name} or
        (@zuper and @zuper.member_transitive?(name))
    end

    def syntax_transitive
      @syntax || (@zuper && @zuper.syntax_transitive)
    end

    def pathname
      (@parent && !@parent.top? ? @parent.pathname+'.' : '')+(@name || '<anonymous>')
    end

    def pathname_relative_to object
      pathname
=begin
      # REVISIT: Implement this properly
      s = ancestry
      o = object.ancestry
      c = s & o
      c.pop if (s-c).empty?
      '.'*(o-c).size + (s-c).map(&:name)*'.'
=end
    end

    def top?
      !@parent
    end

    def inspect
      "#{pathname}#{zuper_name}"
    end

    def zuper_name
      case
      when @zuper && @zuper.parent.parent == nil && @zuper.name == 'Object'
        ':'
      when @zuper
        ': '+@zuper.name
      else
        nil
      end
    end

    def emit level = nil
      unless level
        members.each do |m|
          m.emit('')
        end
        return
      end

      self_assignment = members.detect{|m| Assignment === m && m.variable == self }
      others = members-[self_assignment]
      has_attrs = !others.empty? || @syntax
      print "#{level}#{@name}#{zuper_name}#{has_attrs ? (zuper_name ? ' ' : '')+"{\n" : ''}"
      puts "#{level}\tSyntax = /#{@syntax.to_s.sub(/\?-mix:/,'')}/;" if @syntax
      others.each do |m|
        m.emit(level+"\t")
      end
      print "#{level}}" if has_attrs
      print "#{self_assignment && self_assignment.inline}" unless members.empty?
      puts((!has_attrs || self_assignment) ? ';' : '')
    end
  end

  class Assignment < ADLObject
    attr_reader :variable, :value, :is_final

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
          # REVISIT: Problem is, it's not always relative to our parent (c.f. self_assignment?):
          @value.pathname_relative_to(@parent)
        elsif Array === @value
          "[\n" +
          @value.map do |v|
            level+"\t"+
              if ADLObject === v
                v.pathname_relative_to(@parent)
              else
                v.inspect.gsub(/"/,"'")
              end
          end* ",\n" +
          "\n#{level}]"
        else
          @value.inspect.gsub(/"/,"'")
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
    @object = ADLObject.new(@top, 'Object', nil, nil)
    @regexp = ADLObject.new(@top, 'Regular Expression', @object, nil)
    @syntax = ADLObject.new(@object, 'Syntax', @regexp, nil)
    @reference = ADLObject.new(@top, 'Reference', @object, nil)
    @assignment = ADLObject.new(@top, 'Assignment', @object, nil)
    @alias = ADLObject.new(@top, 'Alias', @object, nil)
    @alias_for = ADLObject.new(@alias, 'For', @reference, nil)
  end

  def parse io, top = nil
    @scanner = Scanner.new(self, io)
    @stack = (top || @top).ancestry+[]
    @scanner.parse
  end

  def emit
    @top.emit
  end

  # print an indent for displaying the parse
  def indent
    print "\t"*(@stack.size-1)
  end

  # Report a parse failure
  def error message
    puts "#{message} at #{@scanner.location}"
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

    start_parent = o
    # Ascend the parent chain until we fail or find our first name:
    # REVISIT: If we descend a supertype's child, this may become contextual!
    unless no_ascend
      until path_name.empty? or path_name[0] == (m = o).name or m = o.member_transitive?(path_name[0])
        unless o.parent
          error("Failed to find #{path_name[0].inspect} from #{@stack.last.name}")
        end
        o = o.parent    # Ascend
      end
      o = m
      path_name.shift
    end
    error("Failed to find #{path_name[0].inspect} in #{start_parent.pathname}") unless o
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

    @stack.push o
    o
  end

  def end_object
    @stack.pop
  end

  def start_block
  end

  def end_block
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
      @input = io.to_s
      @offset = 0
      @current = nil    # The kind of token at @offset (if it has been scanned)
      @value = nil      # The text associated with the current token
    end

    def location
      line_number = @input[0, @offset].count("\n")+1
      column_number = @offset-(@input[0, @offset].rindex("\n") || 1)-1
      line_text = @input[(@offset-column_number)..-1].sub(/\n.*/m,'')
      "Line #{line_number} column #{column_number}:\n#{line_text}\n"
    end

    def next_token rule = nil
      match = (rule || parse_re).match(@input[@offset..-1])
      if !match
        @current = @value = nil
      elsif !rule
        @current, @value = match.names.map{|n| match[n] ? [n, match[n]] : nil}.compact[0]
      else
        @current = match ? rule : nil
        @value = match ? match.to_s : ''
      end
    end

    def parse
      # Any rule that consumes a token that may be followed by white space should skip it before returning
      opt_white
      while o = object
        last = o
      end
      error "Parse terminated at #{peek.inspect}" if peek
      last
    end

    def object
      return nil if peek('close') or !peek
      return @adl.stack.last if expect('semi')   # Empty definition
      object_name = path_name
      definition(object_name)
    end

    def definition(object_name)
      # print "In #{@adl.stack.last.inspect} defining #{object_name} and looking at "; p peek; debugger
      unless defining = reference(object_name) || alias_from(object_name)
        inheriting = peek('inherits')
        supertype_name = type

        # We only want to start a new object if there's a supertype, or a block, or an array indicator
        if inheriting || supertype_name || peek('lbrack')
          defining = @adl.start_object(object_name, supertype_name)
          has_block = block object_name
          defining.is_array = is_array = !!array_indicator
          @adl.end_object
          assignment(defining)
        elsif peek('open')
          defining = @adl.stack.last
          if reopen = @adl.resolve_name(object_name, 0)
            if (@adl.stack & reopen.ancestry) != @adl.stack
              # puts "REVISIT: Contextual extension"
            end

            # Save the stack to restore later, and replace it with reopen.ancestry
            saved = @adl.stack.dup
            @adl.stack.replace reopen.ancestry
            has_block = block object_name
            @adl.stack.replace saved          # restore the stack
            defining = reopen
          end
        elsif object_name and peek('equals') || peek('approx')
          variable = @adl.resolve_name(object_name, 0)
          is_assignment = defining = assignment(variable)
        elsif object_name
          # This may be the trailing namespace to use for a following file.
          defining = @adl.resolve_name(object_name, 0)
        end
        if !has_block || is_array || is_assignment
          peek 'close' or require 'semi'
        end
      end
      opt_white
      defining || true
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

        # Find what this is a reference to
        reference_object = @adl.resolve_name(reference_to, 0)

        # An eponymous reference uses reference_to for object_name. It better not be local.
        if !object_name && reference_object.parent == @adl.stack.last
          error("Reference to #{reference_to*' '} in #{reference_object.parent.pathname} cannot have the same name")
        end

        # Create the reference
        defining = @adl.start_object(object_name||reference_to, ['Reference'])
        defining.is_array = operator == '=>'

        # Add a final assignment (to itself) for its type:
        defining.assign(defining, reference_object, true)

        has_block = block object_name
        @adl.end_object

        # The following assignment has the same parent as the Reference itself
        has_assignment = assignment(defining)

        peek('rbrack') or require 'semi' if !has_block || has_assignment
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
      if operator = ((is_final = !!expect('equals')) || expect('approx'))
        opt_white
        parent = @adl.stack.last

        # Re-assignment is illegal
        local_value, = parent.assigned(variable)
        error("Cannot reassign #{parent.name}.#{variable.name}") if local_value

        # Detect the required value type from the variable, including arrays, and deal with it
        controlling_syntax = variable
        if variable.is_syntax
          val = expect('regexp')
        else
          if variable.is_reference
            existing, p, final = parent.assigned_transitive(variable) || variable.assigned(variable)
            refine_from = (final && existing) || parent.supertypes[-1] # Use Object as a default
          else
            # If this variable is a parameter of the same type as its parent, and this parent is also a subtype,
            # allow any value that the subtype would allow. This special case supports e.g. Number.Minimum
            if variable.supertypes.include?(variable.parent) && parent.supertypes.include?(variable.parent)
              controlling_syntax = parent
            end
            # Find an existing assignment, including inherited, to check for Is Final
            existing, p, final = parent.assigned_transitive(variable) || variable.assigned(variable)
            error("Cannot override final assignment #{parent.name}.#{variable.name} = #{existing.inspect}") if final
          end

          val = value(controlling_syntax, refine_from)
        end

        parent.assign variable, val, is_final
      end
    end

    def value variable, refine_from
      if variable.is_array
        val = array(variable, refine_from)
      else
        atomic_value(variable, refine_from)
      end
    end

    def atomic_value variable, refine_from
      # puts "Looking for value of #{variable.name}#{refine_from ? " refining #{refine_from.name}" : ''}"
      if refine_from
        if supertype_name = type
          # Literal object
          defining = @adl.start_object(nil, supertype_name)
          has_block = block nil
          @adl.end_object
          val = defining
        else
          p = path_name
          error("Assignment to #{variable.name} must name an ADL object") unless p
          val = @adl.resolve_name(p)
          # If the variable is a reference and refine_from is set, the object must be a subtype
        end
        if refine_from && !val.supertypes.include?(refine_from)
          error("Assignment of #{val.inspect} to #{variable.name} must refine the existing final assignment of #{refine_from.name}") unless p
        end
        val
      else
        syntax = variable.syntax_transitive
        error("#{variable.inspect} has no Syntax so cannot be assigned") unless syntax
        # Get the Syntax for the variable and REVISIT: accept a value of that type
        val = next_token syntax
        error("Expected value matching syntax for #{variable.name}") unless val
        consume
        opt_white
        val
        # val = (integer or string)
      end
    end

    def array variable, refine_from
      return nil unless expect('lbrack')
      array_value = []
      while val = atomic_value(variable, refine_from)
        array_value << val
        break unless expect('comma')
      end
      error("Array elements must separated by , and end in ]") unless expect('rbrack')
      array_value
    end

    def integer
      s = expect('integer') and eval(s)
    end

    def string
      s = expect('string') and eval(s)
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
      @adl.error message
    end

    # Consume the current token, returning the value
    def consume
      next_token unless @current
      # puts "consuming #{@current.inspect}" unless @current == 'white'
      t, v, @current, @value = @current, @value, nil, nil
      @offset += v.size
      v
    end

    # If token is next, consume it and return the value, else return nil
    def expect token
      next_token unless @current
      return nil unless @current && @current == token
      [consume, opt_white][0]
    end

    def require token
      error "Expected #{token}" unless t = expect(token)
      t
    end

    # Without consuming it, return or match the next token
    def peek token = nil
      next_token unless @current
      @current && (token ? @current == token : @current)
    end
  end
end

adl = ADL.new
if emit_all = (ARGV[0] == '-a')
  ARGV.shift
end
top = nil
ARGV.each do |file|
  top = adl.parse(File.read(file), top)
end
if emit_all || !top
  adl.emit
else
  top.emit ''
end
