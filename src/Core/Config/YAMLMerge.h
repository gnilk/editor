//
// Created by gnilk on 07.08.23.
//

#ifndef EDITOR_YAMLMERGE_H
#define EDITOR_YAMLMERGE_H

#include <yaml-cpp/yaml.h>

namespace gedit {
    class YAMLMerge {
    public:
        static void MergeNode(YAML::Node target, YAML::Node const& source);
    private:
        static void MergeMap(YAML::Node target, YAML::Node const& source);
        static void MergeSequences(YAML::Node target, YAML::Node const& source);
    };
}


#endif //EDITOR_YAMLMERGE_H
