// Copyright (C) 2026 Mitch Chaiet
// SPDX-License-Identifier: AGPL-3.0-only
// See LICENSE and COPYRIGHT for terms and warranty disclaimer.

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace deepfry
{
inline constexpr float visualizationJpegQuality = 0.92f;

// Normalize the filename before asking whether an existing file may be replaced.
inline juce::File jpegDestination(const juce::File& requestedFile)
{
    if (requestedFile == juce::File()
        || requestedFile.hasFileExtension(".jpg;.jpeg"))
        return requestedFile;

    return requestedFile.withFileExtension(".jpg");
}

// Call from the UI thread, using the exact destination approved by the chooser.
// This function deliberately does not change the filename or its extension.
inline bool writeVisualizationJpeg(const juce::Image& image,
                                   const juce::File& destination)
{
    // JUCE's bundled JPEG encoder limits each image dimension to 65,500 pixels.
    constexpr int maximumJpegDimension = 65500;
    if (! image.isValid()
        || image.getWidth() > maximumJpegDimension
        || image.getHeight() > maximumJpegDimension
        || destination == juce::File()
        || destination.isDirectory()
        || ! destination.getParentDirectory().isDirectory())
        return false;

    // Finish encoding in memory so a disk error cannot interrupt the encoder.
    juce::JPEGImageFormat jpeg;
    jpeg.setQuality(visualizationJpegQuality);
    juce::MemoryOutputStream encoded;
    if (! jpeg.writeImageToStream(image, encoded) || encoded.getDataSize() == 0)
        return false;

    juce::TemporaryFile temporary(destination);
    auto stream = temporary.getFile().createOutputStream();
    if (stream == nullptr || ! stream->openedOk())
        return false;

    const bool written = stream->write(encoded.getData(), encoded.getDataSize());
    stream->flush();
    const bool complete = written && stream->getStatus().wasOk();
    stream.reset();

    return complete && temporary.overwriteTargetFileWithTemporary();
}
}
