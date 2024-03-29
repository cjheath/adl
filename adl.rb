#
# Aspect Definition Language.
#
# Instantiate the ADL::Context and ask it to parse some ADL text.
# The Context will use an ADL::Scanner to parse the input.
#
# The ADL text will be compiled into a tree of Objects, including Assignments,
# which your code can then traverse to produce output. Start traversing the
# tree at context.top(), or at the last open namespace returned from parse().
#
module ADL
  # A builtin value is either a string, an Object, a Regex or an Array of those.
  # All value subclasses retain the original text of the assignment.
  # Implementations may define additional subclasses.
  # Some value subclasses may also create a semantic implementation.
  class Value
    def initialize lexical
      @lexical = lexical
    end

    def representation
      @lexical
    end

    def self.inherited(subtype)
      Value.subtypes << subtype
    end

    def self.subtypes
      @subtypes ||= []
    end
  end

  class StringValue < Value
    def value
      @value ||= eval(@lexical)
    end
  end

  class IntegerValue < StringValue
    def value
      @value ||= eval(@lexical)
    end
  end

  class ObjectValue < Value
    def initialize reference
      super(nil)  # We don't use a lexical representation here
      @reference = reference
    end

    def representation
      if @reference.is_object_literal
        @reference.as_inline
      else
        @reference.pathname
      end
    end

    def obj
      @reference
    end
  end

  class RegexValue < Value
    def regex
      @regex ||= Regexp.new('\A'+@lexical)
    end
  end

  class ArrayValue < Value
    def initialize array
      super(nil)  # We don't use a lexical representation here
      @array = array
    end

    def representation
      # REVISIT: Include newlines and indentation here, and let the caller further indent it.
      "["+@array.map(&:representation)*", "+"]"
    end

    def size
      @array.size
    end

    def [](i)
      @array[i]
    end

    def add element
      @array << element
    end
  end

  class Object
    attr_reader :parent, :name, :zuper, :aspect, :syntax
    attr_accessor :is_array, :is_sterile, :is_complete

    def initialize parent, name, zuper, aspect = nil
      @parent, @name, @zuper, @aspect = parent, name, zuper, aspect || parent

      @parent.adopt(self) if @parent
    end

    # Manage naming
    def pathname
      # Return a string with all ancestor names from the top.
      (@parent && !@parent.top? ? @parent.pathname+'.' : '') +
        (@name || '<anonymous>')
    end

    def pathname_relative_to object
      pathname
=begin
      # REVISIT: Implement emitting relative names properly
      s = ancestry
      o = object.ancestry
      c = s & o
      c.pop if (s-c).empty?
      '.'*(o-c).size + (s-c).map(&:name)*'.'
