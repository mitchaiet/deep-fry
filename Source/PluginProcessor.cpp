// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
float parameterValue(const std::atomic<float>* parameter, float minimum, float maximum, float fallback)
{
    const float value = parameter->load(std::memory_order_relaxed);
    return std::isfinite(value) ? juce::jlimit(minimum, maximum, value) : fallback;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout DeepFryAudioProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percent = juce::AudioParameterFloatAttributes().withLabel("%");
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "quality", 1 }, "JPEG Quality", juce::NormalisableRange<float>(1, 100, 1), 35, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fry", 1 }, "Fry", juce::NormalisableRange<float>(0, 100, 1), 25, percent));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "pixelBits", 1 }, "Pixel Depth", 2, 8, 8, juce::AudioParameterIntAttributes().withLabel("bit")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float>(0, 100, 0.1f), 100, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float>(-24, 6, 0.1f), -3, juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

DeepFryAudioProcessor::DeepFryAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DeepFryState", makeParameters())
{
    qualityParam = parameters.getRawParameterValue("quality");
    fryParam = parameters.getRawParameterValue("fry");
    bitsParam = parameters.getRawParameterValue("pixelBits");
    mixParam = parameters.getRawParameterValue("mix");
    outputParam = parameters.getRawParameterValue("output");
    bypassParam = parameters.getRawParameterValue("bypass");
    setLatencySamples(64);
}

void DeepFryAudioProcessor::prepareToPlay(double sampleRate, int)
{
    tilePosition = visualCounter = 0;
    // FIFO is deliberately not reset here: an open editor may be reading it.
    for (auto& channel : channels)
    {
        channel.incoming.fill(0);
        channel.decoded.fill(0);
        channel.delayedDry.fill(0);
    }
    const auto rate = sampleRate > 0 ? sampleRate : 48000.0;
    visualInterval = std::max(1, static_cast<int>(rate / (64.0 * 60.0)));
    qualitySmooth.reset(rate / 64.0, 0.03);
    frySmooth.reset(rate / 64.0, 0.03);
    mixSmooth.reset(rate, 0.02);
    outputSmooth.reset(rate, 0.02);
    bypassSmooth.reset(rate, 0.01);
    qualitySmooth.setCurrentAndTargetValue(parameterValue(qualityParam, 1, 100, 35));
    frySmooth.setCurrentAndTargetValue(parameterValue(fryParam, 0, 100, 25));
    mixSmooth.setCurrentAndTargetValue(parameterValue(mixParam, 0, 100, 100) * 0.01f);
    outputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(parameterValue(outputParam, -24, 6, -3)));
    bypassSmooth.setCurrentAndTargetValue(parameterValue(bypassParam, 0, 1, 0));
    inputLevel.store(0);
    outputLevel.store(0);
    coefficientRetention.store(0);
    setLatencySamples(64);
}

bool DeepFryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == output;
}

void DeepFryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midi.clear();
    process(buffer, false);
}

void DeepFryAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midi.clear();
    process(buffer, true);
}

