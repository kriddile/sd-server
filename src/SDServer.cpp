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

#include "SDServer.h"

#include <initializer_list>
#include <string_view>

#include <WiFi.h>

static const char HTTP_200_OK[] = "HTTP/1.1 200 OK";
static const char HTTP_303_REDIRECT[] = "HTTP/1.1 303 See Other\r\nLocation: /";
static const char HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not Found";
static const char HTTP_CONTENT_TYPE[] = "Content-Type: ";
static const char HTTP_CONTENT_LENGTH[] = "Content-Length: ";
static const char HTTP_TRANSFER_ENCODING_CHUNKED[] = "Transfer-Encoding: chunked";
static const char HTTP_CONNECTION_CLOSE[] = "Connection: close";

/*void printStringView(std::string_view stringView, const char* prefix = "") {
    Serial.printf("%s%.*s", prefix, stringView.length(), stringView.begin());
}*/

void clientPrint(const char* string, WiFiClient& client) {
#ifdef SDSERVER_DEBUG
    Serial.print(string);
#endif
    client.print(string);
}

void clientPrint(size_t number, int base, WiFiClient& client) {
#ifdef SDSERVER_DEBUG
    Serial.print(number, base);
#endif
    client.print(number, base);
}

void clientPrintln(const char* string, WiFiClient& client) {
#ifdef SDSERVER_DEBUG
    Serial.println(string);
#endif
    client.print(string);
    client.print("\r\n");
}

void sendHTMLResponse(const char* responseStatusLine, WiFiClient& client) {
    clientPrintln(responseStatusLine, client);
    clientPrint(HTTP_CONTENT_TYPE, client);
    clientPrintln("text/html", client);
    clientPrint(HTTP_CONTENT_LENGTH, client);
    clientPrintln("0", client);
    clientPrintln(HTTP_CONNECTION_CLOSE, client);
    clientPrintln("", client);
}

const char* combinedStrLenAsHex(std::initializer_list<const char*> args) {
    static char buffer[5]; // max decimal value of 65535 including null terminator

    size_t length = 0;
    for (auto arg : args) {
        length += strlen(arg);
    }
    sprintf(buffer, "%x", length);

    return buffer;
}

bool requiresURLEncoding(const char* str) {
    const char* reservedCharacters = "!*'();:@&=+$,/?#[] ";
    
    while (*str) {
        if (strchr(reservedCharacters, *str)) {
            return true;
        }
        ++str;
    }

    return false;
}

void readToBoundary(WiFiClient& client) {
    const char* needle = "boundary=";

    size_t index = 0;
    while (client.connected()) {
        char c = client.read();
        if ((c == needle[0] && index == 0) ||
            (c == needle[1] && index == 1) ||
            (c == needle[2] && index == 2) ||
            (c == needle[3] && index == 3) ||
            (c == needle[4] && index == 4) ||
            (c == needle[5] && index == 5) ||
            (c == needle[6] && index == 6) ||
            (c == needle[7] && index == 7)) {
            ++index;
        } else if (c == needle[8] && index == 8) {
            break;
        } else {
            index = 0;
        }
    }
}

// Read until \r\n\r\n
void readToBody(WiFiClient& client) {
    size_t index = 0;
    while (client.connected()) {
        char c = client.read();
        if (c == '\r' && index == 0) {
            index = 1;
        } else if (c == '\n' && index == 1) {
            index = 2;
        } else if (c == '\r' && index == 2) {
            index = 3;
        } else if (c == '\n' && index == 3) {
            break;
        } else {
            index = 0;
        }
    }
}

int SDServer::readHeaderValue(multipart_parser* p, const char* at, size_t length) {
    SDServer* self = static_cast<SDServer*>(multipart_parser_get_data(p));
    if (self->_headerBufferPos + length > self->_workingBufferSize) {
        length = self->_workingBufferSize - self->_headerBufferPos;
    }
    std::copy(at, at + length, self->_workingBuffer + self->_headerBufferPos);
    self->_headerBufferPos += length;

    return 0;
}

int SDServer::readPartData(multipart_parser* p, const char* at, size_t length) {
    SDServer* self = static_cast<SDServer*>(multipart_parser_get_data(p));
    if (self->_fileBeingWritten.isOpen()) {
        self->_fileBeingWritten.write(at, length);
        self->_fileBeingWritten.sync();
    }

    return 0;
}

