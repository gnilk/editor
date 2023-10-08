//
// Created by gnilk on 08.10.23.
//

#include "WindowLocation.h"
#include "XDGEnvironment.h"
#include <fstream>

using namespace gedit;

WindowLocation::WindowLocation() {
    SetDefaults();
}

void WindowLocation::SetDefaults() {
    SetInt("screen_xpos", default_xpos);
    SetInt("screen_ypos", default_ypos);
    SetInt("screen_width", default_width);
    SetInt("screen_height", default_height);
}

bool WindowLocation::Load() {
    auto pathStateDir = XDGEnvironment::Instance().GetUserStatePath();
    auto fileName = pathStateDir / "gedit_lastwinloc.yml";
    if (!exists(fileName)) {
        return false;
    }

    dataNode = YAML::LoadFile(fileName);

    return true;
}
void WindowLocation::Save() {

    auto pathStateDir = XDGEnvironment::Instance().GetUserStatePath();
    auto fileName = pathStateDir / "gedit_lastwinloc.yml";

    //printf("Saving to: %s\n", fileName.c_str());

    std::ofstream fout(fileName);
    fout << GetDataNode();
}

