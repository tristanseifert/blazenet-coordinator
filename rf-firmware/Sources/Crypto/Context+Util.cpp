/**
 * @file
 *
 * @brief Utility routines
 */
#include <sl_se_manager.h>
#include <sl_se_manager_util.h>

#include "Context.h"

using namespace Crypto;



/**
 * @brief Get the security engine's serial number
 *
 * @param snBuffer Buffer to receive the serial number
 *
 * @return 0 on success, an error code otherwise
 */
int Context::getEngineSerial(etl::span<uint8_t, 16> snBuffer) {
    auto err = sl_se_get_serialnumber(&this->ctx, snBuffer.data());
    return ConvertError(err);
}

/**
 * @brief Get the security engine firmware version
 *
 * @param outVersion Variable to receive firmware version
 *
 * @return 0 on success, an error code otherwise
 */
int Context::getEngineVersion(uint32_t &outVersion) {
    auto err = sl_se_get_se_version(&this->ctx, &outVersion);
    return ConvertError(err);
}



/**
 * @brief Read the public key of an immutable device key
 *
 * Depending on the device SKU, multiple immutable keys exist either programmed by the factory, or
 * by user programming them into OTP.
 *
 * @param which Device key to read out
 * @param outBuffer Buffer to receive the raw public key data
 *
 * @return 0 on success, or a negative error code
 */
int Context::getDevicePubkey(const ImmutableKeyType which, etl::span<uint8_t, 64> outBuffer) {
    auto err = sl_se_read_pubkey(&this->ctx, ConvertTo(which), outBuffer.data(), outBuffer.size());
    return ConvertError(err);
}

/**
 * @brief Read out the size of a stored certificate
 *
 * @param which Certificate type to query information for
 * @param outSize Variable to receive the certificate size (in bytes)
 *
 * @return 0 on success, or a negative error code
 */
int Context::getDeviceCertSize(const CertType which, size_t &outSize) {
    sl_se_cert_size_type_t sizes;
    auto err = sl_se_read_cert_size(&this->ctx, &sizes);

    if(err == SL_STATUS_OK) {
        switch(which) {
            case CertType::Batch:
                outSize = sizes.batch_id_size;
                break;
            case CertType::SecureEngineId:
                outSize = sizes.se_id_size;
                break;
            case CertType::HostId:
                outSize = sizes.host_id_size;
                break;
        }
    }

    return ConvertError(err);
}

/**
 * @brief Read out a device certificate
 *
 * @param which Certificate type to retrieve
 * @param outBuffer Buffer to hold the certificate; should be sufficiently large
 *
 * @return 0 on success, or a negative error code
 */
int Context::getDeviceCert(const CertType which, etl::span<uint8_t> outBuffer) {
    auto ok = sl_se_read_cert(&this->ctx, ConvertTo(which), outBuffer.data(),
            outBuffer.size());
    return ConvertError(ok);
}
