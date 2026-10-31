// Windows host identity smoke test: cryptographic IDs and no PS5 root.
#include "platform/random_id.h"
#include "platform/ps5/title_allow_list.h"
#include <iostream>
#include <set>
int main() {
 std::set<std::string> seen;
 for(int i=0;i<256;++i) {
   auto id=playden::RandomUuid();
   if(!id || id->size()!=36 || (*id)[14]!='4' || ((*id)[19]!='8' && (*id)[19]!='9' && (*id)[19]!='a' && (*id)[19]!='b') || !seen.insert(*id).second) return 1;
 }
 bool ran=false;
 if(playden::ps5::WithRoot([&]{ran=true;return true;}) || ran) return 2;
 if(!playden::ps5::HoldEffectiveUid())return 3;
 std::cout << "PASS Windows CNG UUID and no-root policy\n";
}
