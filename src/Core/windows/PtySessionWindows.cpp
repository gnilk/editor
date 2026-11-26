//
// PtySession on Windows - for now a stub: no shell, the panel says so (docs/windows-port.md §7, phase 1). ConPTY over the
// shipped conpty.dll comes in phase 4 (§4.5.1) as 'ConPty' in this directory.
//

#include "Core/PtySession.h"

namespace gedit {
    class NoPty : public PtySession {
    public:
        bool Begin(const std::string &shell, const std::string &args, const std::vector<std::string> &initScript) override {
            static const std::string message = "(no terminal on Windows yet - it comes with ConPTY)\r\n";
            if (onOutput != nullptr) {
                onOutput((const uint8_t *)message.data(), message.size());
            }
            return false;
        }
        void SetWindowSize(int cols, int rows) override {
        }
        int SendCmd(std::u32string &cmd) override {
            return 0;
        }
        int Write(uint8_t chr) override {
            return 0;
        }
        int WriteBytes(const std::string &data) override {
            return 0;
        }
        bool IsForegroundJobRunning() const override {
            return false;
        }
        std::string GetWorkingDirectory() const override {
            return {};
        }
        void Close() override {
            SetState(State::kTerminated);
        }
    };
}

using namespace gedit;

PtySession::Ref PtySession::Create() {
    return std::make_shared<NoPty>();
}
