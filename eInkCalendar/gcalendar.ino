#include "eInkCalendar.h"
#include "grafica.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/pk.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <mbedtls/entropy.h>      
#include <mbedtls/ctr_drbg.h>     


const char* ssid = "<tuo SSID>";
const char* password = "<tua password>";

const char* tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";  // Europe/Rome

                 
const char* calendarIds[][2] = {
  {"calendario1@gmail.com", "LAVORO" },
  {"calendario2@gmail.com", "FAMIGLIA" },
  {"calendario3@group.calendar.google.com", "IMPACT" }
};
const int numCalendars = sizeof(calendarIds) / sizeof(calendarIds[0]);

#define CALENDAR_SCOPE "https://www.googleapis.com/auth/calendar.readonly"

// RTC Memory per salvare il token tra i riavvii
RTC_DATA_ATTR char rtc_accessToken[512] = "";
RTC_DATA_ATTR unsigned long rtc_tokenExpiry = 0;
RTC_DATA_ATTR bool rtc_firstBoot = true;
RTC_DATA_ATTR uint32_t lastEventsHash = 0;

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;

CalendarEvent allEvents[MAX_EVENTS];
int allEventsCount = 0;

char messageWarning[256];

const char PRIVATE_KEY[] PROGMEM = R"KEY(
-----BEGIN PRIVATE KEY-----
MIIE...D7hdIy=
-----END PRIVATE KEY-----
)KEY";

#define PROJECT_ID "ceremonial-port-xxx-r3"
#define CLIENT_EMAIL "gcalendar-esp32@ceremonial-port-xxx-r3.iam.gserviceaccount.com"

void initRandom() {
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  
  const char* pers = "esp32_jwt";
  mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                        (const unsigned char*)pers, strlen(pers));
}

void syncTime() {
  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sincronizzazione NTP");
    
  int attempts = 0;
  while (time(nullptr) < 1600000000 && attempts < 50) {
    Serial.print(".");
    delay(100);
    attempts++;
  }
    
  if (time(nullptr) >= 1600000000) {
    Serial.println("\nOra sincronizzata!");
    time_t now = time(nullptr);
    Serial.println(ctime(&now));
  }
  else {
    Serial.println("\nErrore sincronizzazione!");
  }
}

void connectWiFi() {
  messageWarning[0] = '\0';

  // generatore dei numeri random
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  
  const char* pers = "esp32_jwt_calendar";
  
  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, 
                                   mbedtls_entropy_func, 
                                   &entropy,
                                   (const unsigned char*)pers, 
                                   strlen(pers));

  if (ret != 0) {
    Serial.printf("✗ Errore inizializzazione random: -0x%04x\n", -ret);
    printError("Errore inizializzazione random");
  } else {
    Serial.println("✓ Generatore random inizializzato");
  }
  
  // Aggiungi entropia extra dall'hardware
  uint32_t seed = esp_random();
  mbedtls_entropy_update_manual(&entropy, (unsigned char*)&seed, sizeof(seed));
  
  Serial.println("✓ Entropia hardware aggiunta");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int tentativi=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (tentativi ++ > 50) { 
      printError("Errore connessione WiFi");
    }
  }
  Serial.println("\nWiFi connesso!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Sincronizza l'ora (necessario solo al primo avvio)
  if (rtc_firstBoot) {
    Serial.println("Primo avvio - sincronizzazione NTP...");
    syncTime();
    rtc_firstBoot = false;
  }
  else {
    // Configura NTP e timezone ogni volta
    configTzTime(tz, "pool.ntp.org", "time.nist.gov");
  }
}  

String base64UrlEncode(const uint8_t* input, size_t length) {
  size_t olen;
  // Calcola dimensione output
  mbedtls_base64_encode(NULL, 0, &olen, input, length);
    
  uint8_t* output = (uint8_t*)malloc(olen);
  mbedtls_base64_encode(output, olen, &olen, input, length);
    
  String result = String((char*)output);
  free(output);
    
  // Converti da Base64 a Base64URL
  result.replace("+", "-");
  result.replace("/", "_");
  result.replace("=", "");
    
  return result;
}

