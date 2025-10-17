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

// 定时器专用模式参数
float timerDuration = 10.0;  // 默认10秒
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
boolean colorMode = true;     // 颜色调节模式
boolean timerMode = false;    // 定时器专用模式

// EEPROM地址
#define RED_ADDR     0
#define GREEN_ADDR   1
#define BLUE_ADDR    2
#define TIMER_ADDR   3

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

  // 从EEPROM读取保存的设置
  loadSettings();
  
  // 初始状态：灯板熄灭
  sendColor(0, 0, 0);

  // 显示启动画面
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println(F("定时器模式"));
  display.setTextSize(1);
  display.setCursor(15, 50);
  display.println(F("设置期间灯板熄灭"));
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
      // 发送1码
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(1);
      digitalWrite(LED_PIN, LOW);
      delayMicroseconds(1);
    } else {
      // 发送0码
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
    sendByte(g); // GRB顺序
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
      if (colorMode) {
        // 颜色模式：调整颜色
        adjustColor(colorIndex, 1);
        updateLEDColor(); // 实时预览颜色
      } else {
        // 定时器模式：调整时间
        adjustTimer(0.1);
      }
    } else {
      // 逆时针旋转
      if (colorMode) {
        adjustColor(colorIndex, -1);
        updateLEDColor(); // 实时预览颜色
      } else {
        // 定时器模式：调整时间
        adjustTimer(-0.1);
      }
    }
    saveSettings();
  }
  lastState = clkState;
  
  // 检查编码器按钮
  swState = digitalRead(swPin);
  if (swState == 0 && !buttonPressed) {
    buttonPressed = true;
    
    if (colorIndex == 0) {
      // 红色编码器：切换颜色模式/定时器模式
      colorMode = !colorMode;
      timerMode = !colorMode;
      
      // 模式切换时处理灯板状态
      if (colorMode) {
        // 切换到颜色模式：点亮灯板显示当前颜色
        updateLEDColor();
      } else {
        // 切换到定时器模式：熄灭灯板
        sendColor(0, 0, 0);
      }
    } 
    else if (colorIndex == 1) {
      // 绿色编码器
      if (timerMode && !timerRunning && !timerFinished) {
        // 定时器模式：启动倒计时
        startTimer();
      } else if (timerMode && timerRunning) {
        // 定时器运行中：停止计时
        stopTimer();
      }
    } 
    else if (colorIndex == 2) {
      // 蓝色编码器：重置功能
      if (colorMode) {
        resetColor(colorIndex);
      } else if (timerMode && timerFinished) {
        // 定时结束状态：重置定时器
        resetTimer();
      }
    }
    
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
      if (colorIndex == 0) {
        // 红色编码器长按：重置所有颜色为白色
        resetAllColors();
      } else if (colorIndex == 1) {
        // 绿色编码器长按：快速设置常用定时时间
        if (timerMode) {
          quickSetTimer();
        }
      } else if (colorIndex == 2) {
        // 蓝色编码器长按：保存并应用当前颜色
        saveSettings();
        if (colorMode) {
          updateLEDColor();
        }
      }
      pressTime[colorIndex] = 0;
      delay(200);
    }
  } else {
    static unsigned long pressTime[3] = {0, 0, 0};
    pressTime[colorIndex] = 0;
  }
}

void adjustColor(uint8_t channel, int delta) {
  switch(channel) {
    case 0: redValue = constrain(redValue + delta, 0, 255); break;
    case 1: greenValue = constrain(greenValue + delta, 0, 255); break;
    case 2: blueValue = constrain(blueValue + delta, 0, 255); break;
  }
}

// 更新LED颜色（仅用于颜色模式预览）
void updateLEDColor() {
  if (colorMode && !timerRunning) {
    sendColor(redValue, greenValue, blueValue);
  }
}

void resetColor(uint8_t channel) {
  switch(channel) {
    case 0: redValue = 0; break;
    case 1: greenValue = 0; break;
    case 2: blueValue = 0; break;
  }
  updateLEDColor();
  saveSettings();
}

void resetAllColors() {
  redValue = greenValue = blueValue = 255;
  updateLEDColor();
  saveSettings();
}

void adjustTimer(float delta) {
  if (timerMode && !timerRunning) {
    timerDuration = constrain(timerDuration + delta, 0.1, 60.0);
    saveSettings();
  }
}

void startTimer() {
  if (timerMode && !timerRunning && timerDuration > 0) {
    timerRunning = true;
    timerFinished = false;
    timerStartTime = millis();
    timerRemaining = timerDuration;
    
    // 启动定时器时点亮灯板
    sendColor(redValue, greenValue, blueValue);
  }
}

void stopTimer() {
  if (timerRunning) {
    timerRunning = false;
    // 停止定时器时熄灭灯板
    sendColor(0, 0, 0);
  }
}

void resetTimer() {
  timerRunning = false;
  timerFinished = false;
  timerRemaining = 0;
  // 重置时熄灭灯板
  sendColor(0, 0, 0);
}

void quickSetTimer() {
  if (timerMode && !timerRunning) {
    // 快速设置常用定时时间
    if (timerDuration < 5.0) {
      timerDuration = 5.0;
    } else if (timerDuration < 15.0) {
      timerDuration = 15.0;
    } else if (timerDuration < 30.0) {
      timerDuration = 30.0;
    } else {
      timerDuration = 60.0;
    }
    saveSettings();
  }
}

void processTimer() {
  if (timerRunning && timerRemaining > 0) {
    unsigned long currentTime = millis();
    float elapsed = (currentTime - timerStartTime) / 1000.0;
    timerRemaining = timerDuration - elapsed;
    
    if (timerRemaining <= 0) {
      // 定时时间到 - 直接熄灭灯板，不闪烁
      timerRunning = false;
      timerFinished = true;
      timerRemaining = 0;
      
      // 定时结束时直接熄灭灯板
      sendColor(0, 0, 0);
    }
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (colorMode) {
    // 颜色调节模式界面
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("颜色调节模式"));
    display.drawLine(0, 10, 128, 10, WHITE);

    display.setCursor(0, 15);
    display.print(F("红:"));
    display.print(redValue);
    display.print(F(" 绿:"));
    display.print(greenValue);
    
    display.setCursor(0, 25);
    display.print(F("蓝:"));
    display.print(blueValue);
    
    display.setCursor(0, 50);
    display.println(F("红:模式 绿:- 蓝:重置单色"));
    
    display.setCursor(0, 60);
    display.println(F("长按红:白 绿:- 蓝:保存"));
  } 
  else if (timerMode) {
    // 定时器模式界面
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("定时器模式"));
    display.drawLine(0, 10, 128, 10, WHITE);

    // 显示设置的时间
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print(timerDuration, 1);
    display.println(F(" 秒"));

    display.setTextSize(1);
    
    // 显示状态
    display.setCursor(0, 45);
    if (timerRunning) {
      display.print(F("状态: 运行中 "));
      display.print(timerRemaining, 1);
      display.println(F("秒"));
    } else if (timerFinished) {
      display.println(F("状态: 定时完成!"));
    } else {
      display.println(F("状态: 就绪 (灯板熄灭)"));
    }

    // 操作提示
    display.setCursor(0, 55);
    if (!timerRunning && !timerFinished) {
      display.println(F("红:模式 绿:启动 蓝:-"));
    } else if (timerRunning) {
      display.println(F("红:模式 绿:停止 蓝:-"));
    } else if (timerFinished) {
      display.println(F("红:模式 绿:- 蓝:重置"));
    }
  }

  display.display();
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