void DeepFryAudioProcessor::process(juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int channelCount = std::min({ getTotalNumInputChannels(), buffer.getNumChannels(), 2 });
    for (int channel = channelCount; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    if (channelCount == 0 || buffer.getNumSamples() == 0)
        return;

    qualitySmooth.setTargetValue(parameterValue(qualityParam, 1, 100, 35));
    frySmooth.setTargetValue(parameterValue(fryParam, 0, 100, 25));
    mixSmooth.setTargetValue(parameterValue(mixParam, 0, 100, 100) * 0.01f);
    outputSmooth.setTargetValue(juce::Decibels::decibelsToGain(parameterValue(outputParam, -24, 6, -3)));
    bypassSmooth.setTargetValue(forceBypass ? 1.0f : parameterValue(bypassParam, 0, 1, 0));
    const int bits = static_cast<int>(parameterValue(bitsParam, 2, 8, 8));
    const auto audio = buffer.getArrayOfWritePointers();
    float peakIn = 0, peakOut = 0;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float mix = mixSmooth.getNextValue();
        const float gain = outputSmooth.getNextValue();
        const float active = 1.0f - bypassSmooth.getNextValue();
        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto& state = channels[static_cast<size_t>(channel)];
            const float input = std::isfinite(audio[channel][sample]) ? audio[channel][sample] : 0.0f;
            const auto index = static_cast<size_t>(tilePosition);
            const float dry = state.delayedDry[index];
            const float wet = state.decoded[index];
            state.incoming[index] = input;
            const float processed = (dry * (1.0f - mix) + wet * mix) * gain;
            const float result = active <= 0.0f ? dry : dry * (1.0f - active) + processed * active;
            audio[channel][sample] = std::isfinite(result) ? result : 0.0f;
            peakIn = std::max(peakIn, std::abs(input));
            peakOut = std::max(peakOut, std::abs(audio[channel][sample]));
        }

        if (++tilePosition == 64)
        {
            tilePosition = 0;
            deepfry::CodecSettings settings { qualitySmooth.getNextValue(), frySmooth.getNextValue(), bits };
            const bool capture = ++visualCounter >= visualInterval;
            if (capture)
                visualCounter = 0;
            for (int channel = 0; channel < channelCount; ++channel)
            {
                auto& state = channels[static_cast<size_t>(channel)];
                deepfry::TileFrame frame;
                state.codec.process(state.incoming, state.decoded, settings, capture && channel == 0 ? &frame : nullptr);
                state.delayedDry = state.incoming;
                if (capture && channel == 0)
                {
                    coefficientRetention.store(frame.retained, std::memory_order_relaxed);
                    pushVisualFrame(frame);
                }
            }
        }
    }
    const float decay = std::exp(-static_cast<float>(buffer.getNumSamples()) / static_cast<float>(0.18 * getSampleRate()));
    inputLevel.store(std::max(peakIn, inputLevel.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    outputLevel.store(std::max(peakOut, outputLevel.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
}

void DeepFryAudioProcessor::pushVisualFrame(const deepfry::TileFrame& frame) noexcept
{
    int start1, size1, start2, size2;
    visualFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        visualFrames[static_cast<size_t>(start1)] = frame;
        visualFifo.finishedWrite(1);
    }
}

bool DeepFryAudioProcessor::popVisualFrame(deepfry::TileFrame& frame) noexcept
{
    int start1, size1, start2, size2;
    visualFifo.prepareToRead(1, start1, size1, start2, size2);
    if (size1 == 0)
        return false;
    frame = visualFrames[static_cast<size_t>(start1)];
    visualFifo.finishedRead(1);
    return true;
}

const char* DeepFryAudioProcessor::presetName(int index)
{
    static constexpr const char* names[] { "Clean-ish", "Meme", "Deep fried", "Lost cause" };
    return names[juce::jlimit(0, 3, index)];
}

void DeepFryAudioProcessor::applyPreset(int index)
{
    if (index < 0 || index > 3)
        return;
    static constexpr float values[4][5] {
        { 95, 0, 8, 100, -3 }, { 35, 25, 8, 100, -3 },
        { 8, 65, 6, 100, -6 }, { 1, 100, 3, 100, -9 }
    };
    static constexpr const char* ids[] { "quality", "fry", "pixelBits", "mix", "output" };
    for (int parameter = 0; parameter < 5; ++parameter)
    {
        auto* target = parameters.getParameter(ids[parameter]);
        target->beginChangeGesture();
        target->setValueNotifyingHost(target->convertTo0to1(values[index][parameter]));
        target->endChangeGesture();
    }
    currentProgram.store(index);
}

juce::AudioProcessorParameter* DeepFryAudioProcessor::getBypassParameter() const
{
    return parameters.getParameter("bypass");
}

void DeepFryAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    state.setProperty("preset", currentProgram.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}

void DeepFryAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.isValid())
            {
                for (const auto& child : state)
                    if (parameters.getParameter(child.getProperty("id").toString()) != nullptr
                        && !std::isfinite(static_cast<float>(child.getProperty("value"))))
                        return;
                const float preset = static_cast<float>(state.getProperty("preset", 1));
                currentProgram.store(std::isfinite(preset) ? static_cast<int>(juce::jlimit(0.0f, 3.0f, preset)) : 1);
                parameters.replaceState(state);
            }
        }
}

juce::AudioProcessorEditor* DeepFryAudioProcessor::createEditor()
{
    return new DeepFryAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeepFryAudioProcessor();
}
