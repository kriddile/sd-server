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