# 🗓️ eInk Calendar ESP32

Calendario connesso **WiFi** a **Google Calendar**, visualizzato su un **display eInk da 7,5" (GDEY075Z08)**.  
Mostra in tempo reale gli eventi della giornata, evidenziando **il giorno corrente in rosso** e **l’evento in corso con una cornice nera**.  
Basato su **ESP32-WROOM-32D**, compatibile con **Arduino IDE**.

---

## 📁 Struttura del progetto

```

011-e-ink-calendar/
│
├── eInkCalendar/
│   ├── eInkCalendar.ino        # file principale Arduino
│   ├── gcalendar.ino           # gestione eventi da Google Calendar
│   ├── grafica.cpp             # rendering grafico su eInk
│   ├── Display_EPD_x.x         # libreria diaply e-ink
│   └── ...
│
├── libraries/                  # librerie usate (copia locale)
│   ├── Adafruit_BusIO/
│   ├── Adafruit_GFX_Library/
│   ├── ArduinoJson/
│   └── ESP32epdx/              # opzionale, per simulazione
│
├── schematic/                  # schema elettrico dei collegamenti
│
├── CAD/                        # risorse per la stampa 3D
│
└── README.md                   # questo file

```

---

## 🧠 Descrizione

Questo progetto permette di visualizzare gli eventi del tuo **Google Calendar** su un display e-ink da 7,5" in modo chiaro ed elegante, ottimizzato per:

* Display **Good Display GDEY075Z08** (rosso/nero/bianco)  
* **Arduino IDE** (senza PlatformIO)  
* Gestione automatica dell’**ora legale (Europa/Roma)**  
* Rendering grafico semplificato per eInk a tre colori  
* Evidenziazione chiara di eventi in corso e giorno attuale

---

## 🗝️ Creazione delle chiavi Google Calendar

Per collegare l’ESP32 al tuo calendario, serve un **Service Account JSON**.

### 1️⃣ Crea il progetto su Google Cloud

* Vai su [https://console.cloud.google.com](https://console.cloud.google.com)  
* Crea un nuovo progetto (es. `mio-google-calendar`)

### 2️⃣ Abilita l’API

* Vai in **API e Servizi → Libreria**  
* Cerca **Google Calendar API**  
* Clicca su **Abilita**

### 3️⃣ Crea un account di servizio

* Vai su **API e Servizi → Credenziali**  
* Clicca **Crea credenziali → Account di servizio**  
* Nome: `gcalendar-esp32`  
* Ruolo: **Viewer (Visualizzatore)**

### 4️⃣ Genera la chiave JSON

* Nella lista degli account di servizio, seleziona `gcalendar-esp32`  
* Vai su **Chiavi → Aggiungi chiave → Crea nuova chiave → JSON**  
* Scarica il file `.json` e rinominalo in:

```

keys/gcalendar-esp32.json

```

> ⚠️ **Non condividere mai questo file**: contiene la chiave privata del tuo progetto.

---

## 📅 Condivisione del calendario

1. Apri [Google Calendar](https://calendar.google.com)  
2. Clicca sull’icona **⚙️ Impostazioni**  
3. Seleziona il calendario da sincronizzare  
4. Vai su **Condivisione con persone e gruppi**  
5. Aggiungi l’indirizzo e-mail del Service Account  
   (es. `gcalendar-esp32@nomeprogetto.iam.gserviceaccount.com`)  
6. Concedi il permesso: **Vedere tutti i dettagli dell’evento**  
7. Recupera l’**ID calendario** da:  

```

Impostazioni e condivisione → Integra calendario → ID calendario

````

8. Inserisci l’ID in `gcalendar.ino`, nell’array `calendarIds[][2]`.

---

## ⚙️ Configurazione WiFi

Copia `credentials_example.h` in `credentials.h` e inserisci i tuoi dati:

```cpp
#define WIFI_SSID "NomeRete"
#define WIFI_PASS "PasswordRete"
````

---

## 🖥️ Compilazione (Arduino IDE)

* Scheda: **WEMOS LOLIN32**
* Flash: **4 MB**
* Partition scheme: **No OTA (Large APP)**

Apri `011-e-ink-calendar.ino` e compila normalmente.

---

## 🕓 Gestione oraria

Il firmware usa il fuso **Europe/Rome** con supporto automatico per l’ora legale.
Gli eventi vengono memorizzati anche in formato raw (minuti, ore, giorno, mese) per evitare discrepanze tra orari UTC e locali.

---

## 🎨 Visualizzazione

| Elemento        | Colore / Stile                     |
| --------------- | ---------------------------------- |
| Giorno corrente | Sfondo **rosso**, testo bianco     |
| Evento in corso | **Cornice nera** attorno al blocco |
| Altri eventi    | Sfondo bianco, testo nero          |

Il rendering utilizza **filledRect** per ogni evento, cambiando solo il colore di riempimento o del bordo a seconda dello stato.

---

## 🧰 Autore

**Emanuele Frisoni**
Bologna, Italia
📅 Ottobre 2025
💡 [https://github.com/fremsoft](https://github.com/fremsoft)

---

## 🧾 Licenza

Questo progetto è rilasciato sotto licenza **GNU GPL v3**.
Puoi studiarlo, modificarlo e ridistribuirlo, a condizione di mantenere la stessa licenza e citare l’autore originale.
