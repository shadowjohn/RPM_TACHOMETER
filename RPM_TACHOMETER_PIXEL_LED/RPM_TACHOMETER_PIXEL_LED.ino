/*
 * 七段顯示器 轉速表 RPM TACHOMETER
 * Author: 羽山 (https://3wa.tw)
 * Author: @FB 田峻墉
 * Release Date: 2024-06-20
 * D7 TM1637 CLK
 * D6 TM1637 DIO
 * D1 接至 PC817，為轉速訊號接入端
 * D2 接 Pixel LED DI
 * 注：使用 Nodemcu 建議避開 D0、D3、D5 等接腳，在有接東西時，過電開機或 Reset 有時都不開，拔掉才能正常...
 */
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
#define LED_PIN    D2 //接 Pixel LED
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 16
// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
 
#include <Arduino.h>
#include <TM1637.h> //七段數位模組

#define ToPin D1    //凸台、或轉速訊號線
#define CLK D7      //接 TM1637 CLK
#define DIO D6      //接 TM1637 DIO

TM1637 tm1637(CLK, DIO);  //宣告 TM1637 使用接腳方法
volatile unsigned long C = micros(); //本次偵測到凸台的時間
volatile unsigned long C_old = 0; //上一次偵測到凸台的時間
volatile unsigned long rpm = 0; //換算後的轉速
volatile unsigned long RPM_DELAY = 0; //每一次轉速，凸台與凸台經過的時間
volatile unsigned int isShowCount = 0; //每次加一，每經過 100 次才更新一次七段，以免眼睛跟不上

/*
轉速   60 轉 =  每分鐘    60 轉，每秒  1    轉，1轉 = 1          秒 = 1000.000 ms = 1000000us
轉速   100 轉 = 每分鐘   100 轉，每秒  1.67 轉，1轉 = 0.598802   秒 =  598.802 ms =  598802us
轉速   200 轉 = 每分鐘   200 轉，每秒  3.3  轉，1轉 = 0.300003   秒 =  300.003 ms =  300003us
轉速   600 轉 = 每分鐘   600 轉，每秒  10   轉，1轉 = 0.1        秒 =  100.000 ms =  100000us
轉速  1500 轉 = 每分鐘  1500 轉，每秒  25   轉，1轉 = 0.04       秒 =   40.000 ms =   40000us
轉速  6000 轉 = 每分鐘  6000 轉，每秒  60   轉，1轉 = 0.01666... 秒 =   16.667 ms =   16667us
轉速 14000 轉 = 每分鐘 14000 轉，每秒 233.3 轉，1轉 = 0.0042863. 秒 =    4.286 ms =    4286us
轉速 14060 轉 = 每分鐘 14060 轉，每秒 240   轉，1轉 = 0.0041667. 秒 =    4.167 ms =    4167us
轉速 16000 轉 = 每分鐘 16000 轉，每秒 266.6 轉，1轉 = 0.0037500. 秒 =    3.750 ms =    3750us 
*/
void ICACHE_RAM_ATTR countup() {      
  //新版的 Nodemcu 在使用 ISR 中斷，Function 要加上 ICACHE_RAM_ATTR
  //偵測到凸台RISING，就會觸發此 function countup  
  C = micros(); //記錄當下的時間
  // (1/(16000/60) * 1000 * 1000 = 3750
  // (1/(17000/60) * 1000 * 1000 = 3529  
  // 不可能有超過 17000rpm 的狀況
  RPM_DELAY = C - C_old; //現在的時間減去上一次觸發的時間  
  if(RPM_DELAY < 3500) {
    //超過 17000rpm 了
    return;
  }
  if(RPM_DELAY > 598802) {
    //低於 100rpm
    C_old = C;
    rpm = 0;
    return;
  }    
  //其他轉速，計算得出轉速度
  rpm = 60000000UL / RPM_DELAY;
  //把上一次凸台的時間改成現在時間
  C_old = C;  
}
// 轉速範圍對應顏色
uint32_t getRPMColor(int rpm) {
  if (rpm <= 6000) {
    return strip.Color(0, 255, 0); // 綠色
  } else if (rpm <= 8000) {
    return strip.Color(255, 255, 0); // 黃色
  } else if (rpm <= 9500) {
    return strip.Color(255, 165, 0); // 橙色
  } else if (rpm < 10000) {
    return strip.Color(255, 0, 0); // 紅色
  } else {
    static bool blinkState = false;
    blinkState = !blinkState;
    return blinkState ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0); // 紅色閃爍
  }
}
uint32_t blendColors(uint32_t color1, uint32_t color2, int blend) {
  uint8_t r1 = (color1 >> 16) & 0xFF;
  uint8_t g1 = (color1 >> 8) & 0xFF;
  uint8_t b1 = color1 & 0xFF;

  uint8_t r2 = (color2 >> 16) & 0xFF;
  uint8_t g2 = (color2 >> 8) & 0xFF;
  uint8_t b2 = color2 & 0xFF;

  uint8_t r = (r1 * (255 - blend) + r2 * blend) / 255;
  uint8_t g = (g1 * (255 - blend) + g2 * blend) / 255;
  uint8_t b = (b1 * (255 - blend) + b2 * blend) / 255;

  return strip.Color(r, g, b);
}
void setup() {  
  Serial.begin(250000); //注意包率設很高，要在監視器看的話要改一下
  Serial.println("Counting...");
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness(20);
  
  //宣告觸發腳位為 INPUT_PULLUP
  pinMode(ToPin, INPUT_PULLUP);    
  //註冊中斷觸發
  attachInterrupt(digitalPinToInterrupt(ToPin), countup, RISING); //RISING
  //初始化七段顯示器
  tm1637.init();
  //設定亮度
  tm1637.set(BRIGHT_TYPICAL); //BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7; //七段亮度
  //跑 0000~9999 一次
  playFirstTime();
  //將七段改成 0
  displayOnLed(0);  
}

