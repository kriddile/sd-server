# sd-server
An Arduino library that creates a simple web server that serves the contents of an SD card over Wi-Fi and allows uploading files to the SD card via a web form.

This is a quick-and-dirty way to upload and download files to/from an SD card over Wi-Fi. It doesn't support multiple simultaneous connections. It doesn't support file names containing any of these characters (any such files will be ignored): `!*'();:@&=+$,/?#[] `. It doesn't support deleting files from the SD card, though adding that wouldn't be difficult. My initial use-case didn't require deleting files. It's only been tested with a Pi Pico W, though it will likely work with other Wi-Fi capable Arduino-compatible devices. See the provided example program for usage.
