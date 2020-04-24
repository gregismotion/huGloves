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

enum SwitchRole { NEXT_PAGE, DOWN, SELECT, SECONDARY, SET, INCREASE, MAIN, START_STOP };
const int switch0Pin = 2;
const int switch1Pin = 3;
volatile int switch0Flag = LOW;
volatile int switch1Flag = LOW;
int switch0Role = SECONDARY;
int switch1Role = NEXT_PAGE;
const int switchDelay = 500;
unsigned long lastPress = 0;

unsigned int lastMinuteTime = 100;
unsigned int lastDayDate = 100;
unsigned int currentPage = 0;
const unsigned int maxPage = 1;

bool toggleSecondary = false;
bool secondary = false;
bool secondaryDownFlag = false;
bool secondarySelectFlag = false;
unsigned int currentSecondaryOption = 0;
unsigned int currentMaxSecondaryOption = 0;
char secondaryOptions[5][20];

RTC_DS3231 rtc;

void switch0Changed() {
  switch0Flag = digitalRead(switch0Pin);
}
void switch1Changed() {
  switch1Flag = digitalRead(switch1Pin);
}

void formatTime(char* clockBuf) {
  snprintf(clockBuf, 6, "%02d:%02d", getRtcTime(rtc, HOUR), getRtcTime(rtc, MINUTE));
}
void formatDate(char* dateBuf) {
  snprintf(dateBuf, 13, "%04d. %02d. %02d.", getRtcTime(rtc, YEAR), getRtcTime(rtc, MONTH), getRtcTime(rtc, DAY));
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
void refreshTime() {
  unsigned int currentMinute = getRtcTime(rtc, MINUTE);
  if (currentMinute != lastMinuteTime) {
    lastMinuteTime = currentMinute; 
    drawTime();
  }
}
void refreshDate() {
  unsigned int currentDay = getRtcTime(rtc, DAY);
  if (currentDay != lastDayDate) {
    lastDayDate = currentDay; 
    drawDate();
  }
}
void refreshMain() {
  refreshDate();
  refreshTime();
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

void switchPageChange() {
  if (millis() - lastPress >= switchDelay) {
    if (switch0Flag == HIGH) {
      lastPress = millis();
      switch(switch0Role) {
        case SECONDARY: {
          toggleSecondary = true;
          break;  
        }
        case DOWN: {
          secondaryDownFlag = true;
          break;
        }
      }
    }
    if (switch1Flag == HIGH) {
      lastPress = millis();   
      switch(switch1Role) {
        case NEXT_PAGE: {
          incrementPage();
          refreshPage();
          break;
        }
        case SELECT: {
          secondarySelectFlag = true;
          break;
        }
      }
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
void drawSecondaryOption(int index = -1) {
  int offset = 2;
  if (index == -1) {
    for (int i = 0; i < sizeof(secondaryOptions) / sizeof(*secondaryOptions); i++ ) {
      oledWriteString(&ssoled, 0, 0, offset + i, secondaryOptions[i], FONT_NORMAL, currentSecondaryOption == i, 1);
    } 
  } else {
    if (index == 0) {
      oledWriteString(&ssoled, 0, 0, offset + (currentMaxSecondaryOption - 1), secondaryOptions[(currentMaxSecondaryOption - 1)], FONT_NORMAL, 0, 1);
    } else {
      oledWriteString(&ssoled, 0, 0, offset + currentSecondaryOption - 1, secondaryOptions[index - 1], FONT_NORMAL, 0, 1);
    }
    oledWriteString(&ssoled, 0, 0, offset + currentSecondaryOption, secondaryOptions[index], FONT_NORMAL, 1, 1);
  }
}
void drawSecondary() {
  //MAX 6
  oledFill(&ssoled, 0, 1);
  oledWriteString(&ssoled, 0, 0, 0, "Secondary", FONT_NORMAL, 1, 1);
  currentSecondaryOption = 0;
  switch (currentPage) {
    case 0: {
      currentMaxSecondaryOption = 4;
      strcpy(secondaryOptions[0], "Back");
      strcpy(secondaryOptions[1], "Sync time (BT)");
      strcpy(secondaryOptions[2], "Timer");
      strcpy(secondaryOptions[3], "Stopwatch");
      drawSecondaryOption();
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
  DateTime date;
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
      switch0Role = SECONDARY;
      switch1Role = NEXT_PAGE;
    } else {
      drawSecondary();
      secondary = true;
      switch0Role = DOWN;
      switch1Role = SELECT;
    }
  }
  if (secondary) {
    if (secondaryDownFlag) {
      secondaryDownFlag = false;
      if (currentSecondaryOption+1 < currentMaxSecondaryOption) {
        currentSecondaryOption++;
      } else {
        currentSecondaryOption = 0;
      }
      drawSecondaryOption(currentSecondaryOption);
    }
    if (secondarySelectFlag) {
      secondarySelectFlag = false;
    }
  }  
}

void syncTime() {
  oledFill(&ssoled, 0, 1);
  drawTimeSync();
  syncTimeBT();
}

void setupSwitches() {
  pinMode(switch0Pin, INPUT);
  pinMode(switch1Pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(switch0Pin), switch0Changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switch1Pin), switch1Changed, CHANGE);
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
  setupSwitches();
}

void loop() {
  if (currentPage == 0 && !secondary) {
    refreshMain();
  }
  switchPageChange();
  switchSecondary();
}
