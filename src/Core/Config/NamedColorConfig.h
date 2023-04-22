//
// Created by gnilk on 31.01.23.
//

#ifndef EDITOR_NAMEDCOLORCONFIG_H
#define EDITOR_NAMEDCOLORCONFIG_H

#include "Core/ColorRGBA.h"
#include <string>
#include <unordered_map>

class NamedColorConfig {
public:
    NamedColorConfig();
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


#endif //EDITOR_NAMEDCOLORCONFIG_H
