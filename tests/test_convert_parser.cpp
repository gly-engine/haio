#include <haio_cli.hpp>

#include <cassert>
#include <string>
#include <vector>

namespace {

Haio::Cli::Command parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) argv.push_back(arg.data());
    return Haio::Cli::parseArgs(static_cast<int>(argv.size()), argv.data());
}

}

int main() {
    using enum Haio::Cli::TokenType;

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
        auto cmd = Haio::Cli::parseCommandLine(R"cmd(convert input.png -fx "u*v + 0.2*sin(pi*u)" output.ppm)cmd");
        assert(!cmd.error);
        assert(cmd.tokens[1].type == FilterFx);
        assert(cmd.tokens[1].value == "u*v + 0.2*sin(pi*u)");
    }
    {
        auto args = Haio::Cli::lexCommandLine(R"(convert background.jpg \( foreground.png -resize 800x \) output.png)");
        assert(args.size() == 8);
        assert(args[2] == "(");
        assert(args[5] == "800x");
        assert(args[6] == ")");
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
        auto cmd = parse({"convert", "input.png", "--resize", "8", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "8");
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

    /**
     * the spellings come from one table now, which is the whole point of having one:
     * "--size" was in the grammar and in the usage text and in neither branch of the
     * parser, so it was documented and rejected at the same time.
     */
    {
        auto cmd = parse({"convert", "input.png", "--size", "8x9", "out.png"});
        assert(!cmd.error);
        assert(cmd.tokens[1].type == FilterResize);
        assert(cmd.tokens[1].size->width == 8);
        assert(cmd.tokens[1].size->height == 9);
    }

    /**
     * and a value stuck on with an equals sign belongs to whichever spelling it was
     * stuck to. this used to be matched against the canonical one only, so
     * "--palette=cga" found no value there and took the next word instead -- which was
     * the output path.
     */
    {
        auto cmd = parse({"convert", "input.png", "-filter=bayer", "--palette=cga", "out.png"});
        assert(!cmd.error);
        assert(cmd.tokens.size() == 3);
        assert(cmd.tokens[1].type == FilterPalette);
        assert(cmd.tokens[1].value == "cga");
        assert(cmd.tokens[1].dither == Haio::Dither::Bayer);
        assert(cmd.outputPath == "out.png");
    }

    // every spelling of an option is the same option, however many it has
    for (const auto* spelling : {"-pix_fmt", "--pix_fmt", "-pix_format", "--pix_format"}) {
        auto cmd = parse({"convert", "input.png", spelling, "bgr888", "out.tga"});
        assert(!cmd.error);
        assert(cmd.outputColor == Haio::Color::BGR888);
        assert(cmd.outputFormat == Haio::Format::TGA);
    }
    {
        auto cmd = parse({"convert", "input.png", "-pix_fmt=yuv420p", "out.tga"});
        assert(!cmd.error);
        assert(cmd.outputColor == Haio::Color::YUV420);
    }
    {
        auto cmd = parse({"convert", "input.png", "-pix_fmt", "nonsense", "out.tga"});
        assert(cmd.error);
        assert(cmd.error.token == "nonsense");
    }

    // an option with nothing after it says which one, in the spelling it was given
    {
        auto cmd = parse({"convert", "input.png", "out.png", "--resize"});
        assert(cmd.error);
        assert(cmd.error.token == "--resize");
    }

    return 0;
}
