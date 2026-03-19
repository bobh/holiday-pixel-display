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
// Note: values map directly to BLE effect_id characteristic values.
enum class Effect : uint8_t {
    Fire = 0,
    Candle = 1,
    Ember = 2,
    Sparkle = 3,
    WarmWhite = 4,

    // LOTR-themed effects (Orthanc)
    LOTR_ColdWhite = 10,
    LOTR_Palantir = 11,
    LOTR_ManyColor = 12,
};

// ------------------------------------------------------------
// Configuration State
// ------------------------------------------------------------
enum ConfigState {
    UNCONFIGURED,  // No saved FRAM config — Yellow LED, pixels OFF
    CONFIGURING,   // Central connected, receiving config — Blue LED, pixels preview
    CONFIGURED     // Config saved and running — Green LED, pixels running
};

ConfigState state = UNCONFIGURED;

// ------------------------------------------------------------
// Onboard RGB LED Control (Nano 33 BLE Sense Rev 2)
// Pins: LED_RED = P0.24, LED_GREEN = P0.16, LED_BLUE = P0.06
// Logic is active-LOW: LOW = LED on, HIGH = LED off.
// Pass 0 to turn a channel off, non-zero to turn it on.
// ------------------------------------------------------------
void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
    digitalWrite(LED_RED,   r == 0 ? HIGH : LOW);
    digitalWrite(LED_GREEN, g == 0 ? HIGH : LOW);
    digitalWrite(LED_BLUE,  b == 0 ? HIGH : LOW);
}

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
          globalBrightness(50),
          primaryColor(0xFFFFFF),
          secondaryColor(0x000000)
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

    void setBrightness(uint8_t b)
    {
        globalBrightness = b;
    }

    void setSpeed(uint8_t speed)
    {
        // Map a 0-255 speed value to a reasonable update interval (ms)
        // Lower speed -> slower update; higher speed -> faster updates.
        // Keep it within a sane range (10ms - 200ms).
        const uint16_t minMs = 10;
        const uint16_t maxMs = 200;
        frameIntervalMs = minMs + ((uint16_t)(255 - speed) * (maxMs - minMs)) / 255;
    }

    void setPrimaryColor(uint32_t rgb)
    {
        primaryColor = rgb;
    }

    void setSecondaryColor(uint32_t rgb)
    {
        secondaryColor = rgb;
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
    unsigned long frameIntervalMs;

    // Fire effect parameters (internal only)
    uint8_t fireBaseRed;
    uint8_t fireBaseGreen;
    float   fireFlickerStrength;
    float   fireSlowDriftRate;
    float   fireDriftPhase;

    // Global brightness for all effects (0–255)
    uint8_t globalBrightness;

    // Primary/secondary colors (RGB, no white)
    uint32_t primaryColor;
    uint32_t secondaryColor;

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
        return v <= static_cast<uint8_t>(Effect::LOTR_ManyColor);
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
    // Initialize onboard RGB LED pins (active-LOW)
    pinMode(LED_RED,   OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE,  OUTPUT);

    // Start with all LEDs off before state is determined
    setStatusLED(0, 0, 0);

    display.begin();  // Clears and blanks SK6812 pixels

    // Check FRAM for a previously saved configuration.
    // framHasValidConfig() / loadConfigFromFRAM() are defined in BLEPeripheral.ino.
    if (framHasValidConfig()) {
        loadConfigFromFRAM();          // Populates gConfig from FRAM
        state = CONFIGURED;
        setStatusLED(0, 255, 0);       // GREEN — configured
        // display.update() in loop() will start running the loaded effect
    } else {
        state = UNCONFIGURED;
        setStatusLED(255, 255, 0);     // YELLOW — unconfigured
        // SK6812 pixels remain OFF; loop() skips display.update() while UNCONFIGURED
    }

    setupBLE();
}

// ------------------------------------------------------------
// Arduino Loop
// ------------------------------------------------------------
void loop()
{
    // Only drive SK6812 pixels when a config is active (CONFIGURING = preview,
    // CONFIGURED = running saved effect). UNCONFIGURED keeps pixels OFF.
    if (state != UNCONFIGURED) {
        display.update();
    }
    updateBLE();
}