int SDServer::onHeadersComplete(multipart_parser* p) {
    SDServer* self = static_cast<SDServer*>(multipart_parser_get_data(p));
    std::string_view headerValues(self->_workingBuffer, self->_headerBufferPos);
    const char* needle = "filename=\"";
    auto fileNameBegin = headerValues.find(needle) + 10; // 10 == strlen(needle)
    if (fileNameBegin != headerValues.npos) {
        auto fileNameEnd = headerValues.find('"', fileNameBegin);
        self->_workingBuffer[fileNameEnd] = '\0';
        strcat(self->_workingBuffer, self->_workingBuffer + fileNameBegin); // prepend directory path stored at the front of self->_workingBuffer
        self->_fileBeingWritten = self->_fs->open(self->_workingBuffer, FILE_WRITE);
        self->_fileBeingWritten.truncate(0); // overwrite any existing file with the same name
    }

    return 0;
}

int SDServer::onPartDataEnd(multipart_parser* p) {
    SDServer* self = static_cast<SDServer*>(multipart_parser_get_data(p));
    if (self->_fileBeingWritten.isOpen()) {
        self->_fileBeingWritten.close();
    }

    return 0;
}

void SDServer::begin(
    WiFiServer* server,
    SdFs* fs,
    char* workingBuffer,
    size_t workingBufferSize,
    char* uploadStreamingBuffer,
    size_t uploadStreamingBufferSize
) {
    memset(&_multipartParserCallbacks, 0, sizeof(multipart_parser_settings));
    _multipartParserCallbacks.on_header_value = readHeaderValue;
    _multipartParserCallbacks.on_part_data = readPartData;
    _multipartParserCallbacks.on_headers_complete = onHeadersComplete;
    _multipartParserCallbacks.on_part_data_end = onPartDataEnd;

    _server = server;
    _fs = fs;
    _workingBuffer = workingBuffer;
    _workingBufferSize = workingBufferSize;
    _uploadStreamingBuffer = uploadStreamingBuffer;
    _uploadStreamingBufferSize = uploadStreamingBufferSize;
}

void SDServer::handleClient() {
    if (!_server) return;

    WiFiClient client = _server->available();
    if (client) {
        size_t index = 0;
        while (client.connected()) {
            if (client.available()) { // data is available to read from client
                char c = client.read();
                if (c != '\n' && c != '\r') {
                    _workingBuffer[index++] = c;
                    if (index > _workingBufferSize) {
                        index = _workingBufferSize - 1;
                    }
                    continue;
                }
                _workingBuffer[index] = '\0';
                (strstr(_workingBuffer, " HTTP"))[0] = '\0'; // chop off the " HTTP/1.1"
                bool isGET = strstr(_workingBuffer, "GET ") != 0;
                bool isPOST = strstr(_workingBuffer, "POST ") != 0;
                char* decodedRequestLine = urlDecode(_workingBuffer);
                char* filePath = strcpy(decodedRequestLine, strstr(decodedRequestLine, " ") + 1);

                if (isGET) {
                    FsFile file = _fs->open(filePath);
                    if (!file) {
                        sendHTMLResponse(HTTP_404_NOT_FOUND, client);
                    } else {
                        if (file.isDirectory()) {
                           listFiles(filePath, file, client);
                        } else {
                            clientPrintln(HTTP_200_OK, client);
                            clientPrint(HTTP_CONTENT_TYPE, client);
                            clientPrintln("application/octet-stream", client);
                            clientPrint(HTTP_CONTENT_LENGTH, client);
                            clientPrint(file.size(), DEC, client);
                            clientPrintln("", client);
                            clientPrintln(HTTP_CONNECTION_CLOSE, client);
                            clientPrintln("", client);
                            while (file.available()) {
                                size_t bytesRead = file.read(_workingBuffer, _workingBufferSize);
                                client.write(_workingBuffer, bytesRead);
                            }
                        }
                    }
                    file.close();
                } else if (isPOST) {
                    char* directoryPath = strcpy(filePath, filePath + 1); // remove leading /
                    size_t directoryPathLength = strlen(directoryPath);
                    if (directoryPathLength != 0 && directoryPath[directoryPathLength - 1] != '/')
                    {
                        directoryPath[directoryPathLength] = '/';
                        directoryPath[directoryPathLength + 1] = '\0';
                    }

                    readToBoundary(client);
                    _headerBufferPos = strlen(directoryPath) + 1;
                    size_t bytesRead = client.readBytesUntil('\n', _workingBuffer + _headerBufferPos, _workingBufferSize - _headerBufferPos);
                    _workingBuffer[_headerBufferPos + bytesRead - 1] = '\0'; // replace the \r in \r\n with null terminator
                    readToBody(client); // will probably break if Content-Type is the last header
                    multipart_parser parser;
                    multipart_parser_init(&parser, _workingBuffer + _headerBufferPos, &_multipartParserCallbacks);
                    multipart_parser_set_data(&parser, this);
                    while (bytesRead = client.readBytes(_uploadStreamingBuffer, _uploadStreamingBufferSize)) {
                        multipart_parser_execute(&parser, _uploadStreamingBuffer, bytesRead);
                    }
                    sendHTMLResponse(HTTP_303_REDIRECT, client);
                }
            }

            client.stop();
        }
    }
}

