#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "cJSON.h"

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
unsigned long Get(char *url, char **rres)
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

    *rres = chunk.memory;

    ret_val = (unsigned long)chunk.size;
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();

  return ret_val;
}

unsigned long Post(char *url, char *data, char **rres)
{
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  int ret_val;

  printf("BEFORE POST:\nUrl: %s\nData: %s\n", url, data);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: Bearer sl.AoJgD1y7BBtZx-OdzPuOkJNdjcwG1R0fYTo-aCK3zyHQPxRqj2PzMUq4JVshBGSqLBGx50KTZMfazLZZpCGX5INRA56_uUOdk8eAaiwnt8Xc9lsAhKiyB14gSF7gymggCjxxP7od");
  headers = curl_slist_append(headers, "Content-Type:application/json");

  chunk.memory = malloc(1); /* will be grown as needed by realloc above */
  chunk.size = 0;           /* no data at this point */

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
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

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

      printf("\n\n-----------------\n%s\n------------------\n\n", chunk.memory);

      *rres = malloc(chunk.size * 1000);
      memcpy(*rres, chunk.memory, chunk.size * 1000);
      ret_val = (unsigned long)chunk.size;
      printf("\n\n--------------------\nSIZE%d\n--------------------\n\n", ret_val);
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();
  return ret_val;
}

static int do_getattr(const char *path, struct stat *st)
{
  printf("getting attributes of %s\n", path);

  st->st_uid = getuid();
  st->st_gid = getgid();
  st->st_atime = time(NULL);
  st->st_mtime = time(NULL);

  //* the /get_metadata endpoint is not supported for the root folder
  if (strcmp(path, "/") == 0)
  {
    printf("IN ROOT\n");
    int n_hardlinks = 2;
    char *post_res = NULL;
    Post("https://api.dropboxapi.com/2/files/list_folder", "{\"path\":\"\"}", &post_res);

    printf("After POST\n");
    printf("%s", post_res);
    cJSON *json = cJSON_Parse(post_res);
    cJSON *entries = cJSON_GetObjectItem(json, "entries");

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
      cJSON *fileType = cJSON_GetObjectItem(entry, ".tag");

      if (strcmp(fileType->valuestring, "folder") == 0)
      {
        n_hardlinks++;
      }
    }

    free(post_res);

    st->st_mode = S_IFDIR | 0755;
    st->st_nlink = n_hardlinks;
  }
  else
  {
    //* calling /get_metadata for any other path
    char *post_res = malloc(1000000);
    char args[1024];
    strcpy(args, "{\"path\":\"");
    strcat(args, path);
    strcat(args, "\",\"include_media_info\":true}");

    Post("https://api.dropboxapi.com/2/files/get_metadata", args, &post_res);

    cJSON *json = cJSON_Parse(post_res);
    cJSON *fileType = cJSON_GetObjectItem(json, ".tag");
    if (strcmp(fileType->valuestring, "file") == 0)
    {
      st->st_mode = S_IFREG | 0644;
      st->st_nlink = 1;

      cJSON *fileSize = cJSON_GetObjectItem(json, "size");
      st->st_size = fileSize->valueint;
    }
    else if (strcmp(fileType->valuestring, "folder") == 0)
    {
      int n_hardlinks = 2;
      char *post_res_2 = malloc(1000000);
      strcpy(args, "{\"path\":\"");
      strcat(args, path);
      strcat(args, "\"}");
      Post("https://api.dropboxapi.com/2/files/list_folder", args, &post_res_2);

      cJSON *json = cJSON_Parse(post_res_2);
      cJSON *entries = cJSON_GetObjectItem(json, "entries");

      cJSON *entry = NULL;
      cJSON_ArrayForEach(entry, entries)
      {
        cJSON *fileType = cJSON_GetObjectItem(entry, ".tag");

        if (strcmp(fileType->valuestring, "folder"))
        {
          n_hardlinks++;
        }
      }

      free(post_res_2);

      st->st_mode = S_IFDIR | 0755;
      st->st_nlink = n_hardlinks;
    }

    free(post_res);
  }

  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  printf("--> Getting The List of Files of %s\n", path);

  filler(buffer, ".", NULL, 0);  // Current Directory
  filler(buffer, "..", NULL, 0); // Parent Directory

  if (strcmp(path, "/") == 0) // If the user is trying to show the files/directories of the root directory show the following
  {
    char *post_res = malloc(1000000);
    Post("https://api.dropboxapi.com/2/files/list_folder", "{\"path\":\"\"}", &post_res);

    cJSON *json = cJSON_Parse(post_res);
    cJSON *entries = cJSON_GetObjectItem(json, "entries");

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
      cJSON *fileName = cJSON_GetObjectItem(entry, "name");
      filler(buffer, fileName->valuestring, NULL, 0);
    }
  }

  return 0;
}

// int main()
// {
//   // char *post_res = malloc(1000000);
//   // Post("https://api.dropboxapi.com/2/files/list_folder", "{\"path\":\"\"}", &post_res);

//   // cJSON *json = cJSON_Parse(post_res);
//   // printf("%s", cJSON_Print(json));

//   char *post_res = malloc(1000000);
//   Post("https://api.dropboxapi.com/2/files/list_folder", "{\"path\":\"\"}", &post_res);

//   cJSON *json = cJSON_Parse(post_res);
//   cJSON *entries = cJSON_GetObjectItem(json, "entries");

//   cJSON *entry = NULL;
//   cJSON_ArrayForEach(entry, entries)
//   {
//     cJSON *fileName = cJSON_GetObjectItem(entry, "name");
//     printf("%s\n", fileName->valuestring);
//   }

//   free(post_res);

//   return 0;
// }

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir};

int main(int argc, char *argv[])
{
  return fuse_main(argc, argv, &operations, NULL);
}