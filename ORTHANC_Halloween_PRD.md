# PRD: Orthanc Halloween Lighting Theme (Lord of the Rings)

## 1. Overview
This product is a **Halloween lighting theme** for an **Orthanc-inspired ceramic tower** that contains an **8‑pixel SK6812 RGBW LED string** driven by an **Arduino Nano 33 BLE Sense Rev 2**.

The primary goal is to deliver a **synchronized, mood‑driven lighting experience** that can be controlled over **Bluetooth Low Energy (BLE)** by a central device (smartphone/tablet/computer) and tied to a **voice narration** or scripted timeline.

The lighting themes are inspired by Tolkien’s description of Orthanc and Saruman, including:
- **Cold crystalline Elvish light** (Lore-accurate)
- **Corrupted Palantír power** (fiery heartbeat)
- **Many‑colored shimmer** (Saruman of Many Colors)

---
## 2. Objectives & Success Criteria
### Objectives
1. Deliver a **controllable BLE peripheral** that accepts commands to change lighting themes.
2. Provide **multiple distinct lighting effects** that match the described “Orthanc” mood.
3. Maintain a **smooth, non‑strobing** visual feel appropriate for a narrative experience.
4. Keep the design **extensible** so new effects and transitions can be added later.

### Success Criteria
- **BLE control**: A central device can connect and send commands to switch themes.
- **Theme fidelity**: Each theme convincingly matches the described mood (cool/white, fiery pulse, spectrum shimmer).
- **Performance**: Effects run smoothly at ~50 FPS on the Nano 33 BLE with 8 LEDs.
- **Resilience**: System recovers cleanly from disconnects and keeps running if BLE is not connected.

---
## 3. Scope and Constraints
### In Scope
- Core lighting effects (Fire, Candle, Ember, Sparkle, WarmWhite, Palantír/LOTR themes)
- BLE service and characteristics for effect selection and optional timeline control
- Effect persistence (optionally via FRAM when PCB is available)
- Demo mode for standalone operation (no BLE connected)

### Out of Scope (for initial deliverable)
- Full voice‑narration syncing via BLE (only a trigger/timestamp mechanism)
- Complex animation sequencing beyond per‑effect timing curves
- Full UI/UX implementation on a mobile app (only BLE command specification)

---
## 4. System Architecture
### 4.1 Hardware
- **Arduino Nano 33 BLE Sense Rev 2** (BLE Peripheral)
- **SK6812 RGBW LED string** (8 pixels)
- **FM24CL16B FRAM** (I²C) for persistent storage of current effect selection (future)
- **Voltage divider resistors** for battery monitoring (future)

### 4.2 Software
- `HolidayDisplay.ino` (main firmware)
- `PixelDisplay` class (manages effects, timing, pixel updates)
- BLE service for effect control (GATT) — new module to add
- Optional FRAM module to read/write effect state

---
## 5. Lighting Themes (Effects)
The following themes are the primary focus. Each can be implemented using the existing `PixelDisplay` structure.

### 5.1 Lore‑Accurate (Cold White / Ice Blue)
**Vibe:** Elegant, ancient, crystalline.
- LEDs driven to a cool white (approx. 6000K / ice blue) base.
- Very subtle shimmer: small random intensity variation (~5‑10%) to avoid looking static.
- Optional slow “breathing” to suggest ambient energy.

### 5.2 Corruption / Palantír Heartbeat (Fiery Orange / Deep Red)
**Vibe:** Sinister, mechanical, alive.
- Primary color: deep orange/crimson. Suggested base RGBW values (no white):
  - **Deep Palantír Orange:** `R=255, G=60, B=0, W=0`
  - **Molten Lava Orange:** `R=255, G=40, B=0, W=0`
  - **Ancient Crimson:** `R=220, G=20, B=60, W=0`
- **Heartbeat pulse**:
  - Cycle length: **4–6 seconds** (full bright↔dark↔bright)
  - Minimum brightness floor: **~10%** to keep tower “awake”.
  - Pulse shape: **sin²** or custom “double pulse” for lub‑dub feel.
- Optional per‑pixel flicker to avoid uniform glow.

### 5.3 Many‑Colored Shimmer (Saruman of Many Colors)
**Vibe:** Magical, prideful, shifting.
- The LED string cycles slowly through the spectrum:
  - Purple → Blue → Green → Yellow → Red → Purple
- Cycle duration: **~30‑60 seconds** for a very slow, subtle shimmer.
- Use smooth interpolation (HSV hue cycling) rather than hard steps.
- Maintain low to moderate brightness (40‑60%) to keep focus on color shifts.

### 5.4 Optional: WarmWhite / Candle (Ambient Soft Glow)
**Vibe:** Warm, gentle, candle‑lit.
- Soft amber/white (warm white) with slow, subtle flicker.
- Can leverage the W channel if brightness control demands more stable white without washing out.

---
## 6. BLE Control Interface
### 6.1 BLE Role
- **Peripheral**: Nano 33 BLE Sense acts as BLE peripheral.
- **Central**: a phone/tablet runs controlling app or script.

### 6.2 GATT Service & Characteristics (proposal)
#### Service: `Orthanc Lighting Control` (UUID TBD)
- **Characteristic: Effect Select (uint8)**
  - 0 = Fire (default)
  - 1 = Candle
  - 2 = Ember
  - 3 = Sparkle
  - 4 = WarmWhite
  - 10 = LOTR Cold White
  - 11 = LOTR Palantír
  - 12 = LOTR Many‑Color
- **Characteristic: Effect Parameters (optional)**
  - e.g. pulse speed, intensity, color overrides.
- **Characteristic: Scene Trigger (optional)**
  - Allows the central device to send a timestamp or “scene index” to sync lighting with narration.

### 6.3 BLE Behavior
- On connection: advertise current effect/state.
- On write to Effect Select: immediately switch effect and persist to FRAM (when available).
- If BLE disconnects, continue running the last selected effect.

---
## 7. UX & Behavior Notes
### 7.1 Autonomous Mode
If BLE is unavailable, the device should still run a default effect (e.g., Fire) and accept local triggers (future button input).

### 7.2 Effect Transitions
- Transitions between effects should be smooth (fade over 0.5‑1 second) rather than abrupt.
- Use a shared “crossfade” routine that blends current and target colors.

### 7.3 Power and Brightness
- Keep brightness conservative to avoid overheating and to preserve the “glow” effect.
- Provide a global brightness setting (0‑255) that can be programmatically adjusted by BLE.

---
## 8. Implementation Roadmap
1. **Implement BLE service + effect selector** (core requirement)
2. **Implement LOTR theme effects** (Cold White, Palantír, Many‑Color)
3. **Add effect transition support** (smooth fade/crossfade)
4. **Add FRAM persistence** (save current effect to FM24CL16B)
5. **Add optional timeline/scene trigger characteristic**
6. **Ensure power / brightness safety** (cap max brightness if needed)

---
## 9. Deliverables
- `HolidayDisplay.ino` updated with themed effects and BLE control.
- `ORTHANC_Halloween_PRD.md` (this document) in the project root.
- (Future) Example central device code / app snippet that sends BLE commands.

---
## 10. Notes on Art & Finish (non‑code)
- **Black finish**: Satin/gloss black with edge highlights will maximize contrast.
- **Diffusion**: Use cotton or tissue inside the tower to soften the LED points into a glow.
- **Light leakage**: Paint the interior matte black or line with foil to prevent unwanted glow.

---
*This document is intentionally written to guide development in the spirit of the provided lore notes, while remaining implementable on the Nano 33 BLE platform with 8 SK6812 RGBW LEDs.*
