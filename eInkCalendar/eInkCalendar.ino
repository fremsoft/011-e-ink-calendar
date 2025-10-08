/*
 * Calendario eInk connesso WiFi a Google Calendar 
 * usa display GDEY075Z08 
 * https://www.good-display.com/product/394.html
 *
 * ispirato al progetto di Eric di "That Project"
 * https://github.com/0015/Fridge-Calendar
 *
 * ESP32-WROOM-32D ha una Flash da 4MB (32Mb) 
 * Selezionare WEMOS LOLIN32
 * Partition scheme: No OTA (Large APP)
 *
 * Per giocare usare ESP32epdx Library
 * 
 * Frisoni Emanuele 
 * Bologna, 02-10-2025 
 *
 * ----------------------------------------------
 *
 * Per collegare il calendario a Google Calendar
 * bisogna innanzitutto avere una API KEY:
 * su https://console.cloud.google.com/ 
 *
 * creare un progetto chiamato "mio-google-calendar"
 *
 * Abilita API e Servizi --> Google Calendar API --> Abilita
 *
 * creare un account di servizio:
 * API e Servizi --> Credenziali --> Crea Credenziali --> Service Account
 * nome e ID : gcalendar-esp32
 * ruolo : Viewer (Visualizzatore)
 * 
 * generare le chiavi JSON:
 * gcalendar-esp32 --> Chiavi --> Aggiungi chiave --> Crea nuova chiave --> JSON
 * Salva il file .json e conservalo con cura!
 *
 * Vai su Google Calendar
 * ⚙ --> Impostazioni --> Clicca sul calendario da condividere --> 
 *     Impostazioni calendario --> Condivisione con ..> Aggiungi persone e gruppi -->
 *  - inserire la mail di gcalendar-esp32 che si trova in 
 *    Google Cloud Manager --> gcalendar-esp32 --> Entità con accesso -->
 *          gcalendar-esp32@cerem...iam.gserviceaccount.com
 *  - impostare "Vedere tutti i dettagli dell'evento"
 *  - per ogni calendario che si vuole recuperare: 
 *     tre puntini --> Impostazioni e condivisione --> Integra calendario --> ID calendario
 *     inserire la mail (ID) in calendarIds[][2] = { ... } in gcalendar.ino
 *   
 */

//Tips//
/*
1.Flickering is normal when EPD is performing a full screen update to clear ghosting from the previous image so to ensure better clarity and legibility for the new image.
2.There will be no flicker when EPD performs a partial refresh.
3.Please make sue that EPD enters sleep mode when refresh is completed and always leave the sleep mode command. Otherwise, this may result in a reduced lifespan of EPD.
4.Please refrain from inserting EPD to the FPC socket or unplugging it when the MCU is being powered to prevent potential damage.)
5.Re-initialization is required for every full screen update.
6.When porting the program, set the BUSY pin to input mode and other pins to output mode.
*/

#include "eInkCalendar.h"
#include "grafica.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>

RTC_DATA_ATTR ScreenMode screenMode = GIORNO;
RTC_DATA_ATTR int offset_days = 0;

void printMemoryInfo() {
  // RAM libera generale
  size_t freeHeap = esp_get_free_heap_size();
  
  // DRAM interna
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  // PSRAM libera (se presente)
  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  Serial.println("=== RAM Info ===");
  Serial.print("Free heap total: "); Serial.println(freeHeap);
  Serial.print("Free internal DRAM: "); Serial.println(freeInternal);
  Serial.print("Free PSRAM: "); Serial.println(freePSRAM);
  Serial.println("================");
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Errore: impossibile ottenere l'ora locale");
    return;
  }

  Serial.printf("🕒 Ora locale: %02d:%02d:%02d %02d/%02d/%04d (giorno %d, DST=%d)\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
                timeinfo.tm_wday, timeinfo.tm_isdst);
}


void setup() {
	Serial.begin(115200);

  pinMode(PIN_BUTTON_MODE, INPUT);
  pinMode(PIN_BUTTON_PIU,  INPUT);
  pinMode(PIN_BUTTON_MENO, INPUT);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pin_mask & (1ULL << PIN_BUTTON_MODE)) {
      Serial.println("Sveglio da MODE");
      // -------------------------
      // Controllo pressione lunga
      // -------------------------
      unsigned long pressStart = millis();
      do {
        // aspetta che il pulsante venga rilasciato
        delay(10); // piccolo debounce
        if ((millis() - pressStart) >= LONG_PRESS_MS) {
          offset_days = 0; // azzera offset
          Serial.println("PRESSIONE LUNGA: azzerato offset_days");
          break;
        }
      } while(digitalRead(PIN_BUTTON_MODE) == HIGH);

      if ((millis() - pressStart) < LONG_PRESS_MS) {
        screenMode = ScreenMode((screenMode + 1) % 4);
        Serial.printf("Cambio screenMode: %d\n", screenMode);
      }
    }
    if (wakeup_pin_mask & (1ULL << PIN_BUTTON_PIU)) {
      Serial.println("Sveglio da T1");
      switch(screenMode) {
        case GIORNO    : offset_days ++; break;
        case _5DAYS    : offset_days += 5; break;
        case SETTIMANA : offset_days += 7; break;
        case MESE      : offset_days += 30; break;
      }
    }
    if (wakeup_pin_mask & (1ULL << PIN_BUTTON_MENO)) {
      Serial.println("Sveglio da T2");
      switch(screenMode) {
        case GIORNO    : offset_days --; break;
        case _5DAYS    : offset_days -= 5; break;
        case SETTIMANA : offset_days -= 7; break;
        case MESE      : offset_days -= 30; break;
      }
    }
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Sveglio dal timer");
    // gestisci logica timer
  } else {
    Serial.println("Boot normale (power on / reset)");
    screenMode = MESE;
    offset_days = 0;
  }

  printMemoryInfo();
	setupDisplay();
  printMemoryInfo();
	connectWiFi();
  printMemoryInfo();
  
  getEventsAndDisplay( screenMode, offset_days );

  // 0=T0, 2=T1, 4=T2, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 39
  uint64_t mask = (1ULL << PIN_BUTTON_MODE) | (1ULL << PIN_BUTTON_PIU) | (1ULL << PIN_BUTTON_MENO);
  Serial.printf("Wake up mask: %016llX\r\n", mask);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);

  freeMemGrafica();

  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);

  Serial.println("Vado in deep sleep...");
  Serial.flush();

  esp_deep_sleep_start();
}

void loop() {
  /* do nothing */
  printLocalTime(); 
  delay(1000);
}

//////////////////////////////////END//////////////////////////////////////////////////