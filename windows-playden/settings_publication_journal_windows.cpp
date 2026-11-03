// Windows full-host compile boundary for PS5 publication journaling.
// This is deliberately NOT a functional publication backend. All operations
// refuse or return unavailable until native NTFS journaling is implemented.
#include "runtime/settings_publication_journal.h"
namespace playden::runtime {
namespace {
const char* kUnavailable="Secure Windows settings-publication journal is not implemented.";
Result<void> Refuse() { return Result<void>::Failure(StorageError::kOpenFailed,kUnavailable); }
void Reason(std::string* reason) { if(reason)*reason=kUnavailable; }
PublicationOperationLoad UnavailableOperation() {
    return {PublicationRecoveryState::kUnavailable,std::nullopt,kUnavailable};
}
PublicationRequestLoad UnavailableRequest() {
    return {PublicationRecoveryState::kUnavailable,std::nullopt,kUnavailable};
}
}
std::optional<Bytes> EncodeSettingsPublicationRequest(const SettingsPublicationRequest&,std::string* reason){
    Reason(reason);return std::nullopt;
}
std::optional<SettingsPublicationRequest> DecodeSettingsPublicationRequest(const Bytes&,std::string* reason){
    Reason(reason);return std::nullopt;
}
Result<void> CreateSettingsPublicationRequest(const std::filesystem::path&,const SettingsPublicationRequest&){
    return Refuse();
}
PublicationRequestLoad LoadSettingsPublicationRequest(const std::filesystem::path&,const std::string&){
    return UnavailableRequest();
}
std::optional<SettingsPublicationOperation> FreezeSettingsPublicationOperation(
    SettingsPublicationOperation,std::string* reason){Reason(reason);return std::nullopt;}
std::optional<Bytes> EncodeSettingsPublicationOperation(const SettingsPublicationOperation&,std::string* reason){
    Reason(reason);return std::nullopt;
}
std::optional<SettingsPublicationOperation> DecodeSettingsPublicationOperation(const Bytes&,std::string* reason){
    Reason(reason);return std::nullopt;
}
std::optional<ProfilePublication> RebuildFrozenProfilePublication(
    const SettingsPublicationOperation&,std::string* reason){Reason(reason);return std::nullopt;}
Result<void> CreateSettingsPublicationOperation(const std::filesystem::path&,const SettingsPublicationOperation&){
    return Refuse();
}
PublicationOperationLoad LoadSettingsPublicationOperation(const std::filesystem::path&){
    return UnavailableOperation();
}
Result<std::string> SetAsideSettingsPublicationOperation(const std::filesystem::path&,std::int64_t){
    return Result<std::string>::Failure(StorageError::kOpenFailed,kUnavailable);
}
Result<void> ArchiveSettingsPublicationOperation(const std::filesystem::path&,
    const SettingsPublicationOperation&,const std::function<bool()>&){return Refuse();}
Result<void> CreatePreparedSettingsPublicationOperation(const std::filesystem::path&,
    const SettingsPublicationOperation&){return Refuse();}
PublicationOperationLoad LoadPreparedSettingsPublicationOperation(const std::filesystem::path&,
    const std::string&){return UnavailableOperation();}
} // namespace playden::runtime
