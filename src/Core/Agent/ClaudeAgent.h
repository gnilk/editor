//
// 'claude' (main.ai_integration: claude): Claude Code, the 'claude' command (docs/agent-integration.md).
// - The working agent: 'claude' in the project root.
// - The question agent: 'claude --permission-mode dontAsk --allowedTools "Read Grep Glob Edit(//<root>/.goatedit/agent_qa/a_*)"
//   --session-id <uuid>' - anything not allowed up front is denied without asking: it reads, and writes only answer
//   files (§6.1, §6.3). Plan mode can't write the answer file. From 'claude --help' (2.1.331) - to confirm in a real run.
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
        std::vector<std::string> QuestionAgentCommand(const std::filesystem::path &root, const std::string &sessionId,
                                                      const std::string &model) const override;
        std::optional<std::filesystem::path> TranscriptDir(const std::filesystem::path &root) const override;
        // '<TranscriptDir>/<session id>.jsonl' - or, when that isn't there, the one in another project folder (another
        // spelling of the same path)
        std::optional<std::filesystem::path> TranscriptFile(const std::filesystem::path &root, const std::string &sessionId) const override;
        // Each assistant line's 'message.usage', once per message id (a message is written as several lines, one per
        // content block, each with the same usage)
        std::optional<Usage> UsageSince(const std::filesystem::path &transcript, uintmax_t fromByte) const override;
        std::string PromptWithAnswerFile(const std::string &question, const std::filesystem::path &answerFile) const override;

        // Not trusted in 'root' (nor a folder above it) - its config's 'projects.<dir>.hasTrustDialogAccepted'
        std::string StartBlocker(const std::filesystem::path &root) const override;
        // Trusted in 'root' or a folder above it, by the config JSON in 'configFile' (~/.claude.json)
        static bool IsTrusted(const std::filesystem::path &root, const std::filesystem::path &configFile);
        // '$CLAUDE_CONFIG_DIR/.claude.json', else '~/.claude.json'
        static std::filesystem::path ConfigFile();

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