void loop() {
  //七段顯示器不能一直刷數字，不然人類的眼睛會追不上
  //isShowCount 每一次都加1，計數100次才改變一次七段顯示器的內容，顯示完就歸零
  //如果覺得眼睛還是追不上，可以把 100 調大一些，如 150、200
  isShowCount++;  
  if (isShowCount > 100)
  {    
    isShowCount = 0;   
    if(micros() - C_old > 598802) {
      //2021-09-21 針對訊號源消失的處理
      //低於 100rpm      
      rpm = 0;      
    }
    Serial.println(rpm);        
    //七段最多顯示到 9999，所以超過 10000 都變 9999
    //rpm = (rpm>=10000)?9999:rpm;  
    //顯示在七段上
    //預設為 2 行程使用
    //如果為 4 行程引擎，要 x 2 倍
    //rpm *= 2;
    displayOnLed(rpm);   
  } 
  /*else
  { 
    for(int i=1000;i<=12000;i+=1000)
    {
      displayOnLed(i); // 顯示 1500 RPM
      delay(500);    
    }
  }*/
}
void playFirstTime()
{
  // 七段顯示 0000~1111~2222~9999 跑一次
  for (int i = 0; i <= 9; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      tm1637.display(j, i);
    }
    delay(100);
  }
}

void displayOnLed(int show_rpm)
{
  //將轉速，變成顯示值
  //顯示 千百十個  
  String rpm_str = String(show_rpm);
  if (rpm_str.length() <= 3)
  {
    rpm_str = lpad(rpm_str, 4, "X"); // 變成如 "XXX0"，"X600"
  }
  //Serial.print("\nAfter lpad:");
  //Serial.println(rpm_str);
  for (int i = 0; i < 4; i++)
  {
    if (rpm_str[i] == 'X')
    {
      tm1637.display(i, -1); //-1 代表 blank 一片黑
    }
    else
    {
      // Serial.println(rpm_str[i]);
      // 腦包直接轉回 String 再把單字轉 int
      // From : https://www.arduino.cc/en/Tutorial.StringToIntExample
      tm1637.display(i, String(rpm_str[i]).toInt());
    }
  }


  // 將轉速顯示在NeoPixel LED
  int led_count = map(show_rpm, 0, 9999, 0, NUMPIXELS);
  for (int i = 0; i < NUMPIXELS; i++) {
    if (i < led_count) {
      uint32_t color1, color2;
      int range_start, range_end;

      if (show_rpm <= 6000) {
        color1 = strip.Color(0, 255, 0); // 綠色
        color2 = strip.Color(0, 255, 0); // 綠色
        range_start = 0;
        range_end = 6000;
      } else if (show_rpm <= 8000) {
        color1 = strip.Color(0, 255, 0); // 綠色
        color2 = strip.Color(255, 255, 0); // 黃色
        range_start = 6000;
        range_end = 8000;
      } else if (show_rpm <= 9500) {
        color1 = strip.Color(255, 255, 0); // 黃色
        color2 = strip.Color(255, 165, 0); // 橙色
        range_start = 8000;
        range_end = 9500;
      } else {
        color1 = strip.Color(255, 165, 0); // 橙色
        color2 = strip.Color(255, 0, 0); // 紅色
        range_start = 9500;
        range_end = 10000;
      }

      int local_rpm = map(show_rpm, range_start, range_end, 0, 255);
      uint32_t color = blendColors(color1, color2, local_rpm);
      strip.setPixelColor(i, color);
    } else {
      strip.setPixelColor(i, 0); // 關閉LED
    }
  }
  strip.show();
  
}
String lpad(String temp , byte L , String theword) {
  //用來補LED左邊的空白
  //字串左側補自定值 theword
  byte mylen = temp.length();
  if (mylen > (L - 1))return temp.substring(0, L - 1);
  for (byte i = 0; i < (L - mylen); i++)
    temp = theword + temp;
  return temp;
}
