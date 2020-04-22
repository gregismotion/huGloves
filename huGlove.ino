#include <SPI.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <ss_oled.h>

SSOLED ssoled;
#define SDA_PIN -1
#define SCL_PIN -1
// no reset pin needed
#define RESET_PIN -1
// let ss_oled find the address of our display
#define OLED_ADDR -1
#define FLIP180 0
#define INVERT 0
// Use the default Wire library
#define USE_HW_I2C 1

#define TxD 11
#define RxD 12
SoftwareSerial btSerial(RxD, TxD); 
int btTimeout = 5000;

int switch0Pin = 2;
int switch1Pin = 3;
volatile int switch0Flag = LOW;
volatile int switch1Flag = LOW;
int switchDelay = 500;
unsigned long lastPress = 0;

int currentPage = 0;

unsigned int year = 2020;
unsigned int month = 12;
unsigned int day = 31;
unsigned int hour = 23;
unsigned int minute = 59;
unsigned int second = 59;

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void switch0Changed() {
  switch0Flag = digitalRead(switch0Pin);
}
void switch1Changed() {
  switch1Flag = digitalRead(switch1Pin);
}
void switchPageChange() {
  if (millis() - lastPress >= switchDelay) {
    if (switch0Flag == HIGH) {
      currentPage--;
      lastPress = millis();
      refreshPage();
    }
    if (switch1Flag == HIGH) {
      currentPage++;
      lastPress = millis();   
      refreshPage();
    }
  }
}

void drawTimeSync() {
  oledWriteString(&ssoled, 0, 0, 0, "Syncing over BT...", FONT_SMALL, 0, 1);
}
void drawTime(int hour, int minute) {
  char clockBuf[5];
  sprintf(clockBuf, "%02d:%02d", hour, minute);
  oledWriteString(&ssoled, 0, 25, 3, clockBuf, FONT_STRETCHED, 0, 1);
}
void drawDate(int year, int month, int day) {
  char dateBuf[13];
  sprintf(dateBuf, "%04d. %02d. %02d.", year, month, day);
  oledWriteString(&ssoled, 0, 0, 0, dateBuf, FONT_SMALL, 0, 1);
}

enum DateType { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND };
int getRtcTime(int key) {
  switch(key) {
    case YEAR: {
      return year;  
    }  
    case MONTH: {
      return month;  
    }  
    case DAY: {
      return day;  
    }  
    case HOUR: {
      return hour;  
    }  
    case MINUTE: {
      return minute;  
    }  
    case SECOND: {
      return second;  
    }  
  } 
}
void setRtcTime(int key, unsigned int value) {
  switch(key) {
    case YEAR: {
      year = value;
      break;
    }  
    case MONTH: {
      month = value;
      break;
    }  
    case DAY: {
      day = value;
      break;
    }  
    case HOUR: {
      hour = value;
      break;
    }  
    case MINUTE: {
      minute = value;
      break;
    }  
    case SECOND: {
      second = value;
      break;
    }  
  } 
}

void refreshPage() {
  oledFill(&ssoled, 0, 1);
  switch (currentPage) {
    case -1: {
      //drawTime(hour, minute);
      break;
    }
    case 0: {
      drawDate(getRtcTime(YEAR), getRtcTime(MONTH), getRtcTime(DAY));
      drawTime(getRtcTime(HOUR), getRtcTime(MINUTE));
      break;
    }
    case 1: {
      //drawWeather();
      break;  
    }
  }
}

char btSafeRead() {
  noInterrupts();
  char ch = btSerial.read();
  interrupts();
  return ch;
}
String btSafeReadString() {
  //noInterrupts();
  String str = btSerial.readString();
  interrupts();
  return str;
}
void btSafePrintLn(String str) {
  noInterrupts();
  btSerial.println(str);
  interrupts();
}
void waitForBt() {
  unsigned long waitingStart = millis();
  while(btSerial.available() == 0 && millis() - waitingStart < btTimeout) { }
}

void syncTimeBT() {
  btSafePrintLn("TIME");
  waitForBt();
  String input = btSafeReadString();
  char delimiter = ';';
  setRtcTime(YEAR, getValue(input, delimiter, 0).toInt());
  setRtcTime(MONTH, getValue(input, delimiter, 1).toInt());
  setRtcTime(DAY, getValue(input, delimiter, 2).toInt());
  setRtcTime(HOUR, getValue(input, delimiter, 3).toInt());
  setRtcTime(MINUTE, getValue(input, delimiter, 4).toInt());
  setRtcTime(SECOND, getValue(input, delimiter, 5).toInt());
  btSafePrintLn("DONE");
}

void initScreen() {
  int rc;
  rc = oledInit(&ssoled, OLED_128x64, OLED_ADDR, FLIP180, INVERT, USE_HW_I2C, SDA_PIN, SCL_PIN, RESET_PIN, 400000L);       // Standard HW I2C bus at 400Khz

  if (rc != OLED_NOT_FOUND)
  {
      char *msgs[] =
      {
        (char *)"SSD1306 @ 0x3C",
        (char *)"SSD1306 @ 0x3D",
        (char *)"SH1106 @ 0x3C",
        (char *)"SH1106 @ 0x3D"
      };

      oledFill(&ssoled, 0, 1);
      oledWriteString(&ssoled, 0, 0, 0, (char *)"OLED found:", FONT_NORMAL, 0, 1);
      oledWriteString(&ssoled, 0, 10, 2, msgs[rc], FONT_NORMAL, 0, 1);
  }  
}

void setup() {
  initScreen();
  
  btSerial.begin(9600); 
  
  oledFill(&ssoled, 0, 1);
  drawTimeSync();
  syncTimeBT();
  
  refreshPage();
  
  pinMode(switch0Pin, INPUT);
  pinMode(switch1Pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(switch0Pin), switch0Changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switch1Pin), switch1Changed, CHANGE);
}

void loop() { 
  switchPageChange();
}
