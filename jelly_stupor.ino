#define LED_PIN   9
#define BRIGHTNESS  255
#define MATRIX_WIDTH  32
#define MATRIX_HEIGHT 8
#define NUM_LEDS  256
#define LED_TYPE  WS2811
#define COLOR_ORDER RGB

#define EQ_ADC_PIN A0
#define EQ_STROBE_PIN 7
#define EQ_RESET_PIN 8

#define SPECTRUM_HISTORY_SIZE 512
#define SPECTRUM_IMPULSE_SIZE 5

#define UPDATES_PER_SECOND 200

#include <FastLED.h>

CRGB leds[NUM_LEDS];
uint8_t programIndex = 0;
uint16_t spectrumValue[7];
double impulse;
double envelopeUpper = 0;
double envelopeLower = 0;

typedef bool (*ProgramFunction) (uint16_t a);

bool fftResponse(uint16_t index)
{
  for (int i = 0; i < 8 * 7; i++) {
    int bin = i / 8;
    int k = bin % 2 == 0 ? i % 8 : 7 - i % 8;
    uint8_t brightness;
    int adjValue = spectrumValue[bin];
    if (adjValue >= (k + 1) * 128) {
      brightness = 255;
    } else if (adjValue > k * 128) {
      brightness = (adjValue - k * 128) / 128.0 * 255;
    } else {
      brightness = 0;
    }
    leds[i] = CHSV(30, 255, brightness);
  }

  return false;
}

bool fftNoise(uint16_t index)
{
  static uint8_t hues[NUM_LEDS];
  static uint16_t xOffset = 0;
  static uint16_t yOffset = 0;
  double lower = envelopeLower > 100 ? 100 : envelopeLower;
  double k = (envelopeUpper - lower > 50 ? (impulse - lower) / (envelopeUpper - lower) : 0);
  uint8_t brightness = constrain(50.49752469 * pow(k, 8.930413727e-1), 1, 255);
  //Serial.print(k);
  //Serial.print(" ");
  //Serial.print(brightness);
  //Serial.println();

  for (int i = 0; i < NUM_LEDS; i++) {
    int x, y;

    if (i < 256) {
      x = i / 8;
      y = x % 2 == 0 ? i % 8 : 7 - (i % 8);
    } else {
      x = (255 - (i - 256)) / 8;
      y = x % 2 == 0 ? (i % 8) + 8 : 15 - (i % 8);
    }
    hues[i] = inoise16((x + xOffset) * 2048 + index * 256, (y + yOffset) * 2048 + index * 64) >> 8;
    leds[i] = ColorFromPalette(RainbowColors_p, hues[i], brightness, LINEARBLEND);
  }

  //fftResponse(index);

  return false;
}

ProgramFunction programs[] =
{
  //fluorescent,
  fftNoise,
};

void setup()
{
  //Serial.begin(9600);
  //delay(1000);

  pinMode(EQ_ADC_PIN, INPUT);
  pinMode(EQ_STROBE_PIN, OUTPUT);
  pinMode(EQ_RESET_PIN, OUTPUT);
  analogReference(DEFAULT);

  digitalWrite(EQ_RESET_PIN, LOW);
  digitalWrite(EQ_STROBE_PIN, HIGH);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }

  FastLED.show();
}

void loop()
{
  digitalWrite(EQ_RESET_PIN, HIGH);
  digitalWrite(EQ_RESET_PIN, LOW);

  for (int i = 0; i < 7; i++)
  {
    digitalWrite(EQ_STROBE_PIN, LOW);
    delayMicroseconds(100000); // 30 // to allow the output to settle
    spectrumValue[i] = analogRead(EQ_ADC_PIN);
    digitalWrite(EQ_STROBE_PIN, HIGH);
  }

  impulse = spectrumValue[0] * 0.3 + spectrumValue[1] * 0.3 + spectrumValue[2] * 2 + spectrumValue[3] * 2;
  if (impulse > envelopeUpper) envelopeUpper = impulse;
  if (impulse < envelopeLower) envelopeLower = impulse;
  if (envelopeUpper > 0) envelopeUpper -= 0.1;
  if (envelopeLower < 1024 * 100) envelopeLower += 0.1;

  uint16_t now = millis();
  static uint16_t startIndex = 0;

  bool advance = programs[programIndex](startIndex);
  startIndex = startIndex + 1;

  if (advance) {
    startIndex = 0;

    if (programIndex + 1 >= sizeof(programs) / sizeof(*programs)) {
      programIndex = 0;
    } else {
      programIndex++;
    }
  }

  FastLED.show();
  //FastLED.delay(1000 / UPDATES_PER_SECOND);
}