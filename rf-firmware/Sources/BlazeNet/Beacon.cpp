#include "Log/Logger.h"
#include "Packet/Handler.h"
#include "Rtos/Rtos.h"

#include "Beacon.h"

using namespace BlazeNet;

bool Beacon::gEnabled{false};
TimerHandle_t Beacon::gTimer{};

Packet::Handler::TxPacketBuffer *Beacon::gPacket{nullptr};
SemaphoreHandle_t Beacon::gPacketLock;

/**
 * @brief Initialize the beacon handler
 *
 * Set up the periodic beacon timer (but dormant, for now) and buffers
 */
void Beacon::Init() {
    static StaticTimer_t gTimerStorage;
    static StaticSemaphore_t gLockStorage;

    // set up the timer
    gTimer = xTimerCreateStatic("beaconizer", pdMS_TO_TICKS(5000), pdTRUE,
            nullptr, [](auto) {
        EmitBeacon();
    }, &gTimerStorage);
    REQUIRE(gTimer, "failed to initialize %s", "beaconizer timer");

    // set up mutex guarding our buffer
    gPacketLock = xSemaphoreCreateMutexStatic(&gLockStorage);
    REQUIRE(gPacketLock, "failed to initialize %s", "beaconizer packet lock");
}

/**
 * @brief Notification that host communication has been regained
 *
 * If autonomous beaconing has been enabled, re-enable the timer.
 */
void Beacon::CommsRegained() {
    if(gEnabled) {
        xTimerReset(gTimer, portMAX_DELAY);
    }
}

/**
 * @brief Notification that host communication has been lost
 *
 * Pause the beaconing timer, if it's activated.
 */
void Beacon::CommsLost() {
    xTimerStop(gTimer, portMAX_DELAY);
}

/**
 * @brief Set whether beaconing is enabled
 *
 * @param isEnabled When set, beacons are automatically emitted
 */
void Beacon::SetEnabled(const bool isEnabled) {
    // bail if the state would not change
    if(isEnabled == gEnabled) {
        return;
    }

    // enable or disable the thymer
    gEnabled = isEnabled;

    if(isEnabled) {
        xTimerReset(gTimer, portMAX_DELAY);
    } else {
        xTimerStop(gTimer, portMAX_DELAY);
    }
}

/**
 * @brief Update the beacon interval
 *
 * Set the beacon timer's interval.
 *
 * @param interval Beaconing interval, in msec
 */
void Beacon::SetInterval(const uintptr_t interval) {
    xTimerChangePeriod(gTimer, pdMS_TO_TICKS(interval), portMAX_DELAY);

    if(gEnabled) {
        // force re-evaluation of the expiration time
        xTimerReset(gTimer, portMAX_DELAY);
    } else {
        // stop the timer, as xTimerChangePeriod will have started it
        xTimerStop(gTimer, portMAX_DELAY);
    }
}

/**
 * @brief Update the beacon packet payload
 *
 * Allocate a new packet buffer for use when trasmitting beacon frames.
 *
 * @param payload New payload
 */
int Beacon::SetPayload(etl::span<const uint8_t> payload) {
    int err{0};
    BaseType_t ok;

    // acquire lock…
    ok = xSemaphoreTake(gPacketLock, portMAX_DELAY);
    REQUIRE(ok == pdTRUE, "failed to acquire %s", "beacon packet lock");

    // release the old packet
    if(gPacket) {
        Packet::Handler::DiscardTxPacket(gPacket, true);
        gPacket = nullptr;
    }

    // allocate new packet
    gPacket = Packet::Handler::AllocTxPacket(payload, true);
    if(!gPacket) {
        err = -1;
        goto done;
    }

done:;
    // donezo
    xSemaphoreGive(gPacketLock);

    return err;
}



/**
 * @brief Transmit a beacon frame
 *
 * Format a packet and submit it to the packet handler for transmission.
 */
void Beacon::EmitBeacon() {
    BaseType_t ok;

    REQUIRE(gPacket, "beaconing enabled, but no beacon packet configured");

    // submit the previously allocated packet
    ok = xSemaphoreTake(gPacketLock, portMAX_DELAY);
    REQUIRE(ok == pdTRUE, "failed to acquire %s", "beacon packet lock");

    Packet::Handler::QueueTxPacket(Packet::Handler::TxPacketPriority::NetworkControl, gPacket);
    xSemaphoreGive(gPacketLock);
}
