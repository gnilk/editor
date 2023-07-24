//
// Created by gnilk on 31.01.23.
//

#ifndef EDITOR_NAMEDCOLORS_H
#define EDITOR_NAMEDCOLORS_H

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include "Core/ColorRGBA.h"

namespace gedit {
    class NamedColors  {
    public:
        using Ref = std::shared_ptr<NamedColors>;
        // This is just a helper in order for the JS integration to work...
        struct NamedColor {
            using Ref = std::shared_ptr<NamedColor>;

            NamedColor() = default;
            NamedColor(const std::string &cname, const ColorRGBA &col) : name(cname), color(col) {

            }
            virtual ~NamedColor() = default;

            static Ref Create() {
                return std::make_shared<NamedColor>();
            }
            static Ref Create(const std::string &cname, const ColorRGBA &col) {
                return std::make_shared<NamedColor>(cname, col);
            }

            const std::string &GetName() {
                return name;
            }
            ColorRGBA *GetColor() {
                return &color;
            }

            std::string name;
            ColorRGBA color;
        };
    public:
        NamedColors() = default;

        static Ref Create() {
            return std::make_shared<NamedColors>();
        }

        void SetColor(const std::string &name, ColorRGBA color);

        void ToVector(std::vector<NamedColor> &flatVector);

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
}


#endif //EDITOR_NAMEDCOLORS_H
