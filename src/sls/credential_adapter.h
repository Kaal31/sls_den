#pragma once
// Windows host adapter. No Windows APIs are used by provider_registry.h or source_resolver.
#include "sls/provider_registry.h"
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

namespace playden::sls {
#ifdef _WIN32
inline std::wstring CredentialTarget(const std::string& id) {
    return L"SLS Den/SourceKey/" + std::wstring(id.begin(),id.end());
}
inline bool HasStoredKey(const std::string& id) {
    if(!ValidKeyId(id)) return false;
    PCREDENTIALW credential=nullptr;
    const auto target=CredentialTarget(id);
    const bool found=CredReadW(target.c_str(),CRED_TYPE_GENERIC,0,&credential)!=FALSE;
    const bool present=found && credential && credential->CredentialBlobSize>0;
    if(credential) CredFree(credential);
    return present;
}
inline bool StoreKey(const std::string& id,const std::string& value) {
    if(!ValidKeyId(id)||value.size()>2048) return false;
    const auto target=CredentialTarget(id);
    if(value.empty()) return CredDeleteW(target.c_str(),CRED_TYPE_GENERIC,0)!=FALSE ||
                             GetLastError()==ERROR_NOT_FOUND;
    CREDENTIALW credential{};
    credential.Type=CRED_TYPE_GENERIC;
    credential.TargetName=const_cast<wchar_t*>(target.c_str());
    credential.CredentialBlobSize=static_cast<DWORD>(value.size());
    credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(value.data()));
    credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential,0)!=FALSE;
}
#else
inline bool HasStoredKey(const std::string&) {return false;}
inline bool StoreKey(const std::string&,const std::string&) {return false;}
#endif
} // namespace playden::sls
