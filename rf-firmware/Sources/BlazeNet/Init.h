#ifndef BLAZENET_INIT_H
#define BLAZENET_INIT_H

#include "Beacon.h"

/// High level BlazeNet protocol support
namespace BlazeNet {
/**
 * @brief Initialize BlazeNet protocol support
 */
static inline void Init() {
    Beacon::Init();
}
}

#endif
