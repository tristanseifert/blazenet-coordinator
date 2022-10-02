#ifndef CRYPTO_CONTEXT_H
#define CRYPTO_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#include <etl/span.h>

#include <sl_se_manager.h>

namespace Crypto {
/**
 * @brief Crypto context
 *
 * Wraps the underlying security engine stuff to provide a context on which crypto operations may
 * be performed.
 *
 * @remark Contexts are _not_ thread safe: you cannot use the same context concurrently from
 *         multiple threads. However, you _can_ use distinct crypto contexts from different
 *         threads at the same time.
 */
class Context {
    public:
        /// Error codes returned from various security engine routines
        enum Error: int {
            Success                             = 0,
            /// General error with no additional information
            GenericError                        = -1,
            /// Operation is not supported/implemented
            InvalidOperation                    = -2,
            /// Command is not currently authorized
            InvalidCredentials                  = -3,
            /// Invalid parameters passed to call
            InvalidParameter                    = -4,
            /// Current configuration is invalid (likely: scheduler required)
            NotAvailable                        = -5,
            InvalidSignature                    = -6,
            BusError                            = -7,
            Aborted                             = -8,
            SelftestFailed                      = -9,
            NotInitialized                      = -10,
        };

        /**
         * @brief Device certificate type
         *
         * The security engine in the device encapsulates several different types of
         * certificates that may be read out (and used) by software.
         */
        enum class CertType {
            /// Production batch certificate
            Batch,
            /// Security engine attestation certificate
            SecureEngineId,
            /// Host ID attestation certificate
            HostId,
        };

        /**
         * @brief Immutable device key type
         *
         * Various types of public/private keys can be stored in the OTP of the device, accessible
         * to the SE. Depending on configuration, the keys may or may not be able to be read out
         * as-is.
         *
         * Regardless of the key type, they will all be immutable on the SE.
         *
         * @remark Attestation keys are only present on the "secure vault" (high security) devices
         */
        enum class ImmutableKeyType {
            Boot,
            Auth,
            AES128,
            Attestation,
            EngineAttestation,
        };

    public:
        Context(const bool async = true);
        ~Context();

        // Context configuration
        /**
         * @brief Configure whether the context supports async operations
         *
         * When the context is configured for async operations, it will block the calling task
         * while waiting for command completion.
         *
         * @param async Whether async IO is supported
         */
        inline int setAsync(const bool async) {
            auto err = sl_se_set_yield(&this->ctx, async);
            return ConvertError(err);
        }

        // General SE properties
        int getEngineSerial(etl::span<uint8_t, 16> snBuffer);
        int getEngineVersion(uint32_t &outVersion);

        // Device certificates and keys
        int getDevicePubkey(const ImmutableKeyType which, etl::span<uint8_t, 64> outBuffer);
        int getDeviceCertSize(const CertType which, size_t &outSize);
        int getDeviceCert(const CertType which, etl::span<uint8_t> outBuffer);

    private:
        /**
         * @brief Convert an error code
         *
         * Transform the provided security engine (SE) error code to a crypto context error code.
         */
        constexpr static int ConvertError(const sl_status_t in) {
            switch(in) {
                case SL_STATUS_OK:
                    return Error::Success;

                case SL_STATUS_COMMAND_IS_INVALID:
                    return Error::InvalidOperation;
                case SL_STATUS_INVALID_CREDENTIALS:
                    return Error::InvalidCredentials;
                case SL_STATUS_INVALID_PARAMETER:
                    return Error::InvalidParameter;
                case SL_STATUS_NOT_AVAILABLE:
                    return Error::NotAvailable;
                case SL_STATUS_INVALID_SIGNATURE:
                    return Error::InvalidSignature;
                case SL_STATUS_BUS_ERROR:
                    return Error::BusError;
                case SL_STATUS_ABORT:
                    return Error::Aborted;
                case SL_STATUS_INITIALIZATION:
                    return Error::SelftestFailed;
                case SL_STATUS_NOT_INITIALIZED:
                    return Error::NotInitialized;

                case SL_STATUS_FAIL: [[fallthrough]];
                // treat other values as error codes of unknown origin
                default:
                    return Error::GenericError;
            }
        }

        /// Convert certificate type
        constexpr static inline sl_se_cert_type_t ConvertTo(const CertType which) {
            switch(which) {
                case CertType::Batch:
                    return SL_SE_CERT_BATCH;
                case CertType::SecureEngineId:
                    return SL_SE_CERT_DEVICE_SE;
                case CertType::HostId:
                    return SL_SE_CERT_DEVICE_HOST;
            }
        }

        /// Convert immutable key type
        constexpr static inline sl_se_device_key_type_t ConvertTo(const ImmutableKeyType which) {
            switch(which) {
                case ImmutableKeyType::Boot:
                    return SL_SE_KEY_TYPE_IMMUTABLE_BOOT;
                case ImmutableKeyType::Auth:
                    return SL_SE_KEY_TYPE_IMMUTABLE_AUTH;
                case ImmutableKeyType::AES128:
                    return SL_SE_KEY_TYPE_IMMUTABLE_AES_128;
                case ImmutableKeyType::Attestation:
                    return SL_SE_KEY_TYPE_IMMUTABLE_ATTESTATION;
                case ImmutableKeyType::EngineAttestation:
                    return SL_SE_KEY_TYPE_IMMUTABLE_SE_ATTESTATION;
            }
        }


    private:
        /// Security engine context/state
        sl_se_command_context_t ctx;
};
}

#endif
