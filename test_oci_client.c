// test_oci_client.c
#include "oci_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_fetch_token_invalid() {
    char *token = fetch_token("https://invalid.registry", "invalid_repo");
    assert(token == NULL);
    printf("[PASS] fetch_token returns NULL for invalid registry/repo\n");
}

static void test_fetch_token_valid() {
    // Replace with real registry/repo if available for real testing
    // For this example, we skip or simulate success
    printf("[SKIP] test_fetch_token_valid requires real registry: skipping\n");
}

static void test_fetch_manifest_invalid() {
    char *manifest = fetch_manifest("https://invalid.registry", "repo", "tag", "arch", "os", NULL);
    assert(manifest == NULL);
    printf("[PASS] fetch_manifest returns NULL for invalid URL\n");
}

static void test_manifest_parsing() {
    const char *sample_manifest = "{\
        \"layers\": [\
            {\"mediaType\":\"application/vnd.oci.image.layer.v1.tar+gzip\", \"digest\":\"sha256:123\", \"size\":1234},\
            {\"mediaType\":\"application/vnd.oci.image.layer.v1.tar+gzip\", \"digest\":\"sha256:456\", \"size\":5678}\
        ]\
    }";

    struct OciLayer *layers = NULL;
    int n = oci_manifest_parse_layers(sample_manifest, &layers);
    assert(n == 2);
    assert(layers != NULL);
    assert(strcmp(layers[0].digest, "sha256:123") == 0);
    assert(layers[0].size == 1234);
    assert(strcmp(layers[1].digest, "sha256:456") == 0);
    assert(layers[1].size == 5678);

    oci_layers_free(layers, n);
    printf("[PASS] manifest parsing sample data\n");
}

static void test_extract_tar_gz_invalid() {
    int ret = extract_tar_gz("/non/existent/file.tar.gz", "output");
    assert(ret != 0);
    printf("[PASS] extract_tar_gz returns error on non-existent file\n");
}

int main() {
    oci_client_init();

    test_fetch_token_invalid();
    test_fetch_token_valid();
    test_fetch_manifest_invalid();
    test_manifest_parsing();
    test_extract_tar_gz_invalid();

    oci_client_cleanup();
    printf("All tests passed (or skipped as noted).\n");
    return 0;
}

