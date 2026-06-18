#include <haio_convert.hpp>

#include <cassert>
#include <string>
#include <vector>

namespace {

Haio::Convert::Command parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) argv.push_back(arg.data());
    return Haio::Convert::parseArgs(static_cast<int>(argv.size()), argv.data());
}

}

int main() {
    using enum Haio::Convert::TokenType;

    {
        auto cmd = parse({"convert", "input.png", "-crop", "output.ppm"});
        assert(!cmd.error);
        assert(cmd.tokens.size() == 3);
        assert(cmd.tokens[0].type == InputFile);
        assert(cmd.tokens[1].type == FilterCrop);
        assert(!cmd.tokens[1].rect);
        assert(cmd.tokens[2].type == OutputFile);
        assert(cmd.inputFormat == Haio::Format::PNG);
        assert(cmd.outputFormat == Haio::Format::PPM);
    }
    {
        auto cmd = parse({"convert", "input.png", "-crop", "10x10+5+6", "output.ppm"});
        assert(!cmd.error);
        assert(cmd.tokens[1].type == FilterCrop);
        assert(cmd.tokens[1].rect);
        assert(cmd.tokens[1].rect->width == 10);
        assert(cmd.tokens[1].rect->height == 10);
        assert(cmd.tokens[1].rect->x == 5);
        assert(cmd.tokens[1].rect->y == 6);
    }
    {
        auto cmd = parse({"convert", "input.png", "--crop", "1,2,3,4", "--resize", "8x9", "--radius=2", "out.png"});
        assert(!cmd.error);
        assert(cmd.tokens.size() == 5);
        assert(cmd.tokens[1].rect->x == 1);
        assert(cmd.tokens[1].rect->y == 2);
        assert(cmd.tokens[1].rect->width == 3);
        assert(cmd.tokens[1].rect->height == 4);
        assert(cmd.tokens[2].type == FilterResize);
        assert(cmd.tokens[2].size->width == 8);
        assert(cmd.tokens[2].size->height == 9);
        assert(cmd.tokens[3].type == FilterRadius);
        assert(cmd.tokens[3].radius == 2);
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    {
        auto cmd = parse({"convert", "-size", "512x512", "xc:white", "-fx", "j/h", "out.png"});
        assert(!cmd.error);
        assert(cmd.hasGenerator);
        assert(cmd.tokens[0].type == GeneratorXc);
        assert(cmd.tokens[0].value == "white");
        assert(cmd.tokens[0].arg == "512x512");
        assert(cmd.tokens[1].type == FilterFx);
        assert(cmd.tokens[1].value == "j/h");
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    {
        auto cmd = parse({"convert", "input.png", "--format", "png", "out.bin"});
        assert(!cmd.error);
        assert(cmd.outputPath == "out.bin");
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    {
        auto cmd = parse({"convert", "PNG:-", "PPM:-"});
        assert(!cmd.error);
        assert(cmd.inputPath == "-");
        assert(cmd.outputPath == "-");
        assert(cmd.outputIsStdout);
        assert(cmd.inputFormat == Haio::Format::PNG);
        assert(cmd.outputFormat == Haio::Format::PPM);
    }
    {
        auto cmd = parse({"convert", "foo:bar.png", "foo:out.ppm"});
        assert(!cmd.error);
        assert(cmd.inputPath == "foo:bar.png");
        assert(cmd.outputPath == "foo:out.ppm");
        assert(cmd.inputFormat == Haio::Format::PNG);
        assert(cmd.outputFormat == Haio::Format::PPM);
    }
    {
        auto cmd = parse({"convert", "png:", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "png:");
    }
    {
        auto cmd = parse({"convert", "input.png", "-crop", "10x", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "10x");
    }
    {
        auto cmd = parse({"convert", "-size", "512x512", "-crop", "xc:white", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "512x512");
    }
    {
        auto cmd = parse({"convert", "input.png", "-wat", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "-wat");
    }

    return 0;
}
