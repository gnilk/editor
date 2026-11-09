//
// gvt tests — records parser events as one normalized line each, so a test (or the libvterm oracle, which
// produces the same lines from its own parser callbacks) can compare event lists as text:
//
//   TEXT <utf-8>            consecutive Print calls, merged
//   C0 0d                   Execute
//   CSI ?1049;25h           prefix, parameters (missing = empty, ':' kept), intermediates, final
//   ESC (B                  intermediates, final
//   OSC 0|title             the leading number, '|', the rest after the first ';' (no number: OSC |payload)
//   DCS +q|data             header bytes as received, '|', the data bytes
//   APC payload
//
#ifndef GVT_TESTS_EVENTRECORDER_H
#define GVT_TESTS_EVENTRECORDER_H

#include <string>
#include <vector>
#include <cstdio>

#include "gvt/IParserSink.h"

namespace gvttest {

    inline void AppendUtf8(std::string &out, char32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    inline std::string FormatParams(const gnilk::vt::CsiParams &params) {
        std::string out;
        for (size_t i = 0; i < params.count; i++) {
            if (params.values[i] != gnilk::vt::CsiParams::kMissing) {
                out += std::to_string(params.values[i]);
            }
            if (i + 1 < params.count) {
                out += params.IsFollowedByColon(i) ? ":" : ";";
            }
        }
        return out;
    }

    // "133;A" -> "OSC 133|A", "112" -> "OSC 112|", "title" -> "OSC |title"
    inline std::string FormatOsc(std::string_view payload) {
        size_t numberLength = 0;
        while ((numberLength < payload.size()) && (payload[numberLength] >= '0') && (payload[numberLength] <= '9')) {
            numberLength++;
        }
        bool hasNumber = (numberLength > 0) && ((numberLength == payload.size()) || (payload[numberLength] == ';'));
        if (!hasNumber) {
            return "OSC |" + std::string(payload);
        }
        auto rest = (numberLength < payload.size()) ? payload.substr(numberLength + 1) : std::string_view{};
        return "OSC " + std::string(payload.substr(0, numberLength)) + "|" + std::string(rest);
    }

    class EventRecorder : public gnilk::vt::IParserSink {
    public:
        // libvterm drops NUL, CAN and SUB unless asked not to - the oracle comparison drops them on our side too
        bool isNulCanSubDropped = false;

        const std::vector<std::string> &Events() {
            FlushText();
            return events;
        }

        std::string Joined() {
            std::string out;
            for (auto &e : Events()) {
                out += e;
                out += "\n";
            }
            return out;
        }

        void Print(char32_t codepoint) override {
            AppendUtf8(text, codepoint);
        }

        void Execute(uint8_t control) override {
            if (isNulCanSubDropped && ((control == 0x00) || (control == 0x18) || (control == 0x1A))) {
                return;
            }
            char buf[16];
            snprintf(buf, sizeof(buf), "C0 %02x", control);
            Add(buf);
        }

        void CsiDispatch(const gnilk::vt::CsiParams &params, std::string_view intermediates, uint8_t prefix,
                         uint8_t final) override {
            std::string out = "CSI ";
            if (prefix != 0) {
                out.push_back(static_cast<char>(prefix));
            }
            out += FormatParams(params);
            out += intermediates;
            out.push_back(static_cast<char>(final));
            Add(out);
        }

        void EscDispatch(std::string_view intermediates, uint8_t final) override {
            std::string out = "ESC ";
            out += intermediates;
            out.push_back(static_cast<char>(final));
            Add(out);
        }

        void OscDispatch(std::string_view payload) override {
            Add(FormatOsc(payload));
        }

        void DcsHook(const gnilk::vt::CsiParams &params, std::string_view intermediates, uint8_t prefix,
                     uint8_t final) override {
            dcs = "DCS ";
            if (prefix != 0) {
                dcs.push_back(static_cast<char>(prefix));
            }
            dcs += FormatParams(params);
            dcs += intermediates;
            dcs.push_back(static_cast<char>(final));
            dcs += "|";
        }

        void DcsPut(uint8_t byte) override {
            dcs.push_back(static_cast<char>(byte));
        }

        void DcsUnhook() override {
            Add(dcs);
            dcs.clear();
        }

        void ApcDispatch(std::string_view payload) override {
            Add("APC " + std::string(payload));
        }

        // For the oracle: lines produced from another parser's callbacks go through the same merging
        void AddText(std::string_view utf8) {
            text += utf8;
        }

        void Add(const std::string &line) {
            FlushText();
            events.push_back(line);
        }

    private:
        void FlushText() {
            if (!text.empty()) {
                events.push_back("TEXT " + text);
                text.clear();
            }
        }

    private:
        std::vector<std::string> events;
        std::string text;
        std::string dcs;
    };

}

#endif // GVT_TESTS_EVENTRECORDER_H
