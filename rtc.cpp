#include "rtc.h"
#include "RTClib.h"

const char btDelimiter[1] = ";";

void tokenizeDate(DateTime* date, char* in)
{ 
  char* token = strtok(in, btDelimiter);
  int count = 0;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  while(token) {
    switch(count) {
      case 0: year = atoi(token); break;
      case 1: month = atoi(token); break;
      case 2: day = atoi(token); break;
      case 3: hour = atoi(token); break;
      case 4: minute = atoi(token); break;
      case 5: second = atoi(token); break;
    }
    token = strtok(NULL, btDelimiter);
    count++;
  }
  *date = DateTime(year, month, day, hour, minute, second);
}

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
void setRtcTime(RTC_DS3231 rtc, DateTime date) {
  rtc.adjust(date);
}
