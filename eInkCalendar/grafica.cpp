#include "eInkcalendar.h"
#include "grafica.h"

// Libreria Adafruit GFX
#include "Fonts\FreeSans7pt7b.h"
#include "Fonts\FreeSans9pt7b.h"
#include "Fonts\FreeSans18pt7b.h" 
#include "Fonts\FreeSansOblique12pt7b.h"
#include "Fonts\FreeSansBold7pt7b.h"
#include "Fonts\FreeSansBold9pt7b.h"
#include "Fonts\FreeSansBold12pt7b.h"
#include "Fonts\FreeSansBold18pt7b.h"
#include "Fonts\FreeSansBold24pt7b.h"

// Buffer in RAM per bianco/nero e rosso 
unsigned char *datasBW = nullptr;
unsigned char *datasRW = nullptr;

const char *giorniSett[8] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"}; 
const char *mesiShort[12] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"}; 
const char *mesiLong[12] = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giungo", "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"}; 



#ifndef _swap_int16_t
#define _swap_int16_t(a, b)  { int16_t t = a; a = b; b = t; }
#endif

void setupDisplay() {
	
	pinMode(A14, INPUT);  //BUSY
  pinMode(A15, OUTPUT); //RES 
  pinMode(A16, OUTPUT); //DC   
  pinMode(A17, OUTPUT); //CS 

  // Calcola la dimensione del buffer
  size_t bufferSize = EPD_ARRAY * sizeof(unsigned char);

  Serial.println("Alloca dinamicamente 'datasBW'");
  datasBW = (unsigned char*) malloc(bufferSize);
  if (!datasBW) {
    Serial.println("Errore: impossibile allocare datasBW");
    while(1);  // TODO: GESTIRE DIVERSAMENTE
  }

  Serial.println("Alloca dinamicamente 'datasRW'");
  datasRW = (unsigned char*) malloc(bufferSize);
  if (!datasRW) {
    Serial.println("Errore: impossibile allocare datasRW");
    free(datasBW);
    while(1);  // TODO: GESTIRE DIVERSAMENTE
  }

  //SPI
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); 
  SPI.begin ();  

}

void freeMemGrafica() {
	free(datasBW);
  free(datasRW);
}

// Funzione per disegnare un pixel 
void drawPixel(int x, int y, int color) {
	if (x < 0) return; if (x >= EPD_WIDTH) return; if (y < 0) return; if (y >= EPD_HEIGHT) return;
	
	int byteIndex = (y * EPD_WIDTH + x) / 8; 
	int bitIndex = 7 - (x % 8); 
	if (color == PIXEL_BLACK) { 
		datasBW[byteIndex] |= (1 << bitIndex);  // nero  
		datasRW[byteIndex] &= ~(1 << bitIndex); // non rosso 
	} 
	else if (color == PIXEL_RED) { 
		datasBW[byteIndex] |= (1 << bitIndex); // nero 
		datasRW[byteIndex] |= (1 << bitIndex); // rosso 
	} 
	else /* pixel white */ { 
		datasBW[byteIndex] &= ~(1 << bitIndex); // bianco 
		datasRW[byteIndex] &= ~(1 << bitIndex); // bianco 
	} 
}

void drawLine(int x0, int y0, int x1, int y1, int color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  
	if (steep) { _swap_int16_t(x0, y0); _swap_int16_t(x1, y1); }
  if (x0 > x1) { _swap_int16_t(x0, x1); _swap_int16_t(y0, y1); }

  int16_t dx = x1 - x0, dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) { ystep = 1; } else { ystep = -1; }
  for (; x0 <= x1; x0++) { 
		if (steep) { drawPixel(y0, x0, color); }
		else { drawPixel(x0, y0, color); }
		err -= dy;
    if (err < 0) { y0 += ystep; err += dx; }
  }
}

void drawRect(int x0, int y0, int x1, int y1, int color) {
	drawLine(x0, y0, x0, y1, color);
	drawLine(x0, y0, x1, y0, color);
	drawLine(x1, y1, x0, y1, color);
	drawLine(x1, y1, x1, y0, color);
}

