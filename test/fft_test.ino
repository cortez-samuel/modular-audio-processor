#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <I2S.h>
#include <ADCInput.h>
#include <stdlib.h>
#include <PWMAudio.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 512
#define BITS_PER_SAMPLE 16
#define OLED_SCK 10
#define OLED_SDA 11
#define OLED_RESET 28
#define OLED_DC 29
#define OLED_CS 24
#define I2S_OUT_BCLK 7
#define I2S_OUT_DOUT 9
#define I2S_IN_BCLK 0
#define I2S_IN_DOUT 20
#define ADC_IN 26
#define ALPHA_ADC_PIN 27
#define PWM_PIN 6
#define SWITCH_MODE 13

const float ADC_SMOOTHING_ALPHA = 0.05f;
enum DSPMode{MODE_PASSTHROUGH = 0, MODE_LOWPASS, MODE_HIGHPASS, MODE_COUNT};
const char* modeNames[] = {"SRC", "LPF", "HPF"};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI1, OLED_DC, OLED_RESET, OLED_CS);
I2S i2s_in(INPUT);
I2S i2s_out(OUTPUT);
ADCInput adc(A0,A1);
PWMAudio pwm(PWM_PIN);
DSPMode currentMode = MODE_PASSTHROUGH;
int16_t inputBuffer[BUFFER_SIZE];
int16_t outputBuffer[BUFFER_SIZE];
uint32_t frameCount = 0;
bool useADC = false;
int samplesRead = 0;
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
float currentAlpha = 0.0f;

void updateDisplay() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 3);
  display.println(modeNames[currentMode]);
  if(currentMode == MODE_LOWPASS || currentMode == MODE_HIGHPASS){
    display.setCursor(90, 3);
    display.print("a=");
    char alphaBuf[5];
    dtostrf(currentAlpha, 4, 2, alphaBuf);
    display.println(alphaBuf);
  }
  for(int i = 0; i < 128; i+= 4){display.drawPixel(i, 37, SSD1306_WHITE);}
  int last = 37;
  for (int x = 0; x < 128; x++) {
    int idx = (x * BUFFER_SIZE) / 128;
    int y = 37 - (outputBuffer[idx] / 1300);
    y = constrain(y, 15, 59);
    int diff = y - last;
    if(diff >= 2){
      for(int i = last; i < y; i++){
        display.drawPixel(x, i, SSD1306_WHITE);
      }
    }
    else if(diff <= -2){
      for(int i = last; i > y; i--){
        display.drawPixel(x, i, SSD1306_WHITE);
      }
    }
    display.drawPixel(x, y, SSD1306_WHITE);
    last = y;
  }
  display.display();
}

class LowPassFilter {
private:
  float alpha;
  float prevOutput;
public:
  LowPassFilter(float cutoff = 0.3) : alpha(cutoff), prevOutput(0){}
  void setAlpha(float newAlpha) {alpha = newAlpha;}
  int16_t process(int16_t sample){
    // y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    float output = alpha * sample + (1.0f - alpha) * prevOutput;
    prevOutput = output;
    return (int16_t)output;
  }
  void reset(){prevOutput = 0;}
};

class HighPassFilter {
private:
  float alpha;
  float prevInput;
  float prevOutput;
public:
  HighPassFilter(float cutoff = 0.1) : alpha(cutoff), prevInput(0), prevOutput(0) {}
  void setAlpha(float newAlpha){alpha = newAlpha;}
  int16_t process(int16_t sample) {
    // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    float output = alpha * (prevOutput + sample - prevInput);
    prevInput = sample;
    prevOutput = output;
    return (int16_t)output;
  }
  void reset(){
    prevInput = 0;
    prevOutput = 0;
  }
};

LowPassFilter lowpass(0.3);
HighPassFilter highpass(0.1);

