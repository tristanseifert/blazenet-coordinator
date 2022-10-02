/**
 * @file
 *
 * @brief Initialization and internal helpers
 *
 * This file provides the context initialization and configuration, as well as some internal
 * helper methods. All actual operations are implemented in the `Context+(Category).cpp` files.
 */
#include <sl_se_manager.h>

#include "Log/Logger.h"

#include "Context.h"

using namespace Crypto;

/**
 * @brief Initialize crypto context
 *
 * This sets up a security engine context under the hood, which is used to execute commands on the
 * security engine, and keep track of associated state.
 *
 * @param async Whether the command queue will yield (block the calling task) during command
 *              execution. This requires the scheduler is started, so to access the crypto engine
 *              during system initialization, do not use async mode.
 *
 * @seeAlso setAsync
 */
Context::Context(const bool async) : ctx(SL_SE_COMMAND_CONTEXT_INIT) {
    auto ok = sl_se_init_command_context(&this->ctx);
    REQUIRE(ok == SL_STATUS_OK, "%s failed: %d", "sl_se_init_command_context", ok);

    // apply the context configuration
    auto err = this->setAsync(async);
    REQUIRE(!err, "%s failed: %d", "Crypto::setAsync", err);
}

/**
 * @brief Clean up crypto context
 *
 * Release all resources associated with the context.
 */
Context::~Context() {
    auto ok = sl_se_deinit_command_context(&this->ctx);
    REQUIRE(ok == SL_STATUS_OK, "%s failed: %d", "sl_se_deinit_command_context", ok);
}