void fillRect(int x0, int y0, int x1, int y1, int color) {
  if (y0 > y1) { _swap_int16_t(y0, y1); }
	for (int y=y0; y<=y1; y++) {
		drawLine(x0, y, x1, y, color);
	}
}

void getTextBoundsNoWrap(const char *str, const GFXfont *font, int16_t *dx, int16_t *dy, int16_t *w, int16_t *h, int16_t maxwidth) {
  // Da Adafruit_GFX.cpp :
  uint8_t c;
	int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
	int16_t x, y;
	int16_t textsize_x = 1, textsize_y = 1;
	
	x = y =  *w = *h = 0; 
	*dx = *dy = 0;

  while ((c = *str++)) {
		if (c == '\n') { 
			x = 0; 
			y += textsize_y * (uint8_t)pgm_read_byte(&font->yAdvance) - 5; 
		} 
		else if (c == '\r') { 
			x = 0; 
		} 
		else { 
      uint8_t first = pgm_read_byte(&font->first), last = pgm_read_byte(&font->last);
			if ((c >= first) && (c <= last)) { 
				GFXglyph *glyph = font->glyph + c - first;
				uint8_t gw = pgm_read_byte(&glyph->width),
                gh = pgm_read_byte(&glyph->height),
                xa = pgm_read_byte(&glyph->xAdvance);
        int8_t xo = pgm_read_byte(&glyph->xOffset),
               yo = pgm_read_byte(&glyph->yOffset);
        int16_t x1 = x + xo * textsize_x;
				int16_t y1 = y + yo * textsize_y;
				int16_t x2 = x1 + gw * textsize_x - 1;
				int16_t y2 = y1 + gh * textsize_y - 1;
        if (x1 < minx) { minx = x1; }
        if (y1 < miny) { miny = y1; }
        if (x2 > maxx) { maxx = x2; }
        if (y2 > maxy) { maxy = y2; }

				x += xa * textsize_x;
      }
    }
  }

  if (maxx >= minx) { *w = maxx - minx + 1; }
  if (maxy >= miny) { *h = maxy - miny + 1; }
  *dx = minx;
  *dy = maxy;
}

void getTextBounds(const char *str, const GFXfont *font, 
                   int16_t *dx, int16_t *dy, int16_t *w, int16_t *h, 
                   int16_t maxWidth) 
{
  int16_t cursorX = 0, cursorY = 0;
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
  int16_t textsize_x = 1, textsize_y = 1;

  const char *wordStart = str;

  while (*str) {
    char c = *str;
    // una parola è delimitata da spazio, newline, carriage return, oppure fine stringa
    if ((c == ' ') || (c == '\n') || (c == '\r') || (*(str+1) == '\0')) {
      // calcola larghezza della parola
      int tempWidth = 0;
      const char *p = wordStart;
      while (p <= str) {
        char ch = *p++;
        if ((ch < font->first) || (ch > font->last)) continue;
        GFXglyph *glyph = &(font->glyph[ch - font->first]);
        tempWidth += pgm_read_byte(&glyph->xAdvance) * textsize_x;
      }

      // se la parola non entra, va a capo
      if ((cursorX + tempWidth) > maxWidth) {
        cursorX = 0;
        cursorY += textsize_y * (uint8_t)pgm_read_byte(&font->yAdvance) - 5;
      }

      // misura caratteri della parola
      p = wordStart;
      while (p <= str) {
        char ch = *p++;
        if (ch < font->first || ch > font->last) continue;
        GFXglyph *glyph = &(font->glyph[ch - font->first]);

        uint8_t gw = pgm_read_byte(&glyph->width);
        uint8_t gh = pgm_read_byte(&glyph->height);
        uint8_t xa = pgm_read_byte(&glyph->xAdvance);
        int8_t xo = pgm_read_byte(&glyph->xOffset);
        int8_t yo = pgm_read_byte(&glyph->yOffset);

        int16_t x1 = cursorX + xo * textsize_x;
        int16_t y1 = cursorY + yo * textsize_y;
        int16_t x2 = x1 + gw * textsize_x - 1;
        int16_t y2 = y1 + gh * textsize_y - 1;

        if (x1 < minx) minx = x1;
        if (y1 < miny) miny = y1;
        if (x2 > maxx) maxx = x2;
        if (y2 > maxy) maxy = y2;

        cursorX += xa * textsize_x;
      }

      // gestisci separatori
      if (c == ' ') {
        GFXglyph *glyph = &(font->glyph[c - font->first]);
        cursorX += pgm_read_byte(&glyph->xAdvance) * textsize_x;
      } 
      else if (c == '\n') {
        cursorX = 0;
        cursorY += textsize_y * (uint8_t)pgm_read_byte(&font->yAdvance) - 5;
      } 
      else if (c == '\r') {
        cursorX = 0;
      }

      wordStart = str + 1;
    }
    str++;
  }

  // output finale
  if (maxx >= minx) { *w = maxx - minx + 1; } else { *w = 0; }
  if (maxy >= miny) { *h = maxy - miny + 1; } else { *h = 0; }
  *dx = minx;
  *dy = miny;
}

