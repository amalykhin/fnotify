#include <stdio.h>
#include <stdlib.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xpath.h>

int main (int argc, char *argv[])
{
	htmlDocPtr doc;

	if (--argc != 1) {
		printf("Usage: 4notify_libxml LINK\n");
		return EXIT_FAILURE;
	}

	doc = htmlReadFile(argv[1], "utf-8", HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);


	return doc ? EXIT_SUCCESS : EXIT_FAILURE;
}
