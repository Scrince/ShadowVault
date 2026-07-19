#include <sv/result.hpp>

namespace sv {

const char* error_code_name(ErrorCode code) {
    switch (code) {
    case ErrorCode::Ok:
        return "Ok";
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    case ErrorCode::IoError:
        return "IoError";
    case ErrorCode::CorruptContainer:
        return "CorruptContainer";
    case ErrorCode::WrongCredentials:
        return "WrongCredentials";
    case ErrorCode::HeaderDecryptFailed:
        return "HeaderDecryptFailed";
    case ErrorCode::ManifestDecryptFailed:
        return "ManifestDecryptFailed";
    case ErrorCode::HiddenVolumeProtected:
        return "HiddenVolumeProtected";
    case ErrorCode::TransactionIncomplete:
        return "TransactionIncomplete";
    case ErrorCode::ManifestFull:
        return "ManifestFull";
    case ErrorCode::UnsupportedVersion:
        return "UnsupportedVersion";
    case ErrorCode::PlatformError:
        return "PlatformError";
    case ErrorCode::NotImplemented:
        return "NotImplemented";
    case ErrorCode::VaultLocked:
        return "VaultLocked";
    case ErrorCode::UsageError:
        return "UsageError";
    }
    return "Unknown";
}

} 