void drawCharFont(int x, int y, char c, const GFXfont *font, int color) {
  // Da Adafruit_GFX.cpp :
  if (c < font->first || c > font->last) return;

  c -= (uint8_t)pgm_read_byte(&font->first);
  GFXglyph *glyph = font->glyph + c;
  uint8_t *bitmap = font->bitmap;
  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t  w = pgm_read_byte(&glyph->width), 		         
           h = pgm_read_byte(&glyph->height);
  int8_t   xo = pgm_read_byte(&glyph->xOffset),
           yo = pgm_read_byte(&glyph->yOffset);
  uint8_t  xx, yy, bits = 0, bit = 0;
  int16_t  xo16 = 0, yo16 = 0;
	/*
	  int size_x = 1, size_y = 1; // fattore di scala

	  if (size_x > 1 || size_y > 1) {
        xo16 = xo;
        yo16 = yo;
    }
	*/
   
  for (yy = 0; yy < h; yy++) {
  	for (xx = 0; xx < w; xx++) {
      if (!(bit++ & 7)) {
        bits = pgm_read_byte(&bitmap[bo++]);
      }
      if (bits & 0x80) {
	  		drawPixel(x + xo + xx, y + yo + yy, color);
		  	/*
          if (size_x == 1 && size_y == 1) {
            writePixel(x + xo + xx, y + yo + yy, color);
          } else {
            writeFillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y, size_x, size_y, color);
          }
			*/
      }
      bits <<= 1;
    }
  }
}

void drawTextFont(int x, int y, const char *str, const GFXfont *font, int color, int16_t maxWidth) {
  int cursorX = x;
	int textsize_x = 1, textsize_y = 1;

  const char *wordStart = str;
  int wordWidth = 0;

  while (*str) {
    char c = *str;
    if ((c == ' ') || (c == '\n') || (c == '\r') || (*(str+1) == '\0')) {
      // calcola la larghezza della parola
      int tempWidth = 0;
      const char *p = wordStart;
      while (p <= str) {
        char ch = *p++;
        if ((ch < font->first) || (ch > font->last)) { continue; }
        GFXglyph *glyph = &(font->glyph[ch - font->first]);
        tempWidth += pgm_read_byte(&glyph->xAdvance) * textsize_x;
      }

      // se non entra, vai a capo
      if ((cursorX + tempWidth) > (x + maxWidth)) {
        cursorX = x;
        y += textsize_y * (uint8_t)pgm_read_byte(&font->yAdvance) - 5;
      }

      // disegna la parola
      p = wordStart;
      while (p <= str) {
        char ch = *p++;
        if (ch < font->first || ch > font->last) continue;
        drawCharFont(cursorX, y, ch, font, color);
        GFXglyph *glyph = &(font->glyph[ch - font->first]);
        cursorX += pgm_read_byte(&glyph->xAdvance) * textsize_x;
      }

      // se c’è spazio, aggiungi
      if (c == ' ') {
        GFXglyph *glyph = &(font->glyph[' ' - font->first]);
        cursorX += pgm_read_byte(&glyph->xAdvance) * textsize_x;
      } 
      else if (c == '\n') {
        cursorX = x;
        y += textsize_y * (uint8_t)pgm_read_byte(&font->yAdvance) - 5;
      }
      else if (c == '\r') {
        cursorX = x;
      }

      wordStart = str + 1;
    }
    str++;
  }
}

