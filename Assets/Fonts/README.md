# Bundled interface fonts

Deep Fry embeds unchanged static TrueType files from the official
[IBM Plex repository](https://github.com/IBM/plex), pinned to commit
[`bf260093582f04622aacc1e9f9ca604d7ccd0c42`](https://github.com/IBM/plex/tree/bf260093582f04622aacc1e9f9ca604d7ccd0c42).
The fonts load from the plugin binary so interface typography does not depend on
the fonts installed on the user's computer. No font installation is required.

| File | Interface use | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| `IBMPlexSans-Regular.ttf` | Body text and descriptions | 200500 | `975dcda37d80f038dcd143c22e33ca2d97a0cc5a929aace1c749153b0fe1afa5` |
| `IBMPlexSans-SemiBold.ttf` | Controls and labels | 202632 | `a20caf8286023a6a7a85e40b1d2a4ae9fc3e3b1f9eda8f4c542dd4986af67bb1` |
| `IBMPlexMono-Regular.ttf` | Numeric values and technical readouts | 173052 | `7c6fbddca4b700be918f5f6183d9bd4464fa427fe435f0b480d77fe2bb8c5a43` |
| `LICENSE.txt` | Unchanged upstream license | 4456 | `7e6b2818edbd8f6a01ae80641cc8f16a51080d08fb4e532be3a0b6f74adb07da` |

The three fonts total 576,184 bytes. They have not been subsetted, converted,
renamed, or otherwise modified. The two Sans files contain 1,019 glyphs each;
Mono contains 1,207. All three are static fonts, with no variable-font `fvar`
table.

## Immutable download sources

- [IBM Plex Sans Regular](https://raw.githubusercontent.com/IBM/plex/bf260093582f04622aacc1e9f9ca604d7ccd0c42/packages/plex-sans/fonts/complete/ttf/IBMPlexSans-Regular.ttf)
- [IBM Plex Sans SemiBold](https://raw.githubusercontent.com/IBM/plex/bf260093582f04622aacc1e9f9ca604d7ccd0c42/packages/plex-sans/fonts/complete/ttf/IBMPlexSans-SemiBold.ttf)
- [IBM Plex Mono Regular](https://raw.githubusercontent.com/IBM/plex/bf260093582f04622aacc1e9f9ca604d7ccd0c42/packages/plex-mono/fonts/complete/ttf/IBMPlexMono-Regular.ttf)
- [IBM Plex license](https://raw.githubusercontent.com/IBM/plex/bf260093582f04622aacc1e9f9ca604d7ccd0c42/LICENSE.txt)

## Copyright and license

These fonts retain the [SIL Open Font License 1.1](LICENSE.txt); they are not
relicensed under the application's AGPL license. The unchanged upstream license
includes the notice `Copyright © 2017 IBM Corp. with Reserved Font Name "Plex"`.
The Sans font metadata additionally identifies `Copyright 2018 IBM Corp. All
rights reserved.`; the Mono metadata identifies `Copyright 2017 IBM Corp. All
rights reserved.`

Include the copyright notices and license when redistributing these fonts with
source or binary releases. See the complete license for its terms.

## JUCE integration

For Deep Fry's `juce_add_binary_data` target using the `DeepFryFonts` namespace,
the generated resource names are:

| Font | Data | Size |
| --- | --- | --- |
| Sans Regular | `DeepFryFonts::IBMPlexSansRegular_ttf` | `DeepFryFonts::IBMPlexSansRegular_ttfSize` |
| Sans SemiBold | `DeepFryFonts::IBMPlexSansSemiBold_ttf` | `DeepFryFonts::IBMPlexSansSemiBold_ttfSize` |
| Mono Regular | `DeepFryFonts::IBMPlexMonoRegular_ttf` | `DeepFryFonts::IBMPlexMonoRegular_ttfSize` |

Load each face once with `juce::Typeface::createSystemTypefaceFor(data, size)`
and construct fonts from `juce::FontOptions(typeface).withHeight(height)`.
Use the embedded face directly: the SemiBold file's legacy family name is
`IBM Plex Sans SmBld`, so selecting a system family and requesting a bold style
would not reliably select this asset.
