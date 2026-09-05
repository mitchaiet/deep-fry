# Third-party notices

Deep Fry uses JUCE 8.0.13, pinned in `CMakeLists.txt` to commit
`7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`. The notices below accompany
the macOS VST3, Audio Unit, and standalone package and the Windows VST3 and
standalone package.

The JUCE dependency license files were copied from that JUCE source tree.
Additional copyright and license comments were extracted verbatim, with their
original file paths and line numbers.
[SOURCE-MANIFEST.json](packaging/LICENSES/SOURCE-MANIFEST.json) records their
provenance and SHA-256 checksums. The upstream source is available at
[the pinned JUCE revision](https://github.com/juce-framework/JUCE/tree/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2).

Deep Fry's original source is licensed under **AGPL-3.0-only**, as described in
[COPYRIGHT](COPYRIGHT) and the complete [LICENSE](LICENSE). JUCE is used under
its AGPLv3 option. The components below retain their own copyright notices and
applicable licenses.

| Component | Included license and notices |
| --- | --- |
| JUCE framework | [Upstream license](packaging/LICENSES/JUCE-LICENSE.md), [module copyright and licensing notice](packaging/LICENSES/JUCE-MODULE-NOTICE.txt) |
| Steinberg VST3 SDK | [MIT license](packaging/LICENSES/VST3-LICENSE.txt), [additional SDK source notices](packaging/LICENSES/VST3-ADDITIONAL-NOTICES.txt) |
| Apple AudioUnitSDK (macOS) | [Apache License 2.0](packaging/LICENSES/AudioUnitSDK-LICENSE.txt), [copyright notices](packaging/LICENSES/AudioUnitSDK-COPYRIGHT-NOTICES.txt) |
| FLAC | [BSD license and JUCE integration notice](packaging/LICENSES/FLAC-LICENSE.txt) |
| Ogg and Vorbis | [BSD license and JUCE integration notice](packaging/LICENSES/Ogg-Vorbis-LICENSE.txt), [Vorbis COPYING](packaging/LICENSES/Vorbis-COPYING.txt), [source notices, including LPC routines](packaging/LICENSES/Ogg-Vorbis-SOURCE-NOTICES.txt) |
| Independent JPEG Group JPEG library | [Complete upstream README, including license](packaging/LICENSES/IJG-JPEG-README.txt) |
| PNG reference library | [Upstream license](packaging/LICENSES/PNG-LICENSE.txt) |
| zlib | [Upstream license](packaging/LICENSES/zlib-LICENSE.txt) |
| HarfBuzz | [COPYING](packaging/LICENSES/HarfBuzz-COPYING.txt), [source notices](packaging/LICENSES/HarfBuzz-SOURCE-NOTICES.txt), including the additional MIT and ISC notices and Unicode emoji-data attribution |
| SheenBidi | [Apache License 2.0](packaging/LICENSES/SheenBidi-LICENSE.txt), [source copyright and licensing notices](packaging/LICENSES/SheenBidi-SOURCE-NOTICES.txt) |

This software is based in part on the work of the Independent JPEG Group.

The inventory follows `juce_audio_utils`, its module dependencies, and the
macOS and Windows plugin wrappers. It retains notices at the embedded-library level,
including optional library or SDK implementation files that may not contribute
symbols to each final binary. The VST3 additional notices preserve the SDK's
macOS hosting-helper BSD notice and its JSON parser's public-domain dedication
and disclaimer. HarfBuzz's Unicode emoji-data comment, including the original
Unicode terms-of-use link, is preserved as it appears in the pinned source.

Windows builds use JUCE's WASAPI and DirectSound audio backends and its
Direct2D/DirectWrite rendering support. Their JUCE implementations carry the
module notice listed above and call Windows system APIs; the package does not
redistribute the Windows SDK or DirectX runtime. HarfBuzz's Windows DirectWrite
adapter is covered by the existing `hb-directwrite.cc` and `hb-directwrite.h`
notices in [HarfBuzz-SOURCE-NOTICES.txt](packaging/LICENSES/HarfBuzz-SOURCE-NOTICES.txt).
The same codec, image, font-shaping, and VST3 license inventory therefore covers
both platform packages.

ASIO support is disabled (`JUCE_ASIO=0`), so the bundled ASIO SDK is not compiled
into these binaries. `JUCE_WEB_BROWSER=0` excludes browser and WebView2 code;
no WebView2 runtime is distributed. The project does not link `juce_video`, so
its DirectShow implementation is not part of the build. The complete JUCE
source archive also contains these unused modules and SDKs with their original
upstream licenses intact; their presence in the source archive does not mean
they are included in the plugin binaries.

The interface looks up installed fonts by name. Impact and the fallback font
files are not embedded or redistributed. JUCE's Karla font test data is guarded
by `JUCE_UNIT_TESTS` in `modules/juce_graphics/juce_graphics.cpp`; this project's
plugin build does not enable that flag.

Paths and links inside verbatim upstream files retain their original JUCE-tree
context. Use the source manifest or the pinned upstream revision to resolve
those references.

Windows release builds use the static Microsoft C/C++ runtime (`/MT`). The
Microsoft C++ Standard Library carries Apache-2.0 WITH LLVM-exception; its
complete [LICENSE](packaging/LICENSES/Windows-MSVC-STL-LICENSE.txt) and
[NOTICE](packaging/LICENSES/Windows-MSVC-STL-NOTICE.txt) are included verbatim.
[Windows-MSVC-SOURCE-MANIFEST.json](packaging/LICENSES/Windows-MSVC-SOURCE-MANIFEST.json)
records their upstream revision and hashes. The remaining Microsoft CRT and
Windows SDK components retain Microsoft's terms; see the
[Visual Studio license directory](https://visualstudio.microsoft.com/license-terms/)
and [redistribution information](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution).
The package contains linked release runtime code, without redistributing SDK
headers, development libraries, tools, or debug runtimes. The optional macOS
cross-build recipe uses Clang and LLD with these Microsoft libraries. GNU
windres writes resource data only; no MinGW or GCC runtime is linked by that
recipe.
