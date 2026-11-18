#pragma once

#include <juce_graphics/juce_graphics.h>

namespace squeeze::ui
{

inline const juce::Colour kBackground { 0xff16151a };
inline const juce::Colour kHeader { 0xff16151a };
inline const juce::Colour kText { 0xfff4f1ea };
inline const juce::Colour kMuted { 0xff9a958c };
inline const juce::Colour kLine { 0xff343a42 };
inline const juce::Colour kTrack { 0xff2a2e34 };

inline const juce::Colour kModeColour[4] {
    juce::Colour(0xffe2a53a),
    juce::Colour(0xff49b4f0),
    juce::Colour(0xffb794f6),
    juce::Colour(0xfff0994a),
};

// Keeps a CJK typeface alive so Korean glyphs resolve on Linux as well as macOS.
inline juce::Typeface::Ptr sharedTypeface()
{
    static const juce::Typeface::Ptr face = []() -> juce::Typeface::Ptr {
        const juce::StringArray preferred {
            "Apple SD Gothic Neo",
            "Noto Sans CJK KR",
            "Noto Sans KR",
            "WenQuanYi Micro Hei",
        };
        const auto installed = juce::Font::findAllTypefaceNames();

        for (const auto& name : preferred)
        {
            if (installed.contains(name))
                return juce::Typeface::createSystemTypefaceFor(juce::Font(juce::FontOptions(name, 16.0f, juce::Font::plain)));
        }

        const char* files[] = {
            "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
            "/System/Library/Fonts/AppleSDGothicNeo.ttc",
        };

        for (const char* path : files)
        {
            const juce::File file(path);

            if (! file.existsAsFile())
                continue;

            juce::MemoryBlock data;

            if (file.loadFileAsData(data) && data.getSize() > 0)
                if (auto loaded = juce::Typeface::createSystemTypefaceFor(data.getData(), data.getSize()))
                    return loaded;
        }

        return {};
    }();

    return face;
}

inline juce::Font interfaceFont(float height)
{
    if (auto face = sharedTypeface())
        return juce::Font(juce::FontOptions(face).withHeight(height));

    return juce::Font(juce::FontOptions(height));
}

struct TextFit
{
    float size = 12.0f;
    bool ok = true;
};

inline TextFit fitLine(const juce::String& text, float preferred, float minimum, int width, int pad)
{
    TextFit fit;
    fit.size = preferred;

    if (text.isEmpty())
        return fit;

    const int limit = juce::jmax(0, width - pad);
    const auto widthOf = [](float size, const juce::String& line) {
        return juce::GlyphArrangement::getStringWidthInt(interfaceFont(size), line);
    };

    while (fit.size > minimum && widthOf(fit.size, text) > limit)
        fit.size -= 0.5f;

    fit.ok = widthOf(fit.size, text) <= limit;
    return fit;
}

} // namespace squeeze::ui
