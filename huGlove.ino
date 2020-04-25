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

const int switchDelay = 500;
enum SwitchRole { NEXT_PAGE, DOWN, SELECT, SECONDARY, SET, INCREASE, MAIN, START_STOP };
struct SwitchState {
  const int pin0 = 2;
  const int pin1 = 3;
  volatile int flag0 = LOW;
  volatile int flag1 = LOW;
  int role0 = SECONDARY;
  int role1 = NEXT_PAGE;
  unsigned long lastPress = 0;
};

struct LastState {
  unsigned int minuteTime = 100;
  unsigned int dayDate = 100;
};

struct PageState {
  int current = 0;
  unsigned int max = 1;
};

struct SecondaryState {
  bool toggle = false;
  bool isOn = false;
  bool downFlag = false;
  bool selectFlag = false;
  unsigned int currentOption = 0;
  unsigned int lastOption = 0;
  unsigned int currentMaxOption = 0;
  char options[5][20];
  int optionsRole[5];
};

SecondaryState secondary;
PageState page;
LastState last;
volatile SwitchState switchS;

RTC_DS3231 rtc;

void switch0Changed() {
  switchS.flag0 = digitalRead(switchS.pin0);
}
void switch1Changed() {
  switchS.flag1 = digitalRead(switchS.pin1);
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
  if (currentMinute != last.minuteTime) {
    last.minuteTime = currentMinute; 
    drawTime();
  }
}
void refreshDate() {
  unsigned int currentDay = getRtcTime(rtc, DAY);
  if (currentDay != last.dayDate) {
    last.dayDate = currentDay; 
    drawDate();
  }
}
void refreshMain() {
  refreshDate();
  refreshTime();
}
void refreshPage() {
  oledFill(&ssoled, 0, 1);
  switchS.role0 = SECONDARY;
  switchS.role1 = NEXT_PAGE;
  switch (page.current) {
    case -1: {
      switchS.role0 = SET;
      switchS.role1 = INCREASE;
      drawTime();
      break;
    }
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

void incrementPage() {
    if (page.current >= page.max) {
      page.current = 0;
    } else {
      page.current++;
    }
}
void switchPageChange() {
  if (millis() - switchS.lastPress >= switchDelay) {
    if (switchS.flag0 == HIGH) {
      switchS.lastPress = millis();
      switch(switchS.role0) {
        case SECONDARY: {
          secondary.toggle = true;
          break;  
        }
        case DOWN: {
          secondary.downFlag = true;
          break;
        }
      }
    }
    if (switchS.flag1 == HIGH) {
      switchS.lastPress = millis();   
      switch(switchS.role1) {
        case NEXT_PAGE: {
          incrementPage();
          refreshPage();
          break;
        }
        case SELECT: {
          secondary.selectFlag = true;
          break;
        }
        case INCREASE: {
          break;  
        }
      }
    }
  }
}

void drawTimeSync() {
  oledWriteString(&ssoled, 0, 0, 0, "Syncing over BT...", FONT_SMALL, 0, 1);
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

enum secondaryOptionRole{ BACK, SYNC_TIME_BT, TIMER, STOPWATCH };
void drawSecondaryOption(int index = -1) {
  int offset = 2;
  if (index == -1) {
    for (int i = 0; i < secondary.currentMaxOption; i++ ) {
      oledWriteString(&ssoled, 0, 0, offset + i, secondary.options[i], FONT_NORMAL, secondary.currentOption == i, 1);
    } 
  } else {
    oledWriteString(&ssoled, 0, 0, offset + secondary.lastOption, secondary.options[secondary.lastOption], FONT_NORMAL, 0, 1);
    oledWriteString(&ssoled, 0, 0, offset + secondary.currentOption, secondary.options[index], FONT_NORMAL, 1, 1);
  }
}
void drawSecondary() {
  switchS.role0 = DOWN;
  switchS.role1 = SELECT;
  oledFill(&ssoled, 0, 1);
  oledWriteString(&ssoled, 0, 0, 0, "Secondary", FONT_NORMAL, 1, 1);
  for (int i = 0; i < secondary.currentMaxOption; i++ ) {
    strcpy(secondary.options[i], "");
  } 
  secondary.currentOption = 0;
  secondary.currentMaxOption = 1;
  strcpy(secondary.options[0], "Back");
  secondary.optionsRole[0] = BACK;
  switch (page.current) {
    case 0: {
      strcpy(secondary.options[1], "Sync time (BT)");
      secondary.optionsRole[1] = SYNC_TIME_BT;
      
      strcpy(secondary.options[2], "Timer");
      secondary.optionsRole[2] = TIMER;
      
      strcpy(secondary.options[3], "Stopwatch");
      secondary.optionsRole[3] = STOPWATCH;
      
      secondary.currentMaxOption += 3;
      drawSecondaryOption();
    }    
  }
}
void handleSecondary() {
  if (secondary.downFlag) {
  secondary.downFlag = false;
  secondary.lastOption = secondary.currentOption;
  if (secondary.currentOption+1 < secondary.currentMaxOption) {
    secondary.currentOption++;
  } else {
    secondary.currentOption = 0;
  }
  drawSecondaryOption(secondary.currentOption);
  }
  if (secondary.selectFlag) {
    secondary.selectFlag = false;
    switch(secondary.optionsRole[secondary.currentOption]) {
      case BACK: {
        secondary.toggle = true;
        break;
      }
      case SYNC_TIME_BT: {
        secondary.toggle = true;
        syncTime();
        break;
      }
      case TIMER: {
        secondary.toggle = true;
        page.current = -1;
        break;
      }
      case STOPWATCH: {
        secondary.toggle = true;
        page.current = -2;
        break;
      }
    }
  }
}
void switchSecondary() {
  if (secondary.toggle) {
    secondary.toggle = false;
    if (secondary.isOn) {
      refreshPage();
      secondary.isOn = false;
    } else {
      drawSecondary();
      secondary.isOn = true;
    }
  }  
}

void syncTime() {
  oledFill(&ssoled, 0, 1);
  drawTimeSync();
  syncTimeBT();
}
void setupSwitches() {
  pinMode(switchS.pin0, INPUT);
  pinMode(switchS.pin1, INPUT);
  attachInterrupt(digitalPinToInterrupt(switchS.pin0), switch0Changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switchS.pin1), switch1Changed, CHANGE);
}
bool initScreen() {
  if (oledInit(&ssoled, OLED_128x64, OLED_ADDR, FLIP180, INVERT, USE_HW_I2C, SDA_PIN, SCL_PIN, RESET_PIN, 400000L) == OLED_NOT_FOUND) {
    return false;
  }
  oledFill(&ssoled, 0, 1);
  return true;
}

void setup() {
  btSerial.begin(9600); 
  btSafePrintLn("START");
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
  if (page.current == 0 && !secondary.isOn) {
    refreshMain();
  }
  switchPageChange();
  switchSecondary();
  if (secondary.isOn) {
    handleSecondary();
  }
}
