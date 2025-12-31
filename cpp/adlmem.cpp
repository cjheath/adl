/*
 * Test driver for parsing ADL to an ADL::MemStore
 */
#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>

#include	<array.h>
#include	<strval.h>
#include	<adlparser.h>
#include	<adlstore.h>
#include	<adlmem.h>

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

typedef	ADLStoreSink<ADL::MemStore>	ADLMemStoreSink;

bool load_file(ADLMemStoreSink& sink, const char* filename)
{
	off_t				file_size;
	char*				text = slurp_file(filename, &file_size);

	ADLParser<ADLMemStoreSink>	adl(sink);		// a Parser to feed the Sink
	ADLSourceUTF8Ptr		source(text);		// Some input for the parser

	bool				ok = adl.parse(source);
	off_t				bytes_parsed = source - text;
	printf("%s, parsed %lld of %lld bytes\n", ok ? "Success" : "Failed", bytes_parsed, file_size);

	return bytes_parsed == file_size;
}

int main(int argc, const char** argv)
{
	ADL::MemStore	store;			// Use the memory store
	ADLMemStoreSink	sink(store);		// Use the adapter

	const char*	program_name = argv[0];
	bool		ok = true;
	for (--argc, ++argv; ok && argc > 0; argc--, argv++)
	{
		const char*	filename = *argv;
		ok = load_file(sink, filename);
	}

	ADL::MemStore::Handle		top = store.top();

	p(top);
	exit(ok ? 0 : 1);
}

/*
 * Debugging functions
 */
StrVal inspect(ADL::Value v)
{
	   return v.handle.is_null()
		   ? "\""+v.string+"\""
		   : inspect(v.handle);
}

StrVal inspect(ADL::Handle h, int depth)
{
	auto	super = h.super();
	auto	sp = !super.is_null() ? super.parent() : ADL::Handle();
	auto	v = h.value();
	Array<ADL::Handle>&	c = h.children();
	StrVal	indent = StrVal("\t")*depth;

	return	h.name()					// Object name
		+ (!super.is_null() ? " : "+super.name() : ":")	// Supertype
		+ (!sp.is_null() && sp.parent().is_null() && super.name() == "Assignment"
		   ? (h.is_final() ? "=" : "~") + inspect(v)	// Assigned value
		   : ""
		  )
		+ (c.length() > 0					// Children
		   ?	" {\n\t"
			+ indent
		 	+ c.template map<StringArray, StrVal>(
				[&](const ADL::Handle& h) -> const StrVal
				{ return inspect(h, depth+1); }
			).join("\n\t"+indent)
			+ "\n" + indent + "}"
		   : ";"
		);
}

void p(ADL::Handle h)
{
	printf("%s\n", inspect(h).asUTF8());
}

void p(ADL::MemStore m)
{
	p(m.top());
}

void p(const ADLStoreSink<ADL::MemStore>::Frame& f)
{
	printf(	"Frame {\n"
		"  object_path='%s';\n"
		"  supertype_path='%s';\n"
		"  object_started=%s;\n"
		"  obj_array=%s;\n"
		"  value_type=%d;\n"
		"  value='%s';\n"
	//	"  handle->%p;\n"
		"}\n",
		f.object_path.display().asUTF8(),
		f.supertype_path.display().asUTF8(),
		f.object_started ? "true" : "false",
		f.obj_array ? "true" : "false",
		f.value_type,
		((StrVal)f.value).asUTF8()
	//	&f.handle._object()
	);
}
