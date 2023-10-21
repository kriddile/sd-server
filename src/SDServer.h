/* MIT License

Copyright (c) 2023 Kenny Riddile

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#ifndef SDSERVER_H
#define SDSERVER_H

#include <cstddef>

#include <SdFat.h>

#include "multipart_parser.h"

class WiFiServer;
class WiFiClient;

class SDServer {
public:
    void begin(
        WiFiServer* server,
        SdFs* fs,
        char* workingBuffer,
        size_t workingBufferSize,
        char* uploadStreamingBuffer,
        size_t uploadStreamingBufferSize
    );

    void handleClient();
private:
    static int readHeaderValue(multipart_parser* p, const char* at, size_t length);
    static int readPartData(multipart_parser* p, const char* at, size_t length);
    static int onHeadersComplete(multipart_parser* p);
    static int onPartDataEnd(multipart_parser* p);

    void listFiles(const char* directoryPath, FsFile& directory, WiFiClient& client);
    char* urlDecode(char* text);

    multipart_parser_settings _multipartParserCallbacks;
    WiFiServer* _server = nullptr;
    SdFs* _fs = nullptr;
    FsFile _fileBeingWritten;
    char* _workingBuffer;
    size_t _workingBufferSize;
    char* _uploadStreamingBuffer;
    size_t _uploadStreamingBufferSize;
    size_t _headerBufferPos;
};

#endif