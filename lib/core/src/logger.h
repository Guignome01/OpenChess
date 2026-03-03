#ifndef CORE_LOGGER_H
#define CORE_LOGGER_H

#include <cstdarg>
#include <cstdio>

// Abstract logging interface — allows core library classes to emit diagnostic
// output without depending on Arduino Serial or any other concrete I/O.
// Nullable everywhere: callers guard with `if (logger_)`.
class ILogger {
 public:
  virtual ~ILogger() = default;

  virtual void info(const char* message) = 0;
  virtual void error(const char* message) = 0;

  // Convenience formatted helpers (non-virtual).
  void infof(const char* fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    info(buf);
  }

  void errorf(const char* fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    error(buf);
  }
};

#endif  // CORE_LOGGER_H
