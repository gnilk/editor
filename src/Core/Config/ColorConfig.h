//
// Created by gnilk on 31.01.23.
//

#ifndef EDITOR_COLORCONFIG_H
#define EDITOR_COLORCONFIG_H

#include "Core/ColorRGBA.h"
#include <string>
#include <unordered_map>

class ColorConfig {
public:
    ColorConfig();
    void SetDefaults() noexcept;
    void SetColor(const std::string &name, ColorRGBA color);

    bool HasColor(const std::string &name) const {
        return (colors.find(name) != colors.end());
    }
    const ColorRGBA GetColor(const std::string &name) const {
        if (!HasColor(name)) {
            return {};
        }
        auto it = colors.find(name);
        return it->second;
    }
    const ColorRGBA operator[](const std::string &name) const {
        return GetColor(name);
    }

private:
    std::unordered_map<std::string, ColorRGBA> colors;
};


#endif //EDITOR_COLORCONFIG_H
