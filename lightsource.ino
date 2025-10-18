#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// OLED引脚定义
#define OLED_DC     11
#define OLED_CS     12
#define OLED_CLK    8
#define OLED_MOSI   9
#define OLED_RESET  4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// WS2812B灯板定义
#define LED_PIN     3
#define LED_COUNT   128  // 8x16=128个灯珠

// 三个编码器引脚定义
#define ENCODER_R_CLK A0  // 红色编码器
#define ENCODER_R_DT  A1
#define ENCODER_R_SW  2

#define ENCODER_G_CLK A2  // 绿色编码器
#define ENCODER_G_DT  A3
#define ENCODER_G_SW  5

#define ENCODER_B_CLK A4  // 蓝色编码器
#define ENCODER_B_DT  A5
#define ENCODER_B_SW  6

// 颜色参数
uint8_t redValue = 255;
uint8_t greenValue = 255;
uint8_t blueValue = 255;

// EEPROM写入保护
unsigned long lastColorChangeTime = 0;
const unsigned long EEPROM_SAVE_DELAY = 3000; // 3秒延迟
boolean needsSave = false;

// 预设相关参数
#define PRESET_COUNT 6
#define PRESET_NAME_LENGTH 8
struct Preset {
  char name[PRESET_NAME_LENGTH];
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};
Preset presets[PRESET_COUNT];
int currentPresetIndex = 0;
boolean presetModified = false;

// 定时器专用模式参数
float timerDuration = 10.0;
float timerRemaining = 0.0;
boolean timerRunning = false;
boolean timerFinished = false;
unsigned long timerStartTime = 0;

// 编码器变量
int encoderRLastState = 0;
int encoderGLastState = 0;
int encoderBLastState = 0;
boolean encoderRButtonPressed = false;
boolean encoderGButtonPressed = false;
boolean encoderBButtonPressed = false;

// 按钮状态变量
boolean EncoderRSWState = 0;
boolean EncoderGSWState = 0;
boolean EncoderBSWState = 0;

// 显示模式
boolean colorMode = true;
boolean timerMode = false;
boolean presetMode = false;
boolean presetEditMode = false;
boolean presetNameEditMode = false;

// 命名相关
char nameChars[37] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
int currentCharIndex = 0;
int currentNamePosition = 0;

// EEPROM地址
#define RED_ADDR     0
#define GREEN_ADDR   1
#define BLUE_ADDR    2
#define TIMER_ADDR   3
#define PRESET_BASE_ADDR 100  // 预设存储起始地址

// WS2812B时序参数
#define T0H  400
#define T0L  850  
#define T1H  800
#define T1L  450
#define RES  60

void setup() {
  // 初始化编码器引脚
  pinMode(ENCODER_R_CLK, INPUT_PULLUP);
  pinMode(ENCODER_R_DT, INPUT_PULLUP);
  pinMode(ENCODER_R_SW, INPUT_PULLUP);
  
  pinMode(ENCODER_G_CLK, INPUT_PULLUP);
  pinMode(ENCODER_G_DT, INPUT_PULLUP);
  pinMode(ENCODER_G_SW, INPUT_PULLUP);
  
  pinMode(ENCODER_B_CLK, INPUT_PULLUP);
  pinMode(ENCODER_B_DT, INPUT_PULLUP);
  pinMode(ENCODER_B_SW, INPUT_PULLUP);
  
  // 设置LED引脚为输出
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // 初始化显示屏
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display.setTextColor(WHITE);
  display.clearDisplay();

  // 从EEPROM读取保存的设置和预设
  loadSettings();
  loadPresets();
  
  // 初始状态：灯板熄灭
  sendColor(0, 0, 0);

  // 显示启动画面
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println(F("RGB控制器"));
  display.setTextSize(1);
  display.setCursor(15, 50);
  display.println(F("预设功能已启用"));
  display.display();
  delay(2000);
  
  // 读取初始编码器状态
  encoderRLastState = digitalRead(ENCODER_R_CLK);
  encoderGLastState = digitalRead(ENCODER_G_CLK);
  encoderBLastState = digitalRead(ENCODER_B_CLK);
}

