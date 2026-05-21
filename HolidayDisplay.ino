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
// Power Status — shared with BLEPeripheral.ino for BLE notify
// Arduino owns all voltage decisions; iPhone receives code only.
// ------------------------------------------------------------
enum class PowerStatus : uint8_t {
    OK              = 0x00,  // Battery healthy
    BATTERY_WARNING = 0x01,  // TTE ≤ 5 min or Vbatt ≤ 3.2V
    BATTERY_CUTOFF  = 0x02,  // Vbatt ≤ 3.0V — boost disabled (latched)
    HARDWARE_FAULT  = 0x03,  // AC rail present but degraded
    ON_AC           = 0x04,  // AC rail healthy — battery monitoring suspended
};

PowerStatus currentPowerStatus = PowerStatus::OK;
bool        powerStatusChanged  = false;  // set on transition; cleared by notifyBattery()

// ------------------------------------------------------------
// Power Management — pins, thresholds, state
// ------------------------------------------------------------
static const uint8_t  PIN_BOOST_EN      = 5;          // D5: TPS61023 EN, HIGH=on (confirmed)
static const uint32_t POWER_SAMPLE_MS   = 15000UL;    // 15-second sample interval
static const float    VBATT_CUTOFF      = 3.00f;      // Hard cutoff (V)
static const float    VBATT_WARNING     = 3.20f;      // Soft warning threshold (V)
static const float    VRAIL_PRESENT_V   = 1.60f;      // VA1 > 1.60V → Vrail > 3.2V = AC present
static const float    VRAIL_HEALTHY_V   = 2.25f;      // VA1 > 2.25V → Vrail > 4.5V = AC healthy
static const float    VBATT_PRESENT_MIN = 0.50f;      // Below this, no battery is attached/sensed
static const float    TTE_WARNING_SECS  = 300.0f;     // 5-minute Time-To-Empty warning
static const uint8_t  REGRESSION_N      = 5;          // Sliding window sample count
static const float    ADC_TO_VOLTS      = (3.3f / 4095.0f) * 2.0f; // 12-bit, ÷2 divider

static float          vHistory[REGRESSION_N] = {};
static uint8_t        vHistoryCount           = 0;    // 0–5; full at 5
static unsigned long  lastPowerSample          = 0;
static bool           cutoffLatched            = false;
static bool           ledLocked                = false; // true = battery state owns LED

// Non-blocking blink state for BATTERY_WARNING
static unsigned long  blinkLast = 0;
static bool           blinkOn   = false;
static const uint32_t BLINK_ON  = 100;   // ms — short pulse (low power)
static const uint32_t BLINK_OFF = 900;   // ms

