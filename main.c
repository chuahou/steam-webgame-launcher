// Also see:
// https://unix.stackexchange.com/questions/385023/firefox-reading-out-urls-of-opened-tabs-from-the-command-line

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lz4.h>

// Reads sessionstore file (as compressed) and returns a buffer containing its
// contents. The caller is responsible for free-ing the buffer. Since the
// buffer is not a null-terminated string the size read is output into the
// given int pointer.
char *read_sessionstore(char *, int *);

int main(int argc, char **argv)
{
	int code = EXIT_FAILURE;
	
	// Check we got exactly 3 arguments---
	// argv[1]: the URL of the webgame.
	// argv[2]: path to Firefox
	// argv[3]: path to sessionstore file
	if (argc != 4) {
		fputs("Expected exactly 3 arguments\n", stderr);
		exit(EXIT_FAILURE);
	}
	char *url = argv[1];
	char *firefox = argv[2];
	char *file_path = argv[3];
	
	// Open Firefox to the URL.
	char *command = malloc(strlen(url) + strlen(firefox) + 2);
	sprintf(command, "%s %s", firefox, url);
	system(command);
	free(command);
	
	// Open sessionstore file and read it.
	int file_size;
	char *file_buf = read_sessionstore(file_path, &file_size);
	if (!file_buf) {
		fputs("Could not read session store\n", stderr);
		goto err_file_buf;
	}
	
	// Decompress sessionstore file, skipping first 12 bytes thanks to
	// Mozilla's magic number format. See https://github.com/lz4/lz4/issues/880
	// First 8 bytes are "mozLz40", then 4 bytes of decompressed size.
	// Also see https://github.com/jusw85/mozlz4.
	uint32_t dec_size;
	memcpy(&dec_size, file_buf + 8, sizeof(dec_size));
	printf("%d\n", dec_size);
	char *dec_buf = malloc(dec_size);
	if (!dec_buf) {
		fputs("Failed to allocate decompress buffer\n", stderr);
		goto err_dec_buf;
	}
	dec_size = LZ4_decompress_safe(file_buf + 12, dec_buf,
			file_size - 12, dec_size);
	if (dec_size < 0) {
		fputs("Failed to decompress session store\n", stderr);
		goto err_decompress;
	}
	printf("%d\n", dec_size);
	
	code = EXIT_SUCCESS;

err_decompress:
	free(dec_buf);
err_dec_buf:
	free(file_buf);
err_file_buf:

	exit(code);
}

char *read_sessionstore(char *path, int *size)
{
	// Compute filename to read.
	FILE *file = fopen(path, "rb");
	if (!file) return NULL;
	
	// Check length of file and allocate an appropriately-sized buffer.
	fseek(file, 0L, SEEK_END);
	*size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	char *file_buf = malloc(*size);
	
	// Read and update file size.
	*size = fread((void *) file_buf, 1, *size, file);

	fclose(file);
	return file_buf;
}
