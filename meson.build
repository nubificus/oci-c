# meson.build
project('oci_client_example', 'c',
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=c11']
)

# dependencies
curl_dep = dependency('libcurl')
cjson_dep = dependency('libcjson')
archive_dep = dependency('libarchive')

# get user choice
libtype = get_option('library_type')

# build library based on choice
if libtype == 'shared'
  oci_client_lib = shared_library('oci_client', 'oci_client.c', 
    dependencies: [curl_dep, cjson_dep, archive_dep])
elif libtype == 'static'
  oci_client_lib = static_library('oci_client', 'oci_client.c',
    dependencies: [curl_dep, cjson_dep, archive_dep])
else
  # fallback or error
  error('Invalid library_type option')
endif

# build executable
executable('oci_client_demo', 'main.c',
  link_with: oci_client_lib,
  dependencies: [curl_dep, cjson_dep, archive_dep]
)
test('oci_token', executable('test_token', 'test_token.c',
    link_with: oci_client_lib,
    dependencies: [curl_dep, cjson_dep, archive_dep]))

test('oci_client_thorough', executable('test_oci_client', 'test_oci_client.c',
    link_with: oci_client_lib,
    dependencies: [curl_dep, cjson_dep, archive_dep]))