=end
    end

    # Manage children
    def children
      @children ||= []
    end

    def adopt child
      if child.name && child?(child.name)
        raise "Cannot have two children called #{child.name}"
      end
      children << child
    end

    def child? name
      name && children.detect{|m| m.name == name}
    end

    # Search this object and its supertypes for an object of the given name
    # REVISIT: This is where we need to implement aliases
    def child_transitive? name
      children.detect{|m| m.name == name} or
        (@zuper and @zuper.child_transitive?(name))
    end

    # Manage inheritance
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

    # Search assignments
    def assigned variable
      if variable.parent == (o = supertypes[-1])  # supertypes[-1] is always Object
        result =
          case variable.name
          when 'Name'; [StringValue.new(@name), o, true]
          when 'Parent'; [ObjectValue.new(@parent), o, true]
          when 'Super'; [ObjectValue.new(@zuper), o, true]
          when 'Aspect'; [ObjectValue.new(@aspect), o, true]
          when 'Syntax'; [@syntax, o, true]
          when 'Is Array'; [@is_array, o, true]
          when 'Is Sterile'; [@is_sterile, o, true]
          when 'Is Complete'; [@is_complete, o, true]
          end
        if result
          result[2] = false unless result[0] != nil
          return result
        end
      end
      existing = children.detect{|m| Assignment === m && m.variable == variable}
      existing && [existing.value, existing.parent, existing.is_final]
    end

    def assigned_transitive variable
      assigned(variable) or @zuper && @zuper.assigned_transitive(variable)
    end

    def assign variable, value, is_final
      a, = assigned(variable)
      if a and a != value and variable != self   # Reference variable Super allows self-assignment
        raise "#{inspect} cannot have two assignments to #{variable.inspect}"
      end
      if variable.is_syntax
        @syntax = value   # Will be a RegexValue
      else
        Assignment.new(self, variable, value, is_final)
      end
    end

    # Manage built-ins
    def is_reference
      supertypes.size >= 2 &&
      supertypes[-1].name == 'Object' &&
      supertypes[-2].name == 'Reference'
    end

    def is_syntax
      (s = supertypes) and s.size >= 3 and s[-1].name == 'Object' and s[-2].name == 'Regular Expression' and s[-3].name == 'Syntax'
    end

    def is_object_literal
      parent == nil && name == nil
    end

    def syntax_transitive
      (@syntax && @syntax.regex) || (@zuper && @zuper.syntax_transitive)
    end

    def top?
      !@parent
    end

    # Manage generation of textual output
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

    def as_inline
      # This is used for object literals
      self_assignment = children.detect{|m| m.variable == self}
      others = children-[self_assignment]
      ":#{@zuper.name}#{
        others.empty? ? '' : '{' + others.map{|m| m.variable.name + m.as_inline}*'; ' + '}'
      }#{self_assignment ? self_assignment.as_inline : ''}"
    end

    def emit level = nil
      unless level
        children.each do |m|
          m.emit('')
        end
        return
      end

      self_assignment = children.detect{|m| Assignment === m && m.variable == self }
      others = children-[self_assignment]
      has_attrs = !others.empty? || @syntax

      print "#{level}#{@name}#{zuper_name}#{has_attrs ? (zuper_name ? ' ' : '')+"{\n" : ''}"
      puts "#{level}\tSyntax = /#{@syntax.representation}/;" if @syntax
      others.each do |m|
        m.emit(level+"\t")
      end
      print "#{level}}" if has_attrs
      print "#{self_assignment && self_assignment.as_inline}" unless children.empty?
      puts((!has_attrs || self_assignment) ? ';' : '')
    end
  end

  class Assignment < Object
    attr_reader :variable, :value, :is_final

    def initialize parent, variable, value, is_final
      super parent, nil, @assignment
      raise("REVISIT: Null value assigned") unless value
      @variable, @value, @is_final = variable, value, is_final
    end

    def inspect
      "#{@parent ? @parent.pathname : '-'}.#{@variable.name}#{@is_final ? '=' : '~='} #{@value.inspect}"
    end

    def as_inline level = ''
      (@is_final ? " = " : " ~= ") + value.representation
    end

    def emit level = ''
      puts "#{level}#{@variable.name}#{as_inline level};"
    end
  end

  class Context
    def initialize
      make_built_ins
    end

    attr_reader :top
    attr_reader :syntax   # Expose the Syntax Variable so we can copy Syntax values

    attr_reader :stack
    def stacktop
      @stack.last
    end

    def parse io, filename, top = nil
      @scanner = Scanner.new(self, io, filename)
      @stack = (top || @top).ancestry+[]
      @scanner.parse
    end

    # Report a parse failure
    def error message
      puts "#{message} at #{@scanner.location}"
      exit 1
    end

    def resolve_name path_name, levels_up = 0
      o = stacktop
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
      unless no_ascend
        until path_name.empty? or path_name[0] == (m = o).name or m = o.child_transitive?(path_name[0])
          unless o.parent
            error("Failed to find #{path_name[0].inspect} from #{stacktop.name}")
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
        m = o.child?(n)
        error("Failed to find #{n.inspect} in #{o.pathname}") unless m
        o = m   # Descend
      end
      o
    end

    # Called when the name and supertype have been parsed
    def start_object(object_name, supertype_name, orphan = false)
      # Resolve the object_name prefix to find the parent and local name:
      if object_name
        parent = resolve_name(object_name[0..-2])
        @stack.replace(parent.ancestry)
      else
        parent = stacktop
      end

      # Resolve the supertype_name to find the zuper:
      zuper = supertype_name ? resolve_name(supertype_name) : @object

      local_name = object_name ? object_name[-1] : nil
      if local_name and o = parent.child?(local_name)
        error("Cannot change supertype of #{local_name} from #{o.zuper.name} to #{supertype_name*' '}") if supertype_name && o.zuper.name != supertype_name*' '
      else
        o = Object.new(orphan ? nil : parent, local_name, zuper)
      end

      @stack.push o
      o
    end

    def end_object
      @stack.pop
    end

    def make_built_ins
      @top = Object.new(nil, 'TOP', nil, nil)
      @object = Object.new(@top, 'Object', nil, nil)
      @regexp = Object.new(@top, 'Regular Expression', @object, nil)
      @syntax = Object.new(@object, 'Syntax', @regexp, nil)
      @reference = Object.new(@top, 'Reference', @object, nil)
      @assignment = Object.new(@top, 'Assignment', @object, nil)
      # @alias = Object.new(@top, 'Alias', @object, nil)
      # @alias_for = Object.new(@alias, 'For', @reference, nil)
    end
  end

  class Scanner
    def initialize context, io, filename
      @context = context
      @input = io.to_s
      @filename = filename
      @offset = 0
      @current = nil    # The kind of token at @offset (if it has been scanned)
      @value = nil      # The text associated with the current token
    end

    def location
      line_number = @input[0, @offset].count("\n")+1
      column_number = @offset-(@input[0, @offset].rindex("\n") || 1)-1
      line_text = @input[(@offset-column_number)..-1].sub(/\n.*/m,'')
      "line #{line_number} column #{column_number} of #{@filename}:\n#{line_text}\n"
    end

    def parse
      # Any rule that consumes a token that may be followed by white space should skip it before returning
      opt_white
      while o = definition
        last = o
      end
      error "Parse terminated at #{peek.inspect}" if peek
      last
    end

  private
    def definition
      return nil if peek('close') or !peek
      return @context.stacktop if expect('semi')   # Empty definition
      object_name = path_name
      body(object_name)
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

    def body(object_name)
      # print "In #{@context.stacktop.inspect} defining #{object_name} and looking at "; p peek; debugger
      save = @context.stack.dup
      unless defining = reference(object_name) || alias_from(object_name)
        inheriting = peek('inherits')
        supertype_name = supertype

        # We only want to start a new object if there's a supertype, or a block, or an array indicator
        if inheriting || supertype_name || peek('lbrack')
          defining = @context.start_object(object_name, supertype_name)
          has_block = block object_name
          defining.is_array = is_array = !!array_indicator
          is_assignment = assignment(defining)
          @context.end_object
        elsif peek('open')
          defining = @context.resolve_name(object_name)
          if (@context.stack & defining.ancestry) != @context.stack
            # puts "REVISIT: Contextual extension"
          end
          @context.stack.replace defining.ancestry
          has_block = block object_name
        elsif object_name and peek('equals') || peek('approx')
          reopen = @context.resolve_name(object_name[0..-2])
          @context.stack.replace reopen.ancestry
          variable = @context.resolve_name([object_name[-1]])
          is_assignment = defining = assignment(variable)
        elsif object_name
          # This may be the trailing namespace to use for a following file.
          defining = @context.resolve_name(object_name)
        end
        if !has_block || is_array || is_assignment
          peek 'close' or require 'semi'
        end
      end
      @context.stack.replace(save)
      opt_white
      defining || true
    end

    def supertype
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
        while definition
        end
        require 'close'
        opt_white
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
        reference_object = @context.resolve_name(reference_to)

        # An eponymous reference uses reference_to for object_name. It better not be local.
        if !object_name && reference_object.parent == @context.stacktop
          error("Reference to #{reference_to*' '} in #{reference_object.parent.pathname} cannot have the same name")
        end

        # Create the reference
        defining = @context.start_object(object_name||[reference_to.last], ['Reference'])
        defining.is_array = operator == '=>'

        # Add a final assignment (to itself) for its type:
        defining.assign(defining, ObjectValue.new(reference_object), true)

        has_block = block object_name
        @context.end_object

        # The following assignment has the same parent as the Reference itself
        has_assignment = assignment(defining)

        peek('rbrack') or require 'semi' if !has_block || has_assignment
        opt_white
        defining
      end
    end

    def alias_from object_name
      return nil unless expect('rename')
      defining = @context.start_object(object_name, ['Alias'])
      defining.assign(defining, @alias_for, false)
      @context.end_object
      defining
    end

    def assignment variable
      if operator = ((is_final = !!expect('equals')) || expect('approx'))
        opt_white
        parent = @context.stacktop

        # Re-assignment is illegal
        local_value, = parent.assigned(variable)
        error("Cannot reassign #{parent.name}.#{variable.name} from #{local_value.inspect}") if local_value

        # Detect the required value type from the variable, including arrays, and deal with it
        controlling_syntax = variable
        if variable.is_syntax
          if regexp_string = expect('regexp')
            val = RegexValue.new(regexp_string[1..-2])  # Strip off leading and trailing /
          elsif p = path_name and
            existing_syntax = @context.resolve_name(p) and
            val = existing_syntax.assigned(@context.syntax) and
            val = val[0]
          else
            error("Assignment to Syntax variable requires a regular expression")
          end
        else
          if variable.is_reference
            overriding, p, final = parent.assigned_transitive(variable) || variable.assigned(variable)
            refine_from = (final && overriding) || parent.supertypes[-1] # Use Object as a default
            refine_from =
              if overriding && final
                if ArrayValue === overriding
                  overriding[0].obj
                else
                  overriding.obj
                end
              end
            refine_from ||= parent.supertypes[-1] # Use Object as a default
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

          val = parse_value(controlling_syntax, refine_from)
        end

        parent.assign variable, val, is_final
      end
    end

    def parse_value variable, refine_from
      if variable.is_array and peek('lbrack')
        val = parse_array(variable, refine_from)
      else
        val = atomic_value(variable, refine_from)
        val = ArrayValue.new([val]) if variable.is_array
        val
      end
    end

    def atomic_value variable, refine_from
      # puts "Looking for value of #{variable.name}#{refine_from ? " refining #{refine_from.name}" : ''}"
      if refine_from
        if supertype_name = supertype
          # Literal object. What should the parent of such objects be?
          defining = @context.start_object(nil, supertype_name, true)
          block(nil)
          assignment(defining)
          @context.end_object
        else
          p = path_name
          error("Assignment to #{variable.name} must name an ADL object") unless p
          is_self_assignment = variable == @context.stacktop
          defining = @context.resolve_name(p, is_self_assignment ? 1 : 0)
        end
        val = ObjectValue.new(defining)
        if refine_from && !defining.supertypes.include?(refine_from)
          # If the variable is a reference and refine_from is set, the object must be a subtype
          error("Assignment of #{val.inspect} to #{variable.name} must refine the existing final assignment of #{refine_from.name}")
        end
        val
      else
        syntax = variable.syntax_transitive
        error("#{variable.inspect} has no Syntax so cannot be assigned") unless syntax
        # Get the Syntax for the variable and accept a value of that type
        val = next_token syntax
        error("Expected a value matching the syntax for an #{variable.name}") unless val
        consume
        opt_white
        # Use the Value subtype defined for this variable
        value_type = nil
        variable.supertypes.detect do |t|
          next unless t.name
          compressed = t.name.gsub(/ /,'')
          value_type = Value.subtypes.detect do |k|
            subtype_name = k.name.sub(/ADL::(.*)Value/,'\1')
            subtype_name == compressed
          end
        end
        value_type = StringValue if value_type == ObjectValue

        value_type.new(val)
      end
    end

    def parse_array variable, refine_from
      return nil unless expect('lbrack')
      array_value = []
      while val = atomic_value(variable, refine_from)
        array_value << val
        break unless expect('comma')
      end
      error("Array elements must separated by , and end in ]") unless expect('rbrack')
      ArrayValue.new(array_value)
    end

    def integer
      s = expect('integer') and StringValue.new(s)
    end

    def opt_white
      white or true
    end

    def white
      expect('white')
    end

    Tokens = {
      white: %r{(\s|//.*)+},
      symbol: %r{[_\p{L}][_\p{L}\p{N}\p{Mn}]*},
      integer: %r{0|[-+]?([1-9]\d*)},
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
                  \\s                           # Single whitespace. Explicit space not allowed!
                  | \\[0-3][0-7][0-7]           # An octal character
                  | \\x[0-9A-F][0-9A-F]         # A hexadecimal character
                  | \\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F] # A Unicode character
                  | \\[pP]{[A-Za-z_]+}          # Unicode category or block
                  | \\[0befntr\\*+?()|/\[]      # A control character or escaped regexp char
                  | \\[sd]                      # space, digit REVISIT: add hexdigit
                  | [^*+?()/|\[ ]               # Anything else except these
                )
              | \(                              # A parenthesised group
                  (\?(<[_\p{L}\p{Mn}\p{N}]*>|!))?      # Optional capture name or negative lookahead
                  \g<sequence>*
                \)
              | \[                              # Character class starts with [
                  ^?                            # Optional character class negation
                  -?                            # Optional initial hyphen
                  ( (?![-\]])                   # Not a closing ] or hyphen
                    ( (\g<char> | [+*?()/|] )   # A character including some special chars
                      (-(?![-\]])((\g<char>)|[+*?()/|]))?       # or a range of them
                      | \\[pP]{_[A-Za-z]+}      # Unicode category or block, +ve or -ve
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

    # Report a parse failure
    def error message
      @context.error message
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

if $0 == __FILE__
  context = ADL::Context.new
  if emit_all = (ARGV[0] == '-a')
    ARGV.shift
  end
  top = nil
  ARGV.each do |filename|
    top = context.parse(File.read(filename), filename, top)
  end
  if emit_all || !top
    context.top.emit
  else
    top.emit ''
  end
end
