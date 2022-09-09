#ifndef LOGGING_H
#define LOGGING_H

#include <cstddef>

#include <plog/Log.h>

namespace Support {
void InitLogging(const int logLevel = 0, const bool simple = false);
};

#endif
