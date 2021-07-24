#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		puts("Expected exactly 1 argument (URL)");
		exit(EXIT_FAILURE);
	}

	char *url = argv[1];
	puts(url);

	exit(EXIT_SUCCESS);
}
