// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
constexpr float designWidth = 1120.0f;
constexpr float designHeight = 800.0f;
const juce::Colour ink { 0xff16130f };
const juce::Colour paper { 0xffeee9da };
const juce::Colour white { 0xfffffff3 };
const juce::Colour muted { 0xff625d53 };
const juce::Colour red { 0xffef4029 };
const juce::Colour yellow { 0xfff5ee36 };
const juce::Colour blue { 0xff254edb };
const std::array<float, 6> cellEdges { 24.0f, 256.0f, 488.0f, 678.0f, 868.0f, 1096.0f };

juce::Font mono (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

juce::Font impact (float size)
{
    // Use the installed face; no proprietary font file is bundled with the plugin.
    static const juce::String family = []
    {
        const auto faces = juce::Font::findAllTypefaceNames();
        for (const auto* name : { "Impact", "Anton", "Arial Narrow", "Arial Black" })
            if (faces.contains (name, true))
                return juce::String (name);
        return juce::Font::getDefaultSansSerifFontName();
    }();
    return juce::Font (juce::FontOptions (family, size, juce::Font::plain));
}

void label (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds,
            float size = 12.0f, juce::Colour colour = ink,
            juce::Justification alignment = juce::Justification::centredLeft,
            bool headline = false)
{
    g.setColour (colour);
    g.setFont (headline ? impact (size) : mono (size));
    g.drawFittedText (text, bounds.toNearestInt(), alignment, 1);
}

juce::Path memePath (const juce::String& text, juce::Rectangle<float> bounds, float size,
                     juce::Justification alignment = juce::Justification::centred)
{
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText (impact (size), text, bounds.getX(), bounds.getY(),
                         bounds.getWidth(), bounds.getHeight(), alignment, 1, 0.75f);
    juce::Path path;
    glyphs.createPath (path);
    return path;
}

void memeText (juce::Graphics& g, const juce::Path& path, float outline = 6.0f, bool misprint = false)
{
    g.setColour (ink);
    g.strokePath (path, juce::PathStrokeType (outline, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    if (misprint)
    {
        g.setColour (blue);
        g.fillPath (path, juce::AffineTransform::translation (3.0f, 1.0f));
    }
    g.setColour (white);
    g.fillPath (path);
}

juce::Colour pixelColour (float value, bool colour)
{
    const auto normalized = juce::jlimit (0.0f, 1.0f, value / 255.0f);
    if (! colour)
    {
        const auto brightness = static_cast<juce::uint8> (normalized * 255.0f);
        return juce::Colour::fromRGB (brightness, brightness, brightness);
    }
    // Only the display palette changes. Every pixel still encodes the actual
    // reconstructed amplitude, with no synthetic visual noise in the audio image.
    static const std::array<juce::Colour, 7> palette {
        juce::Colour (0xff100812), juce::Colour (0xff262654),
        juce::Colour (0xff792156), juce::Colour (0xffed4321),
        juce::Colour (0xffff8328), juce::Colour (0xfff5df35),
        juce::Colour (0xfffff5bc)
    };
    const auto position = normalized * 6.0f;
    const auto index = juce::jmin (5, static_cast<int> (position));
    return palette[static_cast<size_t> (index)].interpolatedWith (
        palette[static_cast<size_t> (index + 1)], position - static_cast<float> (index));
}
}

class DeepFryArtwork final
{
public:
    DeepFryArtwork()
        : background (juce::Image::RGB, static_cast<int> (designWidth), static_cast<int> (designHeight), false),
          title (memePath ("DEEP FRY", { 20, 33, 672, 106 }, 108, juce::Justification::centredLeft))
    {
        // Slightly stretched wordmark recalls a repeatedly resized meme.
        title.applyTransform (juce::AffineTransform::scale (1.45f, 1.0f, 20.0f, 33.0f));
        // Print grain lives in the static artwork, never in the signal preview.
        // Cache it and the outlined glyphs once, rather than redrawing noise at 30 Hz.
        {
            juce::Image::BitmapData pixels (background, juce::Image::BitmapData::writeOnly);
            std::uint32_t random = 0x4a504547u;
            for (int y = 0; y < background.getHeight(); ++y)
                for (int x = 0; x < background.getWidth(); ++x)
                {
                    random = random * 1664525u + 1013904223u;
                    const auto grain = static_cast<int> ((random >> 27) & 7u);
                    pixels.setPixelColour (x, y, juce::Colour::fromRGB (
                        static_cast<juce::uint8> (238 - grain),
                        static_cast<juce::uint8> (233 - grain),
                        static_cast<juce::uint8> (218 - grain)));
                }
        }
        juce::Graphics g (background);
        g.setColour (ink);
        g.fillRect (0, 0, 1120, 28);
        g.fillRect (0, 776, 1120, 24);
        g.setColour (red);
        g.fillRect (0, 28, 1120, 118);
        for (int y = 28; y < 146; y += 16)
            for (int x = 0; x < 1120; x += 16)
            {
                const auto hash = (x * 37 + y * 59) % 17;
                g.setColour (hash < 8 ? ink.withAlpha (0.045f) : yellow.withAlpha (0.045f));
                g.fillRect (x, y, 16, juce::jmin (16, 146 - y));
            }
        g.setColour (ink);
        g.fillRect (0, 144, 1120, 3);
        // The small registration checker is decorative, separate from live pixels.
        for (int x = 699; x < 792; x += 12)
            for (int y = 47; y < 129; y += 12)
            {
                g.setColour (((x / 12 + y / 12) % 2) == 0 ? ink : paper);
                g.fillRect (x, y, 12, 12);
            }
    }

    juce::Image background;
    juce::Path title;
};

class DeepFryLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DeepFryLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, ink);
        setColour (juce::Slider::textBoxBackgroundColourId, white);
        setColour (juce::Slider::textBoxOutlineColourId, ink);
        setColour (juce::Slider::textBoxHighlightColourId, yellow);
        setColour (juce::TooltipWindow::backgroundColourId, yellow);
        setColour (juce::TooltipWindow::textColourId, ink);
        setColour (juce::TooltipWindow::outlineColourId, ink);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosition, float, float,
                           const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const auto centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
        const auto left = static_cast<float> (x);
        const auto right = static_cast<float> (x + width);
        const auto position = juce::jlimit (left, right, sliderPosition);
        const juce::Rectangle<float> track (left, centreY - 6.0f, static_cast<float> (width), 12.0f);
        g.setColour (white);
        g.fillRect (track);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRect (track.withWidth (position - left));
        g.setColour (ink);
        g.drawRect (track, 1.5f);
        for (int tick = 0; tick <= 8; ++tick)
        {
            const auto tickX = left + static_cast<float> (width * tick) / 8.0f;
            g.drawLine (tickX, centreY + 10.0f, tickX, centreY + (tick % 4 == 0 ? 16.0f : 13.0f), 1.0f);
        }
        const juce::Rectangle<float> thumb (position - 7.0f, centreY - 14.0f, 14.0f, 28.0f);
        g.setColour (ink);
        g.fillRect (thumb.translated (2, 2));
        g.setColour (slider.isMouseButtonDown() ? yellow : paper);
        g.fillRect (thumb);
        g.setColour (ink);
        g.drawRect (thumb, 2.0f);
        g.drawLine (position - 2.0f, centreY - 6.0f, position - 2.0f, centreY + 6.0f, 1.0f);
        g.drawLine (position + 2.0f, centreY - 6.0f, position + 2.0f, centreY + 6.0f, 1.0f);
        if (slider.hasKeyboardFocus (true))
        {
            g.setColour (blue);
            g.drawRect (thumb.expanded (3), 1.5f);
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* box = juce::LookAndFeel_V4::createSliderTextBox (slider);
        box->setFont (mono (16.0f));
        box->setJustificationType (juce::Justification::centred);
        box->setColour (juce::Label::outlineColourId, ink);
        box->setColour (juce::Label::outlineWhenEditingColourId, blue);
        box->setColour (juce::TextEditor::textColourId, ink);
        box->setColour (juce::TextEditor::backgroundColourId, white);
        return box;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.5f, 1.5f).withTrimmedRight (3).withTrimmedBottom (3);
        g.setColour (ink);
        g.fillRect (bounds.translated (3, 3));
        if (down)
            bounds = bounds.translated (2, 2);
        g.setColour (button.getToggleState() ? yellow : highlighted ? white : paper);
        g.fillRect (bounds);
        g.setColour (button.hasKeyboardFocus (true) ? blue : ink);
        g.drawRect (bounds, 2.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (8, 3).translated (-1.5f, -1.5f);
        if (down)
            bounds = bounds.translated (2, 2);
        label (g, button.getButtonText().toUpperCase(), bounds,
               button.getHeight() >= 32 ? 19.0f : 16.0f, ink, juce::Justification::centred, true);
    }
};

