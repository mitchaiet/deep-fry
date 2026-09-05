#pragma once

#include "PluginProcessor.h"

class DeepFryLookAndFeel;
class DeepFryArtwork;

class DeepFryAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit DeepFryAudioProcessorEditor (DeepFryAudioProcessor&);
    ~DeepFryAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void rebuildImages();
    void drawImagePanel (juce::Graphics&, juce::Rectangle<float>, bool processed);
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, float level,
                    const juce::String& label);
    juce::Rectangle<int> scaledBounds (juce::Rectangle<float>) const;

    DeepFryAudioProcessor& effectProcessor;
    std::unique_ptr<DeepFryLookAndFeel> lookAndFeel;
    std::unique_ptr<DeepFryArtwork> artwork;
    juce::TooltipWindow tooltips { this, 550 };
    std::array<juce::Slider, 5> knobs;
    std::array<std::unique_ptr<SliderAttachment>, 5> knobAttachments;
    std::array<juce::TextButton, 4> presetButtons;
    juce::TextButton bypassButton { "EFFECT ON" };
    juce::TextButton freezeButton { "FREEZE" };
    juce::TextButton helpButton { "?" };
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    std::array<deepfry::TileFrame, 128> tileHistory {};
    juce::Image beforeImage { juce::Image::RGB, 128, 64, true };
    juce::Image afterImage { juce::Image::RGB, 128, 64, true };
    size_t nextTile = 0;
    size_t tileCount = 0;
    bool frozen = false;
    bool helpOpen = false;
    bool applyingPreset = false;
    int ticksSinceFrame = 30;
    float inputMeter = 0.0f;
    float outputMeter = 0.0f;
    float retention = 1.0f;
    float contentScale = 1.0f;
    float contentOffsetX = 0.0f;
    float contentOffsetY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepFryAudioProcessorEditor)
};
