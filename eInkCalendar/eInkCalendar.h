#ifndef EINKCALENDAR_H
#define EINKCALENDAR_H

#include <time.h>

#define PIN_A0 36 
#define PIN_A3 39
#define PIN_A4 32
#define PIN_A5 33
#define PIN_A6 34
#define PIN_A7 35

#define PIN_BUTTON_MODE PIN_A0
#define PIN_BUTTON_MENO PIN_A3
#define PIN_BUTTON_PIU  PIN_A4

#define LONG_PRESS_MS   2000
#define N_MAX_TENTATIVI   10

// Tempo di sleep in microsecondi (10 minuti = 600 secondi)
#define SLEEP_DURATION_US 600000000ULL  // 10 minuti        10000000ULL //10 secondi

#define MAX_EVENTS  100
#define MAX_STRING   53    //32

enum ScreenMode { GIORNO, _5DAYS, SETTIMANA, MESE };

struct CalendarEvent {
  char     title[MAX_STRING];
  time_t   start_time_utc;
  int      start_month; // 0 - 11
  int      start_day;
  int      start_hour;
  int      start_min;
  uint16_t durationMinutes; // durata in minuti
  uint8_t  calendar_id;
};


#endif
