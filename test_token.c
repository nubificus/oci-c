#include "oci_client.h"
#include <stdio.h>
#include <string.h>

int main() {
    // Initialize library
    oci_client_init();

    // Test fetch_token with invalid data (should return NULL)
    char *token = fetch_token("invalid_registry", "invalid_repo");
    if (token != NULL) {
        printf("Unexpected token: %s\n", token);
        free(token);
    } else {
        printf("fetch_token correctly returned NULL on invalid input.\n");
    }

    // Normally, you'd test with real registry info
    // For demonstration, assume test won't reach here

    // Cleanup
    oci_client_cleanup();
    return 0;
}
