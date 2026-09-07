# Deep Fry: a JPEG export window gone wrong

The visual identity comes from early image macros, repeatedly saved JPEGs, and utilitarian image-export controls. It should look like a specific audio instrument with a sense of humor.

- **Impact is the voice.** The DEEP FRY wordmark uses white letters, a thick black outline, a slight horizontal stretch, and blue misregistration. The yellow stamp reads **JPEG YOUR MUSIC**, with no subtitle. Uppercase Impact button text and section headings carry that character through the controls. Bundled IBM Plex Sans keeps small captions and menu text readable; IBM Plex Mono handles numeric values.
- **The image gets the space.** The selected output signal is the main canvas; the input is a smaller reference. Both display matching histories of real 8×8 audio tiles in one shared Colour or Grayscale palette. Stereo is the default: separate labeled L/R lanes each show 64 captured tiles in an 8×8 tile grid. Solo L/R and mono use the full width for up to 128 tiles. The View menu defaults to Output (what you hear), with JPEG only (before mix) available for the compression stage. Empty displays use an image-editor checkerboard and an explicit request for audio.
- **One printed surface.** Cream paper, thick black borders, square controls, and a red masthead hold the interface together. Buttons, slider handles, and the tilted brand stamp use offset ink shadows. Texture is fixed and cached. Control text and values stay sharp.
- **Signal displays stay unobstructed.** Functional labels identify the views, and a short idle hint explains how to start the display. The filename and tilted JPEG YOUR MUSIC stamp carry the visual character without caption overlays.
- **Controls have weight and movement.** Horizontal sliders have chunky tracks and rectangular handles, ticks, units, bordered editable values, and visible focus outlines. Buttons brighten on hover and move into their offset shadows when pressed. Yellow marks selected presets and active toggles. Effect has fixed ON / OFF choices: selected ON is yellow, selected OFF is ink with white text. The labels stay in place so the current state is explicit.
- **Image settings have their own language.** Labeled View and Palette dropdowns use bordered square paper fields and arrows. View chooses Output (what you hear) or JPEG only (before mix); Palette chooses Colour or Grayscale. The footer reads "VIEW + PALETTE: IMAGE ONLY" in high-contrast text. SAVE JPEG and FREEZE IMAGE sit beside the menus; STEREO / L / R controls the channel layout.
- **Inspection stays close to the signal.** The space below INPUT holds two 72-pixel tile inspectors, a channel/tile position and age, and the actual final output peak. Clicking a mosaic tile freezes the picture and outlines its counterpart in both views, including its selected L/R lane. The amplitude legend anchors the palette to −1, zero, and +1.
- **Captures are ready to compare.** Save JPEG renders equal-size input/result images into a 1080×352 sheet with stereo lanes or the selected channel, view, palette, tile count, and the same amplitude legend. It captures the current image without stopping audio and encodes a genuine JPEG at 92% quality. That export quality is independent of the sound controls.

| Color | Role |
| --- | --- |
| `#16130F` | Ink, outlines, meters, status strips |
| `#EEE9DA` | Printed paper and control surface |
| `#FFFFF3` | Wordmark fill and editable value fields |
| `#EF4029` | Masthead and Fry control |
| `#F5EE36` | Stamp, selections, and secondary slider fill |
| `#254EDB` | Quality control, keyboard focus, wordmark misregistration |

Default size: 1120×800. Minimum: 896×640. The layout scales at a fixed 1.4 aspect ratio. Outlined text paths and background grain are built once per editor; the audio images update on the UI timer through the existing bounded queue.

Impact is loaded from the system font collection rather than bundled. Fallback order is Anton, Arial Narrow, Arial Black, then the system sans-serif face. IBM Plex Sans Regular/SemiBold and IBM Plex Mono Regular are embedded, so their rendering does not require those fonts to be installed. See [font sources and licenses](../Assets/Fonts/README.md) and [0.3.1 validation](validation-v0.3.1.md).
