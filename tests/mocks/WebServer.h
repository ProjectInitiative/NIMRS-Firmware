#ifndef WEBSERVER_MOCK_H
#define WEBSERVER_MOCK_H

#include "Arduino.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

enum HTTPMethod {
  HTTP_GET,
  HTTP_POST,
  HTTP_DELETE,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_OPTIONS,
  HTTP_ANY
};

struct HTTPUpload {
  String filename;
  size_t currentSize;
  size_t totalSize;
  int status;
  uint8_t buf[256];
};

#define UPLOAD_FILE_START 0
#define UPLOAD_FILE_WRITE 1
#define UPLOAD_FILE_END 2
#define CONTENT_LENGTH_UNKNOWN 0

class File {
public:
  operator bool() const { return _valid; }
  void close() {}
  size_t write(const uint8_t *buf, size_t size) { return size; }
  const char *name() const { return _name.c_str(); }
  size_t size() const { return _size; }
  bool isDirectory() const { return _isDir; }
  File openNextFile(); // Defined in mocks.cpp
  File() : _name(""), _size(0), _isDir(false), _valid(false), _nextIdx(0) {}
  File(String name, size_t size, bool isDir = false)
      : _name(name), _size(size), _isDir(isDir), _valid(true), _nextIdx(0) {}

private:
  String _name;
  size_t _size;
  bool _isDir;
  bool _valid;
  size_t _nextIdx;
};

class WebServer {
public:
  WebServer(int port) : _method(HTTP_GET), lastCode(0) {}
  void on(const String &uri, std::function<void()> handler) {}
  void on(const String &uri, HTTPMethod method, std::function<void()> handler) {
  }
  void on(const String &uri, HTTPMethod method, std::function<void()> handler,
          std::function<void()> uploadHandler) {}
  void onNotFound(std::function<void()> handler) {}
  void begin() {}
  void handleClient() {}
  void send(int code, const char *content_type, const String &content) {
    lastCode = code;
    lastContentType = content_type;
    lastContent = content;
  }
  void send_P(int code, const char *content_type, const char *content) {
    send(code, content_type, String(content));
  }
  void sendContent(const String &content) {}
  void sendContent(const char *content, size_t length) {}
  void setContentLength(size_t length) {}
  bool hasArg(const String &name) {
    return args.find((std::string)name) != args.end();
  }
  String arg(const String &name) { return args[(std::string)name]; }
  HTTPMethod method() { return _method; }
  String uri() { return _uri; }
  void streamFile(File &file, const String &contentType) {}
  HTTPUpload &upload() { return _upload; }

  // Authentication methods (Mocked)
  bool authenticate(const char *user, const char *pass) { return true; }
  void requestAuthentication() {}

  // Test helpers
  std::map<std::string, String> args;
  HTTPMethod _method;
  String _uri;
  int lastCode;
  String lastContentType;
  String lastContent;
  HTTPUpload _upload;
};

#endif