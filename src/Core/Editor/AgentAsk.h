//
// '.ask' - questions to the Editor Agent, all the way round (docs/agent-integration.md §6.3): consent, start the agent
// if it doesn't run, wait for it to settle, paste the prompt and press Enter, wait for the answer file, open it. One
// question at a time; the rest wait in line. Also what the status line says about the Editor Agent (§6.2 item 6).
//
// Runs on the UI thread: a timer posts a look at the agent's tab (Tick) while a question is out or the agent runs.
// The question itself is plain data (Core/Agent/AgentQuestion) - this is the part that touches tabs, files and modals.
//

#ifndef GEDIT_AGENTASK_H
#define GEDIT_AGENTASK_H

#include <chrono>
#include <deque>
#include <string>

#include "Core/Agent/AgentQuestion.h"
#include "Core/Editor/Views/TerminalView.h"
#include "Core/Timer.h"

namespace gedit {
    class AgentAsk {
    public:
        AgentAsk() = default;
        virtual ~AgentAsk();

        // '.ask <question>': the consent modal; the rest follows when the user accepts. A line for the user.
        std::string Ask(const std::string &question);
        // '.agent start': the Editor Agent's tab - shown when it runs already. A line for the user.
        std::string StartAgent();

        // 'EA: answering' - empty while there is no Editor Agent and no question (as of the last Tick)
        const std::u32string &StatusText() const {
            return lastStatus;
        }

        // One look at the agent's tab (the timer posts it; a test may call it)
        void Tick();
        // How long the agent's output must be quiet: main.ai_settle_ms, else the agent's own
        std::chrono::milliseconds SettleTime() const;

    public:
        static constexpr std::chrono::milliseconds TICK = std::chrono::milliseconds(250);
        // The prompt in the consent modal is wrapped at this
        static constexpr size_t CONSENT_WIDTH = 76;

    private:
        void OnConsent(const AgentQuestion::Ref &question, bool isAccepted);
        bool WriteQuestionFile(const AgentQuestion &question, std::string &outError);
        // The tab with the agent's command for a new conversation - nullptr when there is no panel or command
        TerminalView *OpenAgentTab();
        void Send(TerminalView &agentView, const AgentQuestion &question);
        void OnAnswered(const AgentQuestion &question, const std::string &usage);
        uintmax_t TranscriptSize() const;
        std::string RecordUsage(const AgentQuestion &question);
        // The tab's line under the tab row: the banner, or that a question waits for the user to leave the tab
        void UpdateInfoLine(TerminalView *agentView, bool isWaitingForFocus);
        void ArmTimer();
        std::u32string ComputeStatus(const AgentQuestion::Signals &signals, bool isWaitingForFocus) const;
        std::string ConsentMessage(const AgentQuestion &question, bool willStart) const;

    private:
        std::deque<AgentQuestion::Ref> questions;
        // The Editor Agent this session started, and its conversation's id
        TerminalView *startedView = nullptr;
        std::string sessionId;
        std::string banner;
        bool isShowingWaitLine = false;
        int numberThisSecond = 0;
        std::string lastIdStamp;
        std::u32string lastStatus;
        Timer::Ref timer;
    };
}

#endif //GEDIT_AGENTASK_H
