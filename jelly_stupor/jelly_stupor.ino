#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS 40
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define LED_PIN 9
#define EQ_ADC_PIN A0
#define EQ_STROBE_PIN 7
#define EQ_RESET_PIN 8
#define BUTTON_PIN 5

#define INITIAL_BRIGHTNESS 20
#define UPDATES_PER_SECOND 200

#include <FastLED.h>

CRGB leds[NUM_LEDS];
uint8_t programIndex = 0;
uint16_t spectrumValue[7];
uint16_t buttonDownTime = 0;
double impulse;
double envelopeUpper = 0;
double envelopeLower = 0;

typedef bool (*ProgramFunction)(uint16_t a, uint8_t brightness);

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

bool rainbow(uint16_t index, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = ColorFromPalette(RainbowColors_p, index * 2, brightness, LINEARBLEND);
  }

  return false; //index / UPDATES_PER_SECOND > 5;
}

bool rainbowCircle(uint16_t index, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    uint8_t k = (i % 40) * 6.4;
    leds[i] = ColorFromPalette(RainbowColors_p, index + k * 20, brightness, LINEARBLEND);
  }

  return false; //index / UPDATES_PER_SECOND > 5;
}

bool flash(uint16_t index, uint8_t brightness)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = brightness > 20 ? CRGB::White : CRGB::Black;
  }

  return false; //index / UPDATES_PER_SECOND > 5;
}

ProgramFunction programs[] = {
    rainbowCircle,
    flash,
};

void setup()
{
  pinMode(EQ_ADC_PIN, INPUT);
  pinMode(EQ_STROBE_PIN, OUTPUT);
  pinMode(EQ_RESET_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  analogReference(DEFAULT);

  digitalWrite(EQ_RESET_PIN, LOW);
  digitalWrite(EQ_STROBE_PIN, HIGH);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(INITIAL_BRIGHTNESS);

  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black;
  }

  FastLED.show();
}

void setBrightnessLevel(uint8_t brightness)
{
  FastLED.setBrightness(brightness);

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
    }
    else if (now - buttonDownTime > 5000)
    {
      setBrightnessLevel(255);
    }
    else if (now - buttonDownTime > 4000)
    {
      setBrightnessLevel(180);
    }
    else if (now - buttonDownTime > 3000)
    {
      setBrightnessLevel(100);
    }
    else if (now - buttonDownTime > 2000)
    {
      setBrightnessLevel(80);
    }
    else if (now - buttonDownTime > 1500)
    {
      setBrightnessLevel(50);
    }
    else if (now - buttonDownTime > 1000)
    {
      setBrightnessLevel(INITIAL_BRIGHTNESS);
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
    uint8_t brightness = constrain(50.49752469 * pow(k, 8.930413727e-1), 1, 255);

    static uint16_t startIndex = 0;

    bool advance = programs[programIndex](startIndex, brightness);
    startIndex = startIndex + 1;

    if (buttonDownTime > 0 && now - buttonDownTime > 30)
    {
      advance = true;
    }

    if (advance)
    {
      startIndex = 0;

      if (programIndex + 1 >= sizeof(programs) / sizeof(*programs))
      {
        programIndex = 0;
      }
      else
      {
        programIndex++;
      }
    }

    buttonDownTime = 0;
  }

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}