//
// Created by gnilk on 15.06.26.
//
// DocumentViewMode — how the active document is presented in the view. Lives in its own leaf header so
// both DocumentViewState (the live state) and the session DTOs (Core/Session/SessionState.h) can depend
// on it without the two headers having to include each other.
//

#ifndef EDITOR_DOCUMENTVIEWMODE_H
#define EDITOR_DOCUMENTVIEWMODE_H

namespace gedit {
    // The caret (lineCursor) stays in canonical text coordinates regardless; Hex mode renders the same
    // buffer as an offset|hex|ASCII projection (see HexProjection / ByteStreamReader). Per-view, so two
    // views of one document can differ.
    enum class DocumentViewMode {
        kText,
        kHex,
    };
}

#endif //EDITOR_DOCUMENTVIEWMODE_H
