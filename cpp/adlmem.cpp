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

int main(int argc, const char** argv)
{
	const char*			filename = argv[1];
	off_t				file_size;
	char*				text = slurp_file(filename, &file_size);

	ADLSourceUTF8Ptr		source(text);		// Some input for the parser
	ADL::MemStore			store;			// Use the memory store
	typedef	ADLStoreSink<ADL::MemStore>	ADLMemStoreSink;
	ADLMemStoreSink			sink(store);		// Use the adapter
	ADLParser<ADLMemStoreSink>	adl(sink);		// With a Parser to feed it

	bool				ok = adl.parse(source);
	off_t				bytes_parsed = source - text;

	ADL::MemStore::Handle		top = store.top();

	printf("%s, parsed %lld of %lld bytes\n", ok ? "Success" : "Failed", bytes_parsed, file_size);
	p(store);
	exit(ok ? 0 : 1);
}

/*
 * Debugging functions
 */
void p(ADL::Handle h)
{
	printf("%s", h.name().asUTF8());
	if (h.super())
	{
		printf(" : %s", h.super().name().asUTF8());
		auto sp = h.super().parent();
		if (sp
		 && !sp.parent())
		{
			if (h.super().name() == "Assignment")
			{
				printf(" %s ", h.is_final() ? "=" : "~");
				auto v = h.value();
				if (v.handle)
					p(v.handle);
				else
					p(v.string);
			}
		}
	}
	Array<ADL::Handle>&	c = h.children();
	if (c.length() > 0)
	{
		printf("{\n");
		for (int i = 0; i < c.length(); i++)
			p(c[i]);
		printf("}\n");
	}
	else
		printf(";");
	printf("\n");
}

void p(ADL::MemStore m)
{
	p(m.top());
}
