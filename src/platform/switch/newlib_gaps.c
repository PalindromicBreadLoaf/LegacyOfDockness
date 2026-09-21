#include "newlib_gaps.h"

time_t timegm(struct tm *tm) {
    int year = tm->tm_year + 1900;
    const int month = tm->tm_mon + 1;
    year -= month <= 2;

    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = (unsigned)(year - era * 400);
    const unsigned dayOfYear = (153u * (unsigned)(month + (month > 2 ? -3 : 9)) + 2) / 5 + (unsigned)tm->tm_mday - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const long long days = (long long)era * 146097 + (long long)dayOfEra - 719468;

    return (time_t)(days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}
