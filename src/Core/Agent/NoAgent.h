//
// 'none' (main.ai_integration: none, the default): no agent. The editor as it was - Unaware (docs/agent-integration.md §2).
// Every answer says there is nothing: no command, no transcripts, no prompt.
//

#ifndef GEDIT_NOAGENT_H
#define GEDIT_NOAGENT_H

#include "AgentIntegration.h"

namespace gedit {
    class NoAgent : public AgentIntegration {
    public:
        NoAgent() = default;
        virtual ~NoAgent() = default;

        static AgentIntegration::Ref Create() {
            return std::make_shared<NoAgent>();
        }

        std::string Name() const override {
            return NAME;
        }
        std::string DisplayName() const override {
            return {};
        }
        bool IsAvailable() const override {
            return false;
        }
        std::string Status() const override {
            return "none - no agent (main.ai_integration: none)";
        }
        std::vector<std::string> WorkingAgentCommand(const std::filesystem::path &) const override {
            return {};
        }
        std::vector<std::string> QuestionAgentCommand(const std::filesystem::path &, const std::string &) const override {
            return {};
        }
        std::optional<std::filesystem::path> TranscriptDir(const std::filesystem::path &) const override {
            return {};
        }
        std::string PromptWithAnswerFile(const std::string &, const std::filesystem::path &) const override {
            return {};
        }

    public:
        inline static const std::string NAME = "none";
    };
}

#endif //GEDIT_NOAGENT_H
