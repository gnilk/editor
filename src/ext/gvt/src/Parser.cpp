//
// gvt — streaming DEC/ANSI parser. State machine and terms (clear, collect, param, hook, put, unhook, dispatch)
// follow https://vt100.net/emu/dec_ansi_parser; the deviations are listed in Parser.h.
//
#include <algorithm>

#include "gvt/Parser.h"

using namespace gnilk::vt;

static constexpr uint8_t kCAN = 0x18;
static constexpr uint8_t kSUB = 0x1A;
static constexpr uint8_t kESC = 0x1B;
static constexpr uint8_t kBEL = 0x07;
static constexpr uint8_t kDEL = 0x7F;
static constexpr char32_t kReplacementChar = 0xFFFD;

Parser::Parser(IParserSink &sink) : sink(sink) {
}

void Parser::Feed(std::string_view data) {
    Feed(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

void Parser::Feed(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        Advance(data[i]);
    }
}

void Parser::Reset() {
    state = State::kGround;
    utf8Codepoint = 0;
    utf8Minimum = 0;
    utf8Remaining = 0;
    Clear();
    stringBuffer.clear();
    isStringOverflow = false;
    isStringTerminatorPending = false;
}

void Parser::Advance(uint8_t byte) {
    // "Anywhere" transitions: CAN and SUB cancel whatever is in progress, ESC starts over
    if ((byte == kCAN) || (byte == kSUB)) {
        FlushIncompleteUtf8();
        LeaveState(false);
        state = State::kGround;
        sink.Execute(byte);
        return;
    }
    if (byte == kESC) {
        FlushIncompleteUtf8();
        bool wasString = LeaveState(true);
        EnterEscape();
        isStringTerminatorPending = wasString;
        return;
    }

    switch (state) {
        case State::kGround :
            AdvanceGround(byte);
            break;
        case State::kEscape :
            AdvanceEscape(byte);
            break;
        case State::kEscapeIntermediate :
            AdvanceEscapeIntermediate(byte);
            break;
        case State::kCsiEntry :
        case State::kCsiParam :
        case State::kCsiIntermediate :
        case State::kCsiIgnore :
            AdvanceCsi(byte);
            break;
        case State::kDcsEntry :
        case State::kDcsParam :
        case State::kDcsIntermediate :
        case State::kDcsIgnore :
            AdvanceDcsHeader(byte);
            break;
        case State::kDcsPassthrough :
            AdvanceDcsPassthrough(byte);
            break;
        case State::kOscString :
            AdvanceOscString(byte);
            break;
        case State::kSosPmApcString :
            AdvanceSosPmApcString(byte);
            break;
    }
}

void Parser::AdvanceGround(uint8_t byte) {
    if (byte >= 0x80) {
        DecodeUtf8(byte);
        return;
    }
    // An ASCII byte in the middle of a multi-byte character: the character is broken
    FlushIncompleteUtf8();
    if (IsExecutable(byte)) {
        sink.Execute(byte);
    } else if (byte != kDEL) {
        sink.Print(byte);
    }
}

void Parser::AdvanceEscape(uint8_t byte) {
    if (IsExecutable(byte)) {
        sink.Execute(byte);
        return;
    }
    if ((byte == kDEL) || (byte >= 0x80)) {
        return;
    }
    if (IsIntermediate(byte)) {
        isStringTerminatorPending = false;
        Collect(byte);
        state = State::kEscapeIntermediate;
        return;
    }
    // "ESC \" right after a string is the second half of its terminator (ST), not a sequence of its own
    if (isStringTerminatorPending && (byte == '\\')) {
        isStringTerminatorPending = false;
        state = State::kGround;
        return;
    }
    isStringTerminatorPending = false;

    switch (byte) {
        case '[' :
            Clear();
            state = State::kCsiEntry;
            return;
        case ']' :
            EnterString(State::kOscString);
            return;
        case 'P' :
            Clear();
            state = State::kDcsEntry;
            return;
        case 'X' :
            stringKind = StringKind::kSos;
            EnterString(State::kSosPmApcString);
            return;
        case '^' :
            stringKind = StringKind::kPm;
            EnterString(State::kSosPmApcString);
            return;
        case '_' :
            stringKind = StringKind::kApc;
            EnterString(State::kSosPmApcString);
            return;
        default :
            break;
    }
    sink.EscDispatch({}, byte);
    state = State::kGround;
}

void Parser::AdvanceEscapeIntermediate(uint8_t byte) {
    if (IsExecutable(byte)) {
        sink.Execute(byte);
        return;
    }
    if ((byte == kDEL) || (byte >= 0x80)) {
        return;
    }
    if (IsIntermediate(byte)) {
        Collect(byte);
        return;
    }
    if (!isIntermediateOverflow) {
        sink.EscDispatch(std::string_view(intermediates, intermediateCount), byte);
    }
    state = State::kGround;
}

void Parser::AdvanceCsi(uint8_t byte) {
    if (IsExecutable(byte)) {
        sink.Execute(byte);
        return;
    }
    if ((byte == kDEL) || (byte >= 0x80)) {
        return;
    }

    switch (state) {
        case State::kCsiEntry :
            if (IsPrivateMarker(byte)) {
                prefix = byte;
                state = State::kCsiParam;
                return;
            }
            [[fallthrough]];
        case State::kCsiParam :
            if (IsParamByte(byte)) {
                Param(byte);
                state = State::kCsiParam;
            } else if (IsPrivateMarker(byte)) {
                // A marker after the first parameter byte is malformed
                state = State::kCsiIgnore;
            } else if (IsIntermediate(byte)) {
                Collect(byte);
                state = State::kCsiIntermediate;
            } else {
                DispatchCsi(byte);
                state = State::kGround;
            }
            return;
        case State::kCsiIntermediate :
            if (IsIntermediate(byte)) {
                Collect(byte);
            } else if (byte < 0x40) {
                // Parameter bytes after an intermediate are malformed
                state = State::kCsiIgnore;
            } else {
                DispatchCsi(byte);
                state = State::kGround;
            }
            return;
        case State::kCsiIgnore :
            if (IsFinal(byte)) {
                state = State::kGround;
            }
            return;
        default :
            return;
    }
}

void Parser::AdvanceDcsHeader(uint8_t byte) {
    // C0 controls are ignored inside a DCS header (not executed, unlike CSI)
    if ((byte < 0x20) || (byte == kDEL) || (byte >= 0x80)) {
        return;
    }

    switch (state) {
        case State::kDcsEntry :
            if (IsPrivateMarker(byte)) {
                prefix = byte;
                state = State::kDcsParam;
                return;
            }
            [[fallthrough]];
        case State::kDcsParam :
            if (IsParamByte(byte)) {
                Param(byte);
                state = State::kDcsParam;
            } else if (IsPrivateMarker(byte)) {
                state = State::kDcsIgnore;
            } else if (IsIntermediate(byte)) {
                Collect(byte);
                state = State::kDcsIntermediate;
            } else {
                HookDcs(byte);
            }
            return;
        case State::kDcsIntermediate :
            if (IsIntermediate(byte)) {
                Collect(byte);
            } else if (byte < 0x40) {
                state = State::kDcsIgnore;
            } else {
                HookDcs(byte);
            }
            return;
        default :
            // kDcsIgnore - everything up to the terminator is dropped
            return;
    }
}

void Parser::AdvanceDcsPassthrough(uint8_t byte) {
    if (byte == kDEL) {
        return;
    }
    sink.DcsPut(byte);
}

void Parser::AdvanceOscString(uint8_t byte) {
    // xterm: BEL ends an OSC as well as ST does
    if (byte == kBEL) {
        if (!isStringOverflow) {
            sink.OscDispatch(stringBuffer);
        }
        stringBuffer.clear();
        state = State::kGround;
        return;
    }
    if ((byte < 0x20) || (byte == kDEL)) {
        return;
    }
    // Bytes >= 0x80 are payload - a title or a path is UTF-8
    if (stringBuffer.size() >= kMaxStringLength) {
        isStringOverflow = true;
        return;
    }
    stringBuffer.push_back(static_cast<char>(byte));
}

void Parser::AdvanceSosPmApcString(uint8_t byte) {
    if ((stringKind != StringKind::kApc) || (byte < 0x20) || (byte == kDEL)) {
        return;
    }
    if (stringBuffer.size() >= kMaxStringLength) {
        isStringOverflow = true;
        return;
    }
    stringBuffer.push_back(static_cast<char>(byte));
}

void Parser::DecodeUtf8(uint8_t byte) {
    if (utf8Remaining > 0) {
        if ((byte & 0xC0) == 0x80) {
            utf8Codepoint = (utf8Codepoint << 6) | (byte & 0x3F);
            utf8Remaining--;
            if (utf8Remaining == 0) {
                bool isValid = (utf8Codepoint >= utf8Minimum) && (utf8Codepoint <= 0x10FFFF) &&
                               !((utf8Codepoint >= 0xD800) && (utf8Codepoint <= 0xDFFF));
                sink.Print(isValid ? utf8Codepoint : kReplacementChar);
            }
            return;
        }
        // A new lead byte cut the previous character short
        FlushIncompleteUtf8();
    }

    if ((byte >= 0xC2) && (byte <= 0xDF)) {
        utf8Codepoint = byte & 0x1F;
        utf8Remaining = 1;
        utf8Minimum = 0x80;
    } else if ((byte >= 0xE0) && (byte <= 0xEF)) {
        utf8Codepoint = byte & 0x0F;
        utf8Remaining = 2;
        utf8Minimum = 0x800;
    } else if ((byte >= 0xF0) && (byte <= 0xF4)) {
        utf8Codepoint = byte & 0x07;
        utf8Remaining = 3;
        utf8Minimum = 0x10000;
    } else {
        // A stray continuation byte, or a lead byte that can't start a valid character
        sink.Print(kReplacementChar);
    }
}

void Parser::FlushIncompleteUtf8() {
    if (utf8Remaining > 0) {
        utf8Remaining = 0;
        sink.Print(kReplacementChar);
    }
}

void Parser::EnterEscape() {
    Clear();
    state = State::kEscape;
}

void Parser::EnterString(State newState) {
    stringBuffer.clear();
    isStringOverflow = false;
    state = newState;
}

bool Parser::LeaveState(bool isTerminated) {
    switch (state) {
        case State::kOscString :
            if (isTerminated && !isStringOverflow) {
                sink.OscDispatch(stringBuffer);
            }
            stringBuffer.clear();
            return true;
        case State::kDcsPassthrough :
            sink.DcsUnhook();
            return true;
        case State::kDcsIgnore :
            return true;
        case State::kSosPmApcString :
            if (isTerminated && (stringKind == StringKind::kApc) && !isStringOverflow) {
                sink.ApcDispatch(stringBuffer);
            }
            stringBuffer.clear();
            return true;
        default :
            return false;
    }
}

void Parser::Clear() {
    intermediateCount = 0;
    isIntermediateOverflow = false;
    prefix = 0;
    params = {};
    currentParam = CsiParams::kMissing;
    hasParamBytes = false;
    isParamOverflow = false;
}

void Parser::Collect(uint8_t byte) {
    if (intermediateCount >= kMaxIntermediates) {
        isIntermediateOverflow = true;
        return;
    }
    intermediates[intermediateCount++] = static_cast<char>(byte);
}

void Parser::Param(uint8_t byte) {
    hasParamBytes = true;
    if ((byte >= '0') && (byte <= '9')) {
        if (currentParam == CsiParams::kMissing) {
            currentParam = 0;
        }
        currentParam = std::min(currentParam * 10 + (byte - '0'), CsiParams::kMaxValue);
        return;
    }
    // ';' or ':' closes the current parameter
    if (params.count >= CsiParams::kMaxParams) {
        isParamOverflow = true;
        return;
    }
    if (byte == ':') {
        params.colonMask |= (1u << params.count);
    }
    params.values[params.count++] = currentParam;
    currentParam = CsiParams::kMissing;
}

bool Parser::FinishParams() {
    if (hasParamBytes) {
        if (params.count >= CsiParams::kMaxParams) {
            isParamOverflow = true;
        } else {
            params.values[params.count++] = currentParam;
        }
    }
    return !isParamOverflow;
}

void Parser::DispatchCsi(uint8_t final) {
    if (!FinishParams() || isIntermediateOverflow) {
        return;
    }
    sink.CsiDispatch(params, std::string_view(intermediates, intermediateCount), prefix, final);
}

void Parser::HookDcs(uint8_t final) {
    if (!FinishParams() || isIntermediateOverflow) {
        state = State::kDcsIgnore;
        return;
    }
    sink.DcsHook(params, std::string_view(intermediates, intermediateCount), prefix, final);
    state = State::kDcsPassthrough;
}

// C0 controls executed in ground / escape / CSI states (CAN, SUB and ESC are handled before the state switch)
bool Parser::IsExecutable(uint8_t byte) {
    return (byte <= 0x17) || (byte == 0x19) || ((byte >= 0x1C) && (byte <= 0x1F));
}

bool Parser::IsIntermediate(uint8_t byte) {
    return (byte >= 0x20) && (byte <= 0x2F);
}

// Digits, ':' and ';'
bool Parser::IsParamByte(uint8_t byte) {
    return (byte >= 0x30) && (byte <= 0x3B);
}

bool Parser::IsPrivateMarker(uint8_t byte) {
    return (byte >= 0x3C) && (byte <= 0x3F);
}

bool Parser::IsFinal(uint8_t byte) {
    return (byte >= 0x40) && (byte <= 0x7E);
}
