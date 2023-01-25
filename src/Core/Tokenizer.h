// See tokenzier.cpp for more details
// 14.03.14, FKling, published to github
#pragma once

#include <vector>
#include <string>

namespace gnilk
{

	class Tokenizer {
	public:
		explicit Tokenizer(const char *sInput);
		Tokenizer(const char *sInput, const char *sOperators);
		virtual ~Tokenizer() = default;

		bool HasMore() const;
		const char *Previous();
		const char *Next();
		const char *Peek() const;

		static int Case(const char *sValue, const char *sInput);

    protected:
        bool IsOperator(const char *input, int &outSzOperator);
        bool SkipWhiteSpace(char **input);
        char *GetNextToken(char *dst, int nMax, char **input);
        char *GetNextTokenNoOperator(char *dst, int nMax, char **input);
        void PrepareOperators(const char *operators);
        void PrepareTokens(const char *input);
    protected:
        std::vector<std::string> operators;
        std::vector<std::string> tokens;
        size_t iTokenIndex;

	};
}