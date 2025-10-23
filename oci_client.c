#define _POSIX_C_SOURCE 200809L
#define GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>

#include "oci_client.h"

static int debug_mode = 0;

#define DBG(fmt, ...) \
    do { if(debug_mode) fprintf(stderr, "[OCI-DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

static size_t write_callback(void *contents, size_t size, size_t nmemb,
	void *userp)
{
	size_t realsize = size * nmemb;
	struct Memory *mem = (struct Memory *)userp;
	char *ptr = realloc(mem->data, mem->size + realsize + 1);
	if (!ptr)
		return 0;
	mem->data = ptr;
	memcpy(&(mem->data[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->data[mem->size] = 0;	// null terminate
	return realsize;
}

static struct Memory *curl_get_memory(const char *url,
	struct curl_slist *headers, long *response_code)
{
	struct Memory *mem = malloc(sizeof(struct Memory));
	mem->data = NULL;
	mem->size = 0;
	CURL *curl = curl_easy_init();
	if (!curl)
		return mem;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, mem);
	if (headers)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
		free(mem->data);
		mem->data = NULL;
		mem->size = 0;
	}
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, response_code);
	curl_easy_cleanup(curl);

	return mem;
}

char *fetch_token(const char *registry, const char *repo)
{
	if (!registry || !repo)
		return NULL;

	size_t url_len = strlen(registry) + strlen(repo) + 512;
	char *url = malloc(url_len);
	if (!url)
		return NULL;

	int n =
		snprintf(url, url_len,
		"%s/service/token?service=harbor-registry&scope=repository:%s:pull",
		registry, repo);
	if (n < 0 || (size_t)n >= url_len)
	{
		free(url);
		return NULL;
	}

	DBG("Fetching token from %s", url);

	long code = 0;
	struct Memory *token_json = curl_get_memory(url, NULL, &code);
	free(url);
	DBG("Got %ld", code);
	if (code != 200)
	{
		free(token_json->data);
		free(token_json);
		return NULL;
	}

	cJSON *j = cJSON_Parse(token_json->data);
	if (!j)
	{
		free(token_json->data);
		free(token_json);
		return NULL;
	}

	cJSON *jtoken = cJSON_GetObjectItem(j, "token");
	char *ret = NULL;
	if (jtoken && cJSON_IsString(jtoken))
	{
		ret = strdup(jtoken->valuestring);
	}

	cJSON_Delete(j);
	free(token_json->data);
	free(token_json);

	DBG("Token %s", ret);
	return ret;
}

char *fetch_manifest(const char *registry, const char *repo, const char *tag,
	const char *arch, const char *os, char *token)
{

	char *manifest = NULL;
	long code = 0;

	struct curl_slist *headers = NULL;
	headers =
		curl_slist_append(headers,
		"Accept: application/vnd.oci.image.manifest.v1+json");
	headers =
		curl_slist_append(headers,
		"Accept: application/vnd.oci.image.index.v1+json");

	if (token && token[0] != '\0')
	{
		char auth[1500];
		snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
		headers = curl_slist_append(headers, auth);
	}

	char url[1024];
	snprintf(url, sizeof(url), "%s/v2/%s/manifests/%s", registry, repo, tag);
	DBG("Fetching manifest URL: %s", url);

	struct Memory *m = curl_get_memory(url, headers, &code);
	DBG("%s:%d Token: %p, data: %d", __func__, __LINE__, token, (int)m->size);
	curl_slist_free_all(headers);

	if (!m->data)
	{
		free(m);
		return NULL;
	}

	manifest = m->data;
	free(m);

	if (!manifest)
		return NULL;

	cJSON *j = cJSON_Parse(manifest);
	if (!j)
		return manifest;

	cJSON *mediaType = cJSON_GetObjectItem(j, "mediaType");
	if (mediaType &&
		strcmp(mediaType->valuestring,
			"application/vnd.oci.image.index.v1+json") == 0)
	{
		const char *selected_digest = NULL;
		cJSON *manifests = cJSON_GetObjectItem(j, "manifests");
		if (manifests && cJSON_IsArray(manifests))
		{
			for (int i = 0; i < cJSON_GetArraySize(manifests); i++)
			{
				cJSON *mitem = cJSON_GetArrayItem(manifests, i);
				cJSON *platform = cJSON_GetObjectItem(mitem, "platform");
				if (!platform)
					continue;

				cJSON *a = cJSON_GetObjectItem(platform, "architecture");
				cJSON *o = cJSON_GetObjectItem(platform, "os");
				if (a && o && strcmp(a->valuestring, arch) == 0 &&
					strcmp(o->valuestring, os) == 0)
				{
					cJSON *d = cJSON_GetObjectItem(mitem, "digest");
					if (d && cJSON_IsString(d))
					{
						selected_digest = strdup(d->valuestring);
						break;
					}
				}
			}

			if (!selected_digest)
			{
				cJSON *first = cJSON_GetArrayItem(manifests, 0);
				if (first)
				{
					cJSON *d = cJSON_GetObjectItem(first, "digest");
					if (d && cJSON_IsString(d))
						selected_digest = strdup(d->valuestring);
				}
			}
		}

		DBG("Selected digest: %s", selected_digest ? selected_digest : "NULL");

		if (selected_digest)
		{
			char new_url[2048];
			snprintf(new_url, sizeof(new_url), "%s/v2/%s/manifests/%s",
				registry, repo, selected_digest);

			free(manifest);

			struct curl_slist *headers = NULL;
			if (token && token[0] != '\0')
			{
				char auth[2048];
				snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
				headers = curl_slist_append(headers, auth);
			}

			struct Memory *mm = curl_get_memory(new_url, headers, &code);
			curl_slist_free_all(headers);

			manifest = mm->data;
			free(mm);
			free((void *)selected_digest);
		}
	}

	cJSON_Delete(j);
	return manifest;
}

struct Memory *fetch_blob(const char *registry, const char *repo,
	const char *digest, const char *token, int *out_code)
{
	DBG("%s:%d Token pointer: %p", __func__, __LINE__, token);
	if (!digest)
		return NULL;

	char url[1024];
	snprintf(url, sizeof(url), "%s/v2/%s/blobs/%s", registry, repo, digest);

	struct curl_slist *headers = NULL;

	if (token && token[0] != '\0')
	{
		char auth_header[2048];
		snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
			token);
		headers = curl_slist_append(headers, auth_header);
	}

	long code = 0;
	struct Memory *mem = curl_get_memory(url, headers, &code);
	DBG("HTTP response code pointer: %p, %ld", (void *)out_code, code);

	curl_slist_free_all(headers);

	if (out_code)
		*out_code = (int)code;

	return mem;
}

int extract_tar_gz(const char *filename, const char *dest_dir)
{
	setlocale(LC_ALL, "en_US.UTF-8");
	struct archive *a = archive_read_new();
	archive_read_support_format_tar(a);
	archive_read_support_filter_gzip(a);

	struct archive *ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME |
		ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);

	if (archive_read_open_filename(a, filename, 10240))
	{
		fprintf(stderr, "Failed to open %s: %s\n", filename,
			archive_error_string(a));
		archive_read_free(a);
		archive_write_free(ext);
		return 1;
	}

	struct archive_entry *entry;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
	{
		const char *pathname = archive_entry_pathname(entry);
		char fullpath[PATH_MAX];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", dest_dir, pathname);
		archive_entry_set_pathname(entry, fullpath);

		int r = archive_write_header(ext, entry);
		if (r >= ARCHIVE_OK && archive_entry_size(entry) > 0)
		{
			const void *buff;
			size_t size;
			la_int64_t offset;
			while (archive_read_data_block(a, &buff, &size,
					&offset) == ARCHIVE_OK)
				archive_write_data_block(ext, buff, size, offset);
		}
		archive_write_finish_entry(ext);
	}

	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);
	return 0;
}

