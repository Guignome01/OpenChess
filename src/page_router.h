#include <Arduino.h>

struct Page {
  const char* path;
  const uint8_t* data;
  size_t length;
  const char* mime;
  bool gzip;
};

const Page* findPage(const char* path);
