// Also see:
// https://unix.stackexchange.com/questions/385023/firefox-reading-out-urls-of-opened-tabs-from-the-command-line

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <lz4.h>

#include <fileapi.h>
#include <synchapi.h>
#include <windows.h>

#define FLUSHPUTS(s) puts(s); fflush(stdout);

// Waiting behaviour.
// First wait INITIAL_DELAY milliseconds, then waits for file changes. If the
// URL is absent once, check again after CLOSED_DELAY milliseconds before
// terminating. MAX_INTERVAL is the time to check again anyway even if file
// changes not seen.
#define INITIAL_DELAY 10000
#define CLOSED_DELAY  5000
#define MAX_INTERVAL  60000

// Reads sessionstore file (as compressed) and returns a buffer containing its
// contents. The caller is responsible for free-ing the buffer. Since the
// buffer is not a null-terminated string the size read is output into the
// given int pointer.
char *read_sessionstore(char *, int *);

// Checks if tab is open with the given URL (must be exactly the same!) and
// path to sessionstore file.
bool check_tab_open(char *, char *);

int main(int argc, char **argv)
{
	int code = EXIT_FAILURE;
	
	FLUSHPUTS("steam-webgame-launcher v0.1.1.0");
	
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
	char *path = argv[3];
	
	// Open Firefox to the URL.
	FLUSHPUTS("Opening Firefox");
	char *command = malloc(strlen(url) + strlen(firefox) + 4);
	sprintf(command, "\"%s\" %s", firefox, url);
	system(command);
	free(command);
	
	// First we get the directory name of path to sessionstore file by null-
	// terminating at the last \ or /, for use with
	// FindFirstChangeNotificationA later on.
	char *path_dir = malloc(strlen(path));
	strcpy(path_dir, path);
	char *cp = &path_dir[strlen(path_dir)];
	while (*cp != '\\' && *cp != '/') cp--; *cp = '\0';

	// Wait until the URL is no longer present. See comments above on *_DELAY.
	FLUSHPUTS("Waiting for initial delay");
	Sleep(INITIAL_DELAY);
	while (true) {
		FLUSHPUTS("Checking for url");
		if (!check_tab_open(url, path)) { // Check a second time.
			FLUSHPUTS("Waiting for closed delay");
			Sleep(CLOSED_DELAY);
			FLUSHPUTS("Checking for url after closed delay");
			if (!check_tab_open(url, path)) break;
		}
		FLUSHPUTS("Waiting for file change");
		WaitForSingleObject(
				FindFirstChangeNotificationA(path_dir, FALSE,
					FILE_NOTIFY_CHANGE_LAST_WRITE),
				MAX_INTERVAL);
	};

	exit(EXIT_SUCCESS);
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

bool check_tab_open(char *url, char *path)
{
	// Whether we have succeeded in execution. If this is not set by the time
	// we reach the end, this means there has been a critical error and we
	// should exit() immediately.
	bool succeeded = false;

	// Open sessionstore file and read it.
	int file_size;
	char *file_buf = read_sessionstore(path, &file_size);
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
	free(file_buf); file_buf = NULL; // No longer needed.

	// Parse JSON data.
	cJSON *json = cJSON_Parse(dec_buf);
	if (!json) {
		fputs("Failed to parse decompressed JSON\n", stderr);
		goto err_cjson_parse;
	}

	// Go through list of tabs, checking if URL is present. All partial cJSON
	// functions we use handle NULLs correctly, so we do not check each return.
	// Errors are simply propagated.
	bool url_found = false;
	cJSON *windows = cJSON_GetObjectItemCaseSensitive(json, "windows");
	if (cJSON_IsArray(windows)) {
		for (int i = 0; !url_found && i < cJSON_GetArraySize(windows); i++) {
			cJSON *tabs = cJSON_GetObjectItemCaseSensitive(
					cJSON_GetArrayItem(windows, i), "tabs");

			for (int j = 0; !url_found && j < cJSON_GetArraySize(tabs); j++) {
				cJSON *tab = cJSON_GetArrayItem(tabs, j);
				cJSON *curr_entry = cJSON_GetObjectItemCaseSensitive(
						tab, "index");
				if (!cJSON_IsNumber(curr_entry)) continue;
				cJSON *entry = cJSON_GetArrayItem(
						cJSON_GetObjectItemCaseSensitive(tab, "entries"),
						(int) cJSON_GetNumberValue(curr_entry) - 1);
				char *tab_url = cJSON_GetStringValue(
						cJSON_GetObjectItemCaseSensitive(entry, "url"));

				// Found!
				if (tab_url) {
					FLUSHPUTS(tab_url);
					if (strncmp(tab_url, url, strlen(url)) == 0)
						url_found = true;
				}
			}
		}
	}
	
	succeeded = true;

	cJSON_Delete(json);
err_cjson_parse:
err_decompress:
	free(dec_buf);
err_dec_buf:
	free(file_buf);
err_file_buf:

	if (succeeded) return url_found;
	else exit(EXIT_FAILURE);
}