void SDServer::listFiles(const char* directoryPath, FsFile& directory, WiFiClient& client) {
    clientPrintln(HTTP_200_OK, client);
    clientPrint(HTTP_CONTENT_TYPE, client);
    clientPrintln("text/html", client);
    clientPrintln(HTTP_TRANSFER_ENCODING_CHUNKED, client);
    clientPrintln(HTTP_CONNECTION_CLOSE, client);
    clientPrintln("", client);

    const char* htmlStart = "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC\"></head><body><form method=\"post\" enctype=\"multipart/form-data\"><label>Upload file to this folder: </label><br/><input type=\"file\" name=\"file\" required/><br/><input type=\"submit\"/></form><br/>";
    const char* chunkSize = combinedStrLenAsHex({ htmlStart });
    clientPrintln(chunkSize, client);
    clientPrintln(htmlStart, client);

    size_t directoryPathLength = strlen(directoryPath);
    bool appendPathSeparator = directoryPathLength == 0 || directoryPath[directoryPathLength - 1] != '/';
    const char* directoryPathSuffix = appendPathSeparator ? "/" : "";

    char* fileNameBuffer = _workingBuffer + directoryPathLength + 1;
    size_t fileNameBufferSize = _workingBufferSize - directoryPathLength - 1;

    const char* linkStart = "<a href=\"";
    const char* linkMiddle = "\">";
    const char* linkEnd = "</a><br/>";

    directory.rewindDirectory();
    FsFile entry;
    while (entry = directory.openNextFile()) {
        entry.getName(fileNameBuffer, fileNameBufferSize);
        if (requiresURLEncoding(fileNameBuffer)) continue; // don't support spaces and other special characters in file names
        chunkSize = combinedStrLenAsHex({
            linkStart,
            directoryPath,
            directoryPathSuffix,
            fileNameBuffer,
            linkMiddle,
            fileNameBuffer,
            linkEnd
        });
        clientPrintln(chunkSize, client);
        clientPrint(linkStart, client);
        clientPrint(directoryPath, client);
        clientPrint(directoryPathSuffix, client);
        clientPrint(fileNameBuffer, client);
        clientPrint(linkMiddle, client);
        clientPrint(fileNameBuffer, client);
        clientPrintln(linkEnd, client);

        entry.close();
    }
    directory.rewindDirectory();

    const char* htmlEnd = "</body></html>\n";
    chunkSize = combinedStrLenAsHex({ htmlEnd });
    clientPrintln(chunkSize, client);
    clientPrintln(htmlEnd, client);

    // Terminating chunk
    clientPrintln("0", client);
    clientPrintln("", client);
}

char* SDServer::urlDecode(char* text) {
    char hex[] = "0x00";
    size_t textLength = strlen(text);
    size_t decodedLength = 0;
    size_t i = 0;
    while (i < textLength) {
        char decodedCharacter;
        char encodedCharacter = text[i++];
        if (encodedCharacter == '%' && i + 1 < textLength) {
            hex[2] = text[i++];
            hex[3] = text[i++];
            decodedCharacter = strtol(hex, nullptr, 16);
        } else {
            if (encodedCharacter == '+') decodedCharacter = ' ';
            else decodedCharacter = encodedCharacter;
        }
        text[decodedLength++] = decodedCharacter;
    }
    text[decodedLength] = '\0';

    return text;
}