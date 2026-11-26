//
// Line endings - see LineEnding.h
//

#include "LineEnding.h"
#include "Core/Config/Config.h"
#include "Core/Platform.h"

using namespace gedit;

std::optional<kLineEnding> LineEndings::FromName(std::string_view name, std::string_view platformName) {
    if ((name == "unix") || (name == "lf")) {
        return kLineEnding::kLF;
    }
    if ((name == "windows") || (name == "crlf")) {
        return kLineEnding::kCRLF;
    }
    if (name == "platform") {
        return (platformName == "windows") ? kLineEnding::kCRLF : kLineEnding::kLF;
    }
    return std::nullopt;
}

const char *LineEndings::Name(kLineEnding ending) {
    return (ending == kLineEnding::kCRLF) ? "windows" : "unix";
}

const char32_t *LineEndings::Text(kLineEnding ending) {
    return (ending == kLineEnding::kCRLF) ? U"\r\n" : U"\n";
}

kLineEnding LineEndings::Preferred() {
    auto name = Config::Instance()["main"].GetStr("line_endings", "platform");
    auto platformName = Platform::Name();
    auto preferred = FromName(name, platformName);
    if (!preferred.has_value()) {
        preferred = FromName("platform", platformName);
    }
    return *preferred;
}

std::optional<LineEndings::kMixedPolicy> LineEndings::MixedPolicyFromName(std::string_view name) {
    if (name == "ask") {
        return kMixedPolicy::kAsk;
    }
    if (name == "keep") {
        return kMixedPolicy::kKeep;
    }
    if (name == "convert") {
        return kMixedPolicy::kConvert;
    }
    return std::nullopt;
}

LineEndings::kMixedPolicy LineEndings::MixedPolicy() {
    auto name = Config::Instance()["main"].GetStr("mixed_line_endings", "ask");
    return MixedPolicyFromName(name).value_or(kMixedPolicy::kAsk);
}
