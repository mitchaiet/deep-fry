// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "VisualFrame.h"
#include <array>
#include <atomic>

class DeepFryAudioProcessor final : public juce::AudioProcessor
{
public:
    DeepFryAudioProcessor();
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Deep Fry"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 64.0 / (getSampleRate() > 0 ? getSampleRate() : 48000.0); }
    int getNumPrograms() override { return 4; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int index) override { applyPreset(index); }
    const juce::String getProgramName(int index) override { return presetName(index); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    void applyPreset(int index);
    static const char* presetName(int index);
    bool popVisualFrame(deepfry::VisualFrame&) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, coefficientRetention { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void process(juce::AudioBuffer<float>&, bool forceBypass);
    void pushVisualFrame(const deepfry::VisualFrame&) noexcept;

    struct Channel
    {
        deepfry::JpegCodec codec;
        std::array<float, 64> incoming {}, decoded {}, delayedDry {};
    };
    std::array<Channel, 2> channels;
    int tilePosition = 0, visualCounter = 0, visualInterval = 12;
    std::uint64_t inputSamplePosition = 0;
    std::uint64_t visualStreamGeneration = 0;
    double visualSampleRate = 48000.0;
    deepfry::VisualFrame pendingVisualFrame;
    bool hasPendingVisualFrame = false;
    static constexpr int visualCapacity = 128;
    juce::AbstractFifo visualFifo { visualCapacity };
    std::array<deepfry::VisualFrame, visualCapacity> visualFrames {};
    juce::SmoothedValue<float> qualitySmooth, frySmooth, mixSmooth, outputSmooth, bypassSmooth;
    std::atomic<float>* qualityParam = nullptr;
    std::atomic<float>* fryParam = nullptr;
    std::atomic<float>* bitsParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<int> currentProgram { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepFryAudioProcessor)
};
