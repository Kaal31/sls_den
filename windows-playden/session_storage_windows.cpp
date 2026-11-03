// Windows runtime port: console wrapper session storage is unavailable until
// the native NTFS session ownership and journaling contract is implemented.
// Never report an empty successful scan: it could silently skip recovery.
#include "runtime/session_storage.h"
#include <utility>
namespace playden::runtime {
FilesystemReconcileSessionSource::FilesystemReconcileSessionSource() = default;
FilesystemReconcileSessionSource::FilesystemReconcileSessionSource(HostDataRootForTests mapping)
  : host_data_root_(std::move(mapping.path)) {}
SessionEnumeration FilesystemReconcileSessionSource::Enumerate(const RuntimeInstall&,std::size_t) {
  return SessionEnumeration::Failure("windows-session-unavailable",
                                    "PS5 wrapper-session enumeration is unavailable on Windows.");
}
SessionEnumeration FilesystemReconcileSessionSource::Read(const RuntimeInstall&,const std::string&) {
  return SessionEnumeration::Failure("windows-session-unavailable",
                                    "PS5 wrapper-session reading is unavailable on Windows.");
}
DurableReconcileJournal::DurableReconcileJournal(Storage& storage):storage_(storage){}
ReconcileJournalLoad DurableReconcileJournal::Load(const std::string&) {
  return ReconcileJournalLoad::Failure("windows-journal-unavailable",
                                      "PS5 reconciliation journal is unavailable on Windows.");
}
ReconcileJournalResult DurableReconcileJournal::Save(const ReconcileEntry&) {
  return ReconcileJournalResult::Failure("windows-journal-unavailable",
                                        "PS5 reconciliation journal is unavailable on Windows.");
}
} // namespace playden::runtime
