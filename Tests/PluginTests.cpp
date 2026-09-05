#include "PluginProcessor.h"

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

void prepare(DeepFryAudioProcessor& processor, int channelCount = 2)
{
    auto layout = processor.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
    layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channelCount));
    require(processor.setBusesLayout(layout), "requested processing layout is accepted");
    processor.setRateAndBufferSizeDetails(sampleRate, 511);
    processor.prepareToPlay(sampleRate, 511);
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
                     bool automate = false, bool hostBypass = false)
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

void visualizationAndInvalidInput()
{
    DeepFryAudioProcessor processor;
    setParameter(processor, "output", 0);
    prepare(processor);
    const auto input = stimulus(2, 32768);
    const auto output = processSamples(processor, input);
    deepfry::TileFrame frame;
    int count = 0;
    while (processor.popVisualFrame(frame))
    {
        const int sourceStart = (++count * 12 - 1) * 64;
        require(std::isfinite(frame.retained) && frame.retained >= 0 && frame.retained <= 1,
                "visual coefficient retention is a finite fraction");
        for (std::size_t i = 0; i < frame.before.size(); ++i)
        {
            require(std::isfinite(frame.before[i]) && frame.before[i] >= 0 && frame.before[i] <= 255,
                    "live source pixels are finite luminance values");
            require(std::isfinite(frame.after[i]) && frame.after[i] >= 0 && frame.after[i] <= 255,
                    "live processed pixels are finite luminance values");
            require(frame.before[i] == 128.0f + std::round(input[0][static_cast<std::size_t>(sourceStart) + i] * 127.0f),
                    "live input visualization contains actual input samples mapped to pixels");
            const auto outputIndex = static_cast<std::size_t>(sourceStart + latency) + i;
            if (outputIndex < output[0].size())
                require(std::abs(frame.after[i] - (128.0f + output[0][outputIndex] * 127.0f)) < 0.0001f,
                        "live output visualization is the actual audible decoded tile");
        }
    }
    require(count > 30, "processor supplies a continuous stream of real visual frames");
    require(!processor.popVisualFrame(frame), "visual queue cleanly reports empty after draining");
    require(std::isfinite(processor.inputLevel.load()) && processor.inputLevel.load() > 0,
            "input meter receives a finite live level");
    require(std::isfinite(processor.outputLevel.load()) && processor.outputLevel.load() > 0,
            "output meter receives a finite live level");

    // Fill the bounded FIFO without an editor, then confirm processing continues.
    const auto longInput = stimulus(2, 131072);
    checkFinite(processSamples(processor, longInput));
    count = 0;
    while (processor.popVisualFrame(frame))
        ++count;
    require(count > 0 && count <= 127, "visual queue remains bounded when the editor is closed");

    auto damaged = stimulus();
    damaged[0][0] = std::numeric_limits<float>::quiet_NaN();
    damaged[0][1] = std::numeric_limits<float>::infinity();
    damaged[1][7] = -std::numeric_limits<float>::infinity();
    prepare(processor);
    checkFinite(processSamples(processor, damaged, { 17, 511 }));
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

void captureEditor(juce::AudioProcessorEditor& editor, const juce::File& screenshotFile)
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

    clickButton("Bypass effect");
    require(processor.parameters.getRawParameterValue("bypass")->load() == 1.0f,
            "clicking bypass enables the saved host bypass parameter");
    clickButton("Bypass effect");
    require(processor.parameters.getRawParameterValue("bypass")->load() == 0.0f,
            "clicking bypass again re-enables the effect");

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

    // Exercise a second visible factory selection to restore the demo setting.
    clickButton("Load Deep fried preset");
    require(processor.getCurrentProgram() == 2, "editor can load the demo preset after interaction checks");
    prepare(processor);
}

void makeArtifacts(const juce::File& directory)
{
    require(directory.createDirectory().wasOk(), "artifact directory can be created");
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
            captureEditor(*editor, directory.getChildFile("deep-fry-ui.png"));

            auto* constrainer = editor->getConstrainer();
            require(constrainer != nullptr, "resizable native editor exposes its minimum size");
            editor->setSize(constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-small.png"));
            editor->setBounds(originalBounds);

            clickEditorButton(*editor, "Explain JPEG audio processing");
            captureEditor(*editor, directory.getChildFile("deep-fry-ui-help.png"));
            clickEditorButton(*editor, "Explain JPEG audio processing");
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
