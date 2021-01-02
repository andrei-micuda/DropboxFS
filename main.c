#define FUSE_USE_VERSION 34

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdbool.h>

#include "cJSON.h"
#define TOKEN "wtmYTvddvwUAAAAAAAAAARfWbCN2EaH96Qnc95ugEm1HeOuoWMG2faqedKvbgQpl"

struct MemoryStruct
{
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (ptr == NULL)
  {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

//* returns 0 for error, or the number of bytes received otherwise
unsigned long Get(char *url, struct MemoryStruct *rres)
{
  CURL *curl_handle;
  CURLcode res;

  struct MemoryStruct chunk;

  chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;           /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl_handle);

  unsigned long ret_val;
  /* check for errors */
  if (res != CURLE_OK)
  {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    ret_val = 0;
  }
  else
  {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */

    ret_val = (unsigned long)rres->size;
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();

  return ret_val;
}

unsigned long Post(char *url, char *data, struct MemoryStruct *rres, char* hpath)
{
  CURL *curl;
  CURLcode res;
  // struct MemoryStruct chunk;
  int ret_val;

  struct curl_slist *headers = NULL;
  char auth_token[1024];
  strcpy(auth_token, "Authorization: Bearer ");
  strcat(auth_token, TOKEN);
  headers = curl_slist_append(headers, auth_token);

  if(strcmp(hpath, "") != 0) {
    char header_path[1024];
    strcpy(header_path, "Dropbox-API-Arg: ");
    strcat(header_path, "{\"path\":\"");
    strcat(header_path, hpath);
    strcat(header_path, "\"}");

    printf("%s", header_path);

    headers = curl_slist_append(headers, "Content-Type:text/plain");
    headers = curl_slist_append(headers, header_path);
  }
  else {
    headers = curl_slist_append(headers, "Content-Type:application/json");
  }

  rres->memory = malloc(1); /* will be grown as needed by realloc above */
  rres->size = 0;           /* no data at this point */

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  curl = curl_easy_init();
  if (curl)
  {
    /* First set the URL that is about to receive our POST. This URL can
      just as well be a https:// URL if that is what should receive the
      data. */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    /* Set Content-Type to application/json */
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    /* Now specify the POST data */

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)rres);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);

    /* Check for errors */
    if (res != CURLE_OK)
    {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

      printf("Error occured when posting data");

      ret_val = 0;
    }
    else
    {
      /*
       * Now, our chunk.memory points to a memory block that is chunk.size
       * bytes big and contains the remote file.
       *
       * Do something nice with it!
       */

      // *rres = malloc(chunk.size * 1000);
      // memcpy(*rres, chunk.memory, chunk.size * 1000);
      ret_val = (unsigned long)rres->size;
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  else
  {
    printf("No curl handler");
  }

  curl_global_cleanup();
  return ret_val;
}

//* calls the get_metadata api endpoint on the specified path
//* https://dropbox.github.io/dropbox-api-v2-explorer/#files_get_metadata
//* not supported for the root
cJSON *GetMetadata(const char *path)
{
  struct MemoryStruct post_res;
  char args[1024];
  strcpy(args, "{\"path\":\"");
  strcat(args, path);
  strcat(args, "\"}");

  Post("https://api.dropboxapi.com/2/files/get_metadata", args, &post_res, "");

  //printf("%s", post_res.memory);

  cJSON *json = cJSON_Parse(post_res.memory);

  free(post_res.memory);

  return json;
}

//* calls the list_folder api endpoint on the specified path
//* https://dropbox.github.io/dropbox-api-v2-explorer/#files_list_folder
cJSON *ListFolder(const char *path)
{
  //* we construct the body of the request
  char args[1024];
  strcpy(args, "{\"path\":\"");
  if (strcmp(path, "/") != 0)
  {
    strcat(args, path);
  }
  strcat(args, "\"}");

  struct MemoryStruct post_res;
  Post("https://api.dropboxapi.com/2/files/list_folder", args, &post_res, "");

  cJSON *json = cJSON_Parse(post_res.memory);

  free(post_res.memory);

  return json;
}

//* checks if the object pointed at by path is a folder
//* calls GetMetadata and checks the .tag field
bool IsFolder(const char *path)
{
  if (strcmp(path, "/") == 0)
    return true;

  cJSON *json = GetMetadata(path);
  cJSON *fileType = cJSON_GetObjectItem(json, ".tag");

  if (fileType != NULL && strcmp(fileType->valuestring, "folder") == 0)
  {
    cJSON_Delete(json);
    return true;
  }
  cJSON_Delete(json);
  return false;
}

//* counts the number of subfolders located at the given path
//* number of hardlinks is nr_of_subfolders + 2 (. -> current dir and .. -> parent dir)
//* only call for folders (files have only one hardlink)
int GetNumberOfHardLinks(const char *path)
{
  //* any folder will have at least 2 hardlinks
  int n_hardlinks = 2;

  cJSON *json = ListFolder(path);
  cJSON *entries = cJSON_GetObjectItem(json, "entries");
  if (entries == NULL) //* couldn't parse json
  {
    printf("Couldn't retrieve 'entries' field from the JSON object");
    return -ENOENT;
  }
  else
  {
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
      cJSON *fileType = cJSON_GetObjectItem(entry, ".tag");

      if (fileType != NULL && strcmp(fileType->valuestring, "folder") == 0)
      {
        n_hardlinks++;
      }
    }

    cJSON_Delete(json);
    return n_hardlinks;
  }
}