// ------------------------------------------------------------
// Onboard RGB LED Control (Nano 33 BLE Sense Rev 2)
// Pins: LED_RED = P0.24, LED_GREEN = P0.16, LED_BLUE = P0.06
// Logic is active-LOW: LOW = LED on, HIGH = LED off.
// ledLocked = true when battery WARNING/CUTOFF owns the LED;
// BLE state changes are ignored to prevent override.
// ------------------------------------------------------------
void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
    if (ledLocked) return;
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
                renderFire();
                break;
            }

            case Effect::Candle:
            case Effect::LOTR_ColdWhite:
            case Effect::LOTR_Palantir:
            case Effect::LOTR_ManyColor:
            {
                // Fall back visibly until these effects are implemented.
                renderFire();
                break;
            }

            case Effect::Sparkle:
            {
                renderSparkle();
                break;
            }

            case Effect::Ember:
            {
                renderEmber();
                break;
            }

            case Effect::WarmWhite:
            {
                renderWarmWhite();
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

    void renderFire()
    {
        fireDriftPhase += fireSlowDriftRate;
        if (fireDriftPhase > 2.0f * PI) {
            fireDriftPhase -= 2.0f * PI;
        }

        float drift = (sin(fireDriftPhase) + 1.0f) * 0.5f;
        float driftScale = 0.60f + drift * 0.40f;

        for (uint16_t i = 0; i < ledCount; i++) {
            float flicker = random(350, 1000) / 1000.0f;
            float intensity = driftScale * flicker;
            uint8_t r = static_cast<uint8_t>(240.0f * intensity);
            uint8_t g = static_cast<uint8_t>(95.0f * intensity);

            if (random(0, 100) < 18) {
                r = min<uint8_t>(255, r + random(30, 90));
                g = min<uint8_t>(130, g + random(12, 45));
            }

            strip.SetPixelColor(i, scaleBrightness(RgbwColor(r, g, 0, 0)));
        }
        strip.Show();
    }

    void renderSparkle()
    {
        for (uint16_t i = 0; i < ledCount; i++) {
            strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
        }

        if (random(0, 100) < 45) {
            uint16_t pixel = random(0, ledCount);
            strip.SetPixelColor(pixel, scaleBrightness(RgbwColor(255, 220, 180, 80)));
        }
        if (random(0, 100) < 15) {
            uint16_t pixel = random(0, ledCount);
            strip.SetPixelColor(pixel, scaleBrightness(RgbwColor(120, 170, 255, 30)));
        }

        strip.Show();
    }

    void renderEmber()
    {
        fireDriftPhase += fireSlowDriftRate * 0.45f;
        if (fireDriftPhase > 2.0f * PI) {
            fireDriftPhase -= 2.0f * PI;
        }

        float breath = (sin(fireDriftPhase) + 1.0f) * 0.5f;
        float baseGlow = 0.08f + breath * 0.18f;

        for (uint16_t i = 0; i < ledCount; i++) {
            float pixelVariation = random(55, 105) / 100.0f;
            float intensity = baseGlow * pixelVariation;

            uint8_t r = static_cast<uint8_t>(170.0f * intensity);
            uint8_t g = static_cast<uint8_t>(12.0f * intensity);

            if (random(0, 100) < 2) {
                r = min<uint8_t>(90, r + random(18, 45));
                g = min<uint8_t>(18, g + random(2, 8));
            }

            strip.SetPixelColor(i, scaleBrightness(RgbwColor(r, g, 0, 0)));
        }

        strip.Show();
    }

    void renderWarmWhite()
    {
        float phase  = (millis() % 5000) / 5000.0f;
        float breath = (sinf(phase * 2.0f * PI - PI / 2.0f) + 1.0f) * 0.5f;

        uint8_t w = static_cast<uint8_t>(30 + breath * 210);
        uint8_t r = static_cast<uint8_t>(8 + breath * 24);

        RgbwColor scaled = scaleBrightness(RgbwColor(r, 0, 0, w));
        for (uint16_t i = 0; i < ledCount; i++) {
            strip.SetPixelColor(i, scaled);
        }
        strip.Show();
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
        switch (e) {
            case Effect::Fire:
            case Effect::Candle:
            case Effect::Ember:
            case Effect::Sparkle:
            case Effect::WarmWhite:
            case Effect::LOTR_ColdWhite:
            case Effect::LOTR_Palantir:
            case Effect::LOTR_ManyColor:
                return true;
        }
        return false;
    }
};

// ------------------------------------------------------------
// Global Instance for Test Harness
// ------------------------------------------------------------
PixelDisplay display(3, 8);

// ------------------------------------------------------------
// Power Management Functions
// ------------------------------------------------------------

static float adcToVolts(uint8_t pin) {
    return analogRead(pin) * ADC_TO_VOLTS;
}

// 5-sample least-squares linear regression.
// Returns TTE in seconds, or -1 if window not full or voltage not falling.
static float computeTTE() {
    if (vHistoryCount < REGRESSION_N) return -1.0f;
    // x = {0, 15, 30, 45, 60}s — fixed, precomputed:
    // sum_x = 150, sum_x2 = 6750, denom = n*sum_x2 - sum_x^2 = 11250
    float sum_y = 0.0f, sum_xy = 0.0f;
    for (uint8_t i = 0; i < REGRESSION_N; i++) {
        float xi = i * 15.0f;
        sum_y  += vHistory[i];
        sum_xy += xi * vHistory[i];
    }
    float m = (REGRESSION_N * sum_xy - 150.0f * sum_y) / 11250.0f;
    if (m >= 0.0f) return -1.0f;  // flat or rising — no TTE
    return (VBATT_CUTOFF - vHistory[REGRESSION_N - 1]) / m;
}

static void setPowerStatus(PowerStatus s) {
    if (s != currentPowerStatus) {
        currentPowerStatus = s;
        powerStatusChanged = true;
    }
}

void setupPower() {
    analogReadResolution(12);
    pinMode(PIN_BOOST_EN, OUTPUT);
    digitalWrite(PIN_BOOST_EN, HIGH);  // EN=HIGH: boost converter ON at startup
    lastPowerSample = millis() - POWER_SAMPLE_MS;  // force the first sample immediately
}

