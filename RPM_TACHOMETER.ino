/*
 * 七段顯示器 轉速表 RPM TACHOMETER
 * Author: 羽山 (https://3wa.tw)
 * Author: @FB 田峻墉
 * Release Date: 2021-09-06
 * D7 TM1637 CLK
 * D6 TM1637 DIO
 * D1 接至 PC817，為轉速訊號接入端
 */
#include <Arduino.h>
#include <TM1637.h> //七段數位模組
const int ToPin = D1;  //凸台、或轉速訊號線
#define CLK D7
#define DIO D6
TM1637 tm1637(CLK, DIO);
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
void ICACHE_RAM_ATTR countup() {  //For newest version
  //收到CDI點火，扣掉偵測到凸台RISING時間
  //只要是Rising就是Fire
  C = micros();
  // (1/(17000/60)0 *1000 * 1000 = 3529
  // 不可能有超過 17000rpm 的狀況
  RPM_DELAY = C - C_old; //現在的時間減去上一次觸發的時間  
  if(RPM_DELAY < 3500) {
    //超過 16000rpm 了
    return;
  }
  if(RPM_DELAY > 598802) {
    //低於 100rpm
    C_old = C;
    rpm = 0;
    return;
  }    
  rpm = 60000000UL / RPM_DELAY;
  C_old = C;  
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(250000);
  Serial.println("Counting...");
  pinMode(ToPin, INPUT_PULLUP);    
  //中斷觸發
  attachInterrupt(digitalPinToInterrupt(ToPin), countup, RISING); //RISING
  tm1637.init();
  tm1637.set(BRIGHT_TYPICAL); //BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7; //七段亮度
  playFirstTime();
  diaplayOnLed(0);
}

void loop() {

  // put your main code here, to run repeatedly:
  isShowCount++;  
  if (isShowCount > 100)
  {
    //display_rpm();
    isShowCount = 0;    
    Serial.println(rpm);        
    rpm = (rpm>=10000)?9999:rpm;  
    diaplayOnLed(rpm);   
  }  
}
void playFirstTime()
{
  // 0000~9999 跑二次
  for (int i = 0; i <= 9; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      tm1637.display(j, i);
    }
    delay(100);
  }
}

void diaplayOnLed(int show_rpm)
{
  //將轉速，變成顯示值
  //只顯示 千百十個
  //如果要顯示 千百十個，就不用除了
  //太多數位有點眼花
  //String rpm_str = String(show_rpm/10);
  String rpm_str = String(show_rpm);
  if (rpm_str.length() <= 3)
  {
    rpm_str = lpad(rpm_str, 4, "X"); // 變成如 "XXX0"
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
}
String lpad(String temp , byte L , String theword) {
  //用來補LED左邊的空白
  byte mylen = temp.length();
  if (mylen > (L - 1))return temp.substring(0, L - 1);
  for (byte i = 0; i < (L - mylen); i++)
    temp = theword + temp;
  return temp;
}
