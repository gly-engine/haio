#include <haio_cdn.hpp>

#include <iostream>
#include <string>

using Haio::Parser::Word;
using Kind = Word::Kind;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

/** the words without the output the tokenizer appends, which every query gets */
std::vector<Word> wordsOf(std::string_view query) {
    auto out = Haio::Cdn::wordsOf(query);
    if (!out.empty()) out.pop_back();
    return out;
}

Haio::Parser::Parsed parse(std::string_view query) {
    return Haio::Parser::parse(Haio::Cdn::wordsOf(query), {.bucketPrefix = true, .outputNamed = "the answer"});
}

void testCutting() {
    const auto words = wordsOf("?assets:logo.png&resize=8x4&negate&begin&end");
    check(words.size() == 5, "one word per &");
    if (words.size() != 5) return;

    check(words[0].kind == Kind::Plain && words[0].text == "assets:logo.png", "a colon makes a source");
    check(words[1].kind == Kind::Named && words[1].name == "resize" && words[1].value == "8x4",
          "an equals sign makes an option with a value");
    check(words[2].kind == Kind::Named && words[2].name == "negate" && words[2].value == "",
          "a bare word is an option with no value");
    check(words[3].kind == Kind::Open && words[4].kind == Kind::Close, "begin and end are the parenthesis");

    const auto parens = wordsOf("(&)");
    check(parens.size() == 2 && parens[0].kind == Kind::Open && parens[1].kind == Kind::Close,
          "and so are ( and ), which a query may carry unescaped");
}

/**
 * ":" and "=" both turn up inside each other, so whichever comes first decides. this
 * is the whole rule, and getting it backwards would read a brush as an option.
 */
void testWhichComesFirst() {
    const auto brush = wordsOf("text:a=b");
    check(brush.size() == 1 && brush[0].kind == Kind::Plain && brush[0].text == "text:a=b",
          "a colon before an equals sign is a source, equals sign and all");

    const auto option = wordsOf("define=png:compression-level=9");
    check(option.size() == 1 && option[0].kind == Kind::Named && option[0].name == "define"
              && option[0].value == "png:compression-level=9",
          "an equals sign before a colon is an option whose value has the colon");
}

/**
 * the cut is made on the word as it was written and the escapes are undone after, so
 * a separator somebody escaped on purpose stays a character of the value.
 */
void testEscapes() {
    const auto space = wordsOf("text:hello+world&name=a%20b");
    check(space.size() == 2, "two words");
    if (space.size() != 2) return;
    check(space[0].text == "text:hello world", "a plus is a space, the way a form writes one");
    check(space[1].value == "a b", "and so is %20");

    /**
     * which leaves a geometry, the one value that spelled a plus on purpose. it comes
     * through as a space and the geometry parsers read a space as the plus it came
     * from, so both spellings name the same rectangle; test_convert_parser checks
     * that end of it.
     */
    const auto geometry = wordsOf("crop=4x4+2+1&geometry=-3+4");
    check(geometry.size() == 2, "two words");
    if (geometry.size() != 2) return;
    check(geometry[0].value == "4x4 2 1", "a geometry's plus arrives as a space");
    check(geometry[1].value == "-3 4", "and a minus stays a minus, so the sign survives");

    const auto kept = wordsOf("name=a%2Bb");
    check(kept.size() == 1 && kept[0].value == "a+b", "a plus that has to stay one is %2B");

    const auto escaped = wordsOf("text:a%3Db");
    check(escaped.size() == 1 && escaped[0].kind == Kind::Plain && escaped[0].text == "text:a=b",
          "an escaped equals sign is a character, not a separator");

    const auto slash = wordsOf("assets:a%2Fb.png");
    check(slash.size() == 1 && slash[0].text == "assets:a/b.png", "and an escaped slash is part of the name");
}

/** the output nobody writes, which is what collects format= and name= */
void testOutput() {
    const auto whole = Haio::Cdn::wordsOf("assets:logo.png");
    check(whole.size() == 2 && whole.back().kind == Kind::Output, "the output is appended");
    check(Haio::Cdn::wordsOf("").empty(), "and an empty query gets nothing, not a bare output");
}

void testSources() {
    const auto one = parse("assets:logo.png&resize=8x4");
    check(!one.error, one.error ? "a plain request parses: " + one.error.message : "a plain request parses");
    check(one.inputs.size() == 1, "one source");
    if (one.inputs.size() == 1) {
        check(one.inputs[0].bucket == "assets", "the prefix is the bucket");
        check(one.inputs[0].path == "logo.png", "and the rest is the path");
        check(one.inputs[0].format == Haio::Format::PNG, "whose extension still names the format");
    }

    const auto brush = parse("text:oi&format=png");
    check(!brush.error && brush.inputs.empty(), "a brush is painted, so it is no input");

    const auto bare = parse("logo.png");
    check(bool(bare.error), "a source without a bucket is refused");

    const auto archive = parse("assets:pack.zip/logo.png");
    check(!archive.error && archive.inputs.size() == 1 && archive.inputs[0].path == "pack.zip/logo.png",
          "a path inside an archive is the path, slashes and all");
}

/**
 * the reason the query is read as words and not as a map: a map has no order, and
 * these two are different pictures.
 */
void testOrderIsKept() {
    const auto first = parse("assets:a.png&crop=4x4+0+0&resize=8x8&format=png");
    const auto second = parse("assets:a.png&resize=8x8&crop=4x4+0+0&format=png");
    check(!first.error && !second.error, "both orders parse");
    if (first.error || second.error) return;

    const auto kinds = [](const Haio::Parser::Parsed& parsed) {
        std::vector<Haio::TokenKind> out;
        for (const auto& step : parsed.steps) {
            if (step.kind == Haio::TokenKind::Crop || step.kind == Haio::TokenKind::Resize) out.push_back(step.kind);
        }
        return out;
    };
    check(kinds(first) == std::vector{Haio::TokenKind::Crop, Haio::TokenKind::Resize}, "crop then resize");
    check(kinds(second) == std::vector{Haio::TokenKind::Resize, Haio::TokenKind::Crop}, "resize then crop");
}

/** what is asked of the bytes after the picture is finished */
void testDelivery() {
    const auto asked = parse("assets:logo.png&encoding=base64&name=icone.png");
    check(!asked.error, "an encoding and a name parse");
    check(asked.outputEncoding == Haio::BodyEncoding::Base64, "the encoding is read");
    check(asked.outputName == "icone.png", "and so is the name");

    check(parse("assets:logo.png").outputEncoding == Haio::BodyEncoding::Identity,
          "with none asked for, the bytes are written as they are");

    const auto dashed = parse("assets:logo.png&encoding=xxd-i");
    check(bool(dashed.error), "a name no encoding answers to is refused");

    const auto refused = parse("assets:logo.png&encoding=wat");
    check(refused.error && refused.error.message.find("body encoding") != std::string::npos,
          "and the refusal says what kind of name it wanted");
}

/** an option belongs to the stage after it, here as much as on a command line */
void testOptionsComeFirst() {
    check(!parse("size=8x8&xc:red&format=png").error, "an option before the stage that takes it");
    check(bool(parse("xc:red&size=8x8&format=png").error), "and after it, there is nothing left to take it");
}

}

auto main() -> int {
    testCutting();
    testWhichComesFirst();
    testEscapes();
    testOutput();
    testSources();
    testOrderIsKept();
    testDelivery();
    testOptionsComeFirst();

    if (failures == 0) std::cout << "cdn query: ok\n";
    return failures == 0 ? 0 : 1;
}
