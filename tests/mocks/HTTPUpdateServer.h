#pragma once
#include "WebServer.h"

class HTTPUpdateServer {
public:
  void setup(WebServer *server, const char *path) {}
  void setup(WebServer *server, const char *path, const char *user,
             const char *pass) {}
};