DeepFryAudioProcessorEditor::DeepFryAudioProcessorEditor (DeepFryAudioProcessor& p)
    : AudioProcessorEditor (&p), effectProcessor (p), lookAndFeel (std::make_unique<DeepFryLookAndFeel>()), artwork (std::make_unique<DeepFryArtwork>())
{
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);
    setName ("DEEP FRY audio effect");

    const std::array<const char*, 5> ids { "quality", "fry", "pixelBits", "mix", "output" };
    const std::array<const char*, 5> names { "JPEG quality", "Fry", "Pixel depth", "Dry wet mix", "Output gain" };
    const std::array<const char*, 5> descriptions {
        "Lower quality discards more of each image tile's DCT detail. Double-click to reset.",
        "Push image contrast and sharpen pixel edges for harder, deep-fried distortion. Double-click to reset.",
        "The number of bits used for each grayscale pixel. Fewer bits makes the image and audio grainier.",
        "Blend the original audio with the JPEG-processed signal. 100% is entirely processed.",
        "Final output level in decibels. Use this to match the level of the original signal."
    };

    for (size_t index = 0; index < knobs.size(); ++index)
    {
        auto& knob = knobs[index];
        // Attach before creating the value box so it inherits this editor's
        // look-and-feel rather than caching JUCE's default dark-theme colours.
        addAndMakeVisible (knob);
        knob.setSliderStyle (juce::Slider::LinearHorizontal);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 94, 28);
        knob.setName (names[index]);
        knob.setTitle (names[index]);
        knob.setDescription (descriptions[index]);
        knob.setTooltip (descriptions[index]);
        knob.setWantsKeyboardFocus (true);
        knob.setScrollWheelEnabled (false);
        knob.setColour (juce::Slider::trackColourId, index == 1 ? red : index == 0 ? blue : yellow);
        knob.setColour (juce::Slider::textBoxTextColourId, ink);
        knob.setColour (juce::Slider::textBoxBackgroundColourId, white);
        knob.setColour (juce::Slider::textBoxOutlineColourId, ink);
        knob.setColour (juce::Slider::textBoxHighlightColourId, yellow);
        knobAttachments[index] = std::make_unique<SliderAttachment> (effectProcessor.parameters, ids[index], knob);
        knob.textFromValueFunction = [index] (double value)
        {
            if (index == 2)
                return juce::String (juce::roundToInt (value)) + " bit";
            if (index == 4)
                return juce::String (value, 1) + " dB";
            return juce::String (juce::roundToInt (value)) + " %";
        };
        knob.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("-.0123456789").getDoubleValue();
        };
        knob.updateText();
        knob.onValueChange = [this]
        {
            if (! applyingPreset)
                for (auto& button : presetButtons)
                    button.setToggleState (false, juce::dontSendNotification);
        };
    }

    for (size_t index = 0; index < presetButtons.size(); ++index)
    {
        auto& button = presetButtons[index];
        button.setButtonText (DeepFryAudioProcessor::presetName (static_cast<int> (index)));
        button.setName ("Load " + button.getButtonText() + " preset");
        button.setTooltip ("Load the " + button.getButtonText() + " preset. Changes the five sound controls.");
        button.setWantsKeyboardFocus (true);
        button.onClick = [this, index]
        {
            const juce::ScopedValueSetter<bool> guard (applyingPreset, true);
            effectProcessor.applyPreset (static_cast<int> (index));
            for (size_t buttonIndex = 0; buttonIndex < presetButtons.size(); ++buttonIndex)
                presetButtons[buttonIndex].setToggleState (buttonIndex == index, juce::dontSendNotification);
        };
        addAndMakeVisible (button);
    }

    bypassButton.setClickingTogglesState (true);
    bypassButton.setName ("Bypass effect");
    bypassButton.setTooltip ("Bypass the effect while preserving its reported latency.");
    bypassButton.onStateChange = [this]
    {
        bypassButton.setButtonText (bypassButton.getToggleState() ? "BYPASSED" : "EFFECT ON");
    };
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (effectProcessor.parameters, "bypass", bypassButton);

    freezeButton.setButtonText ("FREEZE IMAGE");
    freezeButton.setClickingTogglesState (true);
    freezeButton.setName ("Freeze visualization");
    freezeButton.setTooltip ("Freeze the image display. Audio processing continues unchanged.");
    freezeButton.onClick = [this]
    {
        setFrozen (freezeButton.getToggleState());
    };
    addAndMakeVisible (freezeButton);

    for (auto* button : { &wetViewButton, &outputViewButton, &paletteButton,
                           &leftChannelButton, &rightChannelButton, &saveImageButton })
    {
        button->setWantsKeyboardFocus (true);
        addAndMakeVisible (*button);
    }
    wetViewButton.setName ("Show JPEG wet signal");
    wetViewButton.setTooltip ("Inspect decoded JPEG samples before Mix, Output gain, and Bypass.");
    outputViewButton.setName ("Show final output");
    outputViewButton.setTooltip ("Inspect the actual output after Mix, gain, and Bypass, aligned with its original input.");
    paletteButton.setName ("Toggle visualization palette");
    paletteButton.setTooltip ("Use the same colour or grayscale mapping in both panels. Colours encode signed amplitude.");
    leftChannelButton.setName ("Inspect left channel");
    rightChannelButton.setName ("Inspect right channel");
    leftChannelButton.setTooltip ("View the left channel, or the mono signal.");
    rightChannelButton.setTooltip ("View the right channel of a stereo signal.");
    saveImageButton.setName ("Save visualization PNG");
    saveImageButton.setTooltip ("Save a paired input/result PNG with the current channel and palette. Audio keeps playing.");
    saveImageButton.setEnabled (false);
    const auto refreshView = [this]
    {
        updateViewControls();
        rebuildImages();
        repaint();
    };
    wetViewButton.onClick = [this, refreshView] { showFinalOutput = false; refreshView(); };
    outputViewButton.onClick = [this, refreshView] { showFinalOutput = true; refreshView(); };
    paletteButton.onClick = [this, refreshView] { useColour = ! useColour; refreshView(); };
    leftChannelButton.onClick = [this, refreshView] { selectedChannel = 0; refreshView(); };
    rightChannelButton.onClick = [this, refreshView] { selectedChannel = 1; refreshView(); };
    saveImageButton.onClick = [this] { saveSnapshot(); };
    updateViewControls();

    helpButton.setButtonText ("HOW?");
    helpButton.setClickingTogglesState (true);
    helpButton.setName ("Explain JPEG audio processing");
    helpButton.setTooltip ("Show how audio becomes an image, then becomes audio again.");
    helpButton.onClick = [this]
    {
        helpOpen = helpButton.getToggleState();
        licenseLink.setVisible (helpOpen);
        sourceLink.setVisible (helpOpen);
        repaint();
    };
    addAndMakeVisible (helpButton);
    for (auto* link : { &licenseLink, &sourceLink })
    {
        link->setColour (juce::HyperlinkButton::textColourId, blue);
        link->setJustificationType (juce::Justification::centredLeft);
        addChildComponent (*link);
    }

    setResizable (true, true);
    setResizeLimits (896, 640, 1400, 1000);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (designWidth / designHeight);
    setSize (static_cast<int> (designWidth), static_cast<int> (designHeight));
    // Discard queued history from before this editor opened.
    deepfry::VisualFrame stale;
    while (effectProcessor.popVisualFrame (stale)) {}
    startTimerHz (30);
}

