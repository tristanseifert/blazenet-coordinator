/**
 * @file
 *
 * @brief Define the host interface task's command handler list
 *
 * This list is gigantic and unwieldy so it's best broken out into its own source file.
 */
#ifndef HOSTIF_COMMANDHANDLER_H
#define HOSTIF_COMMANDHANDLER_H

#include "Handlers/GetInfo.h"
#include "Handlers/RadioConfig.h"
#include "Handlers/GetStatus.h"
#include "Handlers/IrqConfig.h"
#include "Handlers/GetPacketQueueStatus.h"
#include "Handlers/ReadPacket.h"
#include "Handlers/TransmitPacket.h"
#include "Handlers/BeaconConfig.h"
#include "Handlers/GetCounters.h"
#include "Handlers/IrqStatus.h"

#include "Task.h"

using namespace HostIf;

const etl::array<const Task::CommandHandler, Task::kMaxCommandId> Task::gHandlers{{
    // 0x00: NoOp
    {
        .flags          = HandlerFlags::SupportsWrite,
        .read           = nullptr,
        .readComplete   = nullptr,
        .write          = [](auto, auto) -> int { return 0; },
    },
    // 0x01: GetInfo
    {
        .flags          = HandlerFlags::SupportsRead,
        .read           = Handlers::GetInfo::DoRead,
        .readComplete   = nullptr,
        .write          = nullptr,
    },
    // 0x02: RadioConfig
    {
        // TODO: implement read
        .flags          = HandlerFlags::SupportsWrite,
        .read           = nullptr,
        .readComplete   = nullptr,
        .write          = Handlers::RadioConfig::DoWrite,
    },
    // 0x03: GetStatus
    {
        .flags          = HandlerFlags::SupportsRead,
        .read           = Handlers::GetStatus::DoRead,
        .readComplete   = nullptr,
        .write          = nullptr,
    },
    // 0x04: IrqConfig
    {
        .flags          = (HandlerFlags::SupportsRead | HandlerFlags::SupportsWrite),
        .read           = Handlers::IrqConfig::DoRead,
        .readComplete   = nullptr,
        .write          = Handlers::IrqConfig::DoWrite,
    },
    // 0x05: GetPacketQueueStatus
    {
        .flags          = HandlerFlags::SupportsRead,
        .read           = Handlers::GetPacketQueueStatus::DoRead,
        .readComplete   = nullptr,
        .write          = nullptr,
    },
    // 0x06: ReadPacket
    {
        .flags          = (HandlerFlags::SupportsRead | HandlerFlags::WantsPostRead),
        .read           = Handlers::ReadPacket::DoRead,
        .readComplete   = Handlers::ReadPacket::PostRead,
        .write          = nullptr,
    },
    // 0x07: TransmitPacket
    {
        .flags          = HandlerFlags::SupportsWrite,
        .read           = nullptr,
        .readComplete   = nullptr,
        .write          = Handlers::TransmitPacket::DoWrite,
    },
    // 0x08: BeaconConfig
    {
        .flags          = HandlerFlags::SupportsWrite,
        .read           = nullptr,
        .readComplete   = nullptr,
        .write          = Handlers::BeaconConfig::DoWrite,
    },
    // 0x09: GetCounters
    {
        .flags          = HandlerFlags::SupportsRead,
        .read           = Handlers::GetCounters::DoRead,
        .readComplete   = nullptr,
        .write          = nullptr,
    },
    // 0x0A: IrqStatus
    {
        .flags          = (HandlerFlags::SupportsRead | HandlerFlags::SupportsWrite),
        .read           = Handlers::IrqStatus::DoRead,
        .readComplete   = nullptr,
        .write          = Handlers::IrqStatus::DoWrite,
    },
}};

#endif
