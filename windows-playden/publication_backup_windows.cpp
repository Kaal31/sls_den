// Windows fail-closed boundary pending verified NTFS publication implementation.
// Never interpret a missing Unix backup implementation as success.
#include "runtime/publication_backup.h"
namespace playden::runtime {
PublicationBackupLoad LoadPublicationRegistryBackup(
    const std::filesystem::path&, const std::string&,
    const RegistryBackupBinding&) {
  return {PublicationRecoveryState::kUnavailable, {},
          "Secure Windows publication backup is not implemented."};
}
Result<RegistryBackupBinding> CreatePublicationRegistryBackup(
    const std::filesystem::path&, const std::string&,
    std::string_view, unsigned) {
  return Result<RegistryBackupBinding>::Failure(
      StorageError::kOpenFailed,
      "Secure Windows publication backup is not implemented.");
}
} // namespace playden::runtime
