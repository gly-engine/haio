#include <haio_cdn.hpp>
#include <haio_url.hpp>

namespace Haio::Cdn {
namespace {

using Kind = Parser::Word::Kind;

Parser::Word wordOf(std::string_view raw) {
    const auto colon = raw.find(':');
    const auto equals = raw.find('=');

    if (colon != std::string_view::npos && (equals == std::string_view::npos || colon < equals)) {
        return {Kind::Plain, Percent::decode(raw, true)};
    }

    if (equals != std::string_view::npos) {
        auto name = Percent::decode(raw.substr(0, equals), true);
        auto value = Percent::decode(raw.substr(equals + 1), true);
        return {Kind::Named, std::string(raw), name, name, std::move(value)};
    }

    auto name = Percent::decode(raw, true);
    if (name == "begin" || name == "(") return {Kind::Open, std::move(name)};
    if (name == "end" || name == ")") return {Kind::Close, std::move(name)};

    if (name.find_first_of("./") != std::string::npos) return {Kind::Plain, std::move(name)};
    return {Kind::Named, name, name, name, std::string{}};
}

}

std::vector<Parser::Word> wordsOf(std::string_view query) {
    if (!query.empty() && query.front() == '?') query.remove_prefix(1);

    std::vector<Parser::Word> out;
    while (!query.empty()) {
        const auto amp = query.find('&');
        const auto item = query.substr(0, amp);
        if (!item.empty()) out.push_back(wordOf(item));
        if (amp == std::string_view::npos) break;
        query.remove_prefix(amp + 1);
    }

    if (!out.empty()) out.push_back({Kind::Output, "-"});
    return out;
}

}
