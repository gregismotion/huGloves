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
int currentPage = 0;
const unsigned int maxPage = 1;

struct Secondary {
  bool toggle = false;
  bool isOn = false;
  bool downFlag = false;
  bool selectFlag = false;
  unsigned int currentOption = 0;
  unsigned int currentMaxOption = 0;
  char options[5][20];
  int optionsRole[5];
};
Secondary secondary;

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
    case -2: {
      drawTime();
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

void switchPageChange() {
  if (millis() - lastPress >= switchDelay) {
    if (switch0Flag == HIGH) {
      lastPress = millis();
      switch(switch0Role) {
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
    if (switch1Flag == HIGH) {
      lastPress = millis();   
      switch(switch1Role) {
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
enum secondaryOptionRole{ BACK, SYNC_TIME_BT, TIMER, STOPWATCH };
void drawSecondaryOption(int index = -1) {
  int offset = 2;
  if (index == -1) {
    //TODO: CHANGE LEGNTH GETTING THING FUCK
    for (int i = 0; i < sizeof(secondary.options) / sizeof(*secondary.options); i++ ) {
      oledWriteString(&ssoled, 0, 0, offset + i, secondary.options[i], FONT_NORMAL, secondary.currentOption == i, 1);
    } 
  } else {
    if (index == 0) {
      oledWriteString(&ssoled, 0, 0, offset + (secondary.currentMaxOption - 1), secondary.options[(secondary.currentOption - 1)], FONT_NORMAL, 0, 1);
    } else {
      oledWriteString(&ssoled, 0, 0, offset + secondary.currentOption - 1, secondary.options[index - 1], FONT_NORMAL, 0, 1);
    }
    oledWriteString(&ssoled, 0, 0, offset + secondary.currentOption, secondary.options[index], FONT_NORMAL, 1, 1);
  }
}
void drawSecondary() {
  oledFill(&ssoled, 0, 1);
  oledWriteString(&ssoled, 0, 0, 0, "Secondary", FONT_NORMAL, 1, 1);
  for (int i = 0; i < sizeof(secondary.options) / sizeof(*secondary.options); i++ ) {
    strcpy(secondary.options[i], "");
  } 
  secondary.currentOption = 0;
  secondary.currentMaxOption = 1;
  strcpy(secondary.options[0], "Back");
  secondary.optionsRole[0] = BACK;
  switch (currentPage) {
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
  if (secondary.toggle) {
    secondary.toggle = false;
    if (secondary.isOn) {
      refreshPage();
      secondary.isOn = false;
      switch0Role = SECONDARY;
      switch1Role = NEXT_PAGE;
    } else {
      drawSecondary();
      secondary.isOn = true;
      switch0Role = DOWN;
      switch1Role = SELECT;
    }
  }
  if (secondary.isOn) {
    if (secondary.downFlag) {
      secondary.downFlag = false;
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
          switch0Role = SET;
          switch1Role = INCREASE;
          secondary.toggle = true;
          currentPage = -2;
          break;
        }
        case STOPWATCH: {
          secondary.toggle = true;
          currentPage = -3;
          break;
        }
      }
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
  if (currentPage == 0 && !secondary.isOn) {
    refreshMain();
  }
  switchPageChange();
  switchSecondary();
}
