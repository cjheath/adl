/*
 * Test driver for parsing ADL to an ADL::MemStore
 *
 * The Source the parser reads from, and with it the Sink that receives the
 * parse, is switchable, because the two are worth comparing and both should
 * keep working:
 *
 *	-DADL_SOURCE_UTF8PTR	the byte-pointer Source. It views raw bytes, so
 *				every fragment the Sink keeps has to be copied.
 *	(default)		the StrVal Source. It views a StrVal, so every
 *				fragment is a slice() of that one body.
 *
 * adlmem_utf8ptr in the Makefile builds the former, and analysis/ builds and
 * compares both.
 */
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>

#include	<array.h>
#include	<strval.h>
#include	<adlparser.h>
#include	<adlstore.h>
#include	<adlmem.h>
#include	<adlstrval.h>
#include	<adl_display.h>		// The only place that prints what was reported


StrVal inspect(ADL::Handle, int depth = 0);
void p(ADL::Handle h);
void p(ADL::MemStore m);

char* slurp_file(const char* filename, off_t* size_p)
{
	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		text;
	if ((fd = open(filename, O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (text = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, text, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(filename);
		exit(1);
	}
	if (size_p)
		*size_p = stat.st_size;
	text[stat.st_size] = '\0';

	return text;
}

#if	defined(ADL_SOURCE_UTF8PTR)
typedef	ADLStoreSink<ADL::MemStore>	ADLMemStoreSink;
typedef	ADLSourceUTF8Ptr		ADLMemSource;
#else
typedef	ADLStrValSink<ADL::MemStore>	ADLMemStoreSink;
typedef	ADLSourceStrVal			ADLMemSource;
#endif

bool load_file(ADLMemStoreSink& sink, const char* filename, bool last_file)
{
	off_t				file_size;
	char*				raw = slurp_file(filename, &file_size);

	int				total_lines = 0;
	for (off_t i = 0; i < file_size; i++)
		if (raw[i] == '\n')
			total_lines++;

	/*
	 * A Source views its input, it never owns it - but how long the input has
	 * to stay alive differs, and so does the peak, which is one of the things
	 * being measured. The byte-pointer form keeps the buffer for the whole
	 * parse; the StrVal form copies the file into a body of its own, so `raw`
	 * is finished with as soon as that exists.
	 */
#if	defined(ADL_SOURCE_UTF8PTR)
	ADLMemSource			source(raw);			// views raw
#else
	StrVal				text(raw, (StrValIndex)file_size);	// owns a copy
	ADLMemSource			source(&text);
	delete [] raw;							// now finished with
#endif

	ADLParser<ADLMemStoreSink>	adl(sink);		// a Parser to feed the Sink
	bool				ok = adl.parse(source);

	/*
	 * Where the first message was reported, read before the drain empties the
	 * buffer. The parser recovers and carries on, so where it finally stopped
	 * says nothing about how much of the input was good: the first error is
	 * the place parsing stopped being clean, and the text successfully parsed
	 * is what lies before it.
	 */
	int				first_line = 0;
	int				first_column = 0;
	(void)adl_first_error_position(first_line, first_column);

	/*
	 * How far the parse got, and out of what. The StrVal Source counts
	 * characters, since that is what its slices are indexed in; the
	 * byte-pointer one has only bytes. Either way the criterion is that the
	 * whole input was consumed, which is what makes tests/invalid-syntax.adl
	 * fail: parse() returns true having read none of it. Up to the first
	 * error, that reach is measured in this flavour's own unit: lines for the
	 * StrVal Source, bytes for the byte-pointer one, which cannot be counted
	 * in the other's terms.
	 */
#if	defined(ADL_SOURCE_UTF8PTR)
	long long			consumed = source.peek() - raw;
	long long			total = file_size;
	const char*			unit = "bytes";
	long long			successful = first_line > 0
						? adl_bytes_before((const UTF8*)raw, first_line, first_column)
						: consumed;
	delete [] raw;					// Every fragment was a copy, so
							// nothing refers to it now
#else
	long long			consumed = source.line_number()-1;
	long long			total = total_lines;
	const char*			unit = "lines";
	long long			successful = first_line > 0 ? first_line-1 : consumed;
#endif

	/*
	 * Show what the parse reported, and count it in the same act: the buffer
	 * is the only record of what was reported, so the display and the count
	 * cannot disagree.
	 */
	unsigned	errors = adl_display_errors(filename);

	/*
	 * We succeed when: the grammar ran to completion, the whole input
	 * was consumed, and nothing was rejected along the way. What is reported
	 * is then how far the input got before the first error, which is where
	 * parsing stopped being clean - not how far it recovered afterwards.
	 *
	 * A sequence of files is one parse continuing from where the last left
	 * off, and it is the last of them that the reader is depending on, so a
	 * success is announced only for that one. A failure is announced wherever
	 * it happens - that is the file the run stopped on.
	 */
	bool	clean = ok && consumed == total && errors == 0;
	if (!clean || last_file)
	{
		if (errors > 0 && first_line > 0)
			adl_display_line(StrVal::format(
				"Failed, parsed {1} of {2} {3} before the first error at {4}:{5}, {6} {7}",
				VariantArray() << successful << total << unit
					<< first_line << first_column << errors
					<< (errors == 1 ? "error" : "errors")));
		else
			adl_display_line(StrVal::format(
				"{1}, processed {2} of {3} {4}, {5} {6}",
				VariantArray() << (clean ? "Success" : "Failed") << consumed << total << unit
					<< errors << (errors == 1 ? "error" : "errors")));
	}

	return clean;
}

int main(int argc, const char** argv)
{
	ADL::MemStore	store;			// Use the memory store
	ADLMemStoreSink	sink(store);		// Use the adapter
	bool		show_all = false;
	bool		dump_always = false;	// -d: dump the Store even after errors

	/*
	 * Usually each ADL file starts with root_object set to the last object finalised in the previous file.
	 * -T says to start the next file again at TOP, not the previous file's last object.
	 */
	bool		fresh_top = false;

	/*
	 * Which file the run is judged by: the last one named. Counted before the
	 * loop eats its arguments, and by the same rule the loop uses to tell a
	 * filename from an option.
	 */
	int		last_file = 1;
	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "-a") && strcmp(argv[i], "-T") && strcmp(argv[i], "-d"))
			last_file = i;

	const char*	program_name = argv[0];
	bool		ok = true;
	int		arg_index = 1;
	for (--argc, ++argv; ok && argc > 0; argc--, argv++, arg_index++)
	{
		const char*	filename = *argv;
		if (0 == strcmp(filename, "-a"))
		{
			show_all = true;
			continue;
		}
		if (0 == strcmp(filename, "-T"))
		{
			fresh_top = true;
			continue;
		}
		if (0 == strcmp(filename, "-d"))
		{
			dump_always = true;
			continue;
		}
		sink.root_object = fresh_top ? store.top() : sink.last_object();
		ok = load_file(sink, filename, arg_index == last_file);
	}

	/*
	 * Dumping the Store is for a parse that reported nothing: after errors,
	 * what it holds is whatever partial state the parse recovered to, which
	 * is not what a reader wants to see. -d asks for the tree anyway.
	 */
	if (!ok && !dump_always)
		return 1;

	ADL::MemStore::Handle	last = show_all ? store.top() : sink.last_object();

	p(last);
	return 0;
}

/*
 * Debugging functions
 */
StrVal inspect(ADL::Value v)
{
	if (v.elements.length() > 0)
	{
		StringArray	parts;
		const ADL::Value*	elems = v.elements.asElements();
		for (int i = 0; i < v.elements.length(); i++)
			parts.push(inspect(elems[i]));
		return "[" + parts.join(", ") + "]";
	}
	if (v.handle.is_null())
		return "\""+v.string+"\"";
	// Avoid infinite recursion using pathname, not by re-expanding the subtree.
	// (e.g. adl.adl's Object.Parent -> Object) would cause infitite recursion otherwise.
	return "-> " + v.handle.pathname();
}

StrVal inspect(ADL::Handle h, int depth)
{
	if (h.is_null())
		return "<NULL>";
	auto	super = h.super();
	auto	sp = !super.is_null() ? super.parent() : ADL::Handle();
	auto	v = h.value();
	Array<ADL::Handle>&	c = h.children();
	StrVal	indent = StrVal("\t")*depth;
	StrVal	body;
	if (c.length() > 0)					// Children
	{
		StringArray	parts;
		const ADL::Handle*	kids = c.asElements();
		for (int i = 0; i < c.length(); i++)
			parts.push(inspect(kids[i], depth+1));
		body = " {\n\t" + indent + parts.join("\n\t"+indent) + "\n" + indent + "}";
	}
	else
		body = ";";

	return	h.name()					// Object name
		+ (!super.is_null() ? " : "+super.name() : ":")	// Supertype
		+ (!sp.is_null() && sp.parent().is_null() && super.name() == "Assignment"
		   ? (h.is_final() ? "=" : "~") + inspect(v)	// Assigned value
		   : ""
		  )
		+ body;
}

void p(ADL::Handle h)
{
	adl_display_line(inspect(h));
}

void p(ADL::MemStore m)
{
	p(m.top());
}

void p_str(StrVal s) { p(s); }

void p(const ADLMemStoreSink::Frame& f)
{
	adl_display_line(
		StrVal("Frame {\n  object_path='")+f.object_path.display()
		+"';\n  supertype_path='"+f.supertype_path.display()
		+"';\n  object_started="+(f.object_started ? "true" : "false")
		+"\n  obj_array="+(f.obj_array ? "true" : "false")
		+"\n  value_type="+StrVal::fromInt32(f.value_type)
		+"\n  value='"+((StrVal)f.value)
		+"';\n}");
}
