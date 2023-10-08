//
// Created by gnilk on 08.10.23.
//

#ifndef GEDIT_WINDOWLOCATION_H
#define GEDIT_WINDOWLOCATION_H

#include "Core/Config/ConfigNode.h"

namespace gedit {
    class WindowLocation : public ConfigNode {
    public:
        static const int default_xpos = 0;
        static const int default_ypos = 0;
        static const int default_width = 1920;
        static const int default_height = 1080;
    public:
        WindowLocation();
        bool Load();
        void Save();
        void SetDefaults();
        //
        // Setters
        //
        void SetXPos(int newXpos) {
            SetInt("screen_xpos", newXpos);
        }
        void SetYPos(int newYpos) {
            SetInt("screen_ypos", newYpos);
        }
        void SetWidth(int newWidth) {
            SetInt("screen_width", newWidth);
        }
        void SetHeight(int newHeight) {
            SetInt("screen_height", newHeight);
        }

        //
        // Getters
        //
        int XPos() {
            return GetInt("screen_xpos", default_xpos);
        }
        int YPos() {
            return GetInt("screen_ypos", default_ypos);
        }
        int Width() {
            return GetInt("screen_width", default_width);
        }
        int Height() {
            return GetInt("screen_height", default_height);
        }
    };
}

#endif //GEDIT_WINDOWLOCATION_H
