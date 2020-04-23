#include <SoftwareSerial.h>
#include <ss_oled.h>
#include "rtc.h"
#include "RTClib.h"

SSOLED ssoled;
#define SDA_PIN -1
#define SCL_PIN -1
#define RESET_PIN -1
#define OLED_ADDR -1
#define FLIP180 0
#define INVERT 0
#define USE_HW_I2C 1

#define TxD 11
#define RxD 12
SoftwareSerial btSerial(RxD, TxD); 
const int btTimeout = 60000 * 3;
const char btDelimiter[1] = ";";

const int switch0Pin = 2;
const int switch1Pin = 3;
volatile int switch0Flag = LOW;
volatile int switch1Flag = LOW;
const int switchDelay = 500;
unsigned long lastPress = 0;

unsigned long lastTimeRefresh;
const int timeRefreshMs = 3000;
int currentPage = 0;
const int maxPage = 1;
bool toggleSecondary = false;
bool secondary = false;

RTC_DS3231 rtc;

void tokenizeDate(struct Date* date, char* in)
{ 
  char* token = strtok(in, btDelimiter);
  int count = 0;
  while(token) {
    switch(count) {
      case 0: date->year = atoi(token); break;
      case 1: date->month = atoi(token); break;
      case 2: date->day = atoi(token); break;
      case 3: date->hour = atoi(token); break;
      case 4: date->minute = atoi(token); break;
      case 5: date->second = atoi(token); break;
    }
    token = strtok(NULL, btDelimiter);
    count++;
  }
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
      lastPress = millis();
    }
    if (switch1Flag == HIGH) {
      incrementPage();
      lastPress = millis();   
      refreshPage();
    }
  }
}

void incrementPage() {
    if (currentPage >= maxPage) {
      currentPage = 0;
    } else {
      currentPage++;
    }
}

void drawTimeSync() {
  oledWriteString(&ssoled, 0, 0, 0, "Syncing over BT...", FONT_SMALL, 0, 1);
}
void drawDate() {
  char dateBuf[13];
  formatDate(dateBuf);
  oledWriteString(&ssoled, 0, 0, 0, dateBuf, FONT_SMALL, 0, 1);
}
void drawTime() {
  char clockBuf[5];
  formatTime(clockBuf);
  oledWriteString(&ssoled, 0, 25, 3, clockBuf, FONT_STRETCHED, 0, 1);  
}
void drawSecondary() {
  oledFill(&ssoled, 0, 1);
  switch (currentPage) {
    case 0: {
      oledWriteString(&ssoled, 0, 0, 1, "Sync time (BT)", FONT_NORMAL, 1, 1);
      oledWriteString(&ssoled, 0, 0, 2, "Timer", FONT_NORMAL, 0, 1);
      oledWriteString(&ssoled, 0, 0, 2, "Stopwatch", FONT_NORMAL, 0, 1);
    }    
  }
}
void formatTime(char* clockBuf) {
  snprintf(clockBuf, 6, "%02d:%02d", getRtcTime(rtc, HOUR), getRtcTime(rtc, MINUTE));
}
void formatDate(char* dateBuf) {
  snprintf(dateBuf, 13, "%04d. %02d. %02d.", getRtcTime(rtc, YEAR), getRtcTime(rtc, MONTH), getRtcTime(rtc, DAY));
}

void refreshTime() {
  if (millis() - lastTimeRefresh >= timeRefreshMs) {
    lastTimeRefresh = millis();
    drawTime();  
  }
}
void refreshPage() {
  oledFill(&ssoled, 0, 1);
  switch (currentPage) {
    case 0: {
      drawDate();
      drawTime();
      break;
    }
    case 1: {
      drawDate();
      break;  
    }
  }
}

void btSafeReadLine(char* outStr, int len) {
  btSerial.readString().toCharArray(outStr, len);
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
  char input[20];
  btSafeReadLine(input, 20);
  struct Date date;
  tokenizeDate(&date, input);
  setRtcTime(rtc, date);
}

bool initScreen() {
  if (oledInit(&ssoled, OLED_128x64, OLED_ADDR, FLIP180, INVERT, USE_HW_I2C, SDA_PIN, SCL_PIN, RESET_PIN, 400000L) == OLED_NOT_FOUND) {
    return false;
  }
  oledFill(&ssoled, 0, 1);
  return true;
}

void switchSecondary() {
  if (toggleSecondary) {
    toggleSecondary = false;
    if (secondary) {
      refreshPage();
      secondary = false;
    } else {
      drawSecondary();
      secondary = true;
    }
  }  
}

void syncTime() {
  oledFill(&ssoled, 0, 1);
  drawTimeSync();
  syncTimeBT();
}

void setup() {
  btSerial.begin(9600); 
  if (!initScreen()) {
    btSafePrintLn("ERR");
    for (;;) {}
  }
  
  rtc.begin();
  if (rtc.lostPower()) {
      syncTime();
  }
  refreshPage();
  
  pinMode(switch0Pin, INPUT);
  pinMode(switch1Pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(switch0Pin), switch0Changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switch1Pin), switch1Changed, CHANGE);
  // NEEDS NEW HARDWARE, NANO CANT HANDLE EM
  //pinMode(switch2Pin, INPUT);
  //pinMode(switch3Pin, INPUT);
  //attachInterrupt(digitalPinToInterrupt(switch2Pin), switch2Changed, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(switch3Pin), switch3Changed, CHANGE);
}

void loop() {
  if (currentPage == 0) {
    refreshTime();  
  }
  switchPageChange();
  switchSecondary();
}
