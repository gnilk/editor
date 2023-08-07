//
// Created by gnilk on 07.08.23.
// Taken from: https://coryan.github.io/jaybeams/merge__yaml_8cpp_source.html
// see also: https://github.com/coryan/jaybeams/blob/master/jb/merge_yaml.cpp

#include "YAMLMerge.h"

using namespace gedit;

void YAMLMerge::MergeNode(YAML::Node target, YAML::Node const& source) {
    switch (source.Type()) {
        case YAML::NodeType::Scalar:
            target = source.Scalar();
            break;
        case YAML::NodeType::Map:
            MergeMap(target, source);
            break;
        case YAML::NodeType::Sequence:
            MergeSequences(target, source);
            break;
        case YAML::NodeType::Null:
            return; //throw std::runtime_error("MergeNode: Null source nodes not supported");
        case YAML::NodeType::Undefined:
            return; // throw std::runtime_error("MergeNode: Undefined source nodes not supported");
    }
}
void YAMLMerge::MergeMap(YAML::Node target, YAML::Node const& source) {
    for (auto const& j : source) {
        MergeNode(target[j.first.Scalar()], j.second);
    }

}
void YAMLMerge::MergeSequences(YAML::Node target, YAML::Node const& source) {
    for (std::size_t i = 0; i != source.size(); ++i) {
        if (i < target.size()) {
            MergeNode(target[i], source[i]);
        } else {
            target.push_back(YAML::Clone(source[i]));
        }
    }
}
