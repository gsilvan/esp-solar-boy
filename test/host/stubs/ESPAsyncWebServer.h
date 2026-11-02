#pragma once
#include "Arduino.h"
enum { HTTP_GET, HTTP_POST };
class AsyncWebServerRequest {
public:
    bool hasArg(const char *) { return false; }
    String arg(const char *) { return ""; }
    void send(int, const char *, const String &) {}
};
class AsyncWebServer {
public:
    template<class Callback> void on(const char *, int, Callback) {}
};