void processAudioBuffer(int16_t* input, int16_t* output, int size) {
  for(unsigned int i = 0; i < size; i++){
    int16_t sample = input[i];
    switch(currentMode){
      case MODE_PASSTHROUGH:
        output[i] = sample;
        break;
      case MODE_LOWPASS:
        output[i] = lowpass.process(sample);
        break;
      case MODE_HIGHPASS:
        output[i] = highpass.process(sample);
        break;
      default:
        output[i] = sample;
        break;
    }
  }
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  pinMode(SWITCH_MODE, INPUT_PULLUP);
  pinMode(ALPHA_ADC_PIN, INPUT);
  analogReadResolution(12);
  int initialAlphaADC = analogRead(ALPHA_ADC_PIN);
  currentAlpha = (float)initialAlphaADC / ((3.2f / 3.3f) * 4095.0f);
  currentAlpha = constrain(currentAlpha, 0.0f, 1.0f);
  SPI1.setTX(OLED_SDA);
  SPI1.setSCK(OLED_SCK);
  SPI1.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC)){while(1);}
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 10);
  display.println("Modular");
  display.setCursor(5, 25);
  display.println("Audio");
  display.setCursor(5, 40);
  display.println("Processor");
  display.display();
  delay(2000);
  i2s_out.setBCLK(I2S_OUT_BCLK);
  i2s_out.setDATA(I2S_OUT_DOUT);
  i2s_out.setBitsPerSample(16);
  i2s_out.setFrequency(SAMPLE_RATE);
  i2s_out.begin();
  if(useADC){
    adc.setFrequency(SAMPLE_RATE);
    adc.begin();
  }
  else{
    i2s_in.setBCLK(I2S_IN_BCLK);
    i2s_in.setDATA(I2S_IN_DOUT);
    i2s_in.setBitsPerSample(16);
    i2s_in.setFrequency(SAMPLE_RATE);
    i2s_in.begin();
  }
  pwm.setFrequency(44100);
  pwm.begin();
  updateDisplay();
}

void loop(){
  samplesRead = 0;
  if(useADC){
    for(unsigned int i = 0; i < BUFFER_SIZE; i++){
      int16_t adcValue = adc.read();
      inputBuffer[i] = (adcValue - 2048)*32;
      i2s_out.write((int32_t)((adcValue - 2048)*32));
    }
    samplesRead = BUFFER_SIZE; 
  }
  else{
    // Read from I2S Input
    uint8_t tempBuffer[BUFFER_SIZE * 2]; // 16-bit sample is 2 bytes
    size_t bytesRead = i2s_in.read(tempBuffer, BUFFER_SIZE * sizeof(int16_t));
    samplesRead = bytesRead / sizeof(int16_t);
    for(unsigned int i = 0; i < samplesRead; i++){
      int16_t sample = (tempBuffer[i*2] << 8) | tempBuffer[i*2 + 1];
      inputBuffer[i] = sample;
    }
  }

  int rawAlphaADC = analogRead(ALPHA_ADC_PIN);
  float rawAlpha = (float)rawAlphaADC / ((3.2f / 3.3f) * 4095.0f);
  rawAlpha = constrain(rawAlpha, 0.0f, 1.0f);
  currentAlpha = (rawAlpha * ADC_SMOOTHING_ALPHA) + (currentAlpha * (1.0f - ADC_SMOOTHING_ALPHA));
  lowpass.setAlpha(currentAlpha);
  highpass.setAlpha(currentAlpha);
if(samplesRead > 0){
  processAudioBuffer(inputBuffer, outputBuffer, samplesRead);
  for(unsigned int i = 0; i < samplesRead; i++){
    i2s_out.write((int32_t)outputBuffer[i]); 
  }
}
  int reading = digitalRead(SWITCH_MODE);
  if(reading != lastButtonState){lastDebounceTime = millis();} 
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        currentMode = (DSPMode)((currentMode + 1) % MODE_COUNT);
        lowpass.reset();
        highpass.reset();
      }
    }
  }
  lastButtonState = reading;
  updateDisplay();
  frameCount++;
}