DeepFryAudioProcessorEditor::~DeepFryAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> DeepFryAudioProcessorEditor::scaledBounds (juce::Rectangle<float> bounds) const
{
    return bounds.transformedBy (juce::AffineTransform::scale (contentScale)
                                     .translated (contentOffsetX, contentOffsetY)).toNearestInt();
}

void DeepFryAudioProcessorEditor::resized()
{
    const auto availableWidth = static_cast<float> (getWidth());
    const auto availableHeight = static_cast<float> (getHeight());
    contentScale = juce::jmin (availableWidth / designWidth, availableHeight / designHeight);
    contentOffsetX = (availableWidth - designWidth * contentScale) * 0.5f;
    contentOffsetY = (availableHeight - designHeight * contentScale) * 0.5f;

    for (size_t index = 0; index < knobs.size(); ++index)
        knobs[index].setBounds (scaledBounds ({ cellEdges[index] + 16.0f, 604.0f,
                                               cellEdges[index + 1] - cellEdges[index] - 32.0f, 76.0f }));
    for (size_t index = 0; index < presetButtons.size(); ++index)
        presetButtons[index].setBounds (scaledBounds ({ 139.0f + static_cast<float> (index) * 165.0f,
                                                       720.0f, 153.0f, 41.0f }));
    bypassButton.setBounds (scaledBounds ({ 24, 479, 200, 44 }));
    helpButton.setBounds (scaledBounds ({ 236, 479, 92, 44 }));
    leftChannelButton.setBounds (scaledBounds ({ 224, 150, 47, 33 }));
    rightChannelButton.setBounds (scaledBounds ({ 281, 150, 47, 33 }));
    wetViewButton.setBounds (scaledBounds ({ 360, 150, 132, 33 }));
    outputViewButton.setBounds (scaledBounds ({ 502, 150, 132, 33 }));
    paletteButton.setBounds (scaledBounds ({ 646, 150, 108, 33 }));
    saveImageButton.setBounds (scaledBounds ({ 766, 150, 130, 33 }));
    freezeButton.setBounds (scaledBounds ({ 908, 150, 188, 33 }));
    licenseLink.setBounds (scaledBounds ({ 394, 508, 80, 24 }));
    sourceLink.setBounds (scaledBounds ({ 490, 508, 80, 24 }));
    for (auto* link : { &licenseLink, &sourceLink })
        link->setFont (mono (12.0f * contentScale), true);
}

void DeepFryAudioProcessorEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float level, const juce::String& text)
{
    label (g, text, bounds.removeFromLeft (32), 11);
    const auto db = juce::Decibels::gainToDecibels (level, -60.0f);
    const auto amount = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const auto segmentWidth = (bounds.getWidth() - 11.0f) / 12.0f;
    for (int index = 0; index < 12; ++index)
    {
        const auto lit = amount > static_cast<float> (index) / 12.0f;
        const juce::Rectangle<float> segment (bounds.getX() + static_cast<float> (index) * (segmentWidth + 1.0f),
                                              bounds.getCentreY() - 4, segmentWidth, 8);
        g.setColour (lit ? (index >= 10 ? red : ink) : muted.withAlpha (0.18f));
        g.fillRect (segment);
    }
}

void DeepFryAudioProcessorEditor::drawImagePanel (juce::Graphics& g,
                                                 juce::Rectangle<float> bounds, bool processed)
{
    g.setColour (ink);
    g.fillRect (bounds.expanded (3).translated (4, 4));
    if (tileCount > 0)
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (processed ? afterImage : beforeImage, bounds, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        // Empty image-editor checkerboard; real sample pixels only appear once
        // the processor has published a frame. Silence is not a fake animation.
        const int columns = processed ? 32 : 16;
        const float unit = bounds.getWidth() / static_cast<float> (columns);
        for (int y = 0; y < columns / 2; ++y)
            for (int x = 0; x < columns; ++x)
            {
                g.setColour (((x + y) % 2) == 0 ? juce::Colour (0xffbdb7a5) : juce::Colour (0xffcbc5b5));
                g.fillRect (bounds.getX() + static_cast<float> (x) * unit,
                            bounds.getY() + static_cast<float> (y) * unit, unit, unit);
            }
    }
    g.setColour (ink);
    g.drawRect (bounds.expanded (1.5f), 3);
    if (tileCount > 0 && frozen)
    {
        const auto index = selectedTile >= 0 ? selectedTile : static_cast<int> (tileCount - 1);
        const auto cellWidth = bounds.getWidth() / 16.0f;
        const auto cellHeight = bounds.getHeight() / 8.0f;
        const juce::Rectangle<float> selected (bounds.getX() + static_cast<float> (index % 16) * cellWidth,
                                                bounds.getY() + static_cast<float> (index / 16) * cellHeight,
                                                cellWidth, cellHeight);
        g.setColour (ink);
        g.drawRect (selected.reduced (1), 3);
        g.setColour (white);
        g.drawRect (selected.reduced (2), 1);
    }
    if (processed && tileCount == 0)
    {
        const auto hint = juce::Rectangle<float> (368, 30).withCentre (bounds.getCentre());
        g.setColour (ink);
        g.fillRect (hint);
        label (g, "PLAY A CLIP THROUGH DEEP FRY", hint, 12.5f, paper, juce::Justification::centred);
    }
    else if (tileCount == 0)
        label (g, "NO SIGNAL YET", bounds, 12, ink, juce::Justification::centred);
}

void DeepFryAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ink);
    g.addTransform (juce::AffineTransform::scale (contentScale)
                        .translated (contentOffsetX, contentOffsetY));
    g.drawImageAt (artwork->background, 0, 0);

    label (g, "deep_fry_final_FINAL2.jpg", { 24, 1, 610, 25 }, 12, paper);
    label (g, "JPEG PEDAL / AUDIO DEGRADATION", { 695, 1, 401, 25 }, 11, paper,
           juce::Justification::centredRight);
    memeText (g, artwork->title, 9, true);
    {
        juce::Graphics::ScopedSaveState stampState (g);
        g.addTransform (juce::AffineTransform::rotation (-0.048f, 950, 86));
        const juce::Rectangle<float> stamp (816, 47, 272, 81);
        g.setColour (ink);
        g.fillRect (stamp.translated (4, 4));
        g.setColour (yellow);
        g.fillRect (stamp);
        g.setColour (ink);
        g.drawRect (stamp, 3);
        label (g, "JPEG ABUSE", { 828, 47, 248, 50 }, 43, ink, juce::Justification::centred, true);
        label (g, "JPEG AUDIO EFFECT", { 828, 97, 248, 23 }, 11.5f, ink, juce::Justification::centred);
    }

    label (g, "01 / INPUT", { 24, 156, 186, 24 }, 13);
    const auto bypassed = bypassButton.getToggleState();
    const auto hasSignal = inputMeter > 0.0001f && ticksSinceFrame < 15;
    const juce::String status = saveStatusTicks > 0 ? saveStatus : frozen ? "FROZEN" : bypassed ? "BYPASSED"
                                     : ticksSinceFrame >= 15 && tileCount > 0 ? "PLAYBACK STOPPED"
                                     : hasSignal ? "LIVE" : tileCount == 0 ? "WAITING FOR AUDIO" : "INPUT SILENT";
    g.setColour (hasSignal && ! frozen && ! bypassed ? red : muted);
    g.fillRect (193, 355, 6, 6);
    label (g, status, { 205, 347, 123, 24 }, 9.5f, ink, juce::Justification::centredRight);

    drawImagePanel (g, { 24, 186, 304, 152 }, false);
    drawImagePanel (g, { 360, 186, 736, 368 }, true);
    const auto* latest = tileCount > 0 ? historyFrame (tileCount - 1) : nullptr;
    const auto channelText = latest != nullptr && latest->channelCount == 1 ? "MONO" : selectedChannel == 0 ? "LEFT CHANNEL" : "RIGHT CHANNEL";
    label (g, channelText, { 24, 348, 160, 22 }, 10.5f, muted);
    drawTileInspector (g);
    drawAmplitudeLegend (g, { 24, 535, 304, 8 });
    label (g, "-1", { 24, 545, 50, 16 }, 9, muted);
    label (g, "0", { 156, 545, 40, 16 }, 9, muted, juce::Justification::centred);
    label (g, "+1", { 278, 545, 50, 16 }, 9, muted, juce::Justification::centredRight);

    // The export controls form one ruled sheet, rather than independent cards.
    g.setColour (paper);
    g.fillRect (24, 572, 1072, 132);
    g.setColour (ink);
    g.drawRect (24, 572, 1072, 132, 2);
    const std::array<const char*, 5> names { "JPEG QUALITY", "FRY", "PIXEL DEPTH", "MIX", "OUTPUT" };
    const std::array<const char*, 5> hints { "IMAGE DETAIL", "CONTRAST + SHARPEN", "PIXEL RESOLUTION", "DRY / WET", "OUTPUT GAIN" };
    for (size_t index = 0; index < names.size(); ++index)
    {
        const auto x = cellEdges[index];
        const auto width = cellEdges[index + 1] - x;
        if (index > 0)
        {
            g.setColour (ink);
            g.fillRect (x, 572.0f, 2.0f, 132.0f);
        }
        label (g, names[index], { x + 12, 577, width - 24, 29 }, 25, ink,
               juce::Justification::centredLeft, true);
        label (g, hints[index], { x + 8, 684, width - 16, 15 }, 9.5f, muted,
               juce::Justification::centred);
    }

    label (g, "PRESETS", { 24, 720, 106, 37 }, 24, ink, juce::Justification::centredLeft, true);
    drawMeter (g, { 818, 722, 278, 17 }, inputMeter, "IN");
    drawMeter (g, { 818, 743, 278, 17 }, outputMeter, "OUT");

    const auto sampleRate = effectProcessor.getSampleRate();
    const auto rateText = sampleRate > 0 ? juce::String (sampleRate / 1000.0, 1) + " kHz" : "DEVICE IDLE";
    const auto latencyText = sampleRate > 0 ? juce::String (1000.0 * effectProcessor.getLatencySamples() / sampleRate, 2) + " ms" : "-- ms";
    label (g, rateText + " / " + latencyText + " LATENCY", { 24, 777, 366, 22 }, 10, paper);
    const auto viewText = showFinalOutput ? "FINAL OUTPUT" : "JPEG WET";
    label (g, juce::String (viewText) + (useColour ? " / COLOUR" : " / GRAYSCALE") + " / +/-1",
           { 393, 777, 370, 22 }, 10, paper,
           juce::Justification::centred);
    label (g, tileCount == 0 ? "DCT DETAIL: --" : "DCT DETAIL: " + juce::String (juce::roundToInt (retention * 100.0f)) + "% RETAINED",
           { 783, 777, 313, 22 }, 10, paper, juce::Justification::centredRight);

    if (helpOpen)
    {
        const juce::Rectangle<float> sheet (373, 198, 710, 343);
        g.setColour (ink);
        g.fillRect (sheet.translated (5, 5));
        g.setColour (paper);
        g.fillRect (sheet);
        g.setColour (ink);
        g.drawRect (sheet, 3);
        label (g, "HOW IT WORKS", { 394, 210, 668, 48 }, 39, ink,
               juce::Justification::centredLeft, true);
        const std::array<const char*, 3> steps {
            "01 / 64 samples fill an 8x8 tile, one row at a time.",
            "02 / Fry boosts contrast and edges. JPEG removes detail.",
            "03 / Pixel depth adds steps. The pixels become audio."
        };
        for (size_t i = 0; i < steps.size(); ++i)
            label (g, steps[i], { 394, 272 + static_cast<float> (i) * 35, 668, 31 }, 13);
        g.setColour (ink);
        g.fillRect (394, 387, 668, 2);
        label (g, "FINAL OUT includes Mix, gain and Bypass. JPEG WET precedes them.", { 394, 400, 668, 27 }, 11.5f);
        label (g, "Both panes use one palette. The centre colour means zero.", { 394, 425, 668, 27 }, 11.5f);
        label (g, "Click a tile to freeze and inspect it. SAVE PNG exports the pair.", { 394, 450, 668, 27 }, 11.5f);
        label (g, "(C) 2026 Mitch Chaiet. AGPLv3. You may redistribute under this license.",
               { 394, 482, 668, 20 }, 10.5f);
        label (g, "NO WARRANTY", { 858, 508, 204, 24 }, 11, muted,
               juce::Justification::centredRight);
    }
}

