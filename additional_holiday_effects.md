# Additional Holiday Effect Ideas

These are candidate LED effects for future implementation. They are not implemented yet.

## Christmas

- `Christmas_RedGreenChase`  
  Alternating red/green pixels slowly chase around the 8-pixel string.

- `Christmas_TwinkleWhite`  
  Mostly dark or dim cool-white background with random white sparkles.

- `Christmas_CandyCane`  
  Red and white bands rotate slowly; useful for RGB red plus the white channel.

- `Christmas_GoldShimmer`  
  Warm gold base with small brightness flickers, less chaotic than `Fire`.

- `Christmas_Icicle`  
  Cool blue-white fade pulses, with one pixel occasionally dripping brighter downward.

## Halloween

- `Halloween_PumpkinGlow`  
  Orange breathing effect, warmer and smoother than `Fire`.

- `Halloween_WitchGreen`  
  Sickly green pulse with occasional brighter flashes.

- `Halloween_GhostFade`  
  Pale white/blue slow fade in and out, using the white channel sparingly.

- `Halloween_Lightning`  
  Random cold-white strobe bursts followed by darkness.

## Thanksgiving

- `Thanksgiving_HarvestEmber`  
  Slow crossfade among amber, orange, red, and dim brownish tones.

- `Thanksgiving_CandleTable`  
  Multiple independent candle-like flickers, one per pixel, less uniform than current `Candle`.

## New Year

- `NewYear_Champagne`  
  Gold/white sparkle with quick random pops.

- `NewYear_MidnightBlue`  
  Deep blue background with occasional white/gold points.

## Other Holidays

- `July4_RedWhiteBlue`  
  Red, white, and blue rotating bands or soft pulses.

- `Valentine_Heartbeat`  
  Red/pink double-pulse pattern followed by a pause.

- `StPatricks_EmeraldTwinkle`  
  Green base with gold sparkles.

## LOTR / Orthanc

- `LOTR_EyeOfSauron`  
  Deep red/orange center pulse with darker edges.

- `LOTR_MordorEmber`  
  Smoky red ember flicker, darker and slower than `Fire`.

- `LOTR_MoonlitStone`  
  Cold white/blue subtle shimmer for tower illumination.

## Future Enum Organization

If the effect list grows, reserve numeric ranges by family:

- `0-9`: Generic effects
- `10-19`: LOTR / Orthanc effects
- `20-29`: Christmas effects
- `30-39`: Halloween effects
- `40-49`: Thanksgiving effects
- `50-59`: New Year effects
- `60-69`: Other holidays
