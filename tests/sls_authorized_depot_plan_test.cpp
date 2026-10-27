#include "sls/authorized_depot_plan.h"
#include <cassert>
int main() {
  using namespace playden;
  sls::ManifestMetadata md;
  md.app_id = 480;
  md.depots = {{100, 999, true}, {200, 888, true}};
  steam::DepotInfo authorized;
  authorized.id = 100;
  authorized.manifest_gid = 123;
  auto plan = sls::PlanAuthorizedDepots(md, {authorized});
  assert(plan.selected.size() == 1);
  assert(plan.selected[0].id == 100);
  assert(plan.selected[0].manifest_gid && *plan.selected[0].manifest_gid == 123);
  assert(plan.unrecognized.size() == 1 && plan.unrecognized[0] == 200);
}
