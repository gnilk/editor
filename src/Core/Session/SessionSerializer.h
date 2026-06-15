//
// Created by gnilk on 15.06.26.
//
// YAML (de)serialisation for the per-root session (docs/session-cache.md §3.4 / §11.2). Deliberately a
// separate header from SessionState.h: the DTO header is included widely (down to ViewBase), and we do
// NOT want the yaml-cpp dependency leaking into the whole view layer. Only SessionManager (and the tests)
// include this.
//

#ifndef EDITOR_SESSIONSERIALIZER_H
#define EDITOR_SESSIONSERIALIZER_H

#include <string>

#include "Core/Session/SessionState.h"

namespace gedit {

    class SessionSerializer {
    public:
        // Emit a RootSession as YAML text (stable key order, top-level `version:`).
        static std::string ToYaml(const RootSession &session);

        // Parse YAML text into outSession. Returns false (caller should start clean) on a parse error or
        // an unknown/newer version. Never throws.
        static bool FromYaml(const std::string &text, RootSession &outSession);
    };

}

#endif //EDITOR_SESSIONSERIALIZER_H