char * minutesToCustomString(int duration) {
	static char str[16];
	int ore, minuti, giorni, len;
	minuti = duration; ore = 0; giorni = 0; len = 0; str[0] = '\0';

	while (minuti >= 60) { ore ++; minuti -= 60; }
  while (ore >= 24) { giorni ++; ore -= 24; }
  
  if (giorni != 0) { len += sprintf(str + len, "%dg ", giorni); }
	if (ore    != 0) { len += sprintf(str + len, "%dh ", ore); }
	if (minuti != 0) { len += sprintf(str + len, "%d' ", minuti); }
  if (len > 0) { str[len] = '\0'; }

  return str;
}

void drawCalendarDay(time_t offset_days, CalendarEvent allEvents[], int nEvents) {
  time_t base = time(nullptr) + offset_days * 86400;
  struct tm t; 
  t = *localtime(&base);
  time_t tt = mktime(&t);
  int16_t x, y, y2, tabx, dx, dy, w, h;
	char str[32];
  int headerColor = (offset_days == 0) ? PIXEL_RED : PIXEL_BLACK;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t t1 = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  
  EPD_Init(); // inizializza il display 
	
	// Inizializza i buffer a bianco 
	memset(datasBW, 0x00, EPD_ARRAY); 
	memset(datasRW, 0x00, EPD_ARRAY); 

  fillRect(0, 0, EPD_WIDTH, 40, headerColor);
  x = 10;
  drawTextFont(x, 32, giorniSett[t.tm_wday], &FreeSansBold18pt7b, PIXEL_WHITE, EPD_WIDTH-20);
  getTextBounds(giorniSett[t.tm_wday], &FreeSansBold18pt7b, &dx, &dy, &w, &h, EPD_WIDTH-20);
  x += w+10;
  strftime(str, sizeof(str), ", %d", &t);
  drawTextFont(x, 32, str, &FreeSansBold18pt7b, PIXEL_WHITE, EPD_WIDTH-20);
  getTextBounds(str, &FreeSansBold18pt7b, &dx, &dy, &w, &h, EPD_WIDTH-20);
  x += w+20;
  drawTextFont(x, 32, mesiLong[t.tm_mon], &FreeSansBold18pt7b, PIXEL_WHITE, EPD_WIDTH-20);
  getTextBounds(mesiLong[t.tm_mon], &FreeSansBold18pt7b, &dx, &dy, &w, &h, EPD_WIDTH-20);
  x += w+20;
  strftime(str, sizeof(str), "%Y", &t);
  drawTextFont(x, 32, str, &FreeSansBold18pt7b, PIXEL_WHITE, EPD_WIDTH-20);
  getTextBounds(str, &FreeSansBold18pt7b, &dx, &dy, &w, &h, EPD_WIDTH-20);

  // eventi
  y = 60; y2 = EPD_HEIGHT;
	if (nEvents == 0) {
		//drawRect(10, y, EPD_WIDTH-20, y+32, PIXEL_BLACK);
    drawTextFont(15, y+20, "nessun evento per oggi", &FreeSansOblique12pt7b, PIXEL_BLACK, EPD_WIDTH-30);
	}
	else for (int i = 0; i < nEvents; i++) {
    if ((allEvents[i].start_day == t.tm_mday) && (allEvents[i].start_month == t.tm_mon) && (y<EPD_HEIGHT)) {
      char buf[64];
      int colorbg = PIXEL_WHITE;
      int colorday = PIXEL_RED;
      int colortxt = PIXEL_BLACK;
      time_t t2 = allEvents[i].start_hour * 60 + allEvents[i].start_min;

      if ((t1 >= t2) && (t1 <= t2 + allEvents[i].durationMinutes)) {
        colorbg = PIXEL_BLACK;
        colorday = PIXEL_WHITE;
        colortxt = PIXEL_WHITE;
      }
      
      fillRect (7, y, EPD_WIDTH-7, y2, colorbg);

      sprintf(buf, "%02d:%02d / %s", allEvents[i].start_hour, allEvents[i].start_min, minutesToCustomString(allEvents[i].durationMinutes));

			//drawRect(10, y, EPD_WIDTH-20, y+40, PIXEL_BLACK);
      drawTextFont(15, y+21, buf, &FreeSans9pt7b, colorday, EPD_WIDTH-30);
      getTextBounds(buf, &FreeSans9pt7b, &dx, &dy, &w, &h, EPD_WIDTH-30);
      tabx = ((w/50)+1) * 50;
			drawTextFont(15+tabx+15, y+25, allEvents[i].title, &FreeSansBold12pt7b, colortxt, EPD_WIDTH-(30+dx+w+15));
      getTextBounds(allEvents[i].title, &FreeSansBold12pt7b, &dx, &dy, &w, &h, EPD_WIDTH-(30+dx+w+15));
			
      y += h+20;
      if ((y+20) >= EPD_HEIGHT) { y = EPD_HEIGHT; }

    }
    fillRect (7, y, EPD_WIDTH-7, y2, PIXEL_WHITE);
  }

  EPD_WhiteScreen_ALL(datasBW, datasRW); 
	Serial.println("Calendario disegnato!");
}

