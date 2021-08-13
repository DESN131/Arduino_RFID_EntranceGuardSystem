#include <Wire.h>
#include <Adafruit_GFX.h> // 引入驱动OLED0.96所需的库
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
 
#define SCREEN_WIDTH 128 // 设置OLED宽度,单位:像素
#define SCREEN_HEIGHT 64 // 设置OLED高度,单位:像素
#define OLED_RESET 4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define pinBuzzer  7  //设置蜂鸣器的io引脚对应7号引脚
#define SS_PIN 10   //设置rc522模块对应引脚
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);  //实例化类
Servo myservo;  //实例化舵机

byte nuidPICC[4];//   初始化数组用于存储读取到的NUID
bool Mode = 0;
bool sat2 = 1;
char serialData;
const int opentime = 1000;  //设置舵机打开的时间
unsigned long disdelay = 5000;  //设置息屏延时，单位为毫秒
unsigned long distime;

void setup() {
  Serial.begin(9600);
  SPI.begin(); // 初始化SPI总线
  rfid.PCD_Init(); // 初始化 MFRC522
  myservo.attach(6); //初始化舵机，设定为6号引脚
  myservo.write(90); //设置舵机初始角度
  Wire.begin();    // 初始化Wire库
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // 初始化OLED并设置其IIC地址为 0x3C
  pinMode(pinBuzzer, OUTPUT); //设置pinBuzzer脚为输出状态
  digitalWrite(pinBuzzer, HIGH);  //使蜂鸣器静音
  pinMode(2,INPUT_PULLUP);  //设置2号引脚为输入上拉模式，使设备默认模式为Mode 0门禁模式
}

void loop() {
  if (millis() - distime >= disdelay) {  //显示屏自动息屏功能，在无操作disdelay时间后显示屏自动息屏
    display.clearDisplay();
    display.display();
  }
  
  if (digitalRead(2) == HIGH) {  //从引脚2的电位状态判断运行模式， 高电位Mode 0为门禁模式， 低电位Mode 1为读卡模式
    Mode = 0;
    }
  else {
    Mode = 1;
  }
  
  if (sat2 != Mode) {  //当模式切换时在串口和屏幕上提示切换模式并标出目前的模式
    Mode_change();
  }
  sat2 = Mode; //令状态变量与目前的模式变量相等，从而在下次能够判断是否更改了模式
  
  if( Serial.available()>0 && Mode == 0){ //在门禁模式下用串口控制门禁开关，串口输入'1'即可开门，可用蓝牙遥控
    serialData = Serial.read();
    if (serialData == '1' ) {  
      bt_door_open();
      while(Serial.read()>=0);    //在单次开门后清空串口缓存中的数据，从而防止缓存数据堆积导致重复开门
      return;
    }
  } else if (Mode == 1) {  //在读卡模式下及时清除串口缓存中的数据，防止影响门禁模式正常运行
      while(Serial.read()>=0);
  }
    
  if ( ! rfid.PICC_IsNewCardPresent())  // 找卡
    return;
  if ( ! rfid.PICC_ReadCardSerial())  // 验证NUID是否可读
    return;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

 /* if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  // 检查是否MIFARE卡类型
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println("不支持读取此卡类型");
    return;
  }  */
  Serial.print("十六进制UID：");  
  for (byte i = 0; i < 4; i++) {  // 将NUID保存到nuidPICC数组
    nuidPICC[i] = rfid.uid.uidByte[i];
    Serial.print(rfid.uid.uidByte[i], HEX);  //在串口中输出识别出卡的UID
    Serial.print(" ");
  }
  
  if (Mode == 1) {                      //若为读卡模式
    Read_mode();                        //在oled屏幕上显示目前卡的UID
  } else {                              //若为门禁模式
    rfid.PICC_HaltA();                  // 使放置在读卡区的IC卡进入休眠状态，不再重复读卡
    rfid.PCD_StopCrypto1();             // 停止读卡模块编码
  
    if (nuidPICC[0] == 0x8C && nuidPICC[1] == 0x7A && nuidPICC[2] == 0x26 && nuidPICC[3] == 0x49     //此处存放录入卡的UID
      ||nuidPICC[0] == 0x78 && nuidPICC[1] == 0x3C && nuidPICC[2] == 0x1C && nuidPICC[3] == 0x3E) {  //判断识别出的卡是否录入
      door_open();    //如果录入，执行开门函数
    }
    else {
      door_close();   //否则执行关闭函数
    }
  }
}

void Read_mode()  {    //读卡模式
  Serial.println();
  display.clearDisplay();  // 清除屏幕
  display.setTextColor(WHITE);  // 设置字体颜色,白色可见
  display.setTextSize(2);  //设置字体大小
  display.setCursor(0, 0);
  display.print("Read Mode");
  display.setCursor(0, 20);  //设置光标位置
  display.print("Card UID: ");
  display.setCursor(0, 40);
  display.print(nuidPICC[0], HEX);
  display.setCursor(30, 40);
  display.print(nuidPICC[1], HEX);
  display.setCursor(60, 40);
  display.print(nuidPICC[2], HEX);
  display.setCursor(90, 40);
  display.print(nuidPICC[3], HEX);
  display.display();
  distime = millis();

  digitalWrite(pinBuzzer, LOW);
  delay(200);
  digitalWrite(pinBuzzer, HIGH);
  delay(500);
}

void Mode_change() {    //当模式变换时调用此函数
  Serial.print("Mode:");
  Serial.println(Mode);
  display.clearDisplay();
  display.setTextColor(WHITE);  // 设置字体颜色,白色可见
  display.setTextSize(1.5);  //设置字体大小
  display.setCursor(0, 0);
  display.print("Mode Changed");
  display.setCursor(0, 20);
  display.print("Mode:");
  display.setCursor(0, 40);
  display.print(Mode);
  display.display();
  distime = millis();
}

void door_open() {    //开门函数
  Serial.println("身份确认，解锁成功");
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Lock Mode");
  display.setCursor(0, 20);
  display.print("Right User");
  display.setCursor(0, 40);
  display.print("Welcome!");
  display.display();
  distime = millis();

  myservo.write(180);
  digitalWrite(pinBuzzer, LOW);
  delay(50);
  digitalWrite(pinBuzzer, HIGH);
  delay(50);
  digitalWrite(pinBuzzer, LOW);
  delay(50);
  digitalWrite(pinBuzzer, HIGH);
  delay(50);
  delay(opentime);
  myservo.write(90);
}

void door_close() {    //关门函数
  Serial.println("非认证用户，无法解锁");
  display.clearDisplay();
  display.setTextColor(WHITE);  // 设置字体颜色,白色可见
  display.setTextSize(2);  //设置字体大小
  display.setCursor(0, 0);
  display.print("Lock Mode");
  display.setCursor(0, 20);
  display.print("Wrong User");
  display.setCursor(0, 40);
  display.print("Try Again.");
  display.display();
  distime = millis();
  
  digitalWrite(pinBuzzer, LOW);
  delay(200);
  digitalWrite(pinBuzzer, HIGH);
}

void bt_door_open() {    //蓝牙控制开门函数
  Serial.println("蓝牙身份确认，解锁成功");
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Blueooth");
  display.setCursor(0, 20);
  display.print("Control.");
  display.setCursor(0, 40);
  display.print("Welcome!");
  display.display();
  distime = millis();

  myservo.write(180);
  digitalWrite(pinBuzzer, LOW);
  delay(100);
  digitalWrite(pinBuzzer, HIGH);
  delay(opentime);
  myservo.write(90);
}