// WS2812B比特流发送函数
void sendByte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    if (byte & (1 << i)) {
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(1);
      digitalWrite(LED_PIN, LOW);
      delayMicroseconds(1);
    } else {
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(0);
      digitalWrite(LED_PIN, LOW);
      delayMicroseconds(1);
    }
  }
}

void sendColor(uint8_t r, uint8_t g, uint8_t b) {
  noInterrupts();
  
  for (int i = 0; i < LED_COUNT; i++) {
    sendByte(g);
    sendByte(r);
    sendByte(b);
  }
  
  digitalWrite(LED_PIN, LOW);
  delayMicroseconds(RES);
  
  interrupts();
}

void loop() {
  readEncoders();
  processTimer();
  checkEEPROMSave();
  updateDisplay();
  delay(50);
}

void readEncoders() {
  readSingleEncoder(ENCODER_R_CLK, ENCODER_R_DT, ENCODER_R_SW, 
                   encoderRLastState, encoderRButtonPressed, EncoderRSWState, 0);
  readSingleEncoder(ENCODER_G_CLK, ENCODER_G_DT, ENCODER_G_SW, 
                   encoderGLastState, encoderGButtonPressed, EncoderGSWState, 1);
  readSingleEncoder(ENCODER_B_CLK, ENCODER_B_DT, ENCODER_B_SW, 
                   encoderBLastState, encoderBButtonPressed, EncoderBSWState, 2);
}

void readSingleEncoder(uint8_t clkPin, uint8_t dtPin, uint8_t swPin, 
                      int &lastState, boolean &buttonPressed, boolean &swState, uint8_t colorIndex) {
  int clkState = digitalRead(clkPin);
  int dtState = digitalRead(dtPin);
  
  if (clkState != lastState) {
    if (dtState != clkState) {
      // 顺时针旋转
      handleEncoderRotation(1, colorIndex);
    } else {
      // 逆时针旋转
      handleEncoderRotation(-1, colorIndex);
    }
  }
  lastState = clkState;
  
  // 检查编码器按钮
  swState = digitalRead(swPin);
  if (swState == 0 && !buttonPressed) {
    buttonPressed = true;
    handleEncoderClick(colorIndex);
    delay(200);
  } else if (swState == 1) {
    buttonPressed = false;
  }
  
  // 长按功能
  if (swState == 0) {
    static unsigned long pressTime[3] = {0, 0, 0};
    if (pressTime[colorIndex] == 0) {
      pressTime[colorIndex] = millis();
    }
    
    if (millis() - pressTime[colorIndex] > 1000) {
      handleEncoderLongPress(colorIndex);
      pressTime[colorIndex] = 0;
      delay(200);
    }
  } else {
    static unsigned long pressTime[3] = {0, 0, 0};
    pressTime[colorIndex] = 0;
  }
}

void handleEncoderRotation(int direction, uint8_t encoderIndex) {
  if (presetNameEditMode) {
    // 命名编辑模式
    if (encoderIndex == 0) { // 红色编码器选择字符
      currentCharIndex = (currentCharIndex + direction + 36) % 36;
      presetModified = true;
    } else if (encoderIndex == 1) { // 绿色编码器移动光标
      currentNamePosition = constrain(currentNamePosition + direction, 0, PRESET_NAME_LENGTH - 2);
      presetModified = true;
    }
    return;
  }
  
  if (presetEditMode) {
    // 预设编辑模式
    if (encoderIndex == 0) {
      adjustColor(0, direction);
      presets[currentPresetIndex].red = redValue;
      presetModified = true;
    } else if (encoderIndex == 1) {
      adjustColor(1, direction);
      presets[currentPresetIndex].green = greenValue;
      presetModified = true;
    } else if (encoderIndex == 2) {
      adjustColor(2, direction);
      presets[currentPresetIndex].blue = blueValue;
      presetModified = true;
    }
    updateLEDColor();
    return;
  }
  
  if (presetMode) {
    // 预设浏览模式
    if (encoderIndex == 0) {
      currentPresetIndex = (currentPresetIndex + direction + PRESET_COUNT) % PRESET_COUNT;
      updateLEDColor();
    }
    return;
  }
  
  if (colorMode) {
    // 颜色模式
    adjustColor(encoderIndex, direction);
    updateLEDColor();
  } else if (timerMode) {
    // 定时器模式
    if (encoderIndex == 0) {
      adjustTimer(direction * 0.1);
    }
  }
}