int oci_manifest_parse_layers(const char *manifest_json,
	struct OciLayer **layers_out)
{
	*layers_out = NULL;
	int count = 0;
	cJSON *root = cJSON_Parse(manifest_json);
	if (!root)
		return 0;

	cJSON *layers = cJSON_GetObjectItem(root, "layers");
	if (!layers || !cJSON_IsArray(layers))
	{
		cJSON_Delete(root);
		return 0;
	}

	int nr_layers = cJSON_GetArraySize(layers);
	if (nr_layers <= 0)
	{
		cJSON_Delete(root);
		return 0;
	}
	*layers_out = calloc((size_t)nr_layers, sizeof(struct OciLayer));
	for (int i = 0; i < nr_layers; i++)
	{
		cJSON *item = cJSON_GetArrayItem(layers, i);
		cJSON *mediaType = cJSON_GetObjectItem(item, "mediaType");
		cJSON *digest = cJSON_GetObjectItem(item, "digest");
		cJSON *sizeItem = cJSON_GetObjectItem(item, "size");

		(*layers_out)[i].mediaType = mediaType &&
			cJSON_IsString(mediaType) ? strdup(mediaType->valuestring) : NULL;
		(*layers_out)[i].digest = digest &&
			cJSON_IsString(digest) ? strdup(digest->valuestring) : NULL;
		(*layers_out)[i].size = sizeItem &&
			cJSON_IsNumber(sizeItem) ? (size_t)sizeItem->valuedouble : 0;

		count++;
	}
	cJSON_Delete(root);
	return count;
}

void oci_layers_free(struct OciLayer *layers, int n)
{
	if (!layers)
		return;
	for (int i = 0; i < n; i++)
	{
		free(layers[i].mediaType);
		free(layers[i].digest);
	}
	free(layers);
}

void oci_client_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
}

void oci_client_cleanup(void)
{
	curl_global_cleanup();
}