String base64UrlEncode(String input) {
  return base64UrlEncode((const uint8_t*)input.c_str(), input.length());
}

String createSignedJWT() {
  time_t now = time(nullptr);
    
  // Verifica che l'ora sia sincronizzata
  if (now < 1600000000) {
    Serial.println("✗ Errore: ora non sincronizzata!");
    return "";
  }

  // Header JWT
  String header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
  String headerEncoded = base64UrlEncode(header);
    
  // Payload JWT con scope CALENDAR
  String payload = "{";
  payload += "\"iss\":\"" + String(CLIENT_EMAIL) + "\",";
  payload += "\"scope\":\"https://www.googleapis.com/auth/calendar.readonly\",";
  payload += "\"aud\":\"https://oauth2.googleapis.com/token\",";
  payload += "\"exp\":" + String(now + 3600) + ",";
  payload += "\"iat\":" + String(now);
  payload += "}";
    
  Serial.println("JWT Payload:");
  Serial.println(payload);
    
  String payloadEncoded = base64UrlEncode(payload);
  String message = headerEncoded + "." + payloadEncoded;
    
  // Firma con la chiave privata 
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
    
  int ret = mbedtls_pk_parse_key(&pk, 
              (const unsigned char*)PRIVATE_KEY, 
              strlen(PRIVATE_KEY) + 1, 
              NULL, 0, 
              mbedtls_ctr_drbg_random, 
              &ctr_drbg);
    
  if (ret != 0) {
    Serial.printf("Errore parsing chiave privata: -0x%04x\n", -ret);
    mbedtls_pk_free(&pk);
    return "";
  }
    
  Serial.println("✓ Chiave privata caricata");

  // Verifica che sia una chiave RSA
  if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
    Serial.println("✗ Errore: la chiave non è RSA!");
    mbedtls_pk_free(&pk);
    return "";
  }
  
  Serial.println("✓ Chiave RSA verificata");
  
  // Hash SHA256
  uint8_t hash[32];
  mbedtls_md_context_t md_ctx;
  mbedtls_md_init(&md_ctx);
  
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md_info == NULL) {
    Serial.println("✗ Errore: impossibile ottenere info SHA256");
    mbedtls_pk_free(&pk);
    return "";
  }
  
  ret = mbedtls_md_setup(&md_ctx, md_info, 0);
  if (ret != 0) {
    Serial.printf("✗ Errore setup MD: -0x%04x\n", -ret);
    mbedtls_pk_free(&pk);
    return "";
  }
  
  mbedtls_md_starts(&md_ctx);
  mbedtls_md_update(&md_ctx, (const uint8_t*)message.c_str(), message.length());
  mbedtls_md_finish(&md_ctx, hash);
  mbedtls_md_free(&md_ctx);
  
  Serial.println("✓ Hash SHA256 calcolato");
  Serial.print("Hash: ");
  for (int i = 0; i < 32; i++) {
    Serial.printf("%02x", hash[i]);
  }
  Serial.println();
  
  // Firma con PKCS#1 v1.5 padding (richiesto da Google)
  uint8_t signature[256];
  size_t sig_len = 0;
  
  // Usa mbedtls_pk_sign con hash_len = 32 (non sizeof(hash) che potrebbe essere diverso)
  ret = mbedtls_pk_sign(&pk, 
                        MBEDTLS_MD_SHA256, 
                        hash, 
                        32,  // Usa 32 esplicitamente invece di sizeof(hash)
                        signature, 
                        sizeof(signature), 
                        &sig_len, 
                        mbedtls_ctr_drbg_random, 
                        &ctr_drbg);
  
  if (ret != 0) {
    Serial.printf("✗ Errore firma: -0x%04x\n", -ret);
    Serial.println("Dettagli errore:");
    
    if (ret == -0x4320) {
      Serial.println("  MBEDTLS_ERR_RSA_BAD_INPUT_DATA");
      Serial.println("  Possibili cause:");
      Serial.println("  - Chiave privata corrotta o formato errato");
      Serial.println("  - Problema con il generatore random");
      Serial.println("  - Hash length non corretto");
    }
    
    mbedtls_pk_free(&pk);
    return "";
  }
  
  mbedtls_pk_free(&pk);
  
  Serial.printf("✓ Firma creata (%d bytes)\n", sig_len);
  
  // Codifica firma in Base64URL
  String signatureEncoded = base64UrlEncode(signature, sig_len);
  
  // JWT completo
  String jwt = message + "." + signatureEncoded;
  
  Serial.println("✓ JWT creato con successo!");
  Serial.printf("JWT length: %d\n", jwt.length());
  
  return jwt;
}

