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
inline bool ReadStoredKey(const std::string& id,std::string* value) {
    if(!value || !ValidKeyId(id)) return false;
    value->clear();
    PCREDENTIALW credential=nullptr;
    const auto target=CredentialTarget(id);
    if(!CredReadW(target.c_str(),CRED_TYPE_GENERIC,0,&credential)) return false;
    bool ok=false;
    if(credential && credential->CredentialBlob &&
       credential->CredentialBlobSize>0 && credential->CredentialBlobSize<=2048) {
        const auto* bytes=reinterpret_cast<const char*>(credential->CredentialBlob);
        value->assign(bytes,bytes+credential->CredentialBlobSize);
        ok=true;
    }
    if(credential) CredFree(credential);
    return ok;
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

inline bool StoreLuatoolsSessionTokens(const std::string& access,const std::string& refresh) {
    if(access.empty() || access.size()>8192 || refresh.size()>8192)return false;
    auto store=[](const wchar_t* target,const std::string& token)->bool {
        CREDENTIALW credential{};
        credential.Type=CRED_TYPE_GENERIC;
        credential.TargetName=const_cast<wchar_t*>(target);
        credential.CredentialBlobSize=static_cast<DWORD>(token.size());
        credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
        credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
        return CredWriteW(&credential,0)!=FALSE;
    };
    constexpr wchar_t access_target[]=L"SLS Den/lua.tools/access";
    constexpr wchar_t refresh_target[]=L"SLS Den/lua.tools/refresh";
    if(!store(access_target,access))return false;
    if(!refresh.empty() && !store(refresh_target,refresh)) {
        CredDeleteW(access_target,CRED_TYPE_GENERIC,0);
        return false;
    }
    if(refresh.empty())CredDeleteW(refresh_target,CRED_TYPE_GENERIC,0);
    return true;
}
inline bool ReadLuatoolsSessionTokens(std::string* access,std::string* refresh) {
    if(!access || !refresh)return false;
    access->clear();refresh->clear();
    auto read=[](const wchar_t* target,std::string* value)->bool {
        PCREDENTIALW cred=nullptr;
        if(!CredReadW(target,CRED_TYPE_GENERIC,0,&cred))return false;
        const bool valid=cred && cred->CredentialBlob && cred->CredentialBlobSize>0 &&
                         cred->CredentialBlobSize<=8192;
        if(valid)value->assign(reinterpret_cast<const char*>(cred->CredentialBlob),
                               cred->CredentialBlobSize);
        if(cred)CredFree(cred);
        return valid;
    };
    if(!read(L"SLS Den/lua.tools/access",access))return false;
    read(L"SLS Den/lua.tools/refresh",refresh);
    return true;
}
inline void ClearLuatoolsSessionTokens() {
    CredDeleteW(L"SLS Den/lua.tools/access",CRED_TYPE_GENERIC,0);
    CredDeleteW(L"SLS Den/lua.tools/refresh",CRED_TYPE_GENERIC,0);
}
#else
inline bool HasStoredKey(const std::string&) {return false;}
inline bool ReadStoredKey(const std::string&,std::string* value) {if(value)value->clear();return false;}
inline bool StoreKey(const std::string&,const std::string&) {return false;}
inline bool StoreLuatoolsSessionTokens(const std::string&,const std::string&) {return false;}
inline bool ReadLuatoolsSessionTokens(std::string* a,std::string* r) {if(a)a->clear();if(r)r->clear();return false;}
inline void ClearLuatoolsSessionTokens() {}
#endif
} // namespace playden::sls
