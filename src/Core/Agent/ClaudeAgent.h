//
// 'claude' (main.ai_integration: claude): Claude Code, the 'claude' command (docs/agent-integration.md).
// - The working agent: 'claude' in the project root.
// - The question agent: 'claude --permission-mode plan' - plan mode reads, never edits (§6.1). Whether its answer file
//   can be its one write is open (§8); until then the answer is its reply in the tab.
// - Transcripts: '<config dir>/projects/<root, every character not a letter or digit as '-'>/<session id>.jsonl', the
//   config dir '$CLAUDE_CONFIG_DIR' or '~/.claude'.
//

#ifndef GEDIT_CLAUDEAGENT_H
#define GEDIT_CLAUDEAGENT_H

#include "AgentIntegration.h"

namespace gedit {
    class ClaudeAgent : public AgentIntegration {
    public:
        ClaudeAgent() = default;
        virtual ~ClaudeAgent() = default;

        static AgentIntegration::Ref Create() {
            return std::make_shared<ClaudeAgent>();
        }

        std::string Name() const override {
            return NAME;
        }
        std::string DisplayName() const override {
            return "Claude Code";
        }
        bool IsAvailable() const override;
        std::string Status() const override;
        std::vector<std::string> WorkingAgentCommand(const std::filesystem::path &root) const override;
        std::vector<std::string> QuestionAgentCommand(const std::filesystem::path &root) const override;
        std::optional<std::filesystem::path> TranscriptDir(const std::filesystem::path &root) const override;
        std::string PromptWithAnswerFile(const std::string &question, const std::filesystem::path &answerFile) const override;

        // '/Users/a/src/my.proj' -> '-Users-a-src-my-proj' - the folder name Claude Code gives a project
        static std::string ProjectFolderName(const std::filesystem::path &root);
        // '$CLAUDE_CONFIG_DIR', else '~/.claude' (empty when neither is known)
        static std::filesystem::path ConfigDir();

    public:
        inline static const std::string NAME = "claude";
        inline static const std::string COMMAND = "claude";
    };
}

#endif //GEDIT_CLAUDEAGENT_H