const deepfry::VisualFrame* DeepFryAudioProcessorEditor::historyFrame (size_t index) const
{
    if (index >= tileCount)
        return nullptr;
    return &tileHistory[tileCount == tileHistory.size() ? (nextTile + index) % tileHistory.size() : index];
}

const deepfry::VisualChannelFrame* DeepFryAudioProcessorEditor::channelFrame (const deepfry::VisualFrame& frame) const
{
    if (frame.channelCount <= selectedChannel)
        return nullptr;
    return &frame.channels[static_cast<size_t> (selectedChannel)];
}

float DeepFryAudioProcessorEditor::displayedPixel (const deepfry::VisualChannelFrame& frame, size_t sample) const
{
    return showFinalOutput ? 128.0f + juce::jlimit (-1.0f, 1.0f, frame.output[sample]) * 127.0f
                           : frame.image.after[sample];
}

void DeepFryAudioProcessorEditor::updateViewControls()
{
    wetViewButton.setToggleState (! showFinalOutput, juce::dontSendNotification);
    outputViewButton.setToggleState (showFinalOutput, juce::dontSendNotification);
    paletteButton.setToggleState (useColour, juce::dontSendNotification);
    paletteButton.setButtonText (useColour ? "COLOUR" : "GRAY");
    leftChannelButton.setToggleState (selectedChannel == 0, juce::dontSendNotification);
    rightChannelButton.setToggleState (selectedChannel == 1, juce::dontSendNotification);
    const auto* latest = tileCount > 0 ? historyFrame (tileCount - 1) : nullptr;
    rightChannelButton.setEnabled (latest == nullptr || latest->channelCount > 1);
    saveImageButton.setEnabled (tileCount > 0 && ! snapshotDialogOpen);
}

void DeepFryAudioProcessorEditor::setFrozen (bool shouldFreeze)
{
    frozen = shouldFreeze;
    freezeButton.setToggleState (frozen, juce::dontSendNotification);
    freezeButton.setButtonText (frozen ? "RESUME LIVE" : "FREEZE IMAGE");
    if (! frozen)
        selectedTile = -1;
    repaint();
}

void DeepFryAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (helpOpen || tileCount == 0)
        return;
    const juce::Point<float> point ((event.position.x - contentOffsetX) / contentScale,
                                    (event.position.y - contentOffsetY) / contentScale);
    for (const auto bounds : { juce::Rectangle<float> (24, 186, 304, 152),
                               juce::Rectangle<float> (360, 186, 736, 368) })
        if (bounds.contains (point))
        {
            const auto column = static_cast<int> ((point.x - bounds.getX()) * 16.0f / bounds.getWidth());
            const auto row = static_cast<int> ((point.y - bounds.getY()) * 8.0f / bounds.getHeight());
            const auto index = row * 16 + column;
            if (index >= 0 && static_cast<size_t> (index) < tileCount)
            {
                selectedTile = index;
                const auto* frame = channelFrame (*historyFrame (static_cast<size_t> (index)));
                retention = frame != nullptr ? frame->image.retained : 0.0f;
                setFrozen (true);
            }
            return;
        }
}

void DeepFryAudioProcessorEditor::drawAmplitudeLegend (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const int columns = juce::jmax (2, static_cast<int> (bounds.getWidth()));
    const auto width = bounds.getWidth() / static_cast<float> (columns);
    for (int x = 0; x < columns; ++x)
    {
        const auto value = 1.0f + 254.0f * static_cast<float> (x) / static_cast<float> (columns - 1);
        g.setColour (pixelColour (value, useColour));
        g.fillRect (bounds.getX() + static_cast<float> (x) * width, bounds.getY(), width + 0.5f, bounds.getHeight());
    }
    g.setColour (ink);
    g.drawRect (bounds, 1);
}

void DeepFryAudioProcessorEditor::drawTileInspector (juce::Graphics& g)
{
    label (g, "INPUT", { 24, 369, 74, 18 }, 9.5f, muted);
    label (g, showFinalOutput ? "FINAL OUT" : "JPEG WET", { 112, 369, 80, 18 }, 9.5f, muted);
    const auto index = selectedTile >= 0 ? static_cast<size_t> (selectedTile) : tileCount > 0 ? tileCount - 1 : 0;
    const auto* frame = historyFrame (index);
    const auto* channel = frame != nullptr ? channelFrame (*frame) : nullptr;
    for (int panel = 0; panel < 2; ++panel)
    {
        const juce::Rectangle<float> bounds (panel == 0 ? 24.0f : 112.0f, 389, 72, 72);
        g.setColour (paper.darker (0.1f));
        g.fillRect (bounds);
        if (channel != nullptr)
            for (size_t sample = 0; sample < 64; ++sample)
            {
                g.setColour (pixelColour (panel == 0 ? channel->image.before[sample] : displayedPixel (*channel, sample), useColour));
                g.fillRect (bounds.getX() + static_cast<float> (sample % 8) * 9.0f,
                            bounds.getY() + static_cast<float> (sample / 8) * 9.0f, 9.0f, 9.0f);
            }
        g.setColour (ink);
        g.drawRect (bounds, 1);
    }
    label (g, frozen ? "SELECTED TILE" : "LATEST TILE", { 198, 370, 130, 19 }, 9.5f, muted);
    label (g, frame != nullptr ? juce::String (static_cast<int> (index + 1)) + " / " + juce::String (static_cast<int> (tileCount)) : "-- / --",
           { 198, 390, 130, 20 }, 13);
    const auto* latest = tileCount > 0 ? historyFrame (tileCount - 1) : nullptr;
    const double age = frame != nullptr && latest != nullptr && latest->samplePosition >= frame->samplePosition && frame->sampleRate > 0
        ? static_cast<double> (latest->samplePosition - frame->samplePosition) / frame->sampleRate : 0.0;
    label (g, frame != nullptr ? "-" + juce::String (age, 2) + " s / 64 SMP" : "64 SAMPLES", { 198, 412, 130, 19 }, 9.5f, muted);
    float peak = 0.0f;
    if (channel != nullptr)
        for (const auto sample : channel->output)
            peak = juce::jmax (peak, std::abs (sample));
    label (g, channel == nullptr ? "OUT -- dBFS" : peak < 0.000001f ? "OUT -INF dBFS"
                    : "OUT " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS",
           { 198, 434, 130, 19 }, 9.5f, peak > 1.0f ? red : muted);
    label (g, "CLICK A TILE TO FREEZE + INSPECT", { 24, 462, 304, 14 }, 9, muted);
}

juce::Image DeepFryAudioProcessorEditor::createVisualizationSnapshot() const
{
    if (tileCount == 0)
        return {};
    juce::Image snapshot (juce::Image::RGB, 1080, 352, true);
    juce::Graphics g (snapshot);
    g.fillAll (paper);
    label (g, "DEEP FRY / FRAME CAPTURE", { 20, 6, 700, 29 }, 26, ink,
           juce::Justification::centredLeft, true);
    const auto* latest = historyFrame (tileCount - 1);
    const auto channel = latest->channelCount == 1 ? "MONO" : selectedChannel == 0 ? "LEFT" : "RIGHT";
    label (g, juce::String (channel) + (useColour ? " / COLOUR" : " / GRAYSCALE"),
           { 780, 12, 280, 20 }, 11, ink, juce::Justification::centredRight);
    label (g, "01 / INPUT", { 20, 35, 512, 17 }, 10, muted);
    label (g, showFinalOutput ? "02 / FINAL OUTPUT" : "02 / JPEG WET", { 548, 35, 512, 17 }, 10, muted);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (beforeImage, juce::Rectangle<float> (20, 54, 512, 256));
    g.drawImage (afterImage, juce::Rectangle<float> (548, 54, 512, 256));
    g.setColour (ink);
    g.drawRect (20, 54, 512, 256, 1);
    g.drawRect (548, 54, 512, 256, 1);
    drawAmplitudeLegend (g, { 20, 322, 344, 8 });
    label (g, "-1", { 20, 332, 50, 16 }, 9, muted);
    label (g, "0", { 172, 332, 40, 16 }, 9, muted, juce::Justification::centred);
    label (g, "+1", { 314, 332, 50, 16 }, 9, muted, juce::Justification::centredRight);
    label (g, juce::String (static_cast<int> (tileCount)) + " TILES / SAMPLED HISTORY / DISPLAY LIMIT +/-1",
           { 390, 321, 670, 23 }, 10, muted, juce::Justification::centredRight);
    return snapshot;
}

