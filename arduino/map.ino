/*
 * Modular Audio Processor - Arduino Implementation
 * Main file for audio DSP block with I2S input/output
 * For Adafruit Feather RP2040
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <I2S.h>
#include <ADCInput.h>
#include <PWMAudio.h>


// ==================== CONFIGURATION ====================

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 512
#define BITS_PER_SAMPLE 16


      //PURPOSE          // BOARD LABEL
#define OLED_SCK     10  // 10
#define OLED_SDA     11  // 11
#define OLED_RESET   28  // A2
#define OLED_DC      29  // A3
#define OLED_CS      24  // 24

#define I2S_OUT_BCLK  7  // D5, WSEL is 8
#define I2S_OUT_DOUT  9  // D9

#define I2S_IN_BCLK   0  // TX, WSEL is RX
#define I2S_IN_DOUT   20 // MISO

#define ADC_IN        26  // A0

#define SPKR_PWR      5
#define PWM_PIN       6

// DSP Modes
enum DSPMode {
  MODE_PASSTHROUGH = 0,
  MODE_LOWPASS,
  MODE_HIGHPASS,
  MODE_REVERB,
  MODE_DISTORTION,
  MODE_GAIN,
  MODE_COUNT
};

const char* modeNames[] = {
  "Pass",
  "Lo-Pass",
  "Hi-Pass",
  "Reverb",
  "Distort",
  "Gain"
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI1, OLED_DC, OLED_RESET, OLED_CS);
I2S i2s_in(INPUT);
I2S i2s_out(OUTPUT);

ADCInput adc(A0);

PWMAudio pwm(PWM_PIN);

// Global state
DSPMode currentMode = MODE_LOWPASS;
int16_t inputBuffer[BUFFER_SIZE];
int16_t outputBuffer[BUFFER_SIZE];

uint32_t frameCount = 0;
uint32_t lastFpsTime = 0;
uint32_t fps = 0;

bool useADC = true;  
int samplesRead = 0;
uint32_t wavePhase = 0;


// ==================== ADC CALLBACK ====================
void adcCallback() {
}


// ==================== DISPLAY ====================

void updateDisplay() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);
  display.setTextSize(1);

  //Current mode
  display.setCursor(4, 3);
  display.println(modeNames[currentMode]);
  
  //Input indicator
  display.setCursor(104, 3);
  if (useADC) {
    display.print("ADC");
  } else {
    display.print("I2S");
  }
  
  //Dashed center line
  for(int i = 0; i < 128; i+= 4){
    display.drawPixel(i, 37, SSD1306_WHITE);
  }

  //Waveform 
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

void generateTestWaveform(int16_t* buffer, int size) {  
  for (int i = 0; i < size; i++) {
    float phase1 = (float)(wavePhase + i) * 440.0f * 2.0f * PI / SAMPLE_RATE;  // 440 Hz
    float phase2 = (float)(wavePhase + i) * 880.0f * 2.0f * PI / SAMPLE_RATE;  // 880 Hz
    float phase3 = (float)(wavePhase + i) * 220.0f * 2.0f * PI / SAMPLE_RATE;  // 220 Hz
    float phase4 = (float)(wavePhase + i) * 110.0f * 2.0f * PI / SAMPLE_RATE;  // 110 Hz
    
    float sample = (sin(phase1) * 0.3f +
                    sin(phase2) * 0.4f +
                    sin(phase3) * 0.25f +
                    sin(phase4) * 0.15f);
    
    buffer[i] = (int16_t)(sample * 20000.0f);
  }
  
  wavePhase += size;
  if (wavePhase > SAMPLE_RATE) {
    wavePhase -= SAMPLE_RATE;
  }
}

// ==================== DSP EFFECTS ====================

class LowPassFilter {
private:
  float alpha;
  float prevOutput;
  
public:
  LowPassFilter(float cutoff = 0.3) : alpha(cutoff), prevOutput(0) {}
  
  int16_t process(int16_t sample) {
    float output = alpha * sample + (1.0f - alpha) * prevOutput;
    prevOutput = output;
    return (int16_t)output;
  }
  
  void reset() { prevOutput = 0; }
};

class HighPassFilter {
private:
  float alpha;
  float prevInput;
  float prevOutput;
  
public:
  HighPassFilter(float cutoff = 0.1) : alpha(cutoff), prevInput(0), prevOutput(0) {}
  
  int16_t process(int16_t sample) {
    float output = alpha * (prevOutput + sample - prevInput);
    prevInput = sample;
    prevOutput = output;
    return (int16_t)output;
  }
  
  void reset() {
    prevInput = 0;
    prevOutput = 0;
  }
};

class ReverbEffect {
private:
  int16_t* delayBuffer;
  int delayLength;
  int writePos;
  float mix;
  float feedback;
  
public:
  ReverbEffect(int delayMs = 100, float mixAmount = 0.3, float feedbackAmount = 0.5) {
    delayLength = (delayMs * SAMPLE_RATE) / 1000;
    delayBuffer = new int16_t[delayLength];
    memset(delayBuffer, 0, delayLength * sizeof(int16_t));
    writePos = 0;
    mix = mixAmount;
    feedback = feedbackAmount;
  }
  
  ~ReverbEffect() {
    delete[] delayBuffer;
  }
  
  int16_t process(int16_t sample) {
    int16_t delayed = delayBuffer[writePos];
    int32_t toWrite = sample + (int32_t)(delayed * feedback);
    delayBuffer[writePos] = constrain(toWrite, -32768, 32767);
    writePos = (writePos + 1) % delayLength;
    int32_t output = (int32_t)(sample * (1.0f - mix) + delayed * mix);
    return constrain(output, -32768, 32767);
  }
  
  void reset() {
    memset(delayBuffer, 0, delayLength * sizeof(int16_t));
    writePos = 0;
  }
};

class DistortionEffect {
private:
  float gain;
  int16_t threshold;
  
public:
  DistortionEffect(float gainAmount = 3.0, int16_t thresholdValue = 8000) 
    : gain(gainAmount), threshold(thresholdValue) {}
  
  int16_t process(int16_t sample) {
    int32_t output = (int32_t)(sample * gain);
    if (output > threshold) {
      output = threshold + (output - threshold) / 4;
    } else if (output < -threshold) {
      output = -threshold + (output + threshold) / 4;
    }
    return constrain(output, -32768, 32767);
  }
};

class GainEffect {
private:
  float gain;
  
public:
  GainEffect(float gainAmount = 2.0) : gain(gainAmount) {}
  
  int16_t process(int16_t sample) {
    int32_t output = (int32_t)(sample * gain);
    return constrain(output, -32768, 32767);
  }
};

LowPassFilter lowpass(0.3);
HighPassFilter highpass(0.1);
ReverbEffect reverb(150, 0.4, 0.5);
DistortionEffect distortion(3.0, 8000);
GainEffect gain(2.0);


// ==================== AUDIO PROCESSING ====================

void processAudioBuffer(int16_t* input, int16_t* output, int size) {
  for (int i = 0; i < size; i++) {
    int16_t sample = input[i];
    
    switch (currentMode) {
      case MODE_PASSTHROUGH:
        output[i] = sample;
        break;
        
      case MODE_LOWPASS:
        output[i] = lowpass.process(sample);
        break;
        
      case MODE_HIGHPASS:
        output[i] = highpass.process(sample);
        break;
        
      case MODE_REVERB:
        output[i] = reverb.process(sample);
        break;
        
      case MODE_DISTORTION:
        output[i] = distortion.process(sample);
        break;
        
      case MODE_GAIN:
        output[i] = gain.process(sample);
        break;
        
      default:
        output[i] = sample;
        break;
    }
  }
}

// ==================== ARDUINO SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("Modular Audio Processor");
  Serial.println("=================================");
  
  // Initialize SPI for OLED
  SPI1.setTX(OLED_SDA);
  SPI1.setSCK(OLED_SCK);
  SPI1.begin();
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("ERROR: SSD1306 initialization failed!");
    while (1);
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 10);
  display.println("Modular");
  display.setCursor(5, 25);
  display.println("Audio");
  display.setCursor(5, 40);
  display.println("Processor!");
  display.display();
  delay(2000);
  
  lastFpsTime = millis();

  Serial.println("Starting I2S Out...");
  i2s_out.setBCLK(I2S_OUT_BCLK);
  i2s_out.setDATA(I2S_OUT_DOUT);
  i2s_out.setBitsPerSample(16);
  i2s_out.setFrequency(SAMPLE_RATE);
  i2s_out.begin();

  if (useADC) {
    Serial.println("Starting ADC Input...");
    adc.setFrequency(SAMPLE_RATE);
    adc.onReceive(adcCallback);
    adc.begin();
    Serial.println("ADC synchronized to 44.1kHz");
  } else {
    Serial.println("Starting I2S In...");
    i2s_in.setBCLK(I2S_IN_BCLK);
    i2s_in.setDATA(I2S_IN_DOUT);
    i2s_in.setBitsPerSample(16);
    i2s_in.setFrequency(SAMPLE_RATE);
    i2s_in.begin();
  }

  pwm.setFrequency(44100);
  pwm.begin();

  updateDisplay();
  
  Serial.println("System Ready!");
  Serial.print("Sample Rate: ");
  Serial.print(SAMPLE_RATE);
  Serial.println(" Hz");
  Serial.print("Input: ");
  Serial.println(useADC ? "ADC" : "I2S");
  Serial.println("=================================");
}

// ==================== ARDUINO MAIN LOOP ====================

void loop() {
  samplesRead = 0;
  
  // Get input samples based on source
  if (useADC) {
    // Read from ADC
    for (int i = 0; i < BUFFER_SIZE; i++) {
      int16_t adcValue = adc.read();
      inputBuffer[i] = (adcValue - 2048)*32;
      pwm.write((adcValue - 2048)*32);
    }
    samplesRead = BUFFER_SIZE;
    
  } else {
    // Read from I2S input
    uint8_t tempBuffer[BUFFER_SIZE * 2];
    size_t bytesRead = i2s_in.read(tempBuffer, BUFFER_SIZE * sizeof(int16_t));
    
    // Convert bytes to int16_t samples
    samplesRead = bytesRead / sizeof(int16_t);
    for (int i = 0; i < samplesRead; i++) {
      inputBuffer[i] = ((int16_t*)tempBuffer)[i];
    }
  }

  if (samplesRead > 0) {
    // Process audio through DSP
    processAudioBuffer(inputBuffer, outputBuffer, samplesRead);
    
    // Write to I2S output
    for(int i = 0; i < samplesRead; i++){
      i2s_out.write((int16_t)outputBuffer[i]);
    }
    
    frameCount++;
  }

  uint32_t now = millis();
  if (now - lastFpsTime >= 2000) {
    fps = frameCount / 2;
    frameCount = 0;
    lastFpsTime = now;
    currentMode = (DSPMode)((currentMode + 1) % MODE_COUNT); //Switch modes
    
    Serial.print("Mode: ");
    Serial.print(modeNames[currentMode]);
    Serial.print(" | FPS: ");
    Serial.print(fps);
    Serial.print(" | Samples/sec: ");
    Serial.println(fps * BUFFER_SIZE);
  }
  updateDisplay();
  frameCount++;
}