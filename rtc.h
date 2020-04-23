#include "RTClib.h"

enum DateType { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND };
void tokenizeDate(DateTime* date, char* in);
int getRtcTime(RTC_DS3231 rtc, int key);
void setRtcTime(RTC_DS3231 rtc, DateTime); 
