# 🗓️ eInk Calendar ESP32
# 🗓️ eInk Calendar ESP32

Calendario connesso **WiFi** a **Google Calendar**, visualizzato su un **display eInk da 7,5" (GDEY075Z08)**.  
Mostra in tempo reale gli eventi della giornata, evidenziando **il giorno corrente in rosso** e **l’evento in corso con una cornice nera**.  
Basato su **ESP32-WROOM-32D**, compatibile con **Arduino IDE**.
Lavora a batteria 

* Ispirato al progetto di Eric di "That Project"  
* [Fridge Calendar su GitHub](https://github.com/0015/Fridge-Calendar)

---

## 🎥 Video dimostrativo

Per vedere il progetto in azione, guarda il video dimostrativo che mostra come configurare l’ESP32, collegarlo al Google Calendar e visualizzare gli eventi sul display e-ink.

📺 Guarda il video qui: [Guarda il video](https://youtube.com/@fremsoft)

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
* **Arduino IDE** 
* Gestione automatica dell’**ora legale (Europa/Roma)**  
* Rendering grafico semplificato per eInk a tre colori  
* Evidenziazione chiara di eventi in corso e giorno attuale
* **Aggiornamento dei dati ogni 10 minuti** 
* Alimentazione tramite **batteria al litio** con circuito di ricarica **TP4056**, per un funzionamento continuo e sicuro
* Ricarica della batteria con cavo USB-C o micro-USB


---

## 🗝️ Creazione delle chiavi Google Calendar

Per collegare l’ESP32 al tuo calendario, serve un **Service Account JSON**.

### 1️⃣ Crea il progetto su Google Cloud

* Vai su [https://console.cloud.google.com](https://console.cloud.google.com)  
* Crea un nuovo progetto (es. `mio-google-calendar`)

### 2️⃣ Abilita l’API

* Vai in **API e Servizi**  
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

Sostituisci nel file `gcalendar.ino` il testo tra virgolette con il SSID e la password della tua connessione WiFi:

```cpp
const char* ssid = "<tuo SSID>";
const char* password = "<tua password>";
````

---

## 🖥️ Compilazione (Arduino IDE)

Assicurati di aver caricato le librerie allegate nel progetto.

Apri `eInkCalendar.ino`.

Imposta:
* Scheda: **WEMOS LOLIN32**
* Flash: **4 MB**
* Partition scheme: **No OTA (Large APP)**

 e compila normalmente.

---

## 🕓 Gestione oraria

Il firmware usa il fuso **Europe/Rome** con supporto automatico per l’ora legale.
Gli eventi vengono memorizzati anche in formato raw (minuti, ore, giorno, mese) per evitare discrepanze tra orari UTC e locali.
Per modificare la timezone, modifica `gcalendar.ino` alla seguente riga:

```cpp
const char* tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";  // Europe/Rome
````

---

## 🎨 Visualizzazione

| Elemento        | Colore / Stile                     |
| --------------- | ---------------------------------- |
| Giorno corrente | Sfondo **rosso**, testo bianco     |
| Evento in corso | **Cornice nera** attorno al testo  |
| Altri eventi    | Sfondo bianco, testo nero          |

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
