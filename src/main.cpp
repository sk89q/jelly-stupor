#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define LED_PIN 9
#define EQ_ADC_PIN A0
#define EQ_STROBE_PIN 7
#define EQ_RESET_PIN 8
#define BUTTON_PIN 5

#define CONFIG_OFFSET 0
#define INITIAL_BRIGHTNESS 50
#define UPDATES_PER_SECOND 200

#include <FastLED.h>
#include <EEPROM.h>

struct Config
{
  byte brightness;
  byte programIndex;
  boolean fullBrightness;
};

Config config;
CRGB leds[NUM_LEDS];
uint16_t patternStartTime = 0;
uint8_t programIndex = 0;
uint16_t spectrumValue[7];
uint16_t buttonDownTime = 0;
double impulse;
double envelopeUpper = 0;
double envelopeLower = 0;
boolean brightnessChanged = false;

typedef bool (*ProgramFunction)(uint16_t a, uint16_t elapsed, uint8_t brightness);

void setLEDsFromPalette(CRGBPalette16 palette, uint8_t brightness, uint8_t index, uint8_t offset, TBlendType blending)
{
  int half = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int k = i < half ? i : half - (i - half);
    leds[i] = ColorFromPalette(palette, index + k * offset, brightness, blending);
  }
}

void setLEDsFromPalette32(CRGBPalette32 palette, uint8_t brightness, uint8_t index, uint8_t offset, TBlendType blending)
{
  int half = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int k = i < half ? i : half - (i - half);
    leds[i] = ColorFromPalette(palette, index + k * offset, brightness, blending);
  }
}

bool fftResponse(uint16_t index, uint8_t brightness)
{
  for (int i = 0; i < 8 * 7; i++)
  {
    int bin = i / 8;
    int k = bin % 2 == 0 ? i % 8 : 7 - i % 8;
    uint8_t brightness;
    int adjValue = spectrumValue[bin];
    if (adjValue >= (k + 1) * 128)
    {
      brightness = 255;
    }
    else if (adjValue > k * 128)
    {
      brightness = (adjValue - k * 128) / 128.0 * 255;
    }
    else
    {
      brightness = 0;
    }
    leds[i] = CHSV(30, 255, brightness);
  }

  return false;
}

bool rainbow(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = ColorFromPalette(RainbowColors_p, index * 2, brightness, LINEARBLEND);
  }

  return elapsed > 60000;
}

bool rainbowSlow(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = ColorFromPalette(RainbowColors_p, index / 2, brightness, LINEARBLEND);
  }

  return elapsed > 60000;
}

bool rainbowCircle(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    uint8_t k = (i % 40) * 6.4;
    leds[i] = ColorFromPalette(RainbowColors_p, index + k, brightness, LINEARBLEND);
  }

  return elapsed > 30000;
}

bool blueYellow(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  CRGBPalette32 palette = CRGBPalette32(CRGB::Blue, CRGB::Yellow);

  setLEDsFromPalette32(palette, brightness, index * 2, 10, LINEARBLEND);

  return elapsed > 30000;
}

bool plainColor(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = (CRGB) CHSV(200, 255, brightness);
  }

  return elapsed > 30000;
}

bool roTatingPurple(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  CRGBPalette32 palette = CRGBPalette32(CRGB::Purple, CRGB::Black, CRGB::Black);

  setLEDsFromPalette32(palette, brightness, index * 2, 10, NOBLEND);

  return elapsed > 30000;
}

bool blueNoise(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    uint8_t k = inoise8(random16());
    leds[i] = (CRGB)CHSV(160, 255, k > 170 ? brightness : 0);
  }

  return elapsed > 30000;
}

bool cyclicalBars(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  int half = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int k = i < half ? i : half - (i - half);
    leds[i] = (CRGB)CHSV(70, 255, (k / 5 + (index / (UPDATES_PER_SECOND / 5))) % 3 == 0 ? brightness : 0);
  }

  return elapsed > 30000;
}

bool cyclicalBars2(uint16_t index, uint16_t elapsed, uint8_t brightness)
{
  int half = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int k = i < half ? i : half - (i - half);
    leds[i] = (CRGB)CHSV(30, 255, (k / 16 + (index / (UPDATES_PER_SECOND / 5))) % 3 == 0 ? brightness : 0);
  }

  return elapsed > 30000;
}

ProgramFunction programs[] = {
    cyclicalBars2,
    cyclicalBars,
    blueNoise,
    roTatingPurple,
    plainColor,
    blueYellow,
    rainbowCircle,
    rainbow,
};

