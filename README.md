# PrEugen_LightSourse
基于Arduino的WS2812B灯板控制器（8x16网格），用于彩色负片胶卷放大，采用三个旋转编码器进行RGB颜色控制和专用定时器模式。系统提供直观的LED颜色控制和精确的定时功能。

功能

    三编码器控制：三个独立的旋转编码器分别控制RGB颜色
    
    颜色模式：实时RGB颜色调整，带实时预览
    
    定时器模式：设置倒计时时间（0.1-60.0秒），自动控制LED状态
    
    记忆功能：自动保存RGB值和定时设置到EEPROM

硬件要求

    Arduino Nano (ATmega328P)    
    
    WS2812B LED灯板 (8x16, 128个灯珠)    
    
    3个带按键的旋转编码器  
    
    0.96寸OLED显示屏 (SSD1306, 128x64) 
    
    外部5V电源
    

v1.0 发布了项目

A Arduino-based controller for WS2812B LED panels (8x16 grid) featuring three rotary encoders for RGB color control and a dedicated timer mode，for enlarge colorful negative films. The system provides intuitive control over LED colors and precise timing functions.

Features

    Triple Encoder Control: Three separate rotary encoders for independent RGB color adjustment
    
    Color Mode: Real-time RGB color adjustment with live preview
    
    Timer Mode: Set countdown duration (0.1-60.0 seconds) with automatic LED control
    
    Memory Function: Automatically saves RGB values and timer settings to EEPROM
    

Hardware Requirements

    Arduino Nano (ATmega328P)
    
    WS2812B LED Panel (8x16, 128 LEDs)
    
    3x Rotary Encoders with push buttons
    
    0.96" OLED Display (SSD1306, 128x64)
    
    External 5V power supply (recommended for full brightness)
    
v1.0 Updated the project

