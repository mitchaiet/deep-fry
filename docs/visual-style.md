# Deep Fry: a JPEG export window gone wrong

The visual identity comes from early image macros, repeatedly saved JPEGs, and utilitarian image-export controls. It should look like a specific audio instrument with a sense of humor.

- **Impact is the voice.** The wordmark uses white letters, a thick black outline, a slight horizontal stretch, and blue misregistration. UI headings use solid black Impact. Numeric values and metadata use a clear monospace font.
- **The image gets the space.** The selected output signal is the main canvas; the input is a smaller reference. Both display the same history of real 8×8 audio tiles in one shared Colour or Gray palette. Final Out is the default, with JPEG Wet available for the compression stage. Empty displays use an image-editor checkerboard and an explicit request for audio.
- **One printed surface.** Cream paper, black rules, square controls, offset solid shadows, and a red masthead hold the interface together. Texture is fixed and cached. Control text and values stay sharp.
- **Signal displays stay unobstructed.** Functional labels identify the views, and a short idle hint explains how to start the display. The filename and tilted JPEG ABUSE stamp carry the visual character without caption overlays.
- **Controls work like an export dialog.** Horizontal sliders have chunky square handles, ticks, units, editable values, and visible focus outlines. Yellow marks selected presets and active toggles. Bypass and image freeze have explicit labels. A compact toolbar keeps the output view, shared palette, PNG export, and Freeze beside the main image; L / R selects the channel beside INPUT.
- **Inspection stays close to the signal.** The space below INPUT holds two 72-pixel tile inspectors, a history position and age, and the actual final output peak. Clicking a mosaic tile freezes the picture and outlines its counterpart in both views. The amplitude legend anchors the palette to −1, zero, and +1.
- **Captures are ready to compare.** Save PNG renders equal-size input/result images into a 1080×352 sheet with channel, view, palette, tile count, and the same amplitude legend. It captures the current image without stopping audio.

| Color | Role |
| --- | --- |
| `#16130F` | Ink, outlines, meters, status strips |
| `#EEE9DA` | Printed paper and control surface |
| `#FFFFF3` | Wordmark fill and editable value fields |
| `#EF4029` | Masthead and Fry control |
| `#F5EE36` | Stamp, selections, and secondary slider fill |
| `#254EDB` | Quality control, keyboard focus, wordmark misregistration |

Default size: 1120×800. Minimum: 896×640. The layout scales at a fixed 1.4 aspect ratio. Outlined text paths and background grain are built once per editor; the audio images update on the UI timer through the existing bounded queue.

Impact is loaded from the system font collection rather than bundled. Fallback order is Anton, Arial Narrow, Arial Black, then the system sans-serif face. The macOS build was visually checked with the installed Impact font.
