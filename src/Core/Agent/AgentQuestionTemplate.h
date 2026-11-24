//
// A question from a template (docs/agent-integration.md §6.4): a Markdown file, front matter + the prompt -
//
//     ---
//     name: why
//     title: Why is this line here?
//     needs: [file, line]              # filled from the active document and its caret
//     args: [base]                     # optional words after '.ask <name>', in order
//     tools:
//       chain: why.py {file}:{line}    # run in the project root; its output is {tool:chain}
//     max_lines: 300                   # each tool's output is cut there
//     ---
//     The prompt, with {file}, {tool:chain}, ... - '{{' is a '{'.
//
// Placeholders: what 'needs' and 'args' name, {root}, {goatedit} (the running editor) and {tool:<key>}. Anything else
// in braces is an error when the template is read, so a typo never reaches the agent. A tool's command is split on
// spaces first and filled after (a value with a space stays one argument; one that fills to empty is dropped) - no
// shell. Plain data: finding the tool's file and running it is the caller's (AgentAsk).
//
// Templates live in the system 'Questions/' and the user's 'questions/' - never a project's: they run commands.
//

#ifndef GEDIT_AGENTQUESTIONTEMPLATE_H
#define GEDIT_AGENTQUESTIONTEMPLATE_H

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gedit {
    class AgentQuestionTemplate {
    public:
        using Ref = std::shared_ptr<AgentQuestionTemplate>;
        using Values = std::map<std::string, std::string>;

        struct Tool {
            std::string key;
            std::vector<std::string> command;   // as written, split on spaces; [0] is the tool's name
        };
        // How a tool is started: the program and its arguments
        struct Command {
            std::string program;
            std::vector<std::string> arguments;
        };

    public:
        AgentQuestionTemplate() = default;
        virtual ~AgentQuestionTemplate() = default;

        // nullptr and 'outError' when the text isn't a template
        static Ref FromText(const std::string &text, std::string &outError);
        static Ref FromFile(const std::filesystem::path &path, std::string &outError);
        // Every '*.md' in the folders, by name; a later folder's template replaces an earlier one's (system, then
        // user). A file that isn't a template is left out, with a line in 'outErrors'.
        static std::vector<Ref> LoadAll(const std::vector<std::filesystem::path> &folders, std::vector<std::string> &outErrors);
        static Ref Find(const std::vector<Ref> &templates, const std::string &name);

        // A tool's file in the folders - the last folder that has it wins, like the templates - or empty
        static std::filesystem::path FindTool(const std::vector<std::filesystem::path> &folders, const std::string &name);
        // How to start a tool's file: a '.py' with python3, anything else as is
        static Command ToolCommand(const std::filesystem::path &toolFile, const std::vector<std::string> &arguments);
        // A tool's output for the prompt: at most 'maxLines' lines, then one saying how many were cut
        static std::string CapOutput(const std::string &output, size_t maxLines);

        const std::string &Name() const { return name; }
        const std::string &Title() const { return title; }
        const std::vector<std::string> &Needs() const { return needs; }
        const std::vector<std::string> &Args() const { return args; }
        const std::vector<Tool> &Tools() const { return tools; }
        size_t MaxLines() const { return maxLines; }
        const std::filesystem::path &Origin() const { return origin; }

        bool Needs(const std::string &what) const;
        // 'args' from the words after the name - false when there are more words than it takes
        bool ArgValues(const std::vector<std::string> &words, Values &inOutValues) const;
        // A tool's arguments (its name left out), filled
        std::vector<std::string> ToolArguments(const Tool &tool, const Values &values) const;
        // The prompt, filled - a placeholder without a value fills to empty
        std::string FillPrompt(const Values &values) const;
        // 'Why is this line here? (src/Core/Line.cpp:120)' - what the question file calls it
        std::string Describe(const Values &values) const;

    public:
        static constexpr size_t DEFAULT_MAX_LINES = 300;

    private:
        bool CheckPlaceholders(const std::string &text, std::string &outError) const;
        static std::string Fill(const std::string &text, const Values &values);

    private:
        std::string name;
        std::string title;
        std::vector<std::string> needs;
        std::vector<std::string> args;
        std::vector<Tool> tools;
        size_t maxLines = DEFAULT_MAX_LINES;
        std::string prompt;
        std::filesystem::path origin;
    };
}

#endif //GEDIT_AGENTQUESTIONTEMPLATE_H