void updatePower() {
    unsigned long now = millis();

    // WARNING blink runs every loop() call — not gated by sample interval.
    // Uses direct digitalWrite to bypass ledLocked check (battery owns LED here).
    if (currentPowerStatus == PowerStatus::BATTERY_WARNING) {
        unsigned long elapsed = now - blinkLast;
        if (blinkOn && elapsed >= BLINK_ON) {
            digitalWrite(LED_RED, HIGH);  // OFF (active low)
            blinkOn   = false;
            blinkLast = now;
        } else if (!blinkOn && elapsed >= BLINK_OFF) {
            digitalWrite(LED_RED, LOW);   // ON (active low)
            blinkOn   = true;
            blinkLast = now;
        }
    }

    // All voltage sampling is gated to every 15 seconds
    if (now - lastPowerSample < POWER_SAMPLE_MS) return;
    lastPowerSample = now;

    float vrail = adcToVolts(A1);
    float vbatt = adcToVolts(A0);

    // --- AC rail check takes priority over battery state machine ---
    if (vrail > VRAIL_PRESENT_V) {
        digitalWrite(PIN_BOOST_EN, LOW);  // Disable boost — LiPo quiescent drain removed
        ledLocked     = false;            // Restore BLE config-state LED
        vHistoryCount = 0;                // Reset regression window
        if (vrail >= VRAIL_HEALTHY_V) {
            setPowerStatus(PowerStatus::ON_AC);
        } else {
            setPowerStatus(PowerStatus::HARDWARE_FAULT);  // Rail present but degraded
        }
        return;
    }

    // --- AC absent: battery monitoring active ---

    // Transition from AC back to battery: re-enable boost unless cutoff latched
    bool wasOnAc = (currentPowerStatus == PowerStatus::ON_AC ||
                    currentPowerStatus == PowerStatus::HARDWARE_FAULT);
    if (wasOnAc && !cutoffLatched) {
        digitalWrite(PIN_BOOST_EN, HIGH);
        vHistoryCount = 0;  // Restart regression warmup
    }

    // Cutoff latch is permanent until hardware reset
    if (cutoffLatched) {
        setPowerStatus(PowerStatus::BATTERY_CUTOFF);
        return;
    }

    if (vbatt < VBATT_PRESENT_MIN) {
        digitalWrite(PIN_BOOST_EN, LOW);
        ledLocked = false;
        setPowerStatus(PowerStatus::HARDWARE_FAULT);
        return;
    }

    // Hard cutoff: disable boost, lock LED solid RED, latch forever
    if (vbatt <= VBATT_CUTOFF) {
        digitalWrite(PIN_BOOST_EN, LOW);
        // Set solid RED directly before engaging ledLocked
        digitalWrite(LED_RED,   LOW);   // ON
        digitalWrite(LED_GREEN, HIGH);  // OFF
        digitalWrite(LED_BLUE,  HIGH);  // OFF
        ledLocked     = true;
        cutoffLatched = true;
        setPowerStatus(PowerStatus::BATTERY_CUTOFF);
        return;
    }

    // Add voltage sample to sliding window
    if (vHistoryCount < REGRESSION_N) {
        vHistory[vHistoryCount++] = vbatt;
    } else {
        for (uint8_t i = 0; i < REGRESSION_N - 1; i++) vHistory[i] = vHistory[i + 1];
        vHistory[REGRESSION_N - 1] = vbatt;
    }

    // Warning: TTE ≤ 5 min OR voltage at soft threshold
    float tte       = computeTTE();
    bool  warnTTE   = (tte > 0.0f && tte <= TTE_WARNING_SECS);
    bool  warnVolt  = (vbatt <= VBATT_WARNING);

    if (warnTTE || warnVolt) {
        if (currentPowerStatus != PowerStatus::BATTERY_WARNING) {
            // First entry into WARNING: start blink, lock LED
            ledLocked = true;
            blinkOn   = true;
            blinkLast = now;
            digitalWrite(LED_RED, LOW);  // RED on to start pulse
        }
        setPowerStatus(PowerStatus::BATTERY_WARNING);
    } else {
        if (currentPowerStatus == PowerStatus::BATTERY_WARNING) {
            // Exiting WARNING (voltage recovered): restore BLE LED
            ledLocked = false;
        }
        setPowerStatus(PowerStatus::OK);
    }
}

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

    setupPower();     // Configure ADC resolution and D5 boost enable before power decisions
    updatePower();    // Resolve AC/battery state immediately instead of waiting 15 seconds
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
    updatePower();  // Non-blocking: blink runs every call, sampling every 15s
}