void handleEncoderClick(uint8_t encoderIndex) {
  if (presetNameEditMode) {
    // 命名编辑模式确认
    if (encoderIndex == 2) { // 蓝色编码器确认命名
      presetNameEditMode = false;
      // 更新预设名称
      presets[currentPresetIndex].name[currentNamePosition] = nameChars[currentCharIndex];
      presetModified = true;
    }
    return;
  }
  
  if (presetEditMode) {
    // 预设编辑模式
    if (encoderIndex == 0) { // 红色编码器切换命名模式
      presetNameEditMode = true;
      currentNamePosition = 0;
      currentCharIndex = 0;
    } else if (encoderIndex == 2) { // 蓝色编码器保存并退出
      presetEditMode = false;
      // 只在退出时保存预设到EEPROM
      savePreset(currentPresetIndex);
    }
    return;
  }
  
  if (presetMode) {
    // 预设浏览模式
    if (encoderIndex == 0) { // 红色编码器进入编辑
      presetEditMode = true;
      // 进入预设编辑时使用当前颜色值
      presets[currentPresetIndex].red = redValue;
      presets[currentPresetIndex].green = greenValue;
      presets[currentPresetIndex].blue = blueValue;
      presetModified = true;
      updateLEDColor();
    } else if (encoderIndex == 1) { // 绿色编码器应用预设
      loadPreset(currentPresetIndex);
      updateLEDColor();
      // 应用预设后立即保存主颜色到EEPROM
      immediateEEPROMSave();
    } else if (encoderIndex == 2) { // 蓝色编码器删除预设
      deletePreset(currentPresetIndex);
    }
    return;
  }
  
  // 主模式切换
  if (encoderIndex == 0) {
    // 红色编码器：循环切换模式
    if (colorMode) {
      // 从颜色模式切换到其他模式时立即保存
      immediateEEPROMSave();
      colorMode = false;
      timerMode = true;
      presetMode = false;
      sendColor(0, 0, 0);
    } else if (timerMode) {
      colorMode = false;
      timerMode = false;
      presetMode = true;
      // 进入预设模式时显示当前预设颜色
      updateLEDColor();
    } else {
      colorMode = true;
      timerMode = false;
      presetMode = false;
      updateLEDColor();
    }
  } else if (encoderIndex == 1) {
    // 绿色编码器
    if (timerMode && !timerRunning && !timerFinished) {
      startTimer();
    } else if (timerMode && timerRunning) {
      stopTimer();
    }
  }
}

void handleEncoderLongPress(uint8_t encoderIndex) {
  if (presetMode || presetEditMode || presetNameEditMode) {
    // 在预设相关模式下，长按红色编码器返回主模式
    if (encoderIndex == 0) {
      // 如果预设已修改但未保存，询问是否保存
      if (presetModified) {
        // 这里可以添加确认保存的提示，但为了简化，我们直接保存
        savePreset(currentPresetIndex);
      }
      presetMode = false;
      presetEditMode = false;
      presetNameEditMode = false;
      colorMode = true;
      updateLEDColor();
    }
    return;
  }
  
  if (encoderIndex == 0) {
    // 红色编码器长按：重置所有颜色为白色
    resetAllColors();
    updateLEDColor();
    // 重置后立即保存
    immediateEEPROMSave();
  } else if (encoderIndex == 1) {
    // 绿色编码器长按：快速设置常用定时时间
    if (timerMode) {
      quickSetTimer();
      // 快速设置后立即保存
      immediateEEPROMSave();
    }
  } else if (encoderIndex == 2) {
    // 蓝色编码器长按：保存当前颜色到当前预设
    saveToCurrentPreset();
    // 保存预设后立即保存到EEPROM
    immediateEEPROMSave();
  }
}

