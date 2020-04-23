#include "rtc.h"
#include "RTClib.h"

unsigned int year = 2020;
unsigned int month = 12;
unsigned int day = 31;
unsigned int hour = 23;
unsigned int minute = 59;
unsigned int second = 59;

int getRtcTime(RTC_DS3231 rtc, int key) {
  DateTime now = rtc.now();
  switch(key) {
    case YEAR: {
      return now.year();  
    }  
    case MONTH: {
      return now.month();  
    }  
    case DAY: {
      return now.day();  
    }  
    case HOUR: {
      return now.hour();  
    }  
    case MINUTE: {
      return now.minute();  
    }  
    case SECOND: {
      return now.second();  
    }  
  } 
}
void setRtcTime(RTC_DS3231 rtc, struct Date date) {
  rtc.adjust(DateTime(date.year, date.month, date.day, date.hour, date.minute, date.second));
}
