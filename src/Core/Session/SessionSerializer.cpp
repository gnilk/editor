//
// Created by gnilk on 15.06.26.
//

#include <yaml-cpp/yaml.h>

#include "Core/Session/SessionSerializer.h"

using namespace gedit;

// View mode <-> readable string (the session file is meant to be inspectable / hand-editable).
static const char *ViewModeToString(DocumentViewMode mode) {
    return (mode == DocumentViewMode::kHex) ? "hex" : "text";
}
static DocumentViewMode ViewModeFromString(const std::string &s) {
    return (s == "hex") ? DocumentViewMode::kHex : DocumentViewMode::kText;
}

static YAML::Node DocumentToNode(const DocumentSession &doc) {
    YAML::Node node;
    node["path"] = doc.path;
    node["cursorX"] = doc.cursorX;
    node["cursorY"] = doc.cursorY;
    node["wantedColumn"] = doc.wantedColumn;
    node["idxActiveLine"] = doc.idxActiveLine;
    node["viewTopLine"] = doc.viewTopLine;
    node["viewBottomLine"] = doc.viewBottomLine;
    node["viewMode"] = ViewModeToString(doc.viewMode);

    YAML::Node sel;
    sel["active"] = doc.selection.isActive;
    sel["startX"] = doc.selection.startX;
    sel["startY"] = doc.selection.startY;
    sel["endX"] = doc.selection.endX;
    sel["endY"] = doc.selection.endY;
    node["selection"] = sel;
    return node;
}

static DocumentSession NodeToDocument(const YAML::Node &node) {
    DocumentSession doc;
    doc.path = node["path"].as<std::string>("");
    doc.cursorX = node["cursorX"].as<int>(0);
    doc.cursorY = node["cursorY"].as<int>(0);
    doc.wantedColumn = node["wantedColumn"].as<int>(0);
    doc.idxActiveLine = node["idxActiveLine"].as<std::size_t>(0);
    doc.viewTopLine = node["viewTopLine"].as<int>(0);
    doc.viewBottomLine = node["viewBottomLine"].as<int>(0);
    doc.viewMode = ViewModeFromString(node["viewMode"].as<std::string>("text"));

    const auto &sel = node["selection"];
    if (sel) {
        doc.selection.isActive = sel["active"].as<bool>(false);
        doc.selection.startX = sel["startX"].as<int>(0);
        doc.selection.startY = sel["startY"].as<int>(0);
        doc.selection.endX = sel["endX"].as<int>(0);
        doc.selection.endY = sel["endY"].as<int>(0);
    }
    return doc;
}

static YAML::Node LayoutToNode(const LayoutSession &layout) {
    YAML::Node node;

    YAML::Node window;
    window["x"] = layout.window.x;
    window["y"] = layout.window.y;
    window["width"] = layout.window.width;
    window["height"] = layout.window.height;
    node["window"] = window;

    YAML::Node splitters;
    for (const auto &s : layout.splitters) {
        YAML::Node sn;
        sn["id"] = s.id;
        sn["absolute"] = s.absolute;
        sn["relative"] = s.relative;
        splitters.push_back(sn);
    }
    node["splitters"] = splitters;

    node["focusedTopView"] = layout.focusedTopView;
    return node;
}

static LayoutSession NodeToLayout(const YAML::Node &node) {
    LayoutSession layout;
    if (!node) {
        return layout;
    }
    const auto &window = node["window"];
    if (window) {
        layout.window.x = window["x"].as<int>(0);
        layout.window.y = window["y"].as<int>(0);
        layout.window.width = window["width"].as<int>(0);
        layout.window.height = window["height"].as<int>(0);
    }
    const auto &splitters = node["splitters"];
    if (splitters && splitters.IsSequence()) {
        for (const auto &sn : splitters) {
            SplitterSession s;
            s.id = sn["id"].as<std::string>("");
            s.absolute = sn["absolute"].as<int>(0);
            s.relative = sn["relative"].as<float>(0.0f);
            layout.splitters.push_back(s);
        }
    }
    layout.focusedTopView = node["focusedTopView"].as<std::string>("");
    return layout;
}

std::string SessionSerializer::ToYaml(const RootSession &session) {
    YAML::Node root;
    root["version"] = session.version;
    root["primaryRoot"] = session.primaryRoot.string();

    YAML::Node extraRoots;
    for (const auto &r : session.extraRoots) {
        extraRoots.push_back(r);
    }
    root["extraRoots"] = extraRoots;

    YAML::Node documents;
    for (const auto &doc : session.documents) {
        documents.push_back(DocumentToNode(doc));
    }
    root["documents"] = documents;

    root["activeDocumentIndex"] = session.activeDocumentIndex;

    YAML::Node expandCollapse;
    for (const auto &entry : session.expandCollapse) {
        expandCollapse[entry.first] = entry.second;
    }
    root["expandCollapse"] = expandCollapse;

    root["layout"] = LayoutToNode(session.layout);

    return YAML::Dump(root);
}

bool SessionSerializer::FromYaml(const std::string &text, RootSession &outSession) {
    YAML::Node root;
    try {
        root = YAML::Load(text);
    } catch (const std::exception &) {
        return false;   // malformed YAML -> start clean
    }
    if (!root || !root.IsMap()) {
        return false;
    }

    int version = root["version"].as<int>(0);
    // Unknown/newer format -> ignore the file rather than mis-parse it (never crash).
    if ((version <= 0) || (version > RootSession::kVersion)) {
        return false;
    }

    RootSession session;
    session.version = version;
    session.primaryRoot = root["primaryRoot"].as<std::string>("");

    if (const auto &extraRoots = root["extraRoots"]) {
        for (const auto &r : extraRoots) {
            session.extraRoots.push_back(r.as<std::string>(""));
        }
    }

    if (const auto &documents = root["documents"]) {
        for (const auto &dn : documents) {
            session.documents.push_back(NodeToDocument(dn));
        }
    }

    session.activeDocumentIndex = root["activeDocumentIndex"].as<int>(-1);

    if (const auto &expandCollapse = root["expandCollapse"]) {
        for (auto it = expandCollapse.begin(); it != expandCollapse.end(); ++it) {
            session.expandCollapse[it->first.as<std::string>()] = it->second.as<bool>(false);
        }
    }

    session.layout = NodeToLayout(root["layout"]);

    outSession = session;
    return true;
}
