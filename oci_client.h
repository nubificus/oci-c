#pragma once
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

	struct Memory {
		char *data;
		size_t size;
	};

// Initialize global library resources.
	void oci_client_init(void);
// Release global library resources.
	void oci_client_cleanup(void);

// Fetch an authentication token for a given registry/repo.
	char *fetch_token(const char *registry, const char *repo);

// Fetch the manifest JSON for an image by tag. Caller must free result.
	char *fetch_manifest(const char *registry, const char *repo,
		const char *tag, const char *arch, const char *os, char *token);

// Fetch a blob (layer) for a given digest. Caller must free Memory->data.
	struct Memory *fetch_blob(const char *registry, const char *repo,
		const char *digest, const char *token, int *out_code);

// Extract a .tar.gz layer file to a destination directory. Returns 0 on success, 1 on failure.
	int extract_tar_gz(const char *filename, const char *dest_dir);

	struct OciLayer {
		char *mediaType;
		char *digest;
		size_t size;
	};

// Parse the manifest and return an array of layers.
	int oci_manifest_parse_layers(const char *manifest_json,
		struct OciLayer **layers_out);
// Free memory for an array of layers.
	void oci_layers_free(struct OciLayer *layers, int n);

#ifdef __cplusplus
}
#endif
