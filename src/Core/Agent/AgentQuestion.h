//
// One question to the Editor Agent (docs/agent-integration.md §6.3): its files, its prompt and where it is on the way
// round - consent, waiting for the agent to settle, sent, answered. Plain data: what the agent and its tab look like
// comes in as Signals, what the driver must do goes out as a Step - nothing here touches a pty, a file or the UI.
//
// The question is kept with its answer: '<root>/.goatedit/agent_qa/q_<id>.md' (written by the editor before sending)
// and 'a_<id>.md' (written by the agent), the id '<yyyymmdd-hhmmss>-<n>'.
//

#ifndef GEDIT_AGENTQUESTION_H
#define GEDIT_AGENTQUESTION_H

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace gedit {
    class AgentQuestion {
    public:
        using Ref = std::shared_ptr<AgentQuestion>;
        using Clock = std::chrono::steady_clock;

        enum class kState {
            Consent,            // the modal is up
            WaitingToSend,      // accepted - the agent starts or finishes what it does
            Sent,               // in the agent's input, Enter pressed
            Answered,           // the answer file is there and the agent has settled
            NoAnswer,           // the agent settled (or stopped) without writing it
            Cancelled,          // declined, or the tab closed before it was sent
        };
        // What the driver does after Update
        enum class kStep {
            None,
            Send,               // paste the prompt into the agent and press Enter
            Answered,           // open the answer
            NoAnswer,           // say so
            Cancelled,          // say so
        };
        // What the agent's tab looks like now
        struct Signals {
            bool isTabOpen = false;         // the Editor Agent's tab exists
            bool isAgentUp = false;         // the agent runs in it (a job in the foreground, not the shell's prompt)
            bool isSettled = false;         // its input reads (bracketed paste on) and its output has been quiet
            bool isTabFocused = false;      // the user is in it - nothing is typed in under their hands
            bool hasAnswerFile = false;
            Clock::time_point now = {};
        };

    public:
        AgentQuestion(std::string newId, std::string newQuestion, const std::filesystem::path &qaFolder);
        virtual ~AgentQuestion() = default;

        static Ref Create(const std::string &id, const std::string &question, const std::filesystem::path &qaFolder) {
            return std::make_shared<AgentQuestion>(id, question, qaFolder);
        }
        // '20261124-140312-1' - the time it was asked (local) and a number that tells apart two in the same second
        static std::string MakeId(std::chrono::system_clock::time_point when, int number);
        // '<root>/.goatedit/agent_qa'
        static std::filesystem::path QAFolder(const std::filesystem::path &root);

        const std::string &Id() const { return id; }
        const std::string &Question() const { return question; }
        const std::filesystem::path &QuestionFile() const { return questionFile; }
        const std::filesystem::path &AnswerFile() const { return answerFile; }

        // What is typed into the agent - the agent's PromptWithAnswerFile
        void SetPrompt(const std::string &newPrompt) { prompt = newPrompt; }
        const std::string &Prompt() const { return prompt; }

        // q_<id>.md: a header (when, which agent, its session, the answer file), the question, the prompt as sent
        std::string QuestionFileText(const std::string &agentName, const std::string &sessionId, const std::string &askedAt) const;

        kState State() const { return state; }
        bool IsDone() const;

        // The user's answer in the consent modal
        void Accept(Clock::time_point now);
        void Decline();
        // One look at the agent's tab. 'settleTime' also holds back the verdict after sending - the agent echoes the
        // paste and starts working, so a moment of quiet right after Enter is not an answer.
        kStep Update(const Signals &signals, std::chrono::milliseconds settleTime);

    public:
        // The agent never came up in its tab (no command, a login prompt): the question is cancelled
        static constexpr std::chrono::seconds START_TIMEOUT = std::chrono::seconds(60);

    private:
        std::string id;
        std::string question;
        std::filesystem::path questionFile;
        std::filesystem::path answerFile;
        std::string prompt;
        kState state = kState::Consent;
        Clock::time_point acceptedAt = {};
        Clock::time_point sentAt = {};
    };
}

#endif //GEDIT_AGENTQUESTION_H
