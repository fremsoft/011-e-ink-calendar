#ifndef _GRAFICA_H_
#define _GRAFICA_H_

#include <SPI.h>
//EPD
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"

#include <Adafruit_GFX.h>

#define PIXEL_WHITE 1
#define PIXEL_BLACK 2 
#define PIXEL_RED   3

void setupDisplay();
void freeMemGrafica();

void drawCalendarDay(time_t offset_days, CalendarEvent allEvents[], int nEvents);
void drawCalendar5Days(time_t offset_days, CalendarEvent allEvents[], int nEvents);
void drawCalendarWeek(time_t offset_days, CalendarEvent allEvents[], int nEvents);
void drawCalendarMonth(time_t offset_days, CalendarEvent allEvents[], int nEvents);

void drawError(const char * error);

#endif
