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
	exit(ok ? 0 : 1);
}
