//
// FolderMonitor on Windows - for now the base, which never starts (docs/windows-port.md §4.7, phase 3:
// ReadDirectoryChangesW).
//

#include "Core/FolderMonitor.h"

using namespace gedit;

FolderMonitor::Ref FolderMonitor::Create() {
    return std::make_shared<FolderMonitor>();
}
