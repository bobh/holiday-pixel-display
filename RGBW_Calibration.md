# RGBW Dominance Calibration (SK6812 RGBW)

When the PCB arrives and you have the actual SK6812 RGBW strip installed, perform a calibration to compensate for the "dominant W channel" behavior described in the protocol document.

## Goal
Compute a **white-scale factor** that keeps the white channel from overpowering RGB colors while still allowing clean whites.

## Calibration Procedure
1. **Set up**
   - Power the board and run a simple test sketch (or use the main firmware) that lets you set RGBW values.
   - Use a consistent viewing environment (dark room preferred).
   - If available, use a light meter / lux app for objective results.

2. **Measure RGB white**
   - Set LEDs to **(255, 255, 255, 0)** (RGB full, W off).
   - Record the perceived brightness (`RGB_white_brightness`).

3. **Measure pure W**
   - Set LEDs to **(0, 0, 0, 255)** (W full, RGB off).
   - Record the perceived brightness (`W_white_brightness`).

4. **Compute the scale factor**
   - `whiteScaleFactor = RGB_white_brightness / W_white_brightness`
   - If you are eyeballing, adjust W until it perceptually matches RGB white, then:
     - `whiteScaleFactor = W_value_used / 255`

5. **Apply in firmware**
   - Use this factor when computing the W channel from RGB. For example (pseudocode):

```cpp
uint8_t w = min({r, g, b});
w = (uint16_t)w * whiteScaleFactor / 255;

r = max(0, r - w);
g = max(0, g - w);
b = max(0, b - w);
```

## Where to put this in firmware
- Add a `float whiteScaleFactor` in `PixelDisplay`.
- Use it in the RGB→RGBW conversion before writing pixels.
- Store the factor in FRAM (when available) so it survives power cycles.

## Notes
- This calibration is essential for consistent behavior across different SK6812 RGBW batches.
- The BLE protocol is intentionally RGB-only; the board converts RGB → RGBW internally using this calibration.
