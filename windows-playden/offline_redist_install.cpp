// Windows-only offline preview: never install or inspect game redistributables.
#include "runtime/redist_install.h"
namespace playden::runtime {
RedistAction ClassifyRedist(const RedistEntry&, const std::string&, std::string* package) {
 if(package) package->clear();
 return RedistAction::kNotRun;
}
RedistInstallResult InstallRedistributables(const std::string&,const std::string&,const std::string&,std::int64_t){return {};}
std::vector<VcRedist> MicrosoftVcRedists(){return {};}
bool MfcFileName(std::string_view){return false;}
MfcInstallResult InstallMfcRuntime(const VcRedist&,const std::string&,const std::string&,const RangeFetch&,std::int64_t){
 return {"failed","Unavailable in Windows offline preview","",0,"",false};
}
bool MfcInstalled(const VcRedist&,const std::string&){return false;}
std::optional<MfcScanRecord> ReadMfcScan(const std::string&){return std::nullopt;}
bool WriteMfcScan(const std::string&,const MfcScanRecord&){return false;}
std::string GameFilesFingerprint(const std::string&){return {};}
std::string ExtendPath(const std::string& current,const std::vector<std::string>&,const std::vector<std::string>&){return current;}
}
