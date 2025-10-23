#define _POSIX_C_SOURCE 200809L
#define GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "oci_client.h"

int main(int argc, char **argv)
{
	oci_client_init();

	// Default options
	const char *registry = "https://harbor.nbfc.io";
	const char *repo = "nubificus/torchscript-v2-vaccel-gpu";
	const char *tag = "x86_64";
	const char *arch = "amd64";
	const char *os = "linux";
	const char *dest_dir = "output";

	int opt;
	while ((opt = getopt(argc, argv, "r:R:t:a:o:d:h")) != -1)
	{
		switch (opt)
		{
		case 'r':
			registry = optarg;
			break;
		case 'R':
			repo = optarg;
			break;
		case 't':
			tag = optarg;
			break;
		case 'a':
			arch = optarg;
			break;
		case 'o':
			os = optarg;
			break;
		case 'd':
			dest_dir = optarg;
			break;
		case 'h':
		default:
			fprintf(stderr,
				"Usage: %s [-r registry] [-R repo] [-t tag] [-a arch] [-o os] [-d dest]\n\n"
				"Options:\n"
				"  -r <registry>   OCI registry URL (default: %s)\n"
				"  -R <repo>       Repository name (default: %s)\n"
				"  -t <tag>        Image tag (default: %s)\n"
				"  -a <arch>       Architecture (default: %s)\n"
				"  -o <os>         OS (default: %s)\n"
				"  -d <dest>       Output directory (default: %s)\n",
				argv[0], registry, repo, tag, arch, os, dest_dir);
			exit(opt == 'h' ? 0 : 1);
		}
	}

	fprintf(stderr,
		"Using:\n"
		"  Registry: %s\n"
		"  Repo:     %s\n"
		"  Tag:      %s\n"
		"  Arch:     %s\n"
		"  OS:       %s\n"
		"  Out Dir:  %s\n\n", registry, repo, tag, arch, os, dest_dir);

	char *token = fetch_token(registry, repo);
	if (!token)
	{
		fprintf(stderr, "Failed to fetch authentication token\n");
		oci_client_cleanup();
		return 1;
	}

	char *manifest = fetch_manifest(registry, repo, tag, arch, os, token);
	if (!manifest)
	{
		fprintf(stderr, "Failed to fetch manifest\n");
		free(token);
		oci_client_cleanup();
		return 1;
	}
	printf("Manifest fetched successfully.\n");

	struct OciLayer *layers = NULL;
	int nr_layers = oci_manifest_parse_layers(manifest, &layers);
	if (nr_layers == 0 || !layers)
	{
		fprintf(stderr, "No layers found in the manifest\n");
		free(token);
		free(manifest);
		oci_client_cleanup();
		return 1;
	}

	for (int i = 0; i < nr_layers; i++)
	{
		fprintf(stderr,
			"Layer %d: digest=%s, size=%zu, mediaType=%s\n",
			i, layers[i].digest, layers[i].size,
			layers[i].mediaType ? layers[i].mediaType : "(none)");

		int code = 0;
		struct Memory *blob =
			fetch_blob(registry, repo, layers[i].digest, token, &code);
		if (!blob || !blob->data || code != 200)
		{
			fprintf(stderr, "Failed to fetch blob %s (HTTP %d)\n",
				layers[i].digest, code);
			if (blob)
			{
				free(blob->data);
				free(blob);
			}
			continue;
		}

		char filename[256];
		snprintf(filename, sizeof(filename), "layer-%d.tar.gz", i);
		FILE *f = fopen(filename, "wb");
		if (f)
		{
			fwrite(blob->data, 1, blob->size, f);
			fclose(f);
		} else
		{
			fprintf(stderr, "Failed to write %s\n", filename);
			free(blob->data);
			free(blob);
			continue;
		}
		free(blob->data);
		free(blob);

		printf("Extracting %s...\n", filename);
		if (extract_tar_gz(filename, dest_dir) != 0)
		{
			fprintf(stderr, "Extraction failed for %s\n", filename);
		}
	}

	oci_layers_free(layers, nr_layers);
	free(token);
	free(manifest);
	oci_client_cleanup();
	printf("Extraction complete.\n");
	return 0;
}
