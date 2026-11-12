#include "sls/added_game_registry.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace playden::sls;
static void Check(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<"\n";std::exit(1);}}
int main(){
  const auto root=std::filesystem::temp_directory_path()/"sls-registry-v2-ci";
  std::error_code ec;
  std::filesystem::remove_all(root,ec);
  std::filesystem::create_directories(root,ec);
  Check(!ec,"temp directory");
  {std::ofstream legacy(root/"added-games.txt");
   legacy<<"620 \"Portal 2\"\n730 \"Example Game\"\n";}
  AddedGameRegistry registry;
  Check(registry.Load(root)&&registry.entries().size()==2&&
        registry.Find(620)->title=="Portal 2","legacy game migration");
  Check(registry.Upsert(620,"Portal 2")&&registry.Upsert(700,"Other Title"),
        "upsert without creating steam license");
  ManifestMetadata metadata;
  metadata.app_id=620;
  metadata.depots={{621,123456,true,4096},{622,0,false,0}};
  Check(registry.ApplyDiscovery(620,"sushi",metadata),"provider metadata registration");
  Check(registry.Save(root),"atomic versioned persistence");
  AddedGameRegistry restored;
  Check(restored.Load(root)&&restored.entries().size()==3&&restored.Find(620) &&
        restored.Find(620)->source_id=="sushi" &&
        restored.Find(620)->metadata.depots.size()==2 &&
        restored.Find(620)->metadata.depots[0].manifest_gid==123456 &&
        restored.Find(620)->metadata.depots[1].manifest_gid==0 &&
        restored.Find(620)->metadata.depots[0].declared_manifest_bytes==4096,
        "restart restores exact discovery provenance and depot IDs");
  Check(!restored.ApplyDiscovery(620,"spoof",ManifestMetadata{999,{{621,7,true}}}),
        "cross-app metadata rejected");
  Check(restored.Erase(620)&&restored.Save(root),"unregister game");
  AddedGameRegistry after_removal;
  Check(after_removal.Load(root)&&!after_removal.Find(620)&&after_removal.Find(730),
        "unregistered game does not reappear from legacy file");
  {std::ofstream corrupt(root/"added-games-v3.txt",std::ios::trunc);
   corrupt<<"SLS-ADDED-GAMES 3\n620 \"Portal 2\" \"sushi\" 1\nnot-a-depot\n";}
  Check(!after_removal.Load(root)&&!after_removal.Find(620)&&after_removal.Find(730),
        "malformed registry refuses partial mutation");
  // Existing version-2 records remain readable and upgrade on next save.
  std::filesystem::remove(root/"added-games-v3.txt",ec);
  {std::ofstream old(root/"added-games-v2.txt");
   old<<R"(SLS-ADDED-GAMES 2
620 "Portal 2" "legacy-provider" 1
621 123456 1
)";}
  AddedGameRegistry migrated;
  Check(migrated.Load(root) && migrated.Find(620) &&
        migrated.Find(620)->metadata.depots[0].declared_manifest_bytes==0 &&
        migrated.Save(root) &&
        std::filesystem::exists(root/"added-games-v3.txt"),
        "version-2 registry upgrades without losing depot metadata");
  std::filesystem::remove_all(root,ec);
  std::cout<<"PASS: added-game migration, provenance, persistence and isolation\n";
}
