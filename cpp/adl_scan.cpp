/*
 * Test stub for just the ADL Parser, with a null Sink
 */
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>

#include	<adlparser.h>
#include	<adl_display.h>		// The only place that prints what was reported

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

int main(int argc, const char** argv)
{
	const char*		filename = argv[1];
	off_t			file_size;
	char*			text = slurp_file(filename, &file_size);
	ADLSinkStub<>		sink;
	ADLParser<>		adl(sink);
	ADLSourceUTF8Ptr	source(text);

	bool			ok = adl.parse(source);
	off_t			bytes_parsed = source.peek() - text;

	/*
	 * Where the first message was reported, read before the drain empties the
	 * buffer: parse() answers whether the grammar ran to completion, which it
	 * does even having reported, so only the buffer says what the input was
	 * worth, and how far it was good.
	 */
	int			first_line = 0;
	int			first_column = 0;
	(void)adl_first_error_position(first_line, first_column);

	unsigned		errors = adl_display_errors(filename);

	if (errors > 0 && first_line > 0)
		adl_display_line(StrVal::format(
			"Failed, parsed {1} of {2} bytes before the first error at {3}:{4}, {5} {6}",
			VariantArray() << adl_bytes_before((const UTF8*)text, first_line, first_column)
				<< file_size << first_line << first_column
				<< errors << (errors == 1 ? "error" : "errors")));
	else
		adl_display_line(StrVal::format(
			"{1}, parsed {2} of {3} bytes",
			VariantArray() << (ok && errors == 0 ? "Success" : "Failed")
				<< bytes_parsed << file_size));

	exit(ok && errors == 0 ? 0 : 1);
}