void setup()
{
  EEPROM.get(CONFIG_OFFSET, config);

  pinMode(EQ_ADC_PIN, INPUT);
  pinMode(EQ_STROBE_PIN, OUTPUT);
  pinMode(EQ_RESET_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  analogReference(DEFAULT);

  digitalWrite(EQ_RESET_PIN, LOW);
  digitalWrite(EQ_STROBE_PIN, HIGH);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(config.brightness);

  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black;
  }

  programIndex = config.programIndex;
  if (programIndex >= sizeof(programs) / sizeof(*programs)) 
  {
    programIndex = 0;
  }

  FastLED.show();
}

void setBrightnessLevel(uint8_t brightness)
{
  FastLED.setBrightness(brightness);
  config.brightness = brightness;
  EEPROM.put(CONFIG_OFFSET, config);

  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i < 5)
    {
      leds[i] = CRGB::Red;
    }
    else
    {
      leds[i] = CRGB::Black;
    }
  }
}

void loop()
{
  uint16_t now = millis();

  if (digitalRead(BUTTON_PIN) == HIGH)
  {
    if (buttonDownTime == 0)
    {
      buttonDownTime = now;
      brightnessChanged = false;
    }
    else if (now - buttonDownTime > 4500)
    {
      setBrightnessLevel(255);
    }
    else if (now - buttonDownTime > 4000)
    {
      setBrightnessLevel(180);
    }
    else if (now - buttonDownTime > 3500)
    {
      setBrightnessLevel(100);
    }
    else if (now - buttonDownTime > 3000)
    {
      setBrightnessLevel(80);
    }
    else if (now - buttonDownTime > 2500)
    {
      setBrightnessLevel(50);
    }
    else if (now - buttonDownTime > 2000)
    {
      setBrightnessLevel(20);
    }
    else if (now - buttonDownTime > 1500)
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i < 5)
        {
          leds[i] = CRGB::Purple;
        }
        else
        {
          leds[i] = CRGB::Black;
        }
      }
    }
    else if (now - buttonDownTime > 1000)
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i < 5)
        {
          leds[i] = CRGB::Green;
        }
        else
        {
          leds[i] = CRGB::Black;
        }
      }
    }
    else if (now - buttonDownTime > 30)
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i % 5 == 0)
        {
          leds[i] = CRGB::Blue;
        }
        else
        {
          leds[i] = CRGB::Black;
        }
      }
    }
  }
  else
  {
    digitalWrite(EQ_RESET_PIN, HIGH);
    digitalWrite(EQ_RESET_PIN, LOW);

    for (int i = 0; i < 7; i++)
    {
      digitalWrite(EQ_STROBE_PIN, LOW);
      delayMicroseconds(30); // 30 // to allow the output to settle
      spectrumValue[i] = analogRead(EQ_ADC_PIN);
      digitalWrite(EQ_STROBE_PIN, HIGH);
    }

    impulse = spectrumValue[0] * 0.3 + spectrumValue[1] * 0.3 + spectrumValue[2] * 2 + spectrumValue[3] * 2;
    if (impulse > envelopeUpper)
      envelopeUpper = impulse;
    if (impulse < envelopeLower)
      envelopeLower = impulse;
    if (envelopeUpper > 0)
      envelopeUpper -= 0.1;
    if (envelopeLower < 1024 * 100)
      envelopeLower += 0.1;

    double lower = envelopeLower > 100 ? 100 : envelopeLower;
    double k = (envelopeUpper - lower > 50 ? (impulse - lower) / (envelopeUpper - lower) : 0);
    uint8_t brightness = config.fullBrightness ? 255 : constrain(50.49752469 * pow(k, 8.930413727e-1), 1, 255);

    static uint16_t startIndex = 0;

    programs[programIndex](startIndex, now - patternStartTime, brightness);
    startIndex = startIndex + 1;
    bool advance = false;

    if (buttonDownTime > 0 && now - buttonDownTime > 30 && now - buttonDownTime <= 1000)
    {
      advance = true;
    }

    if (buttonDownTime > 0 && now - buttonDownTime > 1000 && now - buttonDownTime <= 1500)
    {
      config.fullBrightness = false;
      EEPROM.put(CONFIG_OFFSET, config);
    }

    if (buttonDownTime > 0 && now - buttonDownTime > 1500 && now - buttonDownTime <= 2000)
    {
      config.fullBrightness = true;
      EEPROM.put(CONFIG_OFFSET, config);
    }

    if (advance)
    {
      startIndex = 0;
      patternStartTime = now;
      programIndex++;
      if (programIndex >= sizeof(programs) / sizeof(*programs))
      {
        programIndex = 0;
      }
      config.programIndex = programIndex;
      EEPROM.put(CONFIG_OFFSET, config);
    }

    buttonDownTime = 0;
  }

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}