void DeepFryAudioProcessorEditor::saveSnapshot()
{
    const auto snapshot = createVisualizationSnapshot();
    if (! snapshot.isValid() || snapshotDialogOpen)
        return;
    snapshotDialogOpen = true;
    updateViewControls();
    const auto name = "Deep-Fry-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".png";
    imageChooser = std::make_unique<juce::FileChooser> ("Save Deep Fry visualization",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name), "*.png");
    const juce::Component::SafePointer<DeepFryAudioProcessorEditor> safeThis (this);
    imageChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis, snapshot] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;
            safeThis->snapshotDialogOpen = false;
            const auto destination = chooser.getResult();
            if (destination != juce::File())
            {
                juce::TemporaryFile temporary (destination);
                auto stream = temporary.getFile().createOutputStream();
                bool saved = false;
                if (stream != nullptr && stream->openedOk())
                {
                    saved = juce::PNGImageFormat().writeImageToStream (snapshot, *stream);
                    stream->flush();
                    saved = saved && stream->getStatus().wasOk();
                    stream.reset();
                    saved = saved && temporary.overwriteTargetFileWithTemporary();
                }
                safeThis->saveStatus = saved ? "PNG SAVED" : "SAVE FAILED";
                safeThis->saveStatusTicks = 150;
            }
            safeThis->updateViewControls();
            safeThis->repaint();
        });
}

void DeepFryAudioProcessorEditor::rebuildImages()
{
    beforeImage.clear (beforeImage.getBounds(), juce::Colour (0xffbdb7a5));
    afterImage.clear (afterImage.getBounds(), juce::Colour (0xffbdb7a5));
    juce::Image::BitmapData beforePixels (beforeImage, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData afterPixels (afterImage, juce::Image::BitmapData::writeOnly);
    for (size_t tile = 0; tile < tileCount; ++tile)
    {
        const auto* frame = channelFrame (*historyFrame (tile));
        if (frame == nullptr)
            continue;
        const auto tileX = static_cast<int> (tile % 16) * 8;
        const auto tileY = static_cast<int> (tile / 16) * 8;
        for (size_t sample = 0; sample < 64; ++sample)
        {
            const auto x = tileX + static_cast<int> (sample % 8);
            const auto y = tileY + static_cast<int> (sample / 8);
            beforePixels.setPixelColour (x, y, pixelColour (frame->image.before[sample], useColour));
            afterPixels.setPixelColour (x, y, pixelColour (displayedPixel (*frame, sample), useColour));
        }
    }
    const auto* selected = historyFrame (selectedTile >= 0 ? static_cast<size_t> (selectedTile) : tileCount > 0 ? tileCount - 1 : 0);
    const auto* channel = selected != nullptr ? channelFrame (*selected) : nullptr;
    retention = channel != nullptr ? channel->image.retained : 0.0f;
}

void DeepFryAudioProcessorEditor::timerCallback()
{
    deepfry::VisualFrame frame;
    bool changed = false;
    bool receivedFrame = false;
    while (effectProcessor.popVisualFrame (frame))
    {
        receivedFrame = true;
        if (! frozen)
        {
            const auto* previous = tileCount > 0 ? historyFrame (tileCount - 1) : nullptr;
            if (previous != nullptr && (frame.streamGeneration != previous->streamGeneration
                || frame.samplePosition <= previous->samplePosition
                || ! juce::approximatelyEqual (frame.sampleRate, previous->sampleRate) || frame.channelCount != previous->channelCount))
            {
                tileCount = nextTile = 0;
                selectedTile = -1;
            }
            if (selectedChannel >= frame.channelCount)
                selectedChannel = 0;
            tileHistory[nextTile] = frame;
            nextTile = (nextTile + 1) % tileHistory.size();
            tileCount = juce::jmin (tileCount + 1, tileHistory.size());
            changed = true;
        }
    }
    if (changed)
        rebuildImages();
    updateViewControls();
    if (saveStatusTicks > 0)
        --saveStatusTicks;
    ticksSinceFrame = receivedFrame ? 0 : juce::jmin (ticksSinceFrame + 1, 120);
    const auto audioIsRunning = ticksSinceFrame < 15;
    inputMeter = juce::jmax (audioIsRunning ? effectProcessor.inputLevel.load (std::memory_order_relaxed) : 0.0f,
                             inputMeter * 0.83f);
    outputMeter = juce::jmax (audioIsRunning ? effectProcessor.outputLevel.load (std::memory_order_relaxed) : 0.0f,
                              outputMeter * 0.83f);
    repaint();
}