void adjustColor(uint8_t channel, int delta) {
  switch(channel) {
    case 0: redValue = constrain(redValue + delta, 0, 255); break;
    case 1: greenValue = constrain(greenValue + delta, 0, 255); break;
    case 2: blueValue = constrain(blueValue + delta, 0, 255); break;
  }
  
  // 标记需要保存，并重置计时器
  if (!presetEditMode) { // 在预设编辑模式下不触发主颜色保存
    needsSave = true;
    lastColorChangeTime = millis();
  }
}

// 更新LED颜色
void updateLEDColor() {
  if ((colorMode || presetMode || presetEditMode) && !timerRunning) {
    sendColor(redValue, greenValue, blueValue);
  }
}

void resetAllColors() {
  redValue = greenValue = blueValue = 255;
  updateLEDColor();
  scheduleEEPROMSave();
}

void adjustTimer(float delta) {
  if (timerMode && !timerRunning) {
    timerDuration = constrain(timerDuration + delta, 0.1, 60.0);
    scheduleEEPROMSave();
  }
}

void startTimer() {
  if (timerMode && !timerRunning && timerDuration > 0) {
    timerRunning = true;
    timerFinished = false;
    timerStartTime = millis();
    timerRemaining = timerDuration;
    sendColor(redValue, greenValue, blueValue);
  }
}

void stopTimer() {
  if (timerRunning) {
    timerRunning = false;
    sendColor(0, 0, 0);
  }
}

void resetTimer() {
  timerRunning = false;
  timerFinished = false;
  timerRemaining = 0;
  sendColor(0, 0, 0);
}

void quickSetTimer() {
  if (timerMode && !timerRunning) {
    if (timerDuration < 5.0) {
      timerDuration = 5.0;
    } else if (timerDuration < 15.0) {
      timerDuration = 15.0;
    } else if (timerDuration < 30.0) {
      timerDuration = 30.0;
    } else {
      timerDuration = 60.0;
    }
    scheduleEEPROMSave();
  }
}

void processTimer() {
  if (timerRunning && timerRemaining > 0) {
    unsigned long currentTime = millis();
    float elapsed = (currentTime - timerStartTime) / 1000.0;
    timerRemaining = timerDuration - elapsed;
    
    if (timerRemaining <= 0) {
      timerRunning = false;
      timerFinished = true;
      timerRemaining = 0;
      sendColor(0, 0, 0);
    }
  }
}

void checkEEPROMSave() {
  if (needsSave && (millis() - lastColorChangeTime > EEPROM_SAVE_DELAY)) {
    saveSettings();
    needsSave = false;
  }
}

void scheduleEEPROMSave() {
  needsSave = true;
  lastColorChangeTime = millis();
}

// 立即保存到EEPROM
void immediateEEPROMSave() {
  saveSettings();
  needsSave = false;
}

// 预设功能
void loadPreset(int index) {
  redValue = presets[index].red;
  greenValue = presets[index].green;
  blueValue = presets[index].blue;
  updateLEDColor();
}

void saveToCurrentPreset() {
  presets[currentPresetIndex].red = redValue;
  presets[currentPresetIndex].green = greenValue;
  presets[currentPresetIndex].blue = blueValue;
  savePreset(currentPresetIndex);
}

void deletePreset(int index) {
  // 重置为默认值
  presets[index].red = 255;
  presets[index].green = 255;
  presets[index].blue = 255;
  strcpy(presets[index].name, "PRESET");
  char numStr[2];
  itoa(index + 1, numStr, 10);
  strcat(presets[index].name, numStr);
  savePreset(index);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (presetNameEditMode) {
    displayPresetNameEdit();
  } else if (presetEditMode) {
    displayPresetEdit();
  } else if (presetMode) {
    displayPresetMode();
  } else if (colorMode) {
    displayColorMode();
  } else if (timerMode) {
    displayTimerMode();
  }

  display.display();
}

void displayColorMode() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("颜色调节模式"));
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 20);
  display.print(F("红: "));
  display.print(redValue);
  display.print(F("/255"));
  
  display.setCursor(0, 35);
  display.print(F("绿: "));
  display.print(greenValue);
  display.print(F("/255"));
  
  display.setCursor(0, 50);
  display.print(F("蓝: "));
  display.print(blueValue);
  display.print(F("/255"));
}

