#pragma once

#include <JuceHeader.h>

// Paleta y métricas globales del tema oscuro.
namespace Theme
{
    inline juce::Colour window()          { return juce::Colour (0xff121212); }
    inline juce::Colour panel()           { return juce::Colour (0xff1a1a1a); }
    inline juce::Colour panelAlt()        { return juce::Colour (0xff202020); }
    inline juce::Colour panelHi()         { return juce::Colour (0xff262626); }
    inline juce::Colour border()          { return juce::Colour (0xff2e2e2e); }
    inline juce::Colour text()            { return juce::Colour (0xffd6d6d6); }
    inline juce::Colour textDim()         { return juce::Colour (0xff8a8a8a); }
    inline juce::Colour accent()          { return juce::Colour (0xff4fc3f7); }
    inline juce::Colour accent2()         { return juce::Colour (0xffffb74d); }
    inline juce::Colour danger()          { return juce::Colour (0xffe53935); }
    inline juce::Colour success()         { return juce::Colour (0xff66bb6a); }
    inline juce::Colour playhead()        { return juce::Colour (0xffff5252); }
    inline juce::Colour gridLine()        { return juce::Colour (0xff282828); }
    inline juce::Colour gridLineStrong()  { return juce::Colour (0xff3a3a3a); }
    inline juce::Colour knobBody()        { return juce::Colour (0xff2c2c2c); }
    inline juce::Colour buttonOff()       { return juce::Colour (0xff2a2a2a); }

    constexpr float splitterWidth   = 5.0f;
    constexpr int   transportHeight = 56;
    constexpr int   trackRowHeight  = 48;
    constexpr int   trackHeaderWidth = 236;
    constexpr int   pxPerBeat       = 40;
    constexpr int   defaultBars     = 64;

    inline juce::Font font (float height, bool bold = false)
    {
        juce::Font f { juce::FontOptions (height) };
        f.setBold (bold);
        return f;
    }
}
