# DropboxFS

A FUSE for interacting with Dropbox written in C.

## Dependencies

* [cJSON](https://github.com/DaveGamble/cJSON) (used as a local library)

* [libcurl](https://curl.se/libcurl/c/)

  You can install it by running `sudo apt-get install libcurl4-openssl-dev`

## Building

``gcc main.c -Wall -Wextra -g -o DropboxFS `pkg-config fuse --cflags --libs` -lcurl cJSON.c``

## Mounting

`./DropboxFS -f <MOUNT_POINT>`

## License

The project is published under the [MIT License](LICENSE.md).