void displayTimerMode() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("定时器模式"));
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setTextSize(2);
  display.setCursor(20, 25);
  display.print(timerDuration, 1);
  display.println(F(" 秒"));

  display.setTextSize(1);
  display.setCursor(0, 50);
  if (timerRunning) {
    display.print(F("运行中: "));
    display.print(timerRemaining, 1);
    display.println(F("秒"));
  } else if (timerFinished) {
    display.println(F("定时完成"));
  } else {
    display.println(F("就绪 - 灯板熄灭"));
  }
}

void displayPresetMode() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("预设模式"));
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 20);
  display.print(F("预设 "));
  display.print(currentPresetIndex + 1);
  display.print(F("/"));
  display.print(PRESET_COUNT);
  
  display.setCursor(0, 35);
  display.print(F("名称: "));
  display.print(presets[currentPresetIndex].name);
  
  display.setCursor(0, 50);
  display.print(F("RGB: "));
  display.print(presets[currentPresetIndex].red);
  display.print(F(","));
  display.print(presets[currentPresetIndex].green);
  display.print(F(","));
  display.print(presets[currentPresetIndex].blue);
}

void displayPresetEdit() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("编辑预设 "));
  display.print(currentPresetIndex + 1);
  if (presetModified) {
    display.print(F(" *")); // 显示修改标记
  }
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 20);
  display.print(F("名称: "));
  display.print(presets[currentPresetIndex].name);
  
  display.setCursor(0, 35);
  display.print(F("红: "));
  display.print(redValue);
  display.print(F("/255"));
  
  display.setCursor(0, 50);
  display.print(F("绿: "));
  display.print(greenValue);
  display.print(F("/255 蓝: "));
  display.print(blueValue);
  display.print(F("/255"));
}

void displayPresetNameEdit() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("命名预设"));
  display.drawLine(0, 10, 128, 10, WHITE);

  // 显示当前名称
  display.setCursor(0, 25);
  for (int i = 0; i < PRESET_NAME_LENGTH - 1; i++) {
    if (i == currentNamePosition) {
      display.setTextColor(BLACK, WHITE); // 反白显示当前光标位置
    }
    display.print(presets[currentPresetIndex].name[i]);
    display.setTextColor(WHITE);
  }
  
  // 显示当前可选字符
  display.setCursor(0, 45);
  display.print(F("字符: "));
  display.print(nameChars[currentCharIndex]);
}

void loadSettings() {
  redValue = EEPROM.read(RED_ADDR);
  greenValue = EEPROM.read(GREEN_ADDR);
  blueValue = EEPROM.read(BLUE_ADDR);
  
  uint16_t timerTenths = 0;
  EEPROM.get(TIMER_ADDR, timerTenths);
  timerDuration = timerTenths / 10.0;
}

void saveSettings() {
  EEPROM.update(RED_ADDR, redValue);
  EEPROM.update(GREEN_ADDR, greenValue);
  EEPROM.update(BLUE_ADDR, blueValue);
  
  uint16_t timerTenths = round(timerDuration * 10);
  EEPROM.put(TIMER_ADDR, timerTenths);
}

void loadPresets() {
  for (int i = 0; i < PRESET_COUNT; i++) {
    int addr = PRESET_BASE_ADDR + i * sizeof(Preset);
    EEPROM.get(addr, presets[i]);
    
    // 如果名称为空，设置默认名称
    if (presets[i].name[0] == 0 || presets[i].name[0] == 255) {
      strcpy(presets[i].name, "PRESET");
      char numStr[2];
      itoa(i + 1, numStr, 10);
      strcat(presets[i].name, numStr);
      presets[i].red = 255;
      presets[i].green = 255;
      presets[i].blue = 255;
    }
  }
}

void savePreset(int index) {
  int addr = PRESET_BASE_ADDR + index * sizeof(Preset);
  EEPROM.put(addr, presets[index]);
  presetModified = false;
}