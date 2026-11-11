#pragma once
// Shared, transport-neutral handoff state machine for Steam-authorized installs.
// Public Steam Store entries and provider manifests are discovery metadata, not licenses.
// Hosts supply an installer adapter; this class never downloads files or grants access.
#include "sls/install_handoff.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace playden::sls {
enum class HandoffJobState {
  AwaitingAuthorization, Queued, Downloading, Verifying, Installed, Failed, Cancelled
};
struct HandoffJob {
  std::uint32_t app_id=0;
  std::string provider_id;
  HandoffJobState state=HandoffJobState::AwaitingAuthorization;
  std::vector<steam::DepotInfo> depots;
  std::uint64_t downloaded_bytes=0;
  std::uint64_t total_bytes=0;
  std::string error;
  std::uint32_t attempts=0;
};
using AuthorizedInstallerSubmit=std::function<bool(
    std::uint32_t app_id,const std::vector<steam::DepotInfo>& authorized,std::string* error)>;

// Only a verified, complete Steam-owned depot plan may reach an installer.
// No source-supplied GID, token or key can add a depot or manufacture a license.
class HandoffJobQueue {
 public:
  const HandoffJob& Prepare(const HandoffRecord& record,
                            const std::vector<steam::DepotInfo>& authorized) {
    HandoffJob next;
    next.app_id=record.app_id;
    next.provider_id=record.provider_id;
    if(record.app_id==0 || authorized.empty()) {
      next.error="Steam download authorization unavailable";
    } else if(record.status!="matched" ||
              record.depots.size()!=authorized.size()) {
      next.error="Provider metadata incomplete or unavailable";
    } else {
      bool valid=true;
      for(std::size_t i=0;i<authorized.size();++i) {
        if(!authorized[i].id || !authorized[i].manifest_gid ||
           record.depots[i].id!=authorized[i].id ||
           record.depots[i].manifest_gid!=authorized[i].manifest_gid) {
          valid=false;break;
        }
      }
      if(valid) {
        next.state=HandoffJobState::Queued;
        next.depots=authorized;
      } else next.error="Depot authorization mismatch";
    }
    auto existing=Find(next.app_id);
    // Repeated discovery must not reset an active or completed installation.
    if(existing && existing->state!=HandoffJobState::AwaitingAuthorization &&
       existing->state!=HandoffJobState::Failed &&
       existing->state!=HandoffJobState::Cancelled) return *existing;
    if(existing) *existing=std::move(next);
    else {
      jobs_.push_back(std::move(next));
      existing=&jobs_.back();
    }
    return *existing;
  }
  HandoffJob* Find(std::uint32_t app_id) {
    auto it=std::find_if(jobs_.begin(),jobs_.end(),
                         [&](const auto& job){return job.app_id==app_id;});
    return it==jobs_.end()?nullptr:&*it;
  }
  const HandoffJob* Find(std::uint32_t app_id) const {
    auto it=std::find_if(jobs_.begin(),jobs_.end(),
                         [&](const auto& job){return job.app_id==app_id;});
    return it==jobs_.end()?nullptr:&*it;
  }
  bool Submit(std::uint32_t app_id,const AuthorizedInstallerSubmit& install) {
    auto* job=Find(app_id);
    if(!job || job->state!=HandoffJobState::Queued || job->depots.empty() || !install)
      return false;
    ++job->attempts;
    std::string error;
    if(!install(app_id,job->depots,&error)) {
      job->state=HandoffJobState::Failed;
      job->error=error.empty()?"Installer rejected request":std::move(error);
      return false;
    }
    job->state=HandoffJobState::Downloading;
    job->error.clear();
    return true;
  }
  bool Progress(std::uint32_t app_id,std::uint64_t downloaded,std::uint64_t total) {
    auto* job=Find(app_id);
    if(!job || job->state!=HandoffJobState::Downloading ||
       !total || downloaded>total || downloaded<job->downloaded_bytes)
      return false;
    job->downloaded_bytes=downloaded;
    job->total_bytes=total;
    if(downloaded==total)job->state=HandoffJobState::Verifying;
    return true;
  }
  bool Finish(std::uint32_t app_id,bool verified,std::string error={}) {
    auto* job=Find(app_id);
    if(!job || job->state!=HandoffJobState::Verifying)return false;
    job->state=verified?HandoffJobState::Installed:HandoffJobState::Failed;
    job->error=verified?std::string{}:(error.empty()?"Verification failed":std::move(error));
    return true;
  }
  bool Fail(std::uint32_t app_id,std::string error) {
    auto* job=Find(app_id);
    if(!job || (job->state!=HandoffJobState::Downloading &&
                job->state!=HandoffJobState::Verifying))return false;
    job->state=HandoffJobState::Failed;
    job->error=std::move(error);
    return true;
  }
  bool Retry(std::uint32_t app_id) {
    auto* job=Find(app_id);
    if(!job || job->state!=HandoffJobState::Failed || job->depots.empty())return false;
    job->state=HandoffJobState::Queued;
    job->downloaded_bytes=0;
    job->total_bytes=0;
    job->error.clear();
    return true;
  }
  using CancelActiveTransfer=std::function<bool(std::uint32_t)>;
  bool Cancel(std::uint32_t app_id,const CancelActiveTransfer& cancel_active={}) {
    auto* job=Find(app_id);
    if(!job || job->state==HandoffJobState::Installed ||
       job->state==HandoffJobState::Cancelled)return false;
    const bool active=job->state==HandoffJobState::Downloading ||
                      job->state==HandoffJobState::Verifying;
    if(active && (!cancel_active || !cancel_active(app_id))) {
      job->error="Installer cancellation was not acknowledged";
      return false;
    }
    job->state=HandoffJobState::Cancelled;
    job->error.clear();
    return true;
  }
 private:
  std::vector<HandoffJob> jobs_;
};
} // namespace playden::sls
