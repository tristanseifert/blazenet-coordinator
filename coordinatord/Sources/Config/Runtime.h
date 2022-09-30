/**
 * @file
 *
 * @brief Runtime config support
 *
 * This file provides support for runtime configuration options, which are read out from confd.
 *
 * @TODO Provide defaults if confd support is not present
 */
#ifndef CONFIG_RUNTIME_H
#define CONFIG_RUNTIME_H

#ifdef CONFIG_WITH_CONFD
#include "RuntimeConfd.h"
#else
#warning TODO: Add support for runtime config sans confd
#endif

#endif