String getAccessTokenFromGoogle() {
  String jwt = createSignedJWT();
    
  if (jwt.length() == 0) {
    Serial.println("✗ Errore nella creazione del JWT");
    return "";
  }
    
  HTTPClient http;
  http.begin("https://oauth2.googleapis.com/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
  String postData = "grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer&assertion=" + jwt;
    
  int httpCode = http.POST(postData);
  String response = http.getString();
    
  Serial.printf("Risposta Google OAuth: %d\n", httpCode);
    
  if (httpCode == 200) {
    StaticJsonDocument<1024> doc;
    //StaticJsonDocument<16384> doc; 
    //DynamicJsonDocument doc(65536); 
    deserializeJson(doc, response);
        
    String accessToken = doc["access_token"].as<String>();
    int expiresIn = doc["expires_in"].as<int>();
        
    Serial.println("✓ Access Token ottenuto!");
    Serial.printf("Valido per %d secondi\n", expiresIn);
        
    http.end();
    return accessToken;
  }
  else {
    Serial.println("✗ Errore nell'ottenere il token:");
    Serial.println(response);
    http.end();
    return "";
  }
}

String getOrRefreshToken() {
  unsigned long now = time(nullptr);
    
  // Se il token è ancora valido (con 5 minuti di margine), usalo
  if ((strlen(rtc_accessToken) > 0) && (now < (rtc_tokenExpiry - 300))) {
    Serial.println("Uso token esistente dalla RTC memory");
    return String(rtc_accessToken);
  }
    
  // Altrimenti, richiedi un nuovo token
  Serial.println("Richiedo nuovo token...");
  String token = getAccessTokenFromGoogle();

  if (token.length() > 0) {
    // Salva in RTC memory
    token.toCharArray(rtc_accessToken, sizeof(rtc_accessToken));
    rtc_tokenExpiry = now + 3600;  // Valido per 1 ora
        
    Serial.println("✓ Token salvato in RTC memory");
    return token;
  }
    
  return "";
}

time_t parseDateTime(const char* str, CalendarEvent *cal) {
    struct tm t = {0};
    sscanf(str, "%4d-%2d-%2dT%2d:%2d:%2d",
           &t.tm_year, &t.tm_mon, &t.tm_mday,
           &t.tm_hour, &t.tm_min, &t.tm_sec);
 
    if (cal != NULL) {
      cal->start_month = t.tm_mon -1;
      cal->start_day   = t.tm_mday;
      cal->start_hour  = t.tm_hour;
      cal->start_min   = t.tm_min;
    }

    t.tm_year -= 1900;  // tm_year = anni dal 1900
    t.tm_mon  -= 1;     // tm_mon  = 0-11
    return mktime(&t); 
}

time_t parseDate(const char* str, CalendarEvent *cal) {
    struct tm t = {0};
    sscanf(str, "%4d-%2d-%2d", &t.tm_year, &t.tm_mon, &t.tm_mday);
    
    if (cal != NULL) {
      cal->start_month = t.tm_mon -1;
      cal->start_day   = t.tm_mday;
      cal->start_hour  = 0;
      cal->start_min   = 0;
    }

    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    return mktime(&t);
}

uint32_t fnv1aHash(const uint8_t *data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t calcEventsHash() {
  uint32_t hash = 2166136261u;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t t1 = ((timeinfo.tm_mon * 100 + timeinfo.tm_mday) * 24 + timeinfo.tm_hour) * 60 + timeinfo.tm_min;
 
  for (int i = 0; i < allEventsCount; i++) {
    if (allEvents[i].title[0] == '\0') continue; // skip vuoti
    
    time_t t2 = ((allEvents[i].start_month * 100 + allEvents[i].start_day) * 24 + allEvents[i].start_hour) * 60 + allEvents[i].start_min;
    if ((t1 >= t2) && (t1 <= t2 + allEvents[i].durationMinutes)) { 
      hash ^= fnv1aHash((uint8_t*)&t2, sizeof(t2));
    }
    Serial.print(allEvents[i].start_time_utc); Serial.println(allEvents[i].title);           
    hash ^= fnv1aHash((uint8_t*)&allEvents[i].start_time_utc, sizeof(allEvents[i].start_time_utc));
    hash ^= fnv1aHash((uint8_t*)&allEvents[i].durationMinutes, sizeof(allEvents[i].durationMinutes));
    hash ^= fnv1aHash((uint8_t*)&allEvents[i].calendar_id, sizeof(allEvents[i].calendar_id));
        
    // includo titolo
    hash ^= fnv1aHash((uint8_t*)allEvents[i].title, strlen(allEvents[i].title));
  }

    return hash;
}

void getAllCalendarEvents(String accessToken, int daysBefore = 1, int daysAfter = 30) {
  allEventsCount = 0;
  messageWarning[0] = '\0';
  int tentativi = 0;

  time_t nowSec = time(nullptr);
  time_t startSec = nowSec - daysBefore*24*60*60;
  time_t endSec   = nowSec + daysAfter*24*60*60;

  struct tm timeinfoStart, timeinfoEnd;
 
  timeinfoStart = *localtime(&startSec);
  timeinfoEnd   = *localtime(&endSec);

  char timeMin[30], timeMax[30];
  sprintf(timeMin, "%04d-%02d-%02dT00:00:00Z", 
            timeinfoStart.tm_year+1900,
            timeinfoStart.tm_mon+1, 
            timeinfoStart.tm_mday);
  sprintf(timeMax, "%04d-%02d-%02dT23:59:59Z", 
            timeinfoEnd.tm_year+1900, 
            timeinfoEnd.tm_mon+1, 
            timeinfoEnd.tm_mday);

  for (int i = 0; i < numCalendars; i++) {
    int esito = 0;
    while ((esito == 0) && ( tentativi < N_MAX_TENTATIVI)) {
      String url = "https://www.googleapis.com/calendar/v3/calendars/";
      url += calendarIds[i][0];
      url += "/events?timeMin=" + String(timeMin);
      url += "&timeMax=" + String(timeMax);
      url += "&singleEvents=true&orderBy=startTime";
      url += "&fields=items(summary,start,end)";
      //url += "&timeZone=CEST"; 
      url += "&timeZone=Europe/Rome";  // così non va

      tentativi ++;

      Serial.println(url);
      Serial.printf("=== Calendario: %s ===\n\n", calendarIds[i][1]);

      HTTPClient http;
      http.begin(url);
      http.addHeader("Authorization", "Bearer " + accessToken);

      int code = http.GET();
      String payload = http.getString(); 
      Serial.println("Payload length: " + String(payload.length()));
    
      if (code == 200) {
        //StaticJsonDocument<2048> doc;
        //DynamicJsonDocument doc(10 * 1024);
        //DeserializationError error = deserializeJson(doc, payload);
        //String payload = http.getString();   // aspetta che arrivi tutto
        DynamicJsonDocument doc(64*1024);   // dimensione grande per un mese
        DeserializationError error = deserializeJson(doc, payload);
   
        if (!error) {
          JsonArray items = doc["items"].as<JsonArray>();
          for (size_t j = 0; (j < items.size()) && (allEventsCount < MAX_EVENTS); j++) {
            JsonObject item = items[j]; 
            // serializeJson(item, Serial);  Serial.println(); 
          
            strncpy(allEvents[allEventsCount].title, (const char*)(item["summary"] | ""), MAX_STRING);
            allEvents[allEventsCount].title[MAX_STRING-1] = 0;
            allEvents[allEventsCount].calendar_id = i;
            allEvents[allEventsCount].start_time_utc  = 0;
            allEvents[allEventsCount].durationMinutes = 0;
           
            if (item["start"].containsKey("dateTime")) {
              // Evento con orario specifico
              String dt = item["start"]["dateTime"].as<String>();
              allEvents[allEventsCount].start_time_utc = parseDateTime(dt.c_str(), &allEvents[allEventsCount]); 
            
              if (item["end"].containsKey("dateTime")) {
                time_t endTime;
                dt = item["end"]["dateTime"].as<String>();
                endTime = parseDateTime(dt.c_str(), NULL); 
                allEvents[allEventsCount].durationMinutes = (endTime - allEvents[allEventsCount].start_time_utc) / 60;
              } 
            } 
            else if (item["start"].containsKey("date")) {
              // Evento giornaliero
              String d = item["start"]["date"].as<String>();
              allEvents[allEventsCount].start_time_utc = parseDate(d.c_str(), &allEvents[allEventsCount]); 
            
              if (item["end"].containsKey("date")) {
                time_t endTime;
                d = item["end"]["date"].as<String>();
                endTime = parseDate(d.c_str(), NULL); 
                allEvents[allEventsCount].durationMinutes = (endTime - allEvents[allEventsCount].start_time_utc) / 60;
              } 
            }

            allEventsCount++;
          }
          esito = 1;
        } 
        else {
          Serial.printf("✗ Errore parsing JSON: %s\n", error.c_str());
          Serial.println("\nRisposta completa:");
          Serial.println(payload);
        }
      } 
      else if (code == 403) {
        Serial.println("✗ ERRORE 403: Accesso negato!");
        Serial.println("SOLUZIONE:");
        Serial.println("1. Vai su Google Calendar");
        Serial.printf("2. Condividi il calendario '%s' con:\n", calendarIds[i][0]);
        Serial.printf("   %s\n", CLIENT_EMAIL);
        Serial.println("3. Imposta permessi 'Visualizza tutti i dettagli'");
        Serial.println("\nRisposta completa:");
        Serial.println(payload);
      }
      else if (code == 401) {
        Serial.println("✗ ERRORE 401: Token non valido o scaduto");
        Serial.println("Risposta:");
        Serial.println(payload);
      } 
      else if (code == 404) {
        Serial.println("✗ ERRORE 404: Calendario non trovato");
        Serial.printf("Verifica che '%s' sia l'ID corretto\n", calendarIds[i][0]);
      } 
      else {
        Serial.printf("✗ Errore HTTP: %d\n", code);
        Serial.println("Risposta:");
        Serial.println(payload);
      }
      http.end();
    }
  }

  if (tentativi > 1) {
    sprintf(messageWarning, "problemi di sincronizzazione con Google Calendar!\nho fatto %d tentativi", tentativi);
  }

  // Ordinamento cronologico (bubble sort semplice)
  for (int i = 0; i < allEventsCount-1; i++) {
    for (int j = i+1; j < allEventsCount; j++) {
      if (allEvents[i].start_time_utc > allEvents[j].start_time_utc) {
        CalendarEvent temp = allEvents[i];
        allEvents[i] = allEvents[j];
        allEvents[j] = temp;
      }
    }
  }
  
  Serial.printf("\nTotale eventi caricati: %d\n", allEventsCount);
}

void getEventsAndDisplay(ScreenMode mode, time_t offset_days) {
   // Ottieni o aggiorna il token
  String accessToken = getOrRefreshToken();
  uint32_t newHash;

  if (accessToken.length() > 0) {
    Serial.println("\n✓ Token pronto per le API Calendar!");
    Serial.printf("Token: %s...\n\n", accessToken.substring(0, 30).c_str());
        
    switch( screenMode ) {
			case GIORNO : 
        getAllCalendarEvents(accessToken, 0-offset_days, 0+offset_days); 
        newHash = calcEventsHash();
        if (newHash != lastEventsHash) {
          Serial.printf("⚡ Eventi cambiati [%08X] → aggiorno display\r\n", newHash);
          lastEventsHash = newHash;
          drawCalendarDay(offset_days, allEvents, allEventsCount);
        } 
        else {
          Serial.printf("✅ Nessuna modifica [%08X] → skip refresh\r\n", newHash);
        }
				break;
			case _5DAYS : 
        getAllCalendarEvents(accessToken, 1-offset_days, 5+offset_days); 
				newHash = calcEventsHash();
        if (newHash != lastEventsHash) {
          Serial.printf("⚡ Eventi cambiati [%08X] → aggiorno display\r\n", newHash);
          lastEventsHash = newHash;
          drawCalendar5Days(offset_days, allEvents, allEventsCount);
        } 
        else {
          Serial.printf("✅ Nessuna modifica [%08X] → skip refresh\r\n", newHash);
        }
				break;
			case SETTIMANA : 
        getAllCalendarEvents(accessToken, 1-offset_days, 6+offset_days); 
				newHash = calcEventsHash();
        if (newHash != lastEventsHash) {
          Serial.printf("⚡ Eventi cambiati [%08X] → aggiorno display\r\n", newHash);
          lastEventsHash = newHash;
          drawCalendarWeek(offset_days, allEvents, allEventsCount);
        } 
        else {
          Serial.printf("✅ Nessuna modifica [%08X] → skip refresh\r\n", newHash);
        }
				break;
			case MESE :
        getAllCalendarEvents(accessToken, 6-offset_days, 35+offset_days);  
				newHash = calcEventsHash();
        if (newHash != lastEventsHash) {
          Serial.printf("⚡ Eventi cambiati [%08X] → aggiorno display\r\n", newHash);
          lastEventsHash = newHash;
          drawCalendarMonth(offset_days, allEvents, allEventsCount);
        } 
        else {
          Serial.printf("✅ Nessuna modifica [%08X] → skip refresh\r\n", newHash);
        }
				break;
		}    
    
    // Stampa eventi
    Serial.println("\n=== EVENTI CALENDARIO ===");
    for (int i = 0; i < allEventsCount; i++) {
      struct tm startInfo = *localtime(&allEvents[i].start_time_utc);
      Serial.print(startInfo.tm_mday);
      Serial.print("/");
      Serial.print(startInfo.tm_mon + 1);
      Serial.print(" ");
      Serial.print(startInfo.tm_hour);
      Serial.print(" - ");
      Serial.print(startInfo.tm_min);
      Serial.print(" [");
      Serial.print(allEvents[i].durationMinutes);
      Serial.print("'] ");
      Serial.print(calendarIds[allEvents[i].calendar_id][1]);
      Serial.print(" : ");
      Serial.println(allEvents[i].title);
      //drawText(10, y, ev.title.c_str(), PIXEL_RED);
		}
  } else {
    Serial.println("Errore: impossibile ottenere il token!");
  }
}

void printError(const char * error) {

  drawError(error);

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