void drawCalendar5Days(time_t offset_days, CalendarEvent allEvents[], int nEvents) {
  time_t oggi = time(nullptr);
  time_t base = oggi + ((offset_days - 1) * 86400);  // partenza da ieri
  struct tm t0, t;
  t0 = *localtime(&oggi);

  int cellWidth = EPD_WIDTH / 5;
  int topHeader = 36;
  int margin = 3;
  int16_t dx, dy, w, h;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t tNowMin = ((timeinfo.tm_mon * 100 + timeinfo.tm_mday) * 24 + timeinfo.tm_hour) * 60 + timeinfo.tm_min;

  EPD_Init();
  memset(datasBW, 0x00, EPD_ARRAY);
  memset(datasRW, 0x00, EPD_ARRAY);

  for (int d = 0; d < 5; d++) {
    t = *localtime(&base);
    int x0 = d * cellWidth;
    int x1 = x0 + cellWidth - 1;
    int y0 = 0;
    int y2 = EPD_HEIGHT - 2;

    // colore intestazione
    int giornoColor = ((t.tm_mday == t0.tm_mday) &&
                       (t.tm_mon == t0.tm_mon) &&
                       (t.tm_year == t0.tm_year)) ? PIXEL_RED : PIXEL_BLACK;

    // rettangolo del giorno
    drawRect(x0, y0, x1, y2, giornoColor);
    fillRect(x0 + 1, y0 + 1, x1 - 1, y0 + topHeader, giornoColor);

    // scritta giorno
    char titolo[32];
    sprintf(titolo, "%s %02d/%02d", giorniSett[t.tm_wday], t.tm_mday, t.tm_mon + 1);
    getTextBounds(titolo, &FreeSansBold12pt7b, &dx, &dy, &w, &h, cellWidth - 6);
    drawTextFont(x0 + (cellWidth - w) / 2 - dx,
                 y0 + (topHeader - h) / 2 - dy,
                 titolo, &FreeSansBold12pt7b, PIXEL_WHITE, cellWidth - 6);

    // eventi
    int y = y0 + topHeader + 10;
    for (int i = 0; i < nEvents; i++) {
      if ((allEvents[i].start_day == t.tm_mday) && (allEvents[i].start_month == t.tm_mon)) {
        char ebuf[64];
        int colorbg = PIXEL_WHITE;
        int colorday = PIXEL_RED;
        int colortxt = PIXEL_BLACK;

        time_t t2 = ((allEvents[i].start_month * 100 + allEvents[i].start_day) * 24 + allEvents[i].start_hour) * 60 + allEvents[i].start_min;
        if ((tNowMin >= t2) && (tNowMin <= t2 + allEvents[i].durationMinutes) && (y<y2)) {
          colorbg = PIXEL_BLACK;
          colorday = PIXEL_WHITE;
          colortxt = PIXEL_WHITE;
        }

        int rowHeight = 30;
        fillRect(x0 + margin, y, x1 - margin, y2, colorbg);

        sprintf(ebuf, "%02d:%02d / %s", allEvents[i].start_hour, allEvents[i].start_min, minutesToCustomString(allEvents[i].durationMinutes));
        drawTextFont(x0 + 5, y + 17, ebuf, &FreeSansBold9pt7b, colorday, cellWidth - 10);
        getTextBounds(ebuf, &FreeSansBold9pt7b, &dx, &dy, &w, &h, cellWidth - 10);
        y += h+8;
        drawTextFont(x0 + 5, y + 17, allEvents[i].title, &FreeSans9pt7b, colortxt, cellWidth - 10);
        getTextBounds(allEvents[i].title, &FreeSans9pt7b, &dx, &dy, &w, &h, cellWidth - 10);
        y += h+13;
        if (y  > y2 - 14) { y = y2; }
      }  
    }
    fillRect(x0 + margin, y, x1 - margin, y2, PIXEL_WHITE);

    base += 86400;
  }

  EPD_WhiteScreen_ALL(datasBW, datasRW);
  Serial.println("Calendario verticale disegnato!");
}

