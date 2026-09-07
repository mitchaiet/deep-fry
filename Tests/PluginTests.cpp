// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "VisualizationExport.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Exact comparisons intentionally verify lossless dry/bypass paths, saved
// parameter preservation, silence, and bit-identical host block partitioning.
#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr int latency = 64;
int checks = 0;
using Audio = std::vector<std::vector<float>>;

void require(bool condition, const std::string& description)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(description);
}

void setParameter(DeepFryAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    require(parameter != nullptr, std::string("parameter exists: ") + id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void prepare(DeepFryAudioProcessor& processor, int channelCount = 2, double rate = sampleRate)
{
    auto layout = processor.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
    layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
    require(processor.setBusesLayout(layout), "requested processing layout is accepted");
    processor.setRateAndBufferSizeDetails(rate, 511);
    processor.prepareToPlay(rate, 511);
}

Audio stimulus(int channels = 2, int length = 8197)
{
    Audio result(static_cast<std::size_t>(channels), std::vector<float>(static_cast<std::size_t>(length)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < length; ++i)
            result[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = static_cast<float>(
                0.37 * std::sin(i * (0.073 + channel * 0.037))
                + 0.19 * std::sin(i * (0.391 + channel * 0.021))
                + 0.08 * std::cos(i * 1.179));
    return result;
}

Audio processSamples(DeepFryAudioProcessor& processor, const Audio& input,
                     const std::vector<int>& blockPattern = { 511 },
                     bool automate = false, bool hostBypass = false,
                     std::vector<deepfry::VisualFrame>* capturedFrames = nullptr)
{
    require(!input.empty() && !blockPattern.empty(), "processing input and partition are nonempty");
    require(std::any_of(blockPattern.begin(), blockPattern.end(), [](int size) { return size > 0; }),
            "partition advances through samples");
    const int length = static_cast<int>(input.front().size());
    const int channels = static_cast<int>(input.size());
    Audio output(input.size(), std::vector<float>(input.front().size()));
    int position = 0;
    std::size_t block = 0;
    bool automationApplied = false;
    while (position < length)
    {
        if (automate && !automationApplied && position == 1024)
        {
            setParameter(processor, "quality", 4);
            setParameter(processor, "fry", 89);
            setParameter(processor, "mix", 63);
            setParameter(processor, "output", -8);
            automationApplied = true;
        }
        int size = std::min(blockPattern[block++ % blockPattern.size()], length - position);
        if (automate && !automationApplied && position < 1024)
            size = std::min(size, 1024 - position);
        juce::AudioBuffer<float> buffer(channels, size);
        for (int channel = 0; channel < channels; ++channel)
            if (size > 0)
                buffer.copyFrom(channel, 0, input[static_cast<std::size_t>(channel)].data() + position, size);
        juce::MidiBuffer midi;
        if (hostBypass)
            processor.processBlockBypassed(buffer, midi);
        else
            processor.processBlock(buffer, midi);
        for (int channel = 0; channel < channels; ++channel)
            if (size > 0)
                std::copy_n(buffer.getReadPointer(channel), size,
                            output[static_cast<std::size_t>(channel)].begin() + position);
        position += size;
        if (capturedFrames != nullptr)
        {
            deepfry::VisualFrame frame;
            while (processor.popVisualFrame(frame))
                capturedFrames->push_back(frame);
        }
    }
    return output;
}

void checkFinite(const Audio& audio)
{
    for (const auto& channel : audio)
        for (float sample : channel)
            require(std::isfinite(sample), "processor output is finite");
}

void latencyAndBypass()
{
    const auto input = stimulus();
    for (int mode = 0; mode < 3; ++mode)
    {
        DeepFryAudioProcessor processor;
        setParameter(processor, "mix", mode == 0 ? 0.0f : 100.0f);
        setParameter(processor, "output", mode == 0 ? 0.0f : -19.0f);
        setParameter(processor, "bypass", mode == 0 ? 0.0f : 1.0f);
        prepare(processor);
        require(processor.getLatencySamples() == latency, "processor reports exactly 64 samples of latency");
        const auto output = processSamples(processor, input, { 1, 0, 17, 64, 511 }, false, mode == 2);
        for (std::size_t channel = 0; channel < input.size(); ++channel)
            for (std::size_t i = 0; i < input[channel].size(); ++i)
                require(output[channel][i] == (i < latency ? 0.0f : input[channel][i - latency]),
                        "dry mix, parameter bypass, and host bypass preserve exact latency-aligned samples");
    }

    // The host can enter its bypass callback independently of the parameter.
    // After the documented smoothing interval it must be dry and gain neutral.
    DeepFryAudioProcessor processor;
    setParameter(processor, "output", -19);
    prepare(processor);
    const auto output = processSamples(processor, input, { 17, 511 }, false, true);
    for (std::size_t channel = 0; channel < input.size(); ++channel)
        for (std::size_t i = 1024; i < input[channel].size(); ++i)
            require(output[channel][i] == input[channel][i - latency], "host bypass settles to exact delayed dry audio");
}

void layoutsAndChannels()
{
    DeepFryAudioProcessor processor;
    auto layout = processor.getBusesLayout();
    for (int channelCount : { 1, 2 })
    {
        layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
        layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
        require(processor.isBusesLayoutSupported(layout), "matched mono and stereo layouts are supported");
    }
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    require(!processor.isBusesLayoutSupported(layout), "mismatched input and output layouts are rejected");
    layout.inputBuses.set(0, juce::AudioChannelSet::create5point1());
    layout.outputBuses.set(0, juce::AudioChannelSet::create5point1());
    require(!processor.isBusesLayoutSupported(layout), "surround layouts are rejected");
    layout.inputBuses.set(0, juce::AudioChannelSet::disabled());
    layout.outputBuses.set(0, juce::AudioChannelSet::disabled());
    require(!processor.isBusesLayoutSupported(layout), "disabled main buses are rejected");

    for (int activeChannel = 0; activeChannel < 2; ++activeChannel)
    {
        auto input = stimulus();
        std::fill(input[static_cast<std::size_t>(1 - activeChannel)].begin(),
                  input[static_cast<std::size_t>(1 - activeChannel)].end(), 0.0f);
        processor.applyPreset(3);
        prepare(processor);
        const auto output = processSamples(processor, input, { 17, 511 });
        for (float sample : output[static_cast<std::size_t>(1 - activeChannel)])
            require(sample == 0.0f, "stereo processing never leaks the active channel into the silent channel");
        require(std::any_of(output[static_cast<std::size_t>(activeChannel)].begin(),
                            output[static_cast<std::size_t>(activeChannel)].end(),
                            [](float sample) { return std::abs(sample) > 0.01f; }), "active channel produces audio");
    }
    prepare(processor, 1);
    checkFinite(processSamples(processor, stimulus(1), { 1, 64, 511 }));
    juce::AudioBuffer<float> buffer(1, 17);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(buffer, midi);
    require(midi.isEmpty(), "audio effect does not emit incoming MIDI");
}

void partitionInvariance()
{
    const auto input = stimulus(2, 10037);
    for (bool automate : { false, true })
    {
        DeepFryAudioProcessor referenceProcessor;
        prepare(referenceProcessor);
        const auto reference = processSamples(referenceProcessor, input, { 511 }, automate);
        for (const auto& pattern : std::vector<std::vector<int>> { { 1 }, { 17 }, { 64 }, { 0, 1, 17, 0, 64, 511 } })
        {
            DeepFryAudioProcessor processor;
            prepare(processor);
            const auto actual = processSamples(processor, input, pattern, automate);
            require(actual == reference, "audio is bit-identical across arbitrary host block sizes and zero blocks, including automation");
        }
    }
}

void presetsAndSilence()
{
    const auto input = stimulus();
    std::array<Audio, 4> processed;
    for (int preset = 0; preset < 4; ++preset)
    {
        DeepFryAudioProcessor processor;
        processor.applyPreset(preset);
        require(processor.getCurrentProgram() == preset, "preset updates the host program index");
        require(processor.getProgramName(preset).isNotEmpty(), "each host program has a name");
        prepare(processor);
        const Audio silent(2, std::vector<float>(4097));
        const auto silentOutput = processSamples(processor, silent, { 1, 17, 64, 511 });
        for (const auto& channel : silentOutput)
            for (float sample : channel)
                require(sample == 0.0f, "all presets preserve exact silence without DC or idle noise");

        setParameter(processor, "output", 0);
        prepare(processor);
        processed[static_cast<std::size_t>(preset)] = processSamples(processor, input);
        checkFinite(processed[static_cast<std::size_t>(preset)]);
        double error = 0;
        for (std::size_t i = latency; i < input[0].size(); ++i)
        {
            const double difference = processed[static_cast<std::size_t>(preset)][0][i] - input[0][i - latency];
            error += difference * difference;
        }
        const double rms = std::sqrt(error / static_cast<double>(input[0].size() - latency));
        require(rms > 0.001, "each preset changes audio beyond floating point error even with neutral output gain");
        std::cout << "Preset " << processor.getProgramName(preset) << " input/output RMS difference: " << rms << '\n';
    }
    for (std::size_t i = 0; i < processed.size(); ++i)
        for (std::size_t j = i + 1; j < processed.size(); ++j)
            require(processed[i] != processed[j], "all factory presets have distinct audio output");
}

void stateRecall()
{
    static constexpr std::array<const char*, 6> ids { "quality", "fry", "pixelBits", "mix", "output", "bypass" };
    static constexpr std::array<float, 6> values { 73.0f, 46.0f, 4.0f, 57.3f, -7.2f, 1.0f };
    DeepFryAudioProcessor source;
    source.applyPreset(3);
    for (std::size_t i = 0; i < ids.size(); ++i)
        setParameter(source, ids[i], values[i]);
    require(source.getBypassParameter() == source.parameters.getParameter("bypass"), "host bypass maps to the saved bypass parameter");
    juce::MemoryBlock state;
    source.getStateInformation(state);
    require(state.getSize() > 0, "processor serializes a nonempty state");
    DeepFryAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(restored.getCurrentProgram() == 3, "state recall restores the program index");
    for (const auto* id : ids)
        require(source.parameters.getRawParameterValue(id)->load() == restored.parameters.getRawParameterValue(id)->load(),
                std::string("state recall preserves customized automation parameter: ") + id);

    const std::array<char, 17> corrupt { 'n', 'o', 't', '-', 'a', '-', 'p', 'l', 'u', 'g', 'i', 'n', '-', 's', 't', 'a', 't' };
    restored.setStateInformation(corrupt.data(), static_cast<int>(corrupt.size()));
    restored.setStateInformation(state.getData(), 2);
    require(restored.getCurrentProgram() == 3, "corrupt state leaves the existing program intact");
    for (const auto* id : ids)
        require(source.parameters.getRawParameterValue(id)->load() == restored.parameters.getRawParameterValue(id)->load(),
                "corrupt or truncated state leaves parameters intact");
    for (const auto* damagedId : { "mix", "output", "pixelBits" })
        for (const auto* invalidValue : { "nan", "inf", "-inf" })
        {
            auto malformed = source.parameters.copyState();
            malformed.setProperty("preset", 0, nullptr);
            bool found = false;
            for (auto parameter : malformed)
                if (parameter.getProperty("id").toString() == damagedId)
                {
                    parameter.setProperty("value", invalidValue, nullptr);
                    found = true;
                }
            require(found, "malformed-state regression changes a real saved PARAM child");
            auto xml = malformed.createXml();
            juce::MemoryBlock malformedBinary;
            juce::AudioProcessor::copyXmlToBinary(*xml, malformedBinary);
            restored.setStateInformation(malformedBinary.getData(), static_cast<int>(malformedBinary.getSize()));
            require(restored.getCurrentProgram() == 3, "nonfinite parameter state is rejected before changing the saved program");
            for (const auto* id : ids)
                require(source.parameters.getRawParameterValue(id)->load() == restored.parameters.getRawParameterValue(id)->load(),
                        "matching state XML with a nonfinite PARAM leaves all parameters intact");
        }
    restored.applyPreset(-1);
    restored.applyPreset(4);
    require(restored.getCurrentProgram() == 3, "invalid program selection leaves the current program intact");
    prepare(source);
    prepare(restored);
    const auto input = stimulus();
    require(processSamples(source, input) == processSamples(restored, input), "restored state produces identical audio");
}

std::vector<deepfry::VisualFrame> drainVisualFrames(DeepFryAudioProcessor& processor)
{
    std::vector<deepfry::VisualFrame> frames;
    deepfry::VisualFrame frame;
    while (processor.popVisualFrame(frame))
        frames.push_back(frame);
    require(!processor.popVisualFrame(frame), "visual queue cleanly reports empty after draining");
    return frames;
}

void checkCapturedFrames(const std::vector<deepfry::VisualFrame>& frames,
                         const Audio& input, const Audio& output, bool wetOnly = false, double rate = sampleRate)
{
    require(!frames.empty(), "processor supplies completed visualization frames");
    std::uint64_t previousPosition = 0;
    bool first = true;
    for (const auto& frame : frames)
    {
        require(frame.channelCount == static_cast<int>(input.size()), "visual frame identifies the active mono or stereo layout");
        require(frame.sampleRate == rate, "visual frame carries the processing sample rate");
        require(frame.streamGeneration == frames.front().streamGeneration,
                "all frames captured during one uninterrupted prepare session identify the same audio stream");
        require(frame.samplePosition % latency == 0, "visual source position begins at an audio tile boundary");
        require(first || frame.samplePosition > previousPosition, "published visual tiles retain chronological sample positions");
        previousPosition = frame.samplePosition;
        first = false;
        require(frame.samplePosition + latency * 2 <= output.front().size(),
                "visual frame is published only after its entire delayed output tile has been emitted");
        for (std::size_t channel = 0; channel < input.size(); ++channel)
        {
            const auto& captured = frame.channels[channel];
            require(std::isfinite(captured.image.retained) && captured.image.retained >= 0 && captured.image.retained <= 1,
                    "each channel has a finite visual coefficient-retention fraction");
            for (std::size_t i = 0; i < captured.output.size(); ++i)
            {
                const auto sourceIndex = static_cast<std::size_t>(frame.samplePosition) + i;
                const auto outputIndex = sourceIndex + latency;
                const float source = std::isfinite(input[channel][sourceIndex]) ? input[channel][sourceIndex] : 0.0f;
                require(std::isfinite(captured.image.before[i]) && captured.image.before[i] >= 0 && captured.image.before[i] <= 255,
                        "live source pixels are finite luminance values");
                require(std::isfinite(captured.image.after[i]) && captured.image.after[i] >= 0 && captured.image.after[i] <= 255,
                        "live JPEG pixels are finite luminance values");
                require(captured.image.before[i] == 128.0f + std::round(std::clamp(source, -1.0f, 1.0f) * 127.0f),
                        "each channel's input image is derived from its own actual source samples");
                require(std::isfinite(captured.output[i]) && captured.output[i] == output[channel][outputIndex],
                        "final-output visualization exactly matches the delayed audible sample after Mix, gain, and bypass");
                if (wetOnly)
                    require(std::abs(captured.image.after[i] - (128.0f + captured.output[i] * 127.0f)) < 0.0001f,
                            "JPEG pixels map back to the corresponding audible sample at 100 percent wet and unity gain");
            }
        }
    }
}

void visualizationAndInvalidInput()
{
    const auto input = stimulus(2, 32768);
    for (int mode = 0; mode < 5; ++mode)
    {
        DeepFryAudioProcessor processor;
        setParameter(processor, "mix", mode == 0 ? 100.0f : mode == 1 ? 0.0f : 37.0f);
        setParameter(processor, "output", mode == 0 ? 0.0f : -9.0f);
        setParameter(processor, "bypass", mode == 3 ? 1.0f : 0.0f);
        prepare(processor);
        const auto output = processSamples(processor, input, { 0, 1, 17, 64, 511 }, mode == 2, mode == 4);
        const auto frames = drainVisualFrames(processor);
        require(frames.size() > 30, "processor supplies a continuous stream of stereo visual frames");
        checkCapturedFrames(frames, input, output, mode == 0);
        require(std::isfinite(processor.inputLevel.load()) && processor.inputLevel.load() > 0,
                "input meter receives a finite live level");
        require(std::isfinite(processor.outputLevel.load()) && processor.outputLevel.load() > 0,
                "output meter receives a finite live level");
    }

    // A bypass transition inside a selected output tile must be represented
    // sample for sample, including smoothing and host bypass's gain-neutral path.
    for (bool hostBypass : { false, true })
    {
        DeepFryAudioProcessor processor;
        setParameter(processor, "mix", 59);
        setParameter(processor, "output", -17);
        prepare(processor);
        Audio output(2);
        int position = 0;
        for (const int end : { 1559, 2317, static_cast<int>(input.front().size()) })
        {
            Audio section(2);
            for (std::size_t channel = 0; channel < input.size(); ++channel)
                section[channel].assign(input[channel].begin() + position, input[channel].begin() + end);
            const bool bypassed = position == 1559;
            if (!hostBypass)
                setParameter(processor, "bypass", bypassed ? 1.0f : 0.0f);
            const auto rendered = processSamples(processor, section, { 17, 0, 1, 511 }, false, hostBypass && bypassed);
            for (std::size_t channel = 0; channel < output.size(); ++channel)
                output[channel].insert(output[channel].end(), rendered[channel].begin(), rendered[channel].end());
            position = end;
        }
        checkCapturedFrames(drainVisualFrames(processor), input, output);
    }

    // Pixel colors saturate at full scale, but captured final audio must retain
    // finite values above it so the inspector can report actual output headroom.
    DeepFryAudioProcessor overrange;
    setParameter(overrange, "mix", 0);
    setParameter(overrange, "output", 6);
    prepare(overrange);
    auto hotInput = stimulus(2, 4096);
    for (auto& channel : hotInput)
        for (auto& sample : channel)
            sample *= 4.0f;
    const auto hotOutput = processSamples(overrange, hotInput, { 1, 511, 17 });
    const auto hotFrames = drainVisualFrames(overrange);
    checkCapturedFrames(hotFrames, hotInput, hotOutput);
    bool sawOverrange = false;
    for (const auto& frame : hotFrames)
        for (const auto& channel : frame.channels)
            for (float sample : channel.output)
                sawOverrange = sawOverrange || std::abs(sample) > 1.0f;
    require(sawOverrange, "final-output capture preserves samples above full scale without clipping them to image bounds");

    // Draining the display on every block and leaving its bounded FIFO full must
    // produce identical audio, even when automation crosses irregular buffers.
    DeepFryAudioProcessor unattended, observed;
    prepare(unattended);
    prepare(observed);
    const auto longInput = stimulus(2, 131072);
    const auto unattendedOutput = processSamples(unattended, longInput, { 17, 0, 511 }, true);
    std::vector<deepfry::VisualFrame> observedFrames;
    const auto observedOutput = processSamples(observed, longInput, { 17, 0, 511 }, true, false, &observedFrames);
    require(unattendedOutput == observedOutput, "a full visualization FIFO never changes or interrupts audio");
    const auto queuedFrames = drainVisualFrames(unattended);
    require(!queuedFrames.empty() && queuedFrames.size() <= 127, "visual queue remains bounded when the editor is closed");
    require(observedFrames.size() > queuedFrames.size(), "an unattended full queue drops visual frames while processing continues");
    checkCapturedFrames(queuedFrames, longInput, unattendedOutput);
    checkCapturedFrames(observedFrames, longInput, observedOutput);

    auto damaged = stimulus();
    damaged[0][704] = std::numeric_limits<float>::quiet_NaN();
    damaged[0][705] = std::numeric_limits<float>::infinity();
    damaged[1][711] = -std::numeric_limits<float>::infinity();
    prepare(observed);
    const auto sanitizedOutput = processSamples(observed, damaged, { 17, 511 });
    checkFinite(sanitizedOutput);
    checkCapturedFrames(drainVisualFrames(observed), damaged, sanitizedOutput);
}

void visualizationPublicationAndReset()
{
    DeepFryAudioProcessor processor;
    prepare(processor);
    const auto input = stimulus(2, 832);
    Audio output(2);
    int position = 0;
    for (const int end : { 768, 831, 832 })
    {
        Audio section(2);
        for (std::size_t channel = 0; channel < input.size(); ++channel)
            section[channel].assign(input[channel].begin() + position, input[channel].begin() + end);
        const auto rendered = processSamples(processor, section, { 17, 0, 1, 511 });
        for (std::size_t channel = 0; channel < output.size(); ++channel)
            output[channel].insert(output[channel].end(), rendered[channel].begin(), rendered[channel].end());
        if (end < 832)
        {
            deepfry::VisualFrame frame;
            require(!processor.popVisualFrame(frame), "an incomplete final-output tile is never exposed to the editor");
        }
        position = end;
    }
    const auto completed = drainVisualFrames(processor);
    require(completed.size() == 1 && completed.front().samplePosition == 704,
            "first visualization includes source tile 704 through 767 and publishes after output sample 831");
    checkCapturedFrames(completed, input, output);

    // Re-prepare while half the pending output tile is captured. None of that
    // previous stereo session may leak into the newly prepared mono frame.
    prepare(processor);
    processSamples(processor, stimulus(2, 800));
    prepare(processor, 1);
    const auto monoInput = stimulus(1, 832);
    const auto monoOutput = processSamples(processor, monoInput, { 0, 17, 511 });
    const auto monoFrames = drainVisualFrames(processor);
    require(monoFrames.size() == 1 && monoFrames.front().samplePosition == 704,
            "prepare discards a partial capture and restarts the source sample timeline");
    require(monoFrames.front().streamGeneration != completed.front().streamGeneration,
            "prepare identifies a new visual stream even when sample positions repeat");
    checkCapturedFrames(monoFrames, monoInput, monoOutput);
    for (float sample : monoFrames.front().channels[1].output)
        require(sample == 0.0f, "mono visual frames contain no stale output from the previous right channel");

    // At low sample rates every tile is selected, so completing one output
    // capture and opening the next must work at the same sample boundary.
    for (double rate : { 3200.0, 44100.0, 96000.0, 192000.0 })
    {
        prepare(processor, 2, rate);
        const auto rateInput = stimulus(2, 8192);
        std::vector<deepfry::VisualFrame> frames;
        const auto rateOutput = processSamples(processor, rateInput, { 0, 1, 17, 511 }, true, false, &frames);
        checkCapturedFrames(frames, rateInput, rateOutput, false, rate);
        if (rate == 3200.0)
            require(frames.size() == 127 && frames.front().samplePosition == 0,
                    "back-to-back capture retains every completed tile at the minimum visual interval");
    }
}

Audio musicalDemo()
{
    constexpr int seconds = 8;
    const int length = static_cast<int>(sampleRate) * seconds;
    Audio result(2, std::vector<float>(static_cast<std::size_t>(length)));
    constexpr double beatDuration = 60.0 / 110.0;
    constexpr std::array<double, 4> roots { 73.41619198, 65.40639133, 87.30705786, 55.0 };
    std::uint32_t random = 0x4a504547;
    for (int i = 0; i < length; ++i)
    {
        const double time = i / sampleRate;
        const int beat = static_cast<int>(time / beatDuration);
        const double beatTime = std::fmod(time, beatDuration);
        const double eighthTime = std::fmod(time, beatDuration * 0.5);
        const double root = roots[static_cast<std::size_t>((beat / 4) % 4)];
        random = random * 1664525u + 1013904223u;
        const double noise = static_cast<double>(random >> 8) / 8388607.5 - 1.0;
        const double kick = 0.47 * std::exp(-beatTime * 19.0)
            * std::sin(2.0 * pi * (48.0 * beatTime + 85.0 * (1.0 - std::exp(-beatTime * 35.0)) / 35.0));
        const double snare = beat % 2 == 1 ? 0.20 * std::exp(-beatTime * 28.0)
            * (noise * 0.75 + 0.25 * std::sin(2.0 * pi * 185.0 * beatTime)) : 0.0;
        const double hat = 0.048 * noise * std::exp(-eighthTime * 110.0);
        const double bassEnvelope = (1.0 - std::exp(-beatTime * 150.0)) * std::exp(-beatTime * 3.6);
        const double bass = 0.26 * bassEnvelope * (std::sin(2.0 * pi * root * time)
            + 0.21 * std::sin(2.0 * pi * root * 2.0 * time));
        const double fade = std::min({ 1.0, time / 0.02, (seconds - time) / 0.20 });
        for (std::size_t channel = 0; channel < result.size(); ++channel)
        {
            const double detune = channel == 0 ? 0.999 : 1.001;
            const double chord = 0.042 * (std::sin(2.0 * pi * root * 2.0 * detune * time)
                + std::sin(2.0 * pi * root * 2.0 * std::pow(2.0, 3.0 / 12.0) * detune * time)
                + std::sin(2.0 * pi * root * 3.0 * detune * time));
            result[channel][static_cast<std::size_t>(i)] = static_cast<float>(0.82 * fade * (kick + snare + hat + bass + chord));
        }
    }
    return result;
}

void writeWave(const juce::File& file, const Audio& audio)
{
    std::ofstream stream(file.getFullPathName().toStdString(), std::ios::binary | std::ios::trunc);
    require(stream.good(), "demo WAV output can be opened");
    const auto write16 = [&stream](std::uint16_t value)
    {
        stream.put(static_cast<char>(value & 0xff));
        stream.put(static_cast<char>((value >> 8) & 0xff));
    };
    const auto write32 = [&stream](std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            stream.put(static_cast<char>((value >> shift) & 0xff));
    };
    const auto channelCount = static_cast<std::uint16_t>(audio.size());
    const auto dataBytes = static_cast<std::uint32_t>(audio.front().size() * audio.size() * 2);
    stream.write("RIFF", 4);
    write32(36 + dataBytes);
    stream.write("WAVEfmt ", 8);
    write32(16);
    write16(1);
    write16(channelCount);
    write32(static_cast<std::uint32_t>(sampleRate));
    write32(static_cast<std::uint32_t>(sampleRate) * channelCount * 2);
    write16(static_cast<std::uint16_t>(channelCount * 2));
    write16(16);
    stream.write("data", 4);
    write32(dataBytes);
    for (std::size_t sample = 0; sample < audio.front().size(); ++sample)
        for (const auto& channel : audio)
        {
            const auto pcm = static_cast<std::int16_t>(std::lround(std::clamp(channel[sample], -1.0f, 1.0f) * 32767.0f));
            write16(static_cast<std::uint16_t>(pcm));
        }
    stream.flush();
    require(stream.good(), "demo WAV was written successfully");
}

juce::Component* findEditorControl(juce::AudioProcessorEditor& editor, const char* name)
{
    for (int i = 0; i < editor.getNumChildComponents(); ++i)
        if (editor.getChildComponent(i)->getName() == name)
            return editor.getChildComponent(i);
    return nullptr;
}

void clickEditorButton(juce::AudioProcessorEditor& editor, const char* name)
{
    auto* button = dynamic_cast<juce::Button*>(findEditorControl(editor, name));
    require(button != nullptr, std::string("accessible editor button exists: ") + name);
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
}

void selectEditorChoice(juce::AudioProcessorEditor& editor, const char* name, int itemId)
{
    auto* choice = dynamic_cast<juce::ComboBox*>(findEditorControl(editor, name));
    require(choice != nullptr, std::string("accessible editor choice exists: ") + name);
    choice->setSelectedId(itemId, juce::sendNotificationAsync);
    // Native menu selection notifies asynchronously. Let a visualization timer
    // refresh intervene first, so stale display state cannot undo the choice.
    juce::Thread::sleep(40);
    juce::Timer::callPendingTimersSynchronously();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    require(choice->getSelectedId() == itemId, std::string("requested display choice is selected: ") + name);
}

void checkEffectSelection(DeepFryAudioProcessor& processor, juce::AudioProcessorEditor& editor,
                          bool effectOn, const char* context)
{
    auto* on = dynamic_cast<juce::Button*>(findEditorControl(editor, "Turn effect on"));
    auto* off = dynamic_cast<juce::Button*>(findEditorControl(editor, "Turn effect off"));
    require(on != nullptr && off != nullptr, "effect exposes separate accessible ON and OFF controls");
    require(on->getButtonText() == "ON" && off->getButtonText() == "OFF",
            "effect control labels retain their meaning in both states");
    require(on->getToggleState() == effectOn && off->getToggleState() != effectOn
                && processor.parameters.getRawParameterValue("bypass")->load() == (effectOn ? 0.0f : 1.0f),
            std::string(context) + ": selected effect state matches the saved host bypass value");
}

bool imagesEqual(const juce::Image& first, const juce::Image& second)
{
    if (first.getBounds() != second.getBounds() || first.isValid() != second.isValid())
        return false;
    for (int y = 0; y < first.getHeight(); ++y)
        for (int x = 0; x < first.getWidth(); ++x)
            if (first.getPixelAt(x, y) != second.getPixelAt(x, y))
                return false;
    return true;
}

void requireJpegRoundTrip(const juce::File& file, const juce::Image& original, double maximumMeanError)
{
    juce::MemoryBlock bytes;
    require(file.loadFileAsData(bytes) && bytes.getSize() > 4, "JPEG export writes nonempty image data");
    const auto* data = static_cast<const unsigned char*>(bytes.getData());
    require(data[0] == 0xff && data[1] == 0xd8
                && data[bytes.getSize() - 2] == 0xff && data[bytes.getSize() - 1] == 0xd9,
            "JPEG export contains JPEG start and end markers, rather than renamed PNG data");
    juce::JPEGImageFormat jpeg;
    juce::MemoryInputStream input(bytes.getData(), bytes.getSize(), false);
    const auto decoded = jpeg.decodeImage(input);
    require(decoded.isValid() && decoded.getBounds() == original.getBounds(),
            "saved JPEG decodes at the original snapshot dimensions");
    double error = 0.0;
    for (int y = 0; y < original.getHeight(); ++y)
        for (int x = 0; x < original.getWidth(); ++x)
        {
            const auto expected = original.getPixelAt(x, y);
            const auto actual = decoded.getPixelAt(x, y);
            error += std::abs(static_cast<int>(expected.getRed()) - static_cast<int>(actual.getRed()));
            error += std::abs(static_cast<int>(expected.getGreen()) - static_cast<int>(actual.getGreen()));
            error += std::abs(static_cast<int>(expected.getBlue()) - static_cast<int>(actual.getBlue()));
        }
    const double samples = static_cast<double>(original.getWidth()) * original.getHeight() * 3.0;
    require(error / samples < maximumMeanError, "lossy JPEG export preserves the source image within a bounded mean color error");
}

void jpegExportFiles()
{
    juce::TemporaryFile workspace;
    const auto directory = workspace.getFile();
    require(directory.createDirectory().wasOk(), "JPEG export tests create an isolated temporary directory");
    require(deepfry::jpegDestination({}) == juce::File(), "cancelling the chooser keeps an empty JPEG destination");
    for (const char* name : { "image.jpg", "image.jpeg", "image.JPG", "image.JpEg" })
    {
        const auto file = directory.getChildFile(name);
        require(deepfry::jpegDestination(file) == file, "JPEG filename normalization preserves supported extension case and spelling");
    }
    for (const char* name : { "image", "image.png", "image.gif" })
        require(deepfry::jpegDestination(directory.getChildFile(name)) == directory.getChildFile("image.jpg"),
                "JPEG filenames receive the final .jpg extension before overwrite confirmation");

    juce::Image gradient(juce::Image::RGB, 192, 96, true);
    for (int y = 0; y < gradient.getHeight(); ++y)
        for (int x = 0; x < gradient.getWidth(); ++x)
            gradient.setPixelAt(x, y, juce::Colour(static_cast<juce::uint8>(x * 255 / 191),
                                                  static_cast<juce::uint8>(y * 255 / 95),
                                                  static_cast<juce::uint8>((x + y) * 255 / 286)));
    const auto destination = directory.getChildFile("image.jpg");
    const juce::String sentinel("Existing image must survive a failed export.");
    require(destination.replaceWithText(sentinel), "JPEG preservation test creates an existing destination");
    require(!deepfry::writeVisualizationJpeg({}, destination), "JPEG export rejects an invalid image");
    require(destination.loadFileAsString() == sentinel, "an invalid image cannot truncate an existing destination");
    const juce::Image oversized(juce::Image::RGB, 65501, 1, true);
    require(!deepfry::writeVisualizationJpeg(oversized, destination)
                && destination.loadFileAsString() == sentinel,
            "an unsupported JPEG image size leaves the existing destination intact");
    require(!deepfry::writeVisualizationJpeg(gradient, {}), "JPEG export rejects a cancelled destination");
    require(!deepfry::writeVisualizationJpeg(gradient, destination.getChildFile("nested.jpg"))
                && destination.loadFileAsString() == sentinel,
            "JPEG export cannot replace an existing file used as a parent directory");
    const auto occupiedDirectory = directory.getChildFile("folder.jpg");
    require(occupiedDirectory.createDirectory().wasOk(), "JPEG destination collision test creates a directory");
    const auto marker = occupiedDirectory.getChildFile("keep.txt");
    require(marker.replaceWithText(sentinel), "JPEG destination collision test stores an existing child");
    require(!deepfry::writeVisualizationJpeg(gradient, occupiedDirectory) && marker.loadFileAsString() == sentinel,
            "JPEG export never replaces a directory or its contents");
    require(deepfry::writeVisualizationJpeg(gradient, destination), "JPEG export safely replaces an approved existing file");
    requireJpegRoundTrip(destination, gradient, 8.0);

    const auto exactDestination = directory.getChildFile("approved.custom");
    const auto differentDestination = deepfry::jpegDestination(exactDestination);
    require(differentDestination.replaceWithText(sentinel), "exact-path regression creates the unapproved normalized destination");
    require(deepfry::writeVisualizationJpeg(gradient, exactDestination), "JPEG writer writes exactly the approved destination");
    requireJpegRoundTrip(exactDestination, gradient, 8.0);
    require(differentDestination.loadFileAsString() == sentinel,
            "JPEG writer never silently normalizes a path and overwrites a different file");
    require(directory.findChildFiles(juce::File::findFilesAndDirectories, false).size() == 4,
            "JPEG export removes temporary files after successful and rejected writes");
}

void inspectImageTile(juce::AudioProcessorEditor& editor, float designX = 450.0f, float designY = 210.0f)
{
    // Use an actual click inside the image, translated from the editor's design
    // coordinates, so inspection exercises the same path as a user's mouse.
    const float scale = std::min(static_cast<float>(editor.getWidth()) / 1120.0f,
                                 static_cast<float>(editor.getHeight()) / 800.0f);
    const juce::Point<float> point {
        (static_cast<float>(editor.getWidth()) - 1120.0f * scale) * 0.5f + designX * scale,
        (static_cast<float>(editor.getHeight()) - 800.0f * scale) * 0.5f + designY * scale
    };
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), point,
                                 juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &editor, &editor, now, point, now, 1, false);
    editor.mouseDown(click);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto* freeze = dynamic_cast<juce::Button*>(findEditorControl(editor, "Freeze visualization"));
    require(freeze != nullptr && freeze->getToggleState(), "clicking an image tile freezes its matching input and processed history for inspection");
}

void captureEditor(juce::Component& editor, const juce::File& screenshotFile)
{
    const auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    require(snapshot.isValid() && snapshot.getWidth() == editor.getWidth()
                && snapshot.getHeight() == editor.getHeight(),
            "native editor renders a full-resolution snapshot");
    auto imageStream = screenshotFile.createOutputStream();
    require(imageStream != nullptr && imageStream->openedOk(), "screenshot output can be opened");
    imageStream->setPosition(0);
    imageStream->truncate();
    juce::PNGImageFormat png;
    require(png.writeImageToStream(snapshot, *imageStream), "native editor screenshot is encoded as PNG");
    imageStream->flush();
}

void captureButtonAppearance(juce::AudioProcessorEditor& editor, const char* name,
                             juce::Button::ButtonState state, const juce::File& screenshotFile)
{
    auto* button = dynamic_cast<juce::TextButton*>(findEditorControl(editor, name));
    require(button != nullptr, std::string("native button appearance capture exists: ") + name);
    const auto previousState = button->getState();
    // JUCE's appearance state API does not click or change the toggle value.
    // Capture synchronously so mouse dispatch cannot reset the forced state.
    button->setState(state);
    captureEditor(editor, screenshotFile);
    button->setState(previousState);
}

void captureChoiceMenu(juce::AudioProcessorEditor& editor, const char* name, const juce::File& screenshotFile)
{
    auto* choice = dynamic_cast<juce::ComboBox*>(findEditorControl(editor, name));
    require(choice != nullptr, std::string("display menu exists: ") + name);
    auto& desktop = juce::Desktop::getInstance();
    std::vector<juce::Component*> existingWindows;
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        existingWindows.push_back(desktop.getComponent(i));
    choice->showPopup();
    // JUCE creates its native popup synchronously. Capture it before dispatch:
    // this console verifier has no foreground host window, so JUCE's normal
    // application-focus handling may otherwise dismiss the menu immediately.
    require(choice->isPopupActive(), std::string("display menu opens: ") + name);
    bool captured = false;
    for (int i = 0; i < desktop.getNumComponents(); ++i)
    {
        auto* window = desktop.getComponent(i);
        if (window->isVisible() && std::find(existingWindows.begin(), existingWindows.end(), window) == existingWindows.end())
        {
            captureEditor(*window, screenshotFile);
            captured = true;
            break;
        }
    }
    require(captured, std::string("display menu renders its choices in a native popup: ") + name);
    choice->hidePopup();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    require(!choice->isPopupActive(), std::string("display menu dismisses cleanly: ") + name);
}

void visualizationRestartWhileFrozen()
{
    DeepFryAudioProcessor processor, reference;
    for (auto* candidate : { &processor, &reference })
    {
        setParameter(*candidate, "mix", 0);
        setParameter(*candidate, "output", 0);
    }
    prepare(processor);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* visualEditor = dynamic_cast<DeepFryAudioProcessorEditor*>(editor.get());
    require(visualEditor != nullptr, "stream restart regression creates the real visualization editor");
    processSamples(processor, Audio(2, std::vector<float>(4096, -0.5f)));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    const auto beforeRestart = visualEditor->createVisualizationSnapshot();
    require(beforeRestart.isValid(), "stream restart regression begins with real negative-amplitude image history");
    clickEditorButton(*editor, "Freeze visualization");

    prepare(processor);
    prepare(reference);
    const Audio newStream(2, std::vector<float>(16384, 0.5f));
    processSamples(processor, newStream);
    processSamples(reference, newStream);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    require(imagesEqual(beforeRestart, visualEditor->createVisualizationSnapshot()),
            "frozen image history remains unchanged while a restarted audio stream advances beyond the old sample positions");

    // The reference editor starts with no history after the restart has already
    // advanced. Its next two tiles are exactly what the resumed editor should
    // retain, with all previous-generation imagery discarded.
    const auto discardedReferenceFrames = drainVisualFrames(reference);
    require(!discardedReferenceFrames.empty() && discardedReferenceFrames.back().samplePosition > 4096,
            "new stream has overtaken the complete previous timeline before visualization resumes");
    std::unique_ptr<juce::AudioProcessorEditor> referenceEditor(reference.createEditor());
    auto* referenceVisualEditor = dynamic_cast<DeepFryAudioProcessorEditor*>(referenceEditor.get());
    require(referenceVisualEditor != nullptr && !referenceVisualEditor->createVisualizationSnapshot().isValid(),
            "reference editor starts without any historical tiles");
    clickEditorButton(*editor, "Freeze visualization");
    const Audio resumedInput(2, std::vector<float>(1536, 0.5f));
    require(processSamples(processor, resumedInput) == processSamples(reference, resumedInput),
            "freezing across stream restart and then resuming never changes the audio");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    const auto resumedSnapshot = visualEditor->createVisualizationSnapshot();
    const auto referenceSnapshot = referenceVisualEditor->createVisualizationSnapshot();
    require(resumedSnapshot.isValid() && referenceSnapshot.isValid(), "both editors receive fresh tiles after visualization resumes");
    require(imagesEqual(resumedSnapshot, referenceSnapshot),
            "resuming after stream restart discards all old tiles even when the new sample positions exceed the old history");
    require(!imagesEqual(beforeRestart, resumedSnapshot), "resumed history displays the newly prepared stream");
}

void stereoVisualization()
{
    DeepFryAudioProcessor processor;
    setParameter(processor, "mix", 0);
    setParameter(processor, "output", 0);
    prepare(processor);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* visualEditor = dynamic_cast<DeepFryAudioProcessorEditor*>(editor.get());
    require(visualEditor != nullptr, "stereo visualization test creates the real editor");
    auto* stereoButton = dynamic_cast<juce::Button*>(findEditorControl(*editor, "Show stereo channels"));
    require(stereoButton != nullptr, "stereo visualization exposes an accessible combined-channel view");

    // Eighty distinct captures exceed the stereo view's 64 tiles per channel.
    // Constant samples within each tile make the expected lane and time mapping
    // unambiguous while retaining different left and right audio throughout.
    constexpr int capturedTileCount = 80;
    constexpr int samplesPerCapture = 768;
    Audio input(2, std::vector<float>(capturedTileCount * samplesPerCapture + latency));
    for (std::size_t i = 0; i < input.front().size(); ++i)
    {
        const float tile = static_cast<float>(i / samplesPerCapture);
        input[0][i] = -0.8f + 0.006f * tile;
        input[1][i] = 0.15f + 0.007f * tile;
    }
    processSamples(processor, input);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    require(stereoButton->getToggleState(), "stereo input initially displays both channels together");
    const auto stereo = visualEditor->createVisualizationSnapshot();
    require(stereo.isValid() && stereo.getWidth() == 1080 && stereo.getHeight() == 352,
            "stereo visualization retains the paired export dimensions");

    const auto tilePixel = [](const juce::Image& snapshot, bool processed, int tile,
                              int columns, int lane, int sample)
    {
        const int imageX = lane * 64 + (tile % columns) * 8 + sample % 8;
        const int imageY = (tile / columns) * 8 + sample / 8;
        return snapshot.getPixelAt((processed ? 548 : 20) + imageX * 4 + 2, 54 + imageY * 4 + 2);
    };
    require(tilePixel(stereo, false, 0, 8, 0, 0) != tilePixel(stereo, false, 0, 8, 1, 0),
            "stereo input shows distinct left and right amplitudes simultaneously");
    require(tilePixel(stereo, true, 0, 8, 0, 0) != tilePixel(stereo, true, 0, 8, 1, 0),
            "stereo output shows distinct left and right amplitudes simultaneously");

    clickEditorButton(*editor, "Inspect left channel");
    require(!stereoButton->getToggleState(), "choosing the left channel changes from stereo to a full-width solo view");
    const auto left = visualEditor->createVisualizationSnapshot();
    clickEditorButton(*editor, "Inspect right channel");
    require(!stereoButton->getToggleState(), "choosing the right channel keeps a full-width solo view");
    const auto right = visualEditor->createVisualizationSnapshot();
    for (const bool processed : { false, true })
        for (int tile = 0; tile < 64; ++tile)
            for (int sample = 0; sample < 64; ++sample)
            {
                require(tilePixel(stereo, processed, tile, 8, 0, sample)
                            == tilePixel(left, processed, capturedTileCount - 64 + tile, 16, 0, sample),
                        "stereo left lane contains the latest 64 source-aligned tiles from the full left history");
                require(tilePixel(stereo, processed, tile, 8, 1, sample)
                            == tilePixel(right, processed, capturedTileCount - 64 + tile, 16, 0, sample),
                        "stereo right lane contains the latest 64 source-aligned tiles from the full right history");
            }

    clickEditorButton(*editor, "Show stereo channels");
    // Right lane, row 2, column 3: stereo tile 19, chronological tile 35.
    // This distinguishes both lane selection and the hidden-history offset.
    inspectImageTile(*editor, 889.0f, 301.0f);
    const auto inspected = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    require(inspected.getPixelAt(28, 393) == tilePixel(stereo, false, 19, 8, 1, 0),
            "clicking a right-lane tile freezes the matching right input in the tile inspector");
    require(inspected.getPixelAt(116, 393) == tilePixel(stereo, true, 19, 8, 1, 0),
            "clicking a right-lane tile freezes the matching right output in the tile inspector");
    require(inspected.getPixelAt(28, 393) != tilePixel(stereo, false, 63, 8, 1, 0),
            "right-lane inspection selects the clicked historical tile rather than the latest tile");
    clickEditorButton(*editor, "Freeze visualization");

    prepare(processor, 1);
    processSamples(processor, Audio(1, std::vector<float>(8197, 0.3f)));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    auto* rightButton = dynamic_cast<juce::Button*>(findEditorControl(*editor, "Inspect right channel"));
    require(rightButton != nullptr && !rightButton->isEnabled() && !rightButton->getToggleState(),
            "mono input disables and deselects the unavailable right channel");
    const auto mono = visualEditor->createVisualizationSnapshot();
    require(mono.isValid() && tilePixel(mono, false, 8, 16, 0, 0) == tilePixel(mono, false, 0, 16, 0, 0),
            "mono input uses the full image width instead of reserving a second stereo lane");
    require(tilePixel(mono, true, 8, 16, 0, 0) == tilePixel(mono, true, 0, 16, 0, 0),
            "mono output uses the full image width without duplicating an unavailable channel");
    prepare(processor);
    processSamples(processor, input);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    require(stereoButton->getToggleState() && rightButton->isEnabled(),
            "the stereo view preference returns when a mono host stream becomes stereo again");
    const auto resumed = visualEditor->createVisualizationSnapshot();
    require(tilePixel(resumed, false, 0, 8, 0, 0) == tilePixel(stereo, false, 0, 8, 0, 0)
                && tilePixel(resumed, false, 0, 8, 1, 0) == tilePixel(stereo, false, 0, 8, 1, 0),
            "stereo resumes with correctly aligned new left and right histories after mono");
}

void editorInteractions(DeepFryAudioProcessor& processor, juce::AudioProcessorEditor& editor)
{
    const auto clickButton = [&editor](const char* name)
    {
        clickEditorButton(editor, name);
    };

    clickButton("Load Meme preset");
    require(processor.getCurrentProgram() == 1
                && processor.parameters.getRawParameterValue("quality")->load() == 35.0f,
            "clicking the preset button updates the host program and audio parameters");
    auto* quality = dynamic_cast<juce::Slider*>(findEditorControl(editor, "JPEG quality"));
    require(quality != nullptr, "accessible JPEG quality control exists");
    quality->setValue(22.0, juce::sendNotificationSync);
    require(processor.parameters.getRawParameterValue("quality")->load() == 22.0f,
            "editing a knob updates its automatable audio parameter");
    setParameter(processor, "quality", 47);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    require(quality->getValue() == 47.0, "host parameter automation updates the visible knob");

    checkEffectSelection(processor, editor, true, "initial active effect");
    juce::MemoryBlock effectOnState, effectOffState;
    processor.getStateInformation(effectOnState);
    clickButton("Turn effect on");
    checkEffectSelection(processor, editor, true, "clicking the selected ON choice");
    clickButton("Turn effect off");
    checkEffectSelection(processor, editor, false, "choosing OFF");
    processor.getStateInformation(effectOffState);
    clickButton("Turn effect off");
    checkEffectSelection(processor, editor, false, "clicking the selected OFF choice");

    for (const bool effectOn : { true, false })
    {
        setParameter(processor, "bypass", effectOn ? 0.0f : 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        checkEffectSelection(processor, editor, effectOn, "host parameter automation");
    }
    for (const auto* state : { &effectOnState, &effectOffState })
    {
        processor.setStateInformation(state->getData(), static_cast<int>(state->getSize()));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        checkEffectSelection(processor, editor, state == &effectOnState, "restoring a saved state with an open editor");
    }
    {
        DeepFryAudioProcessor restoredProcessor;
        restoredProcessor.setStateInformation(effectOffState.getData(), static_cast<int>(effectOffState.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> restoredEditor(restoredProcessor.createEditor());
        checkEffectSelection(restoredProcessor, *restoredEditor, false, "creating an editor for an already bypassed effect");
    }
    clickButton("Turn effect on");
    checkEffectSelection(processor, editor, true, "choosing ON after restoring bypass");

    juce::MemoryBlock beforeFreeze;
    processor.getStateInformation(beforeFreeze);
    clickButton("Freeze visualization");
    juce::MemoryBlock afterFreeze;
    processor.getStateInformation(afterFreeze);
    require(beforeFreeze == afterFreeze, "freezing the visualization leaves the saved sound parameters unchanged");
    DeepFryAudioProcessor reference;
    reference.setStateInformation(beforeFreeze.getData(), static_cast<int>(beforeFreeze.getSize()));
    prepare(processor);
    prepare(reference);
    const auto input = stimulus();
    require(processSamples(processor, input) == processSamples(reference, input),
            "audio continues identically while the visualization is frozen");
    clickButton("Freeze visualization");

    auto* visualEditor = dynamic_cast<DeepFryAudioProcessorEditor*>(&editor);
    require(visualEditor != nullptr, "editor exposes its visualization export without an OS file dialog");
    processSamples(processor, input);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    const auto savedSnapshot = visualEditor->createVisualizationSnapshot();
    require(savedSnapshot.isValid(), "live history can be exported as a paired visualization image");
    auto* saveImage = dynamic_cast<juce::Button*>(findEditorControl(editor, "Save visualization JPEG"));
    require(saveImage != nullptr && saveImage->isEnabled(), "JPEG export becomes available after audio supplies image history");
    const auto retainedPixels = savedSnapshot.createCopy();
    juce::TemporaryFile exportedSnapshot(".jpg");
    require(deepfry::writeVisualizationJpeg(savedSnapshot, exportedSnapshot.getFile()),
            "the actual visualization snapshot saves through the JPEG export helper");
    requireJpegRoundTrip(exportedSnapshot.getFile(), savedSnapshot, 20.0);

    // Display controls are local to the editor. Exercise them while repeatedly
    // rendering the same sound state to catch accidental DSP coupling. A different
    // input also proves that a previously exported image is an independent copy.
    auto changedInput = input;
    for (auto& channel : changedInput)
        for (auto& sample : channel)
            sample *= 0.63f;
    struct DisplayAction { const char* name; int itemId; };
    for (const auto& control : { DisplayAction { "Image view", 2 }, { "Image palette", 2 },
                                { "Inspect right channel", 0 }, { "Image view", 1 },
                                { "Inspect left channel", 0 }, { "Image palette", 1 },
                                { "Show stereo channels", 0 } })
    {
        if (control.itemId == 0)
            clickButton(control.name);
        else
            selectEditorChoice(editor, control.name, control.itemId);
        juce::MemoryBlock afterDisplayChange;
        processor.getStateInformation(afterDisplayChange);
        require(beforeFreeze == afterDisplayChange, "visual view, palette, and channel choices leave saved sound parameters unchanged");
        prepare(processor);
        prepare(reference);
        require(processSamples(processor, changedInput) == processSamples(reference, changedInput),
                "visual view, palette, and channel choices never alter callback audio");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    }
    require(imagesEqual(savedSnapshot, retainedPixels), "a saved visualization owns its pixels independently of later display and audio updates");
    require(!imagesEqual(savedSnapshot, visualEditor->createVisualizationSnapshot()),
            "later audio updates produce a new visualization without changing the saved image");

    inspectImageTile(editor);
    juce::MemoryBlock afterInspection;
    processor.getStateInformation(afterInspection);
    require(beforeFreeze == afterInspection, "tile inspection leaves saved sound parameters unchanged");
    prepare(processor);
    prepare(reference);
    require(processSamples(processor, input) == processSamples(reference, input), "audio continues identically during tile inspection");
    clickButton("Freeze visualization");

    // Switching from a selected stereo right channel to mono must fall back to
    // the mono signal, without leaving an apparently active unavailable channel.
    clickButton("Inspect right channel");
    prepare(processor, 1);
    processSamples(processor, stimulus(1));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    auto* rightChannel = dynamic_cast<juce::Button*>(findEditorControl(editor, "Inspect right channel"));
    require(rightChannel != nullptr && !rightChannel->isEnabled() && !rightChannel->getToggleState(),
            "mono history disables the unavailable right-channel view and clears its selection");
    require(visualEditor->createVisualizationSnapshot().isValid(), "mono history remains available for visualization export");

    // Exercise a second visible factory selection to restore the demo setting.
    clickButton("Load Deep fried preset");
    require(processor.getCurrentProgram() == 2, "editor can load the demo preset after interaction checks");
    prepare(processor);
}

void makeArtifacts(const juce::File& directory)
{
    require(directory.createDirectory().wasOk(), "artifact directory can be created");
    visualizationRestartWhileFrozen();
    stereoVisualization();
    auto dry = musicalDemo();
    DeepFryAudioProcessor processor;
    prepare(processor);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0,
            "native editor creates with a valid initial size");
    const auto originalBounds = editor->getBounds();
    editor->setSize(originalBounds.getWidth() + 120, originalBounds.getHeight() + 80);
    editor->setBounds(originalBounds);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    captureEditor(*editor, directory.getChildFile("deep-fry-ui-idle.png"));
    auto* visualEditor = dynamic_cast<DeepFryAudioProcessorEditor*>(editor.get());
    require(visualEditor != nullptr && !visualEditor->createVisualizationSnapshot().isValid(),
            "export reports no image until real audio history has arrived");
    auto* saveImage = dynamic_cast<juce::Button*>(findEditorControl(*editor, "Save visualization JPEG"));
    require(saveImage != nullptr && !saveImage->isEnabled(), "JPEG export is disabled before any real audio history exists");
    editorInteractions(processor, *editor);

    Audio wet(2, std::vector<float>(dry[0].size() + latency));
    bool capturedLiveStates = false;
    int position = 0, blocks = 0;
    const int length = static_cast<int>(wet[0].size());
    while (position < length)
    {
        const int size = std::min(511, length - position);
        juce::AudioBuffer<float> buffer(2, size);
        buffer.clear();
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < size; ++sample)
                if (static_cast<std::size_t>(position + sample) < dry[0].size())
                    buffer.setSample(channel, sample, dry[static_cast<std::size_t>(channel)][static_cast<std::size_t>(position + sample)]);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            std::copy_n(buffer.getReadPointer(channel), size, wet[static_cast<std::size_t>(channel)].begin() + position);
        position += size;
        if (++blocks % 8 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(8);
        if (!capturedLiveStates && position >= static_cast<int>(sampleRate * 4.0))
        {
            // Capture the real live image history during the musical passage,
            // while preserving the complete eight-second audio render below.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            clickEditorButton(*editor, "Show stereo channels");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui.png"));
            captureButtonAppearance(*editor, "Save visualization JPEG", juce::Button::buttonOver,
                                    directory.getChildFile("deep-fry-ui-hover.png"));
            captureButtonAppearance(*editor, "Save visualization JPEG", juce::Button::buttonDown,
                                    directory.getChildFile("deep-fry-ui-pressed.png"));
            captureChoiceMenu(*editor, "Image view", directory.getChildFile("deep-fry-ui-view-menu.png"));
            captureChoiceMenu(*editor, "Image palette", directory.getChildFile("deep-fry-ui-palette-menu.png"));
            selectEditorChoice(*editor, "Image palette", 2);
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-gray.png"));
            selectEditorChoice(*editor, "Image palette", 1);
            selectEditorChoice(*editor, "Image view", 2);
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-wet.png"));
            selectEditorChoice(*editor, "Image view", 1);
            clickEditorButton(*editor, "Inspect right channel");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-right.png"));
            clickEditorButton(*editor, "Inspect left channel");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-left.png"));
            clickEditorButton(*editor, "Show stereo channels");
            inspectImageTile(*editor, 889.0f, 301.0f);
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-inspect.png"));
            clickEditorButton(*editor, "Freeze visualization");
            const auto exportedImage = visualEditor->createVisualizationSnapshot();
            const auto exportFile = directory.getChildFile("deep-fry-visualization.jpg");
            require(deepfry::writeVisualizationJpeg(exportedImage, exportFile), "paired visualization artifact saves as a genuine JPEG");
            requireJpegRoundTrip(exportFile, exportedImage, 20.0);

            auto* constrainer = editor->getConstrainer();
            require(constrainer != nullptr, "resizable native editor exposes its minimum size");
            editor->setSize(constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-small.png"));
            selectEditorChoice(*editor, "Image palette", 2);
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-small-gray.png"));
            selectEditorChoice(*editor, "Image palette", 1);
            editor->setBounds(originalBounds);

            clickEditorButton(*editor, "Explain JPEG audio processing");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-help.png"));
            clickEditorButton(*editor, "Explain JPEG audio processing");
            // No callback runs between these changes, so the additional control
            // state screenshot cannot alter any sample in the musical demo.
            clickEditorButton(*editor, "Turn effect off");
            checkEffectSelection(processor, *editor, false, "effect-off screenshot");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-effect-off.png"));
            captureButtonAppearance(*editor, "Turn effect off", juce::Button::buttonOver,
                                    directory.getChildFile("deep-fry-ui-effect-off-hover.png"));
            clickEditorButton(*editor, "Turn effect on");
            capturedLiveStates = true;
        }
    }
    require(capturedLiveStates, "native editor captures live, minimum-size, and help render states");
    editor.reset();

    // Compensate reported plugin latency so both demo files align for A/B use.
    for (auto& channel : wet)
        channel.erase(channel.begin(), channel.begin() + latency);
    checkFinite(wet);
    writeWave(directory.getChildFile("demo-dry.wav"), dry);
    writeWave(directory.getChildFile("demo-deep-fried.wav"), wet);
    std::cout << "Artifacts: " << directory.getFullPathName() << '\n';
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try
    {
        latencyAndBypass();
        layoutsAndChannels();
        partitionInvariance();
        presetsAndSilence();
        stateRecall();
        visualizationAndInvalidInput();
        visualizationPublicationAndReset();
        jpegExportFiles();
        if (argc == 3 && std::string(argv[1]) == "--artifacts")
            makeArtifacts(juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[2])));
        else
            require(argc == 1, "usage: DeepFryVerify [--artifacts <directory>]");
        std::cout << "All " << checks << " native plugin integration checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}

#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif
