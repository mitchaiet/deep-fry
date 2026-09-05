# Third-party notices

Deep Fry uses JUCE 8.0.13, pinned in `CMakeLists.txt` to commit
`7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`. The notices below accompany
the macOS VST3, Audio Unit, and standalone package.

The license files were copied from that JUCE source tree. Additional copyright
and license comments were extracted verbatim, with their original file paths
and line numbers. [SOURCE-MANIFEST.json](packaging/LICENSES/SOURCE-MANIFEST.json)
records provenance and SHA-256 checksums. The upstream source is available at
[the pinned JUCE revision](https://github.com/juce-framework/JUCE/tree/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2).

These notices describe third-party components only; they do not grant a license
to Deep Fry's original source code. JUCE's upstream license describes its
AGPLv3/commercial licensing. Reproducing that statement here does not choose a
licensing route for this project.

| Component | Included license and notices |
| --- | --- |
| JUCE framework | [Upstream license](packaging/LICENSES/JUCE-LICENSE.md), [module copyright and licensing notice](packaging/LICENSES/JUCE-MODULE-NOTICE.txt) |
| Steinberg VST3 SDK | [MIT license](packaging/LICENSES/VST3-LICENSE.txt), [additional SDK source notices](packaging/LICENSES/VST3-ADDITIONAL-NOTICES.txt) |
| Apple AudioUnitSDK | [Apache License 2.0](packaging/LICENSES/AudioUnitSDK-LICENSE.txt), [copyright notices](packaging/LICENSES/AudioUnitSDK-COPYRIGHT-NOTICES.txt) |
| FLAC | [BSD license and JUCE integration notice](packaging/LICENSES/FLAC-LICENSE.txt) |
| Ogg and Vorbis | [BSD license and JUCE integration notice](packaging/LICENSES/Ogg-Vorbis-LICENSE.txt), [Vorbis COPYING](packaging/LICENSES/Vorbis-COPYING.txt), [source notices, including LPC routines](packaging/LICENSES/Ogg-Vorbis-SOURCE-NOTICES.txt) |
| Independent JPEG Group JPEG library | [Complete upstream README, including license](packaging/LICENSES/IJG-JPEG-README.txt) |
| PNG reference library | [Upstream license](packaging/LICENSES/PNG-LICENSE.txt) |
| zlib | [Upstream license](packaging/LICENSES/zlib-LICENSE.txt) |
| HarfBuzz | [COPYING](packaging/LICENSES/HarfBuzz-COPYING.txt), [source notices](packaging/LICENSES/HarfBuzz-SOURCE-NOTICES.txt), including the additional MIT and ISC notices and Unicode emoji-data attribution |
| SheenBidi | [Apache License 2.0](packaging/LICENSES/SheenBidi-LICENSE.txt), [source copyright and licensing notices](packaging/LICENSES/SheenBidi-SOURCE-NOTICES.txt) |

This software is based in part on the work of the Independent JPEG Group.

The inventory follows `juce_audio_utils`, its module dependencies, and the
macOS plugin wrappers. It retains notices at the embedded-library level,
including optional library or SDK implementation files that may not contribute
symbols to each final binary. The VST3 additional notices preserve the SDK's
macOS hosting-helper BSD notice and its JSON parser's public-domain dedication
and disclaimer. HarfBuzz's Unicode emoji-data comment, including the original
Unicode terms-of-use link, is preserved as it appears in the pinned source.

The interface looks up installed fonts by name. Impact and the fallback font
files are not embedded or redistributed. JUCE's Karla font test data is guarded
by `JUCE_UNIT_TESTS` in `modules/juce_graphics/juce_graphics.cpp`; this project's
plugin build does not enable that flag.

Paths and links inside verbatim upstream files retain their original JUCE-tree
context. Use the source manifest or the pinned upstream revision to resolve
those references.
