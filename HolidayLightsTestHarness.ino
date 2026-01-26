//Holiday Lights Test Harness


#include <Wire.h>
#include <NeoPixelBus.h>
/*
If you want, I can also sketch the minimal FRAM read/write routines for that chip when you’re ready to bring it online.
*/

// ============================================================
// SK6812 RGBW Pixel Display - Test Harness with PixelDisplay Class
// Board: Arduino Nano 33 BLE Sense Rev 2
// LEDs: SK6812 RGBW (8 pixels)
// Effect: Fire Flicker (current working effect)
// FRAM: Stubbed (no hardware required)
// ============================================================

// ------------------------------------------------------------
// Effect Enumeration
// ------------------------------------------------------------
enum class Effect : uint8_t {
    Fire = 0,
    Candle,
    Ember,
    Sparkle,
    WarmWhite
};

// ------------------------------------------------------------
// PixelDisplay Class
// ------------------------------------------------------------
class PixelDisplay
{
public:
    PixelDisplay(uint8_t pin, uint16_t count)
        : ledPin(pin),
          ledCount(count),
          strip(count, pin),
          currentEffect(Effect::Fire),
          lastUpdate(0),
          frameIntervalMs(20),
          // Fire effect parameters
          fireBaseRed(230),
          fireBaseGreen(80),
          fireFlickerStrength(0.40f),
          fireSlowDriftRate(0.0025f),
          fireDriftPhase(0.0f),
          globalBrightness(50)
    {
    }

    void begin()
    {
        // Initialize I2C (FRAM stubbed, but safe to start Wire)
        Wire.begin();

        // Initialize LED strip
        strip.Begin();
        clearStrip();
        strip.Show();

        // Load effect from FRAM (stubbed)
        currentEffect = loadEffectFromFram();

        // If FRAM not really used, ensure we start with something valid
        // For now, default to Fire if value is out of range
        if (!isValidEffect(currentEffect)) {
            currentEffect = Effect::Fire;
        }

        // Seed randomness for flicker
        randomSeed(analogRead(0));
    }

    void update()
    {
        unsigned long now = millis();
        if (now - lastUpdate < frameIntervalMs) {
            return; // frame pacing
        }
        lastUpdate = now;

        switch (currentEffect)
        {
            case Effect::Fire:
            {
                // PURE RGB FIRE FLICKER (no white channel)
                // -----------------------------------------
                // Slow thermal drift
                fireDriftPhase += fireSlowDriftRate;
                if (fireDriftPhase > 2.0f * PI) {
                    fireDriftPhase -= 2.0f * PI;
                }

                float drift = (sin(fireDriftPhase) + 1.0f) * 0.5f;   // 0–1
                float driftScale = 0.75f + drift * 0.25f;            // 0.75–1.00

                // Fast micro-flicker
                float flicker = 1.0f - (random(0, 1000) / 1000.0f) * fireFlickerStrength;

                // Combined intensity
                float intensity = driftScale * flicker;

                // Fire color profile (RGB only)
                uint8_t r = static_cast<uint8_t>(fireBaseRed   * intensity);
                uint8_t g = static_cast<uint8_t>(fireBaseGreen * intensity);
                uint8_t b = 0;

                // Occasional red surge (flame tongues)
                if (random(0, 100) < 10) {   // ~10% chance each frame
                    uint8_t surge = static_cast<uint8_t>(random(30, 80));
                    r = (r > 255 - surge) ? 255 : r + surge;
                }

                // White channel disabled for this effect
                uint8_t w = 0;

                RgbwColor color(r, g, b, w);
                RgbwColor scaled = scaleBrightness(color);

                for (uint16_t i = 0; i < ledCount; i++) {
                    strip.SetPixelColor(i, scaled);
                }

                strip.Show();
                break;
            }

            case Effect::Candle:
            {
                // Placeholder for future candle effect
                // For now, reuse Fire behavior or leave dark
                break;
            }

            case Effect::Ember:
            {
                // Placeholder for future ember effect
                break;
            }

            case Effect::Sparkle:
            {
                // Placeholder for future sparkle effect
                break;
            }

            case Effect::WarmWhite:
            {
                // Placeholder for future warm-white calibration effect
                break;
            }
        }
    }

    void setEffect(Effect e)
    {
        if (!isValidEffect(e)) {
            return;
        }
        currentEffect = e;
        saveEffectToFram(e);
    }

    Effect getEffect() const
    {
        return currentEffect;
    }

private:
    uint8_t  ledPin;
    uint16_t ledCount;
    NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip;

    Effect currentEffect;
    unsigned long lastUpdate;
    const unsigned long frameIntervalMs;

    // Fire effect parameters (internal only)
    uint8_t fireBaseRed;
    uint8_t fireBaseGreen;
    float   fireFlickerStrength;
    float   fireSlowDriftRate;
    float   fireDriftPhase;

    // Global brightness for all effects (0–255)
    uint8_t globalBrightness;

    // FRAM address for effect ID (stubbed)
    static constexpr uint16_t FRAM_EFFECT_ADDR = 0x0000;

    // --------------------------------------------------------
    // Utility: Clear strip
    // --------------------------------------------------------
    void clearStrip()
    {
        for (uint16_t i = 0; i < ledCount; i++) {
            strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
        }
    }

    // --------------------------------------------------------
    // Utility: Brightness scaling
    // --------------------------------------------------------
    RgbwColor scaleBrightness(const RgbwColor& c)
    {
        float scale = globalBrightness / 255.0f;
        return RgbwColor(
            static_cast<uint8_t>(c.R * scale),
            static_cast<uint8_t>(c.G * scale),
            static_cast<uint8_t>(c.B * scale),
            static_cast<uint8_t>(c.W * scale)
        );
    }

    // --------------------------------------------------------
    // FRAM Stub: Read/Write Effect ID
    // --------------------------------------------------------
    Effect loadEffectFromFram()
    {
        // Stubbed: FRAM not present.
        // In a real implementation, you would:
        //  - Use Wire to talk to FM24CL16B
        //  - Read one byte from FRAM_EFFECT_ADDR
        // For now, always return Fire.
        return Effect::Fire;
    }

    void saveEffectToFram(Effect e)
    {
        // Stubbed: FRAM not present.
        // In a real implementation, you would:
        //  - Use Wire to talk to FM24CL16B
        //  - Write one byte (static_cast<uint8_t>(e)) to FRAM_EFFECT_ADDR
        // This stub intentionally does nothing and never blocks.
        (void)e;
    }

    bool isValidEffect(Effect e)
    {
        uint8_t v = static_cast<uint8_t>(e);
        return v <= static_cast<uint8_t>(Effect::WarmWhite);
    }
};

// ------------------------------------------------------------
// Global Instance for Test Harness
// ------------------------------------------------------------
PixelDisplay display(3, 8);

// ------------------------------------------------------------
// Arduino Setup
// ------------------------------------------------------------
void setup()
{
    display.begin();

    // For this test harness, explicitly set Fire effect
    display.setEffect(Effect::Fire);
}

// ------------------------------------------------------------
// Arduino Loop
// ------------------------------------------------------------
void loop()
{
    display.update();
    // No delay here; frame pacing is handled inside the class
}
