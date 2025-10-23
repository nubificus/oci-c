# OCI Client Library

A C library and example program for fetching OCI container images (manifests,
blobs) from OCI-compliant registries. Supports token authentication, manifest
parsing, blob downloading, and extracting tar.gz layers.

## Table of Contents

[About the Project](#About-the-project)

[Features](#Features)

[Prerequisites](#Prerequisites)

[Building](#Building)

[Usage](#Usage)

[API](#API)

[Contributing](#Contributing)

[License](#License)

## About the Project

This project provides a simple C OCI client library exposing a clean API to:

- Fetch authentication tokens from OCI registries
- Get manifests by image tag and select platform-specific manifests
- Download layer blobs by digest
- Extract downloaded .tar.gz layers to directories

The library hides dependencies on `libcurl`, `cJSON`, and `libarchive` so that
applications only need to link against this library without dealing with
external dependencies directly.

The included example program demonstrates how to fetch and extract all layers
of an OCI image using the library.

## Features

- Full OCI registry API support for image pulls
- Token authentication support
- Multi-architecture manifest parsing
- Blob fetching and tar.gz extraction
- Simple, dependency-encapsulating API
- Shared or static library build via Meson
- Graceful error handling and resource cleanup

## Prerequisites

- C compiler supporting C11 (e.g., GCC 7+, Clang)
- Meson build system
- Ninja build tool

Dependencies installed on system:

- libcurl (development files)
- libcjson (development files)
- libarchive (development files)

On Debian/Ubuntu, install dependencies with:

```bash
sudo apt install build-essential meson ninja-build libcurl4-openssl-dev libcjson-dev libarchive-dev
```

## Building
Configure and build the project using Meson:

```bash
meson setup builddir -Dlibrary_type=shared   # or static
meson compile -C builddir
```

This builds the `oci_client` library (shared or static) and the example executable `oci_client_demo`.

## Usage
Run the example program to fetch and extract an image:

```bash
./builddir/oci_client_demo -r https://harbor.nbfc.io -R nubificus/torchscript-v2-vaccel-gpu -t x86_64 -a amd64 -o linux -d output
```

The program downloads all layers, saves them as `layer-*.tar.gz`, and extracts them under the `output` directory.

## API
Main functions:

- `void oci_client_init(void)`
- `void oci_client_cleanup(void)`
- `char *fetch_token(const char *registry, const char *repo)`
- `char *fetch_manifest(const char *registry, const char *repo, const char *tag, const char *arch, const char *os, char *token)`
- `struct Memory *fetch_blob(const char *registry, const char *repo, const char *digest, const char *token, int *out_code)`
- `int extract_tar_gz(const char *filename, const char *dest_dir)`
- `int oci_manifest_parse_layers(const char *manifest_json, struct OciLayer **layers_out)`
- `void oci_layers_free(struct OciLayer *layers, int n)`

See `oci_client.h` for full declarations.

## Contributing
Contributions welcome! Please fork the repo, make changes, and submit pull
requests. Report bugs or suggest features via issues.

## License
This project is licensed under the Apache-2.0 License. See LICENSE for details.