void drawCalendarWeek(time_t offset_days, CalendarEvent allEvents[], int nEvents) {
  int dayWidth = EPD_WIDTH / 4;  // 2 righe da 4 giorni
  int dayHeight = EPD_HEIGHT / 2;
  time_t oggi = time(nullptr);
	time_t base = oggi + ((offset_days-1) * 86400);  // partenza da ieri
  struct tm t0, t, te;
  int16_t dx, dy, w, h;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t t1 = ((timeinfo.tm_mon * 100 + timeinfo.tm_mday) * 24 + timeinfo.tm_hour) * 60 + timeinfo.tm_min;
  
  EPD_Init(); // inizializza il display 
	
	// Inizializza i buffer a bianco 
	memset(datasBW, 0x00, EPD_ARRAY); 
	memset(datasRW, 0x00, EPD_ARRAY); 
	
  t0 = *localtime(&oggi);
  for (int d = 0; d < 8; d++) {
    t = *localtime(&base);
    int col = d % 4;
    int row = d / 4;

    int x0 = col * dayWidth;
    int y0 = row * dayHeight;
    int y2 = y0  + dayHeight -2;

    int giornoColor = ((t.tm_mday == t0.tm_mday) &&
                       (t.tm_mon  == t0.tm_mon) &&
                       (t.tm_year == t0.tm_year)) ? (PIXEL_RED) : (PIXEL_BLACK);
                     
    // bordo esterno
    drawRect(x0, y0, x0+dayWidth-1, y0+dayHeight-1, giornoColor);
    // intestazione colorata
    fillRect(x0+1, y0+1, x0+dayWidth-2, y0+25, giornoColor);

    // giorno in italiano
    char buf[32];
    sprintf(buf, "%s %02d/%02d", giorniSett[t.tm_wday], t.tm_mday, t.tm_mon+1); // tm_wday 0=Dom
    //int textColor = (giornoColor == PIXEL_RED) ? (PIXEL_WHITE) : (PIXEL_BLACK);
    getTextBounds(buf, &FreeSansBold9pt7b, &dx, &dy, &w, &h, dayWidth);
	  drawTextFont(x0  + (dayWidth  - w)/2 - dx, 
                 y0  + (25        - h)/2 - dy, buf, &FreeSansBold9pt7b, PIXEL_WHITE, dayWidth);
    
    int y = y0 + 35;
    for (int i = 0; i < nEvents; i++) {
      if ((allEvents[i].start_day == t.tm_mday) && (allEvents[i].start_month == t.tm_mon) && (y<y2)) {
        char ebuf[32];
        int colorbg = PIXEL_WHITE;
        int colorday = PIXEL_RED;
        int colortxt = PIXEL_BLACK;
        time_t t2 = ((allEvents[i].start_month * 100 + allEvents[i].start_day) * 24 + allEvents[i].start_hour) * 60 + allEvents[i].start_min;
  
        if ((t1 >= t2) && (t1 <= t2 + allEvents[i].durationMinutes)) {
          colorbg = PIXEL_BLACK;
          colorday = PIXEL_WHITE;
          colortxt = PIXEL_WHITE;
        }
      
        fillRect (x0+2, y, x0+dayWidth-3, y2, colorbg);

        sprintf(ebuf, "%02d:%02d / %s", allEvents[i].start_hour, allEvents[i].start_min, minutesToCustomString(allEvents[i].durationMinutes));
        drawTextFont(x0+5, y+14, ebuf, &FreeSansBold7pt7b, colorday, dayWidth-10);
			  getTextBounds(ebuf, &FreeSansBold7pt7b, &dx, &dy, &w, &h, dayWidth-10);
			  y += 5+h;
        drawTextFont(x0+5, y+14, allEvents[i].title, &FreeSansBold7pt7b, colortxt, dayWidth-10);
			  getTextBounds(allEvents[i].title, &FreeSansBold7pt7b, &dx, &dy, &w, &h, dayWidth-10);
			  y += 10+h;
        if ((y+10) >= y2-14) { y = y2; }
      }
      fillRect (x0+2, y, x0+dayWidth-3, y2, PIXEL_WHITE);

    }

    base += 86400; // giorno successivo
  }

	// Mostra tutto sul display 
	EPD_WhiteScreen_ALL(datasBW, datasRW); 

	Serial.println("Calendario disegnato!");
}