static int do_getattr(const char *path, struct stat *st)
{
  printf("\n--> Getting The Attributes of %s\n", path);

  st->st_uid = getuid();
  st->st_gid = getgid();
  st->st_atime = time(NULL);
  st->st_mtime = time(NULL);

  //* the /get_metadata endpoint is not supported for the root folder
  if (strcmp(path, "/") == 0)
  {
    st->st_mode = S_IFDIR | 0755;
    st->st_nlink = GetNumberOfHardLinks("/");
  }
  else
  {
    //* calling /get_metadata for any other path

    cJSON *json = GetMetadata(path);
    cJSON *fileType = cJSON_GetObjectItem(json, ".tag");

    if (fileType != NULL && strcmp(fileType->valuestring, "file") == 0)
    {
      st->st_mode = S_IFREG | 0644;
      st->st_nlink = 1;

      cJSON *fileSize = cJSON_GetObjectItem(json, "size");
      st->st_size = fileSize->valueint;
    }
    else if (fileType != NULL && strcmp(fileType->valuestring, "folder") == 0)
    {
      st->st_mode = S_IFDIR | 0755;
      st->st_nlink = GetNumberOfHardLinks(path);
    }
    else
    {
      cJSON_Delete(json);
      return -ENOENT;
    }

    cJSON_Delete(json);
  }

  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  printf("\n--> Getting The List of Files of %s\n", path);

  //* check if the path is a folder
  //* if it is, call list_folder and use filler to add the folder entries into the buffer
  //* if it's a file return -ENOENT (No such file or directory)
  if (IsFolder(path))
  {
    filler(buffer, ".", NULL, 0);  //* Current Directory
    filler(buffer, "..", NULL, 0); //* Parent Directory

    cJSON *json = ListFolder(path);
    cJSON *entries = cJSON_GetObjectItem(json, "entries");

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
      cJSON *fileName = cJSON_GetObjectItem(entry, "name");
      filler(buffer, fileName->valuestring, NULL, 0);
    }

    cJSON_Delete(json);
  }
  else
  {
    return -ENOENT;
  }

  return 0;
}

static int do_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* fi)
{
  size_t len;

  struct MemoryStruct post_res;
  Post("https://content.dropboxapi.com/2/files/download", "", &post_res, path);
  //printf("%s", post_res.memory);

  len = strlen(post_res.memory);
  if(offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buffer, post_res.memory + offset, size);
  }
  else
    size = 0;
  return size;
}

static int do_mkdir(const char* path, mode_t mode)
{
  struct MemoryStruct post_res;

  char args[1024];
  strcpy(args, "{\"path\":\"");
  strcat(args, path);
  strcat(args, "\", \"autorename\": false}");

  printf("%s", args);

  Post("https://api.dropboxapi.com/2/files/create_folder_v2", args , &post_res, "");

  printf("%s", post_res.memory);

  return 0;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .read    = do_read,
    .mkdir   = do_mkdir};

int main(int argc, char *argv[])
{
  return fuse_main(argc, argv, &operations, NULL);
}
