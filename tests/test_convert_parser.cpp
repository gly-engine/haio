#include <haio_cli.hpp>

#include <cassert>
#include <string>
#include <vector>

namespace {

/** the transforms and merges, without the sources and parentheses between them */
std::vector<Haio::Token> transformsOf(const Haio::Cli::Command& cmd) {
    std::vector<Haio::Token> out;
    for (const auto& token : cmd.steps) {
        using enum Haio::TokenKind;
        if (token.kind != Source && token.kind != Decode && token.kind != Generate && token.kind != Open && token.kind != Close) {
            out.push_back(token);
        }
    }
    return out;
}

Haio::Cli::Command parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) argv.push_back(arg.data());
    return Haio::Cli::parseArgs(static_cast<int>(argv.size()), argv.data());
}

}

int main() {
    using enum Haio::TokenKind;

    {
        auto cmd = parse({"convert", "input.png", "-crop", "10x10+5+6", "output.ppm"});
        assert(!cmd.error);
        assert(transformsOf(cmd).size() == 1);
        assert(transformsOf(cmd)[0].kind == Crop);
        assert(transformsOf(cmd)[0].rect.width == 10);
        assert(transformsOf(cmd)[0].rect.height == 10);
        assert(transformsOf(cmd)[0].rect.x == 5);
        assert(transformsOf(cmd)[0].rect.y == 6);
        assert(cmd.inputs[0].format == Haio::Format::PNG);
        assert(cmd.outputFormat == Haio::Format::PPM);
    }
    {
        auto cmd = parse({"convert", "input.png", "--crop", "1,2,3,4", "--resize", "8x9", "--radius=2", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd).size() == 3);
        assert(transformsOf(cmd)[0].rect.x == 1);
        assert(transformsOf(cmd)[0].rect.y == 2);
        assert(transformsOf(cmd)[0].rect.width == 3);
        assert(transformsOf(cmd)[0].rect.height == 4);
        assert(transformsOf(cmd)[1].kind == Resize);
        assert(transformsOf(cmd)[1].size.width == 8);
        assert(transformsOf(cmd)[1].size.height == 9);
        assert(transformsOf(cmd)[2].kind == Radius);
        assert(transformsOf(cmd)[2].radius == 2);
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    {
        auto cmd = parse({"convert", "input.png", "-resize", "30pct", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd)[0].percent == 30);
    }
    {
        auto cmd = Haio::Cli::parseCommandLine(R"cmd(convert input.png -fx "u*v + 0.2*sin(pi*u)" output.ppm)cmd");
        assert(!cmd.error);
        assert(transformsOf(cmd)[0].kind == Fx);
        assert(transformsOf(cmd)[0].expression == "u*v + 0.2*sin(pi*u)");
    }
    {
        auto args = Haio::Cli::lexCommandLine(R"(convert background.jpg \( foreground.png -resize 800x \) output.png)");
        assert(args.size() == 8);
        assert(args[2] == "(");
        assert(args[5] == "800x");
        assert(args[6] == ")");
    }

    // -size is an option like any other, and the brush xc: is what takes it; a brush
    // paints rather than reads, so it is a step and never an input
    {
        auto cmd = parse({"convert", "-size", "512x512", "xc:white", "-fx", "j/h", "out.png"});
        assert(!cmd.error);
        assert(cmd.inputs.empty());
        assert(cmd.steps[0].kind == Generate);
        assert(cmd.steps[0].brush == Haio::Brush::Xc);
        assert(cmd.steps[0].expression == "white");
        assert(cmd.steps[0].settings.size() == 1);
        assert(cmd.steps[0].settings[0].name == "size");
        assert(cmd.steps[0].settings[0].value == "512x512");
        assert(transformsOf(cmd)[0].kind == Fx);
        assert(transformsOf(cmd)[0].expression == "j/h");
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    // and it is optional there, the way imagemagick has it
    {
        auto cmd = parse({"convert", "xc:red", "out.png"});
        assert(!cmd.error);
        assert(cmd.steps[0].kind == Generate);
        assert(cmd.steps[0].settings.empty());
    }
    // a brush answers to its own name, in any case, and to the others it declared
    {
        auto cmd = parse({"convert", "-size", "2x2", "RADIAL-GRADIENT:red-blue", "(", "canvas:red", ")", "-composite",
                          "(", "fractal:", ")", "-composite", "out.png"});
        assert(!cmd.error);
        std::vector<Haio::Brush> painted;
        for (const auto& token : cmd.steps) {
            if (token.kind == Generate) painted.push_back(token.brush);
        }
        assert((painted == std::vector{Haio::Brush::RadialGradient, Haio::Brush::Xc, Haio::Brush::Plasma}));
    }
    // and takes the options it declared and nothing else
    {
        auto cmd = parse({"convert", "-size", "8x8", "hald:2", "out.png"});
        assert(cmd.error);
        assert(cmd.error.message == "unrecognized option `-size' for hald");
    }

    // the output takes what its codec declared, and only that
    {
        auto cmd = parse({"convert", "input.png", "-quality", "80", "out.jpg"});
        assert(!cmd.error);
        assert(cmd.outputSettings.size() == 1);
        assert(cmd.outputSettings[0].name == "quality");
        assert(cmd.outputSettings[0].value == "80");
    }
    {
        auto cmd = parse({"convert", "input.png", "-quality", "80", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-quality");
        // and it says which stage it was not an option of, which imagemagick cannot
        assert(cmd.error.message == "unrecognized option `-quality' for png");
    }
    // -format settles the codec before its options are taken
    {
        auto cmd = parse({"convert", "input.png", "-quality", "80", "-format", "jpeg", "out.bin"});
        assert(!cmd.error);
        assert(cmd.outputFormat == Haio::Format::JPEG);
        assert(cmd.outputSettings[0].name == "quality");
    }
    {
        auto cmd = parse({"convert", "input.png", "-define", "png:compression-level=9", "out.png"});
        assert(!cmd.error);
        assert(cmd.outputSettings.size() == 1);
        assert(cmd.outputSettings[0].name == "png:compression-level");
        assert(cmd.outputSettings[0].value == "9");
    }
    // a -define the codec does not know was written to change something, so it is refused
    {
        auto cmd = parse({"convert", "input.png", "-define", "png:compression-level=9", "out.jpg"});
        assert(cmd.error);
        assert(cmd.error.token == "png:compression-level=9");
    }
    {
        auto cmd = parse({"convert", "input.png", "-define", "png:nonsense=1", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "png:nonsense=1");
    }
    // a define is only reached through -define, never as an option of its own
    {
        auto cmd = parse({"convert", "input.png", "-png:compression-level", "9", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-png:compression-level");
    }
    // -define may be said more than once, but each key only once
    {
        auto cmd = parse({"convert", "input.png", "-define", "png:compression-level=1", "-define",
                          "png:compression-level=2", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "png:compression-level=2");
    }

    // what a value has to be is declared with its option, and checked as it is read
    {
        auto cmd = parse({"convert", "input.png", "-quality", "0", "out.jpg"});
        assert(cmd.error);
        assert(cmd.error.token == "0");
    }
    {
        auto cmd = parse({"convert", "-size", "10", "xc:red", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "10");
    }
    {
        auto cmd = parse({"convert", "input.png", "-define", "png:compression-level=12", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "png:compression-level=12");
    }
    // a name is one of a list, in any case, and strict is one now that the enum says so
    {
        auto cmd = parse({"convert", "input.png", "-filter", "Strict", "-palette", "cga", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd)[0].dither == Haio::Dither::Strict);
    }

    // a name is a name however it is written: case, spaces, dashes and underscores aside
    for (const auto* spelled : {"Code128", "code 128", "CODE-128", "code_128"}) {
        auto cmd = parse({"convert", "-format", spelled, "code:haio", "out.png"});
        assert(!cmd.error);
        assert(cmd.steps[0].kind == Generate && cmd.steps[0].brush == Haio::Brush::Code);
    }
    {
        auto cmd = parse({"convert", "-format", "pdf417", "code:haio", "out.png"});
        assert(cmd.error);
        assert(cmd.error.message == "unrecognized barcode format `pdf417'");
    }
    // and -format before qr: is qr's, before the output the output's
    {
        auto cmd = parse({"convert", "-format", "data matrix", "qr:haio", "-format", "tga", "out.bin"});
        assert(!cmd.error);
        assert(cmd.steps[0].settings[0].value == "data matrix");
        assert(cmd.outputFormat == Haio::Format::TGA);
    }

    // -filter is for the stage after it to read, and -resize reads it too
    {
        auto cmd = parse({"convert", "input.png", "-filter", "Point", "-resize", "8x8", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd)[0].filter == Haio::ResizeFilter::Point);
    }
    {
        auto cmd = parse({"convert", "input.png", "-filter", "lanczos", "-resize", "8x8", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "lanczos");
    }

    // a file already has a size, so it takes none
    {
        auto cmd = parse({"convert", "-size", "8x8", "input.png", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-size");
    }
    // a transform before any picture has nothing to change
    {
        auto cmd = parse({"convert", "-size", "512x512", "-crop", "1x1", "xc:white", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "-crop");
    }

    {
        auto cmd = parse({"convert", "input.png", "--format", "png", "out.bin"});
        assert(!cmd.error);
        assert(cmd.outputPath == "out.bin");
        assert(cmd.outputFormat == Haio::Format::PNG);
    }
    // -format is the output's, so it has to be written right before it
    {
        auto cmd = parse({"convert", "input.png", "-format", "png", "-resize", "8x8", "out.bin"});
        assert(cmd.error);
        assert(cmd.error.token == "-format");
    }
    {
        auto cmd = parse({"convert", "PNG:-", "PPM:-"});
        assert(!cmd.error);
        assert(cmd.inputs[0].path == "-");
        assert(cmd.outputPath == "-");
        assert(cmd.outputIsStdout);
        assert(cmd.inputs[0].format == Haio::Format::PNG);
        assert(cmd.outputFormat == Haio::Format::PPM);
    }
    {
        auto cmd = parse({"convert", "foo:bar.png", "foo:out.ppm"});
        assert(!cmd.error);
        assert(cmd.inputs[0].path == "foo:bar.png");
        assert(cmd.outputPath == "foo:out.ppm");
        assert(cmd.inputs[0].format == Haio::Format::PNG);
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
    // -crop always takes a geometry now, so a bare one swallows the output
    {
        auto cmd = parse({"convert", "input.png", "-crop", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "out.ppm");
    }
    {
        auto cmd = parse({"convert", "input.png", "--resize", "8", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "8");
    }

    // an option nothing is left to take is an error, whatever it is called
    {
        auto cmd = parse({"convert", "input.png", "-wat", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "-wat");
        assert(cmd.error.message == "unrecognized option `-wat'");
    }
    // and so is one the next stage does not take
    {
        auto cmd = parse({"convert", "input.png", "-wat", "1", "-resize", "8x8", "out.ppm"});
        assert(cmd.error);
        assert(cmd.error.token == "-wat");
    }

    /**
     * "--size" used to be a spelling of -resize. two dashes are one now, so it is
     * -size, which only a brush takes.
     */
    {
        auto cmd = parse({"convert", "input.png", "--size", "8x9", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "--size");
    }

    /**
     * a value stuck on with an equals sign belongs to whichever spelling it was stuck
     * to, and an option is read by the stage that follows it.
     */
    {
        auto cmd = parse({"convert", "input.png", "-filter=bayer", "--palette=cga", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd).size() == 1);
        assert(transformsOf(cmd)[0].kind == Palette);
        assert(transformsOf(cmd)[0].palette == "cga");
        assert(transformsOf(cmd)[0].dither == Haio::Dither::Bayer);
        assert(cmd.outputPath == "out.png");
    }
    {
        auto cmd = parse({"convert", "input.png", "-limit", "sort:4", "-filter", "floyd", "-palete", "cga", "out.png"});
        assert(!cmd.error);
        assert(transformsOf(cmd)[0].limit == 4);
        assert(transformsOf(cmd)[0].limitHow == Haio::Limit::Sort);
        assert(transformsOf(cmd)[0].dither == Haio::Dither::Floyd);
    }
    // -filter is required by -palette, and written after it is too late
    {
        auto cmd = parse({"convert", "input.png", "-palette", "cga", "-filter", "bayer", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-palette");
    }
    // what -filter means is for the stage to say, so a bad one is its error
    {
        auto cmd = parse({"convert", "input.png", "-filter", "cubic", "-palette", "cga", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "cubic");
    }
    {
        auto cmd = parse({"convert", "input.png", "-filter", "bayer", "-filter", "floyd", "-palette", "cga", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-filter");
    }

    // every source puts a picture on the stack, and -composite joins two of them
    {
        auto cmd = Haio::Cli::parseCommandLine(
            R"(convert xc:blue \( -size 1x1 xc:red \) -geometry +0+0 -composite output.png)");
        assert(!cmd.error);
        assert(cmd.inputs.empty());
        assert(cmd.steps[2].kind == Generate && cmd.steps[2].expression == "red");
        assert(cmd.steps[2].settings[0].value == "1x1");
        assert(transformsOf(cmd).size() == 1);
        assert(transformsOf(cmd)[0].kind == Composite);
        assert(transformsOf(cmd)[0].gravity == Haio::Gravity::NorthWest);
    }
    // a parenthesis is not needed for that; it only scopes what changes
    {
        auto cmd = parse({"convert", "base.png", "layer.png", "-gravity", "SouthEast", "-geometry", "-3+4",
                          "-composite", "out.png"});
        assert(!cmd.error);
        assert(cmd.inputs.size() == 2);
        assert(transformsOf(cmd)[0].gravity == Haio::Gravity::SouthEast);
        assert(transformsOf(cmd)[0].rect.x == -3 && transformsOf(cmd)[0].rect.y == 4);
    }
    {
        auto cmd = parse({"convert", "a.png", "(", "b.png", "(", "c.png", "-resize", "2x2", ")", "-composite", ")",
                          "-composite", "out.png"});
        assert(!cmd.error);
        assert(cmd.inputs.size() == 3);
        assert(cmd.inputs[2].path == "c.png");
        std::vector<Haio::TokenKind> kinds;
        for (const auto& token : cmd.steps) {
            if (token.kind != Source) kinds.push_back(token.kind);
        }
        assert((kinds == std::vector<Haio::TokenKind>{Decode, Open, Decode, Open, Decode, Resize, Close, Composite,
                                                        Close, Composite}));
    }
    // file: is not a prefix haio knows, so it is part of the path like any other
    {
        auto cmd = parse({"convert", "file:a.png", "out.png"});
        assert(!cmd.error);
        assert(cmd.inputs[0].path == "file:a.png");
        assert(cmd.inputs[0].format == Haio::Format::PNG);
    }
    // the tokenizer is the command line's; what it hands over is plain words
    {
        const std::vector<std::string> args{"convert", "a.png", "(", "-resize=8x8", ")", "-geometry", "-3+4", "o.png"};
        const auto words = Haio::Cli::wordsOf(args);
        using Kind = Haio::Parser::Word::Kind;
        assert(words.size() == 7);
        assert(words[0].kind == Kind::Plain);
        assert(words[1].kind == Kind::Open);
        assert(words[2].kind == Kind::Named && words[2].name == "resize" && words[2].value == "8x8");
        assert(words[3].kind == Kind::Close);
        assert(words[5].kind == Kind::Named && words[5].text == "-3+4");
        assert(words[6].kind == Kind::Output);
    }
    // the output takes one picture, so two left over is an error rather than two files
    {
        auto cmd = parse({"convert", "a.png", "b.png", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "out.png");
    }
    {
        auto cmd = parse({"convert", "a.png", "b.png", "c.png", "-composite", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-composite");
    }
    {
        auto cmd = parse({"convert", "a.png", "-composite", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-composite");
    }
    {
        // imagemagick blames the output here too: it is where the parenthesis was still open
        auto cmd = parse({"convert", "a.png", "(", "b.png", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "out.png");
        assert(cmd.error.message == "unbalanced parenthesis `out.png'");
    }
    {
        auto cmd = parse({"convert", "a.png", ")", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == ")");
    }
    // options do not cross a parenthesis in either direction
    {
        auto cmd = parse({"convert", "a.png", "(", "b.png", "-gravity", "center", ")", "-composite", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-gravity");
    }
    {
        auto cmd = parse({"convert", "a.png", "-gravity", "center", "(", "b.png", ")", "-composite", "out.png"});
        assert(cmd.error);
        assert(cmd.error.token == "-gravity");
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
        auto cmd = parse({"convert", "input.png", "-pix_fmt", "rgb565", "-pix_format", "bgr888", "out.tga"});
        assert(cmd.error);
        assert(cmd.error.token == "-pix_format");
    }
    {
        auto cmd = parse({"convert", "input.png", "-pix_fmt", "nonsense", "out.tga"});
        assert(cmd.error);
        assert(cmd.error.token == "nonsense");
    }

    // a transform with nothing after it says which one, in the spelling it was given
    {
        auto cmd = parse({"convert", "input.png", "out.png", "--resize"});
        assert(cmd.error);
        assert(cmd.error.token == "--resize");
    }

    return 0;
}
