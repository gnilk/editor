//
// See AgentQuestion.h
//

#include <ctime>

#include <fmt/format.h>

#include "AgentQuestion.h"

using namespace gedit;

AgentQuestion::AgentQuestion(std::string newId, std::string newQuestion, const std::filesystem::path &qaFolder) :
        id(std::move(newId)),
        question(std::move(newQuestion)) {
    questionFile = qaFolder / ("q_" + id + ".md");
    answerFile = qaFolder / ("a_" + id + ".md");
}

std::string AgentQuestion::MakeId(std::chrono::system_clock::time_point when, int number) {
    auto time = std::chrono::system_clock::to_time_t(when);
    std::tm local = {};
    localtime_r(&time, &local);
    char stamp[32] = {};
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    return fmt::format("{}-{}", stamp, number);
}

std::filesystem::path AgentQuestion::QAFolder(const std::filesystem::path &root) {
    return root / ".goatedit" / "agent_qa";
}

std::string AgentQuestion::QuestionFileText(const std::string &agentName, const std::string &sessionId, const std::string &askedAt) const {
    std::string text = "---\n";
    text += "id: " + id + "\n";
    text += "asked: " + askedAt + "\n";
    text += "agent: " + agentName + "\n";
    if (!sessionId.empty()) {
        text += "session: " + sessionId + "\n";
    }
    text += "answer: " + answerFile.filename().string() + "\n";
    text += "---\n\n";
    text += "# Question\n\n" + question + "\n\n";
    text += "## Prompt as sent\n\n```text\n" + prompt + "\n```\n";
    return text;
}

std::string AgentQuestion::UsageText(const AgentIntegration::Usage &usage) {
    std::string text = "\n## Usage\n\n";
    if (!usage.model.empty()) {
        text += "- model: " + usage.model + "\n";
    }
    text += fmt::format("- model calls: {}\n", usage.numCalls);
    text += fmt::format("- input tokens: {} (+ {} written to the cache, {} read from it)\n", usage.inputTokens,
                        usage.cacheWriteTokens, usage.cacheReadTokens);
    text += fmt::format("- output tokens: {}\n", usage.outputTokens);
    return text;
}

// 31234 -> '31k', 950 -> '950'
static std::string Kilo(uint64_t count) {
    return (count >= 10000) ? fmt::format("{}k", (count + 500) / 1000) : fmt::format("{}", count);
}

std::string AgentQuestion::UsageSummary(const AgentIntegration::Usage &usage) {
    return fmt::format("in {} + {} cached, out {}", Kilo(usage.inputTokens + usage.cacheWriteTokens),
                       Kilo(usage.cacheReadTokens), Kilo(usage.outputTokens));
}

bool AgentQuestion::IsDone() const {
    return (state == kState::Answered) || (state == kState::NoAnswer) || (state == kState::Cancelled);
}

void AgentQuestion::Accept(Clock::time_point now) {
    if (state != kState::Consent) {
        return;
    }
    state = kState::WaitingToSend;
    acceptedAt = now;
}

void AgentQuestion::Decline() {
    if (state == kState::Consent) {
        state = kState::Cancelled;
    }
}

AgentQuestion::kStep AgentQuestion::Update(const Signals &signals, std::chrono::milliseconds settleTime) {
    switch (state) {
        case kState::WaitingToSend :
            if (!signals.isTabOpen || (!signals.isAgentUp && ((signals.now - acceptedAt) > START_TIMEOUT))) {
                state = kState::Cancelled;
                return kStep::Cancelled;
            }
            if (signals.isAgentUp && signals.isSettled && !signals.isTabFocused) {
                state = kState::Sent;
                sentAt = signals.now;
                return kStep::Send;
            }
            return kStep::None;
        case kState::Sent :
            if (!signals.isTabOpen || !signals.isAgentUp) {
                // The agent stopped - whatever it wrote is the answer
                state = signals.hasAnswerFile ? kState::Answered : kState::NoAnswer;
                return signals.hasAnswerFile ? kStep::Answered : kStep::NoAnswer;
            }
            if (!signals.isSettled || ((signals.now - sentAt) < settleTime)) {
                return kStep::None;
            }
            state = signals.hasAnswerFile ? kState::Answered : kState::NoAnswer;
            return signals.hasAnswerFile ? kStep::Answered : kStep::NoAnswer;
        default:
            return kStep::None;
    }
}
