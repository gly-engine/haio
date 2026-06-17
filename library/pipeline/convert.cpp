#include <haio.hpp>

#include <stdexcept>

namespace {

Haio::Format inferInputFormat(const Haio::Blob& blob, Haio::Format requested) {
    if (requested != Haio::Format::RAW) return requested;
    if (blob.format != Haio::Format::RAW) return blob.format;
    return Haio::formatFromExtension(blob.path);
}

Haio::Image decodeByFormat(const Haio::Blob& blob, Haio::Format format) {
    Haio::Image raw{Haio::Format::RAW, 0, 0, blob.data};
    switch (format) {
        case Haio::Format::PNG: return Haio::Decode<Haio::Format::PNG>()(raw);
        case Haio::Format::WEBP: return Haio::Decode<Haio::Format::WEBP>()(raw);
        case Haio::Format::KTX: return Haio::Decode<Haio::Format::KTX>()(raw);
        case Haio::Format::KTX2: return Haio::Decode<Haio::Format::KTX2>()(raw);
        case Haio::Format::PVR: return Haio::Decode<Haio::Format::PVR>()(raw);
        case Haio::Format::DDS: return Haio::Decode<Haio::Format::DDS>()(raw);
        default: throw std::runtime_error("unsupported decode format: " + std::string(Haio::formatName(format)));
    }
}

Haio::Blob blobFromImage(Haio::Image image, std::string path = {}) {
    auto format = image.type;
    return Haio::Blob{format, std::string(Haio::contentTypeFor(format)), std::move(path), std::move(image.data)};
}

}

namespace Haio {

Image decodeBlob(const Blob& blob, Format requested) {
    return decodeByFormat(blob, inferInputFormat(blob, requested));
}

Blob encodeImage(const Image& image, Format format, std::string path) {
    switch (format) {
        case Format::PNG: return blobFromImage(Encode<Format::PNG>()(image), std::move(path));
        case Format::PPM: return blobFromImage(Encode<Format::PPM>()(image), std::move(path));
        case Format::WEBP: return blobFromImage(Encode<Format::WEBP>()(image), std::move(path));
        case Format::ETC1: return blobFromImage(Encode<Format::ETC1>()(image), std::move(path));
        case Format::RGB565: return blobFromImage(Encode<Format::RGB565>()(image), std::move(path));
        case Format::KTX: return blobFromImage(Encode<Format::KTX>()(image), std::move(path));
        case Format::KTX2: return blobFromImage(Encode<Format::KTX2>()(image), std::move(path));
        case Format::PVR: return blobFromImage(Encode<Format::PVR>()(image), std::move(path));
        case Format::DDS: return blobFromImage(Encode<Format::DDS>()(image), std::move(path));
        default: throw std::runtime_error("unsupported encode format: " + std::string(formatName(format)));
    }
}

Blob runPipeline(Blob input, const Pipeline& pipeline) {
    bool hasImage = false;
    Image image{};
    Format explicitDecode = Format::RAW;
    Format outputFormat = Format::RAW;

    auto ensureImage = [&] {
        if (!hasImage) {
            image = decodeBlob(input, explicitDecode);
            hasImage = true;
        }
    };

    for (const auto& token : pipeline.tokens()) {
        switch (token.kind) {
            case TokenKind::Source:
                break;
            case TokenKind::DecodeAuto:
                explicitDecode = Format::RAW;
                ensureImage();
                break;
            case TokenKind::Decode:
                explicitDecode = token.format;
                ensureImage();
                break;
            case TokenKind::Crop:
                ensureImage();
                image = cropImage(image, token.rect);
                break;
            case TokenKind::Resize:
                ensureImage();
                image = resizeImage(image, token.size);
                break;
            case TokenKind::Radius:
                ensureImage();
                image = roundImageCorners(image, token.radius);
                break;
            case TokenKind::Encode:
                outputFormat = token.format;
                break;
        }
    }

    if (outputFormat == Format::RAW) {
        if (!hasImage) return input;
        return Blob{image.type, std::string(contentTypeFor(image.type)), input.path, std::move(image.data)};
    }

    ensureImage();
    return encodeImage(image, outputFormat, input.path);
}

}
