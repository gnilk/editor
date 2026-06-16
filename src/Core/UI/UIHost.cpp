//
// AI-3 (docs/ui-refactor.md §8)
//

#include "UIHost.h"

using namespace gedit;

UIHost &UIHost::Instance() {
    static UIHost glbInstance;
    return glbInstance;
}