void drawCalendarMonth(time_t offset_days, CalendarEvent allEvents[], int nEvents) {
  // dimensioni celle
  int cellWidth  = EPD_WIDTH / 7;   // 7 giorni per riga
  int cellHeight = (EPD_HEIGHT - 20) / 5;  // 5 righe di settimane
  time_t oggi = time(nullptr);
  time_t base = oggi + (offset_days * 86400);  // partenza da ieri
  struct tm t0, t, te;
  int16_t dx, dy, w, h;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t t1 = ((timeinfo.tm_mon * 100 + timeinfo.tm_mday) * 24 + timeinfo.tm_hour) * 60 + timeinfo.tm_min;
 
  EPD_Init();  

  memset(datasBW, 0x00, EPD_ARRAY); 
  memset(datasRW, 0x00, EPD_ARRAY); 

  // disegna intestazione giorni della settimana
  for (int d = 0; d < 7; d++) {
    int x0 = d * cellWidth;
    int y0 = 0;

    // bordo superiore
    fillRect(x0, y0, x0 + cellWidth - 1, y0 + 20, PIXEL_RED);
    drawRect(x0, y0, x0 + cellWidth - 1, y0 + 21, PIXEL_BLACK);
    getTextBounds(giorniSett[d+1], &FreeSansBold7pt7b, &dx, &dy, &w, &h, cellWidth - 2);
		drawTextFont(x0 + (cellWidth  - w)/2 - dx, 
		             y0 + (20         - h)/2 - dy, 
								 giorniSett[d+1], &FreeSansBold7pt7b, PIXEL_WHITE, cellWidth - 2);   
  }

  t0 = *localtime(&oggi); // oggi
  
  // calcola primo giorno del mese
  struct tm firstDay = *localtime(&base);
  int year = firstDay.tm_year + 1900;
  int month = firstDay.tm_mon + 1;
  firstDay.tm_mday = 1;
  firstDay.tm_hour = 0;
  firstDay.tm_min  = 0;
  firstDay.tm_sec  = 0;

  time_t firstTime = mktime(&firstDay);
  t = *localtime(&firstTime);
    
  // tm_wday in C: 0=Dom, 1=Lun ... vogliamo LUN=0
  int startWeekday = (t.tm_wday + 6) % 7; 

  int dayCounter = 1;
  int daysInMonth;
  {
    // giorni nel mese
    struct tm nextMonth = firstDay;
    nextMonth.tm_mon += 1;
    time_t nextTime = mktime(&nextMonth);
    daysInMonth = (int)difftime(nextTime, firstTime) / 86400;
  }

  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 7; col++) {
      int x0 = (col * cellWidth);
      int y0 = 20 + (row * cellHeight);
      int y2 = y0  + cellHeight -2;

      int currentDay = 0;
      if ((row == 0) && (col < startWeekday)) {
        // giorni del mese precedente (lascia vuoto o grigio)
        //fillRect(x0+1, y0+1, x0+cellWidth-2, y0+cellHeight-2, PIXEL_WHITE);
        drawRect(x0, y0, x0+cellWidth-1, y0+cellHeight-1, PIXEL_BLACK);
      } 
      else if (dayCounter <= daysInMonth) {
        currentDay = dayCounter++;
        // evidenzia oggi
        int colorbg = PIXEL_WHITE;
        int colorcell = PIXEL_BLACK;
        int colorday = PIXEL_RED;
        int colortxt = PIXEL_BLACK;
        if ((t0.tm_mday == currentDay) && (t0.tm_mon == month-1) && (t0.tm_year == year-1900)) {
          colorbg = PIXEL_RED;
          colorcell = PIXEL_RED;
          colorday = PIXEL_WHITE;
          colortxt = PIXEL_WHITE;
        }
        // bordo della casella
        drawRect(x0,   y0,   x0+cellWidth-1, y0+cellHeight-1, colorcell);
        fillRect(x0+1, y0+1, x0+cellWidth-2, y0+cellHeight-2, colorbg);
        
        // numero giorno
        char buf[16];
        sprintf(buf, "% 2d %s", currentDay, mesiShort[firstDay.tm_mon]);
        getTextBounds(buf, &FreeSansBold9pt7b, &dx, &dy, &w, &h, cellWidth - 2);
        drawTextFont(x0+5, y0+5+h, buf, &FreeSansBold9pt7b, colorday, cellWidth-10);

        // stampa eventi del giorno (solo ora e durata)
        int yEvent = y0 + 10 + h;
        for (int i = 0; i < nEvents; i++) {
          if ((allEvents[i].start_day == currentDay) && (allEvents[i].start_month == month-1) && (yEvent<y2)) {
            char ebuf[32];
            time_t t2 = ((allEvents[i].start_month * 100 + allEvents[i].start_day) * 24 + allEvents[i].start_hour) * 60 + allEvents[i].start_min;
            if ((t1 >= t2) && (t1 <= t2 + allEvents[i].durationMinutes)) {
              fillRect (x0+2, yEvent,x0+cellWidth-3, y2, PIXEL_BLACK);
            }
            sprintf(ebuf, "%02d:%02d / %s", allEvents[i].start_hour, allEvents[i].start_min, minutesToCustomString(allEvents[i].durationMinutes));
            getTextBounds(ebuf, &FreeSansBold7pt7b, &dx, &dy, &w, &h, cellWidth-10);
            drawTextFont(x0+5, yEvent+h, ebuf, &FreeSansBold7pt7b, colortxt, cellWidth-10);
            yEvent += h + 3;
            if (yEvent >= y2 -2) { yEvent = y2; }
          }
          fillRect (x0+2, yEvent,x0+cellWidth-3, y2, colorbg);
        }
      } 
      else {
        // giorni mese successivo
        fillRect(x0+1, y0+1, x0+cellWidth-2, y0+cellHeight-2, PIXEL_WHITE);
        drawRect(x0, y0, x0+cellWidth-1, y0+cellHeight-1, PIXEL_BLACK);
      }
    }
  }

  EPD_WhiteScreen_ALL(datasBW, datasRW);
  Serial.println("Calendario mensile disegnato!");
}
