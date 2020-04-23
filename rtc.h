#include "RTClib.h"

enum DateType { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND };
struct Date {
  unsigned int year = 2020;
  unsigned int month = 4;
  unsigned int day = 20;
  unsigned int hour = 4;
  unsigned int minute = 20;
  unsigned int second = 21;
};
int getRtcTime(RTC_DS3231 rtc, int key);
void setRtcTime(RTC_DS3231 rtc, struct Date); 
