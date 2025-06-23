#include <SPI.h>
#include <MFRC522.h>       // RFID için kütüphane
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SD.h>            // SD kart için kütüphane
#include <BluetoothSerial.h> // Bluetooth için ekle
#include <WiFi.h> // ESP32 WiFi kütüphanesi
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp_task_wdt.h>  // Watchdog timer için

// Store constant strings in PROGMEM
const char PROGMEM str_obsidibyte[] = "Obsidibyte";
const char PROGMEM str_immortal[] = "The Immortal";
const char PROGMEM str_rfid[] = "RFID";
const char PROGMEM str_sd_card[] = "SD Card";
const char PROGMEM str_settings[] = "Ayarlar";
const char PROGMEM str_wifi[] = "WiFi";
const char PROGMEM str_exit[] = "Exit";
const char PROGMEM str_start[] = "Start";
const char PROGMEM str_rf_modul[] = "RF Modul";
const char PROGMEM str_sd_kart[] = "SD KART";
const char PROGMEM str_kart_takili[] = "Kart Takili";
const char PROGMEM str_boyut[] = "Boyut: ";
const char PROGMEM str_mb[] = " MB";
const char PROGMEM str_lutfen_sd[] = "Lutfen SD Kart";
const char PROGMEM str_takin[] = "Takin";
const char PROGMEM str_cikis[] = "Cikis icin SELECT";
const char PROGMEM str_about[] = "Hakkinda";
const char PROGMEM str_frekans[] = "Frekans: ";
const char PROGMEM str_mhz[] = " MHz";
const char PROGMEM str_sinyal[] = "Sinyal";
const char PROGMEM str_yakalandi[] = "Yakalandi!";
const char PROGMEM str_guc[] = "Guc: ";
const char PROGMEM str_dbm[] = " dBm";
const char PROGMEM str_kaydet[] = "KAYDET";
const char PROGMEM str_geri[] = "GERI";
const char PROGMEM str_oynat[] = "OYNAT";
const char PROGMEM str_sil[] = "SIL";
const char PROGMEM str_rf_kayitlar[] = "RF Kayitlar";
const char PROGMEM str_kayit_yok[] = "Kayit Yok";
const char PROGMEM str_tarama[] = "TARAMA";
const char PROGMEM str_durdur[] = "DURDUR";

// TFT pinleri
#define TFT_CS    15
#define TFT_RST   4
#define TFT_DC    2

// SD kart pini
#define SD_CS     5   // SD kart için ayrı CS pin

// Buton pinleri
#define BUTTON_UP     12
#define BUTTON_DOWN   14
#define BUTTON_SELECT 27

// RFID pinleri
#define SS_PIN      22   // SDA / SS pin
#define RST_PIN     33   // Reset pin

// SPI pinleri (ESP32 standart)
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 mfrc522(SS_PIN, RST_PIN);  // RFID objesi

#define BLACK ST77XX_BLACK
#define RED   ST77XX_RED
#define GREEN ST77XX_GREEN
#define WHITE ST77XX_WHITE
#define BLUE  ST77XX_BLUE
#define CYAN  ST77XX_CYAN
#define DARKGREY 0x7BEF
#define YELLOW ST77XX_YELLOW

// Function declarations
void drawText(const char* text, int y, uint16_t color, int size, bool centered = true, int speed = 0);
void drawText(String text, int y, uint16_t color, int size, bool centered = true, int speed = 0);
void drawSDCardMenuPage();

// Menü öğeleri (Hakkında eklendi!)
const char* const menuItems[] PROGMEM = {str_start, str_settings, str_about, str_exit};
const int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedItem = 0;

// Alt Menü öğeleri
const char* const subMenuItems[] PROGMEM = {
  str_rf_modul,
  str_rfid,
  str_sd_card,
  "RF Kayıtlar",
  "RFID Kayıtlar",
  "Jammer",
  str_exit
};
const int subMenuItemCount = sizeof(subMenuItems) / sizeof(subMenuItems[0]);
int subSelectedItem = 0;

bool inSubMenu = false;
bool isInRFModulePage = false;
bool isInRFIDPage = false;
bool isInSDCardPage = false;
bool isInSettingsPage = false;
bool isInAboutPage = false; // <-- yeni değişken
bool bluetoothEnabled = false;
int settingsSelected = 0; // 0: Bluetooth, 1: WiFi, 2: Exit

unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 150;

unsigned long lastWaveTime = 0;
int waveX = 0;
bool waveDirection = true;

unsigned long lastRFIDAnimTime = 0;

bool rfidTextDrawn = false;  // Üst üste yazı çizimini engellemek için

// RFID kart UID ekranda gösterme flag ve değişkenleri
bool cardDetected = false;
String cardUID = "";

// SD kart bağlı mı ve boyutu
bool sdCardPresent = false;
uint64_t sdCardSize = 0;

BluetoothSerial SerialBT;

// WiFi sayfaları için değişkenler
bool wifiEnabled = false;
bool isInWiFiPage = false;
int selectedWiFi = 0;
int wifiNetworkCount = 0;

// Sayfa durumları
#define MAIN_PAGE 0
#define SETTINGS_PAGE 1
#define WIFI_LIST_PAGE 2

int currentPage = MAIN_PAGE;

#define DEBOUNCE_DELAY 200  // Tuş debounce süresi (ms)

// Bluetooth tarama sayfası için değişkenler
bool isInBluetoothPage = false;
int selectedBTDevice = 0;
int btDeviceCount = 0;
#define MAX_BT_DEVICES 10

// Bluetooth bağlantı durumu için değişkenler
bool deviceConnected = false;
bool oldDeviceConnected = false;
String deviceName = "Obsidibyte_ESP32";

// Bluetooth cihaz bilgilerini saklamak için yapı
struct BTDevice {
    String name;
    String address;
    int rssi;
};

BTDevice btDevices[MAX_BT_DEVICES];

// Bluetooth tarama callback sınıfı
class BTScanCallback: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (btDeviceCount < MAX_BT_DEVICES) {
            btDevices[btDeviceCount].address = advertisedDevice.getAddress().toString().c_str();
            btDevices[btDeviceCount].rssi = advertisedDevice.getRSSI();
            
            if (advertisedDevice.haveName()) {
                btDevices[btDeviceCount].name = advertisedDevice.getName().c_str();
            } else {
                btDevices[btDeviceCount].name = "Isimsiz Cihaz";
            }
            
            btDeviceCount++;
        }
    }
};

// WiFi ve Bluetooth listeleri için ortak yerleşim değişkenleri
#define LIST_START_Y 35      // Liste başlangıç Y koordinatı
#define ITEM_HEIGHT 18       // Her öğenin yüksekliği
#define VISIBLE_ITEMS 5      // Görünür öğe sayısı
#define UP_ARROW_Y 28        // Yukarı ok Y koordinatı
#define DOWN_ARROW_Y 145     // Aşağı ok Y koordinatı

// Çarpı işareti koordinatları
#define EXIT_X (tft.width() - 12)  // Çarpı X koordinatı
#define EXIT_Y 2                    // Çarpı Y koordinatı
#define EXIT_SIZE 10                // Çarpı boyutu

// Çarpı işaretine tıklanıp tıklanmadığını kontrol et
bool isExitButtonPressed() {
    if (digitalRead(BUTTON_SELECT) == LOW) {
        // Çarpı işareti bölgesi kontrolü
        return true; // Simüle edilmiş tıklama - gerçek dokunmatik ekranda koordinat kontrolü yapılacak
    }
    return false;
}

// Bluetooth tarama ekranını göster
void drawBluetoothScanningPage() {
    tft.fillScreen(BLACK);
    drawText("BLUETOOTH", 20, CYAN, 2, true, 0);
    
    // Animasyonlu tarama metni
    static int dots = 0;
    String scanText = "Aygitlar Taraniyor";
    for(int i = 0; i < dots; i++) {
        scanText += ".";
    }
    dots = (dots + 1) % 4;
    
    drawText(scanText, 60, WHITE, 1, true, 0);
    drawText("Lutfen Bekleyin", 80, GREEN, 1, true, 0);
}

// Bluetooth tarama sayfasını çiz
void drawBluetoothPage() {
    tft.fillScreen(BLACK);
    drawText("BLUETOOTH", 5, CYAN, 2, true, 0);
    tft.drawFastHLine(0, 25, tft.width(), WHITE);
    
    if (btDeviceCount == 0) {
        drawText("Cihaz Bulunamadi!", 50, RED, 1, true, 0);
        drawText("Tekrar Taratmak", 70, WHITE, 1, true, 0);
        drawText("Icin SELECT", 85, WHITE, 1, true, 0);
    } else {
        // Görüntülenecek cihaz sayısı ve başlangıç indeksi
        int startIdx = max(0, selectedBTDevice - (VISIBLE_ITEMS / 2));
        startIdx = min(startIdx, max(0, btDeviceCount - VISIBLE_ITEMS));
        int endIdx = min(startIdx + VISIBLE_ITEMS, btDeviceCount);

        // Yukarı ok
        if (startIdx > 0) {
            tft.fillTriangle(tft.width()/2 - 5, UP_ARROW_Y + 5, 
                           tft.width()/2 + 5, UP_ARROW_Y + 5, 
                           tft.width()/2, UP_ARROW_Y, CYAN);
        }

        // Cihazları listele
        for (int i = startIdx; i < endIdx; i++) {
            int yPos = LIST_START_Y + ((i - startIdx) * ITEM_HEIGHT);
            
            // Seçili cihazı vurgula
            if (i == selectedBTDevice) {
                tft.fillRect(2, yPos - 2, tft.width() - 4, ITEM_HEIGHT, DARKGREY);
            }
            
            // Cihaz adını yaz
            tft.setTextSize(1);
            tft.setTextColor(WHITE);
            tft.setCursor(5, yPos + 4);
            
            String btDeviceName = btDevices[i].name;
            if (btDeviceName.length() > 16) {
                btDeviceName = btDeviceName.substring(0, 13) + "...";
            }
            tft.print(btDeviceName);
            
            // Sinyal gücü göstergesi
            int rssi = btDevices[i].rssi;
            int barCount = map(rssi, -100, -40, 1, 4);
            for (int b = 0; b < 4; b++) {
                int barHeight = (b + 1) * 2;
                int barX = tft.width() - 18 + (b * 4);
                int barY = yPos + 8 - barHeight;
                if (b < barCount) {
                    tft.fillRect(barX, barY, 3, barHeight, GREEN);
                } else {
                    tft.fillRect(barX, barY, 3, barHeight, DARKGREY);
                }
            }
        }

        // Aşağı ok
        if (endIdx < btDeviceCount) {
            tft.fillTriangle(tft.width()/2 - 5, DOWN_ARROW_Y, 
                           tft.width()/2 + 5, DOWN_ARROW_Y, 
                           tft.width()/2, DOWN_ARROW_Y + 5, CYAN);
        }
    }
    
    drawExitBox(true);
}

// Bluetooth işlemini başlat
void startBluetoothScan() {
    btDeviceCount = 0;
    
    try {
        // ESP32'yi Bluetooth cihazı olarak görünür hale getir
        BLEDevice::init("Obsidibyte_ESP32");
        BLEServer* pServer = BLEDevice::createServer();
        BLEService *pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
        pService->start();
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
        BLEDevice::startAdvertising();
        
        // Tarama ekranını göster
        drawBluetoothScanningPage();
        
        // Tarama işlemini başlat
        BLEScan* pBLEScan = BLEDevice::getScan();
        BTScanCallback* pCallback = new BTScanCallback();
        pBLEScan->setAdvertisedDeviceCallbacks(pCallback);
        pBLEScan->setActiveScan(true);
        pBLEScan->start(3); // 3 saniyelik tarama
        
        // Tarama animasyonu
        for(int i = 0; i < 3; i++) { // 3 saniye boyunca animasyon
            drawBluetoothScanningPage();
            delay(1000);
        }
        
        // Tarama sonuçlarını göster
        drawBluetoothPage();
        
    } catch (...) {
        Serial.println("Bluetooth tarama hatası!");
        drawText("BT Tarama Hatasi!", 60, RED, 1, true, 0);
        delay(2000);
        drawBluetoothPage();
    }
}

// Bluetooth sayfası buton kontrolü
void handleBluetoothButtons() {
    static unsigned long lastPress = 0;
    unsigned long now = millis();
    if (now - lastPress < debounceDelay) return;

    if (digitalRead(BUTTON_UP) == LOW) {
        if (selectedBTDevice > 0) {
            selectedBTDevice--;
            drawBluetoothPage();
        }
        lastPress = now;
    }
    
    if (digitalRead(BUTTON_DOWN) == LOW) {
        if (selectedBTDevice < btDeviceCount - 1) {
            selectedBTDevice++;
            drawBluetoothPage();
        }
        lastPress = now;
    }
    
    if (digitalRead(BUTTON_SELECT) == LOW) {
        lastPress = now;
        if (btDeviceCount == 0) {
            startBluetoothScan();
        } else {
            isInBluetoothPage = false;
            isInSettingsPage = true;
            drawSettingsPage();
            delay(200);
        }
    }
}

// Settings sayfasındaki buton kontrolü
void handleSettingsButtons() {
    static unsigned long lastPress = 0;
    static bool exitPressed = false;
    unsigned long now = millis();
    if (now - lastPress < debounceDelay) return;

    bool up = digitalRead(BUTTON_UP) == LOW;
    bool down = digitalRead(BUTTON_DOWN) == LOW;
    bool select = digitalRead(BUTTON_SELECT) == LOW;

    if (up) {
        settingsSelected = (settingsSelected - 1 + 3) % 3;
        drawSettingsPage();
        lastPress = now;
    } else if (down) {
        settingsSelected = (settingsSelected + 1) % 3;
        drawSettingsPage();
        lastPress = now;
    } else if (select && !exitPressed) {
        lastPress = now;
        exitPressed = true;
        
        if (settingsSelected == 0) {
            isInBluetoothPage = true;
            isInSettingsPage = false;
            selectedBTDevice = 0;
            btDeviceCount = 0;
            startBluetoothScan();
        } else if (settingsSelected == 1) {
            isInWiFiPage = true;
            isInSettingsPage = false;
            openWiFiScanAndListPages();
        } else if (settingsSelected == 2) {
            delay(300);
            isInSettingsPage = false;
            inSubMenu = false;
            selectedItem = 1;
            drawMenu();
        }
    } else if (!select) {
        exitPressed = false;
    }
}

// Bluetooth server ve advertising için değişkenler
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

// Bluetooth server callback sınıfı
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Cihaz bağlandı!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Cihaz ayrıldı!");
      // Yeniden advertising başlat
      pServer->getAdvertising()->start();
    }
};

// Bluetooth server'ı başlat ve advertising yap
void startBluetoothServer() {
    // BLE device'ı başlat
    BLEDevice::init(deviceName.c_str());
    
    // Server oluştur
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Service oluştur
    BLEService *pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    
    // Characteristic oluştur
    pCharacteristic = pService->createCharacteristic(
                        "beb5483e-36e1-4688-b7f5-ea07361b26a8",
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
    
    // Descriptor ekle
    pCharacteristic->addDescriptor(new BLE2902());
    
    // Service'i başlat
    pService->start();
    
    // Advertising başlat
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();
    
    Serial.println("Bluetooth server başlatıldı!");
    Serial.print("Cihaz adı: ");
    Serial.println(deviceName);
}

void setup() {
  Serial.begin(115200);
  
  // Watchdog timer başlat - ESP32 için doğru yöntem
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  // Güvenli donanım kontrolü
  Serial.println("Donanim kontrolu basliyor...");
  
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);  // SPI başlat (ESP32 pinleri)
  
  // TFT ekran başlatma
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.setTextWrap(false);
  tft.fillScreen(BLACK);
  
  // Butonları ayarla
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  
  // RFID modül kontrolü
  mfrc522.PCD_Init();
  Serial.println("RFID modul basariyla baslatildi");

  // SD kart CS pinini çıkış olarak ayarla
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);  // Başlangıçta devre dışı bırak

  // WiFi'yi başlat
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP("ESP32_AP", "12345678")) {
    Serial.println("WiFi AP başlatıldı");
    Serial.print("IP Adresi: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("WiFi AP baslatilamadi!");
  }

  // Bellek kontrolü
  uint32_t freeHeap = ESP.getFreeHeap();
  Serial.print("Bos bellek: ");
  Serial.print(freeHeap / 1024);
  Serial.println(" KB");
  
  if (freeHeap < 10000) {
    Serial.println("UYARI: Bellek az!");
    drawText("BELLEK AZ!", 80, YELLOW, 1, true, 0);
    delay(1000);
  }

  drawText(str_obsidibyte, 10, GREEN, 2, true, 0);
  delay(1000);
  drawText(str_immortal, 70, RED, 2, true, 0);

  tft.fillScreen(BLACK);
  drawMenu();

  Serial.println("Sistem basariyla baslatildi!");
}

void loop() {
  // Watchdog timer reset
  esp_task_wdt_reset();
  
  // Bluetooth bağlantı durumunu kontrol et
  if (deviceConnected != oldDeviceConnected) {
    if (deviceConnected) {
      Serial.println("Cihaz bağlandı!");
    } else {
      Serial.println("Cihaz ayrıldı!");
    }
    oldDeviceConnected = deviceConnected;
    
    // Bluetooth sayfası açıksa ekranı güncelle
    if (isInBluetoothPage) {
      drawBluetoothPage();
    }
  }
  
  if (isInWiFiPage) {
    handleWiFiListButtons();
    return;
  }
  if (isInSettingsPage) {
    handleSettingsButtons();
    delay(50); // Ayarlar sayfasında daha stabil kontrol için kısa bekleme
    return;
  }
  if (isInAboutPage) { // --- Hakkında sayfası açıkken
    static unsigned long lastAboutPress = 0;
    static bool aboutExitPressed = false;
    unsigned long now = millis();
    
    if (digitalRead(BUTTON_SELECT) == LOW && !aboutExitPressed && (now - lastAboutPress > debounceDelay)) {
      aboutExitPressed = true;
      lastAboutPress = now;
      delay(300); // Daha uzun bekleme süresi
      isInAboutPage = false;
      drawMenu();
    } else if (digitalRead(BUTTON_SELECT) == HIGH) {
      aboutExitPressed = false;
    }
    return;
  }
  if (isInRFIDPage) {
    checkRFIDCard();

    // SELECT ile ana menüye dön
    if (digitalRead(BUTTON_SELECT) == LOW) {
      delay(200);
      isInRFIDPage = false;
      inSubMenu = true;
      cardDetected = false; // SELECT'e basıldığında kart durumunu sıfırla
      cardUID = "";        // UID'yi temizle
      drawSubMenu();
    }

  } else if (isInSDCardPage) {
    static bool sdPageExitPressed = false;
    static bool pageDrawn = false;
    
    // Sadece ilk kez veya gerektiğinde SD kart sayfasını çiz
    if (!pageDrawn) {
      tft.fillScreen(BLACK);
      drawText(str_sd_kart, 15, GREEN, 2, true, 0);
      
      if (sdCardPresent) {
        drawText(str_kart_takili, 45, GREEN, 2, true, 0);
        char sizeBuf[32];
        snprintf_P(sizeBuf, sizeof(sizeBuf), PSTR("%s%d%s"), str_boyut, sdCardSize, str_mb);
        drawText(sizeBuf, 75, WHITE, 1, true, 0);
      } else {
        drawText(str_lutfen_sd, 50, RED, 1, true, 0);
        drawText(str_takin, 70, RED, 1, true, 0);
      }
      
      // Çıkış için çarpı işareti ve yazı
      drawExitBox(true);
      drawText(str_cikis, tft.height() - 15, WHITE, 1, true, 0);
      
      pageDrawn = true;
    }
    
    // Sadece SELECT tuşuna basıldığında çık
    if (digitalRead(BUTTON_SELECT) == LOW && !sdPageExitPressed) {
      sdPageExitPressed = true;
      delay(200);
      isInSDCardPage = false;
      inSubMenu = true;
      pageDrawn = false;  // Sayfa çizim bayrağını sıfırla
      drawSubMenu();
    } else if (digitalRead(BUTTON_SELECT) == HIGH) {
      sdPageExitPressed = false;
    }
    
    delay(100); // Ekranın çok hızlı yenilenmesini önle
  } else if (isInBluetoothPage) {
    handleBluetoothButtons();
    return;
  } else {
    handleButtons();
  }
  
  // Bellek kontrolü (her 1000 döngüde bir)
  static int loopCount = 0;
  if (++loopCount % 1000 == 0) {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 5000) {
      Serial.println("KRITIK: Bellek az! Sistem yeniden başlatılıyor...");
      ESP.restart();
    }
  }
}

void checkRFIDCard() {
  // RFID modülünün başlatılıp başlatılmadığını kontrol et
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    if (cardDetected) {
      // Kart kaldırıldığında UID'yi silme, sadece SELECT ile çıkış yapılsın
      return;
    }
    return;
  }

  // Kart tespit edildiğinde
  cardDetected = true;
  
  // UID'yi oku
  cardUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
    if (i != mfrc522.uid.size - 1) cardUID += ":";
  }
  cardUID.toUpperCase();

  // Kartı durdur
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  // Ekranı temizle ve bilgileri göster
  tft.fillScreen(BLACK);
  drawText(str_rfid, 40, CYAN, 3, true, 0);
  drawText(cardUID, 80, GREEN, 2, true, 0);
  drawText(str_cikis, tft.height() - 20, WHITE, 1, true, 0);
}

void drawRFIDPage() {
  tft.fillScreen(BLACK);
  drawText(str_rfid, 40, CYAN, 3, true, 0);
}

void handleButtons() {
  static int lastSelected = -1;
  unsigned long now = millis();

  bool up = digitalRead(BUTTON_UP) == LOW;
  bool down = digitalRead(BUTTON_DOWN) == LOW;
  bool select = digitalRead(BUTTON_SELECT) == LOW;

  if (now - lastButtonPress > debounceDelay) {
    if (up) {
      if (inSubMenu) {
        subSelectedItem = (subSelectedItem - 1 + subMenuItemCount) % subMenuItemCount;
      } else {
        selectedItem = (selectedItem - 1 + menuItemCount) % menuItemCount;
      }
      lastButtonPress = now;
    }
    else if (down) {
      if (inSubMenu) {
        subSelectedItem = (subSelectedItem + 1) % subMenuItemCount;
      } else {
        selectedItem = (selectedItem + 1) % menuItemCount;
      }
      lastButtonPress = now;
    }
    else if (select) {
      lastButtonPress = now;
      if (!inSubMenu) {
        // --- Menüde Hakkında sayfası kontrolü ---
        if (strcmp_P(menuItems[selectedItem], str_start) == 0) {
          inSubMenu = true;
          subSelectedItem = 0;
          drawSubMenu();
        }
        else if (strcmp_P(menuItems[selectedItem], str_settings) == 0) {
          isInSettingsPage = true;
          inSubMenu = false;
          settingsSelected = 0;
          drawSettingsPage();
          delay(200); // Sayfanın düzgün çizilmesi için bekle
        }
        else if (strcmp_P(menuItems[selectedItem], str_about) == 0) { // <-- yeni eklenen
          isInAboutPage = true;
          drawAboutPage();
        }
        else if (strcmp_P(menuItems[selectedItem], str_exit) == 0) {
          Serial.println("Program terminated");
        }
      } else {
        if (strcmp_P(subMenuItems[subSelectedItem], str_exit) == 0) {
          inSubMenu = false;
          drawMenu();
        } else if (strcmp_P(subMenuItems[subSelectedItem], str_rf_modul) == 0) {
          isInRFModulePage = true;
          inSubMenu = false;
          drawRFModulePage();
          return;
        } else if (strcmp_P(subMenuItems[subSelectedItem], str_rfid) == 0) {
          drawRFIDPage();
          delay(500);
          isInRFIDPage = true;
          tft.fillScreen(BLACK);
        } else if (strcmp_P(subMenuItems[subSelectedItem], str_sd_card) == 0) {
          drawSDCardMenuPage();
          delay(500);
          isInSDCardPage = true;
          tft.fillScreen(BLACK);
          
          // SD kartı başlatmadan önce SPI'ı doğru şekilde ayarla
          SPI.end();
          delay(10);
          SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
          
          // SD kartı başlatmayı dene
          pinMode(SD_CS, OUTPUT);
          digitalWrite(SD_CS, HIGH);
          delay(10);
          
          // SD kartı başlat ve durumunu kontrol et
          sdCardPresent = SD.begin(SD_CS);
          
          if (sdCardPresent) {
            sdCardSize = SD.cardSize() / (1024 * 1024); // MB cinsinden
            Serial.println("SD Kart takili ve basariyla baslatildi");
            Serial.print("Kart boyutu: ");
            Serial.print(sdCardSize);
            Serial.println(" MB");
          } else {
            sdCardSize = 0;
            Serial.println("SD Kart takilmamis veya baslatilamadi");
          }
        } else {
          Serial.print("Submenu Selected: ");
          Serial.println(subMenuItems[subSelectedItem]);
        }
      }
    }
  }

  if (!inSubMenu && selectedItem != lastSelected) {
    drawMenu();
    lastSelected = selectedItem;
  }
  else if (inSubMenu && subSelectedItem != lastSelected) {
    drawSubMenu();
    lastSelected = subSelectedItem;
  }
}

void drawMenuItems(const char* const* items, int itemCount, int selectedItem, bool isSubMenu) {
  tft.fillScreen(BLACK);
  if (!isSubMenu) {
    drawText(str_obsidibyte, 10, GREEN, 2, true, 0);
  }

  int spacing = isSubMenu ? 18 : 20;
  int menuHeight = spacing * itemCount;
  int yStart = isSubMenu ? ((tft.height() - menuHeight) / 2) + 5 : 40;

  for (int i = 0; i < itemCount; i++) {
    int y = yStart + i * spacing;
    tft.setTextSize(isSubMenu ? 1 : 2);
    int16_t x1, y1;
    uint16_t w, h;
    char buffer[32];
    strcpy_P(buffer, (PGM_P)pgm_read_ptr(&items[i]));
    tft.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    int x = (tft.width() - w) / 2;

    if (i == selectedItem) {
      uint16_t frameColor = WHITE;
      if (isSubMenu) {
        if (strcmp_P(buffer, str_exit) == 0) frameColor = RED;
        else if (strcmp_P(buffer, str_rf_modul) == 0) frameColor = GREEN;
        else if (strcmp_P(buffer, str_rfid) == 0) frameColor = CYAN;
        else if (strcmp_P(buffer, str_sd_card) == 0) frameColor = GREEN;
      }
      
      if (isSubMenu) {
        for (int k = 0; k < 2; k++) {
          tft.drawRect(x - 3 - k, y - 3 - k, w + 6 + 2 * k, h + 6 + 2 * k, frameColor);
        }
      } else {
        tft.drawRect(x - 4, y - 2, w + 8, h + 4, WHITE);
      }
      tft.setCursor(x, y);
      tft.setTextColor(WHITE);
      tft.print(buffer);
    } else {
      drawText(buffer, y, WHITE, isSubMenu ? 1 : 2, true, 0);
    }
  }
}

void drawMenu() {
  drawMenuItems(menuItems, menuItemCount, selectedItem, false);
}

void drawSubMenu() {
  drawMenuItems(subMenuItems, subMenuItemCount, subSelectedItem, true);
}

void drawRFModulePage() {
  tft.fillScreen(BLACK);
  drawText(str_rf_modul, 20, GREEN, 2, true, 0);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(str_rf_modul, 0, 0, &x1, &y1, &w, &h);
  int x = (tft.width() - w) / 2;
  int y = 20;

  for (int k = 0; k < 2; k++) {
    tft.drawRect(x - 4 - k, y - 3 - k, w + 8 + 2 * k, h + 6 + 2 * k, GREEN);
  }
}

void drawSettingsPage() {
  tft.fillScreen(BLACK);
  drawText("Ayarlar", 10, GREEN, 2, true, 0);

  const int boxX = 20;
  const int boxY = 40;
  const int boxW = tft.width() - 40;
  const int boxH = 35;
  const int boxSpacing = 15;

  // Bluetooth box
  uint16_t btColor = (settingsSelected == 0) ? GREEN : WHITE;
  tft.drawRect(boxX, boxY, boxW, boxH, btColor);
  drawText("Bluetooth", boxY + 10, btColor, 2, true, 0);

  // WiFi box
  int boxY2 = boxY + boxH + boxSpacing;
  uint16_t wifiColor = wifiEnabled ? GREEN : WHITE;
  if (settingsSelected == 1) wifiColor = GREEN;
  tft.drawRect(boxX, boxY2, boxW, boxH, wifiColor);
  drawText("WiFi", boxY2 + 10, wifiColor, 2, true, 0);

  // Exit box
  drawExitBox(settingsSelected == 2);
  
  delay(100); // Sayfanın düzgün çizilmesi için kısa bekleme
}

// WiFi tarama ekranını göster
void drawWiFiScanningPage() {
    tft.fillScreen(BLACK);
    drawText("WIFI", 20, CYAN, 2, true, 0);
    
    // Animasyonlu tarama metni
    static int dots = 0;
    String scanText = "Aglar Taraniyor";
    for(int i = 0; i < dots; i++) {
        scanText += ".";
    }
    dots = (dots + 1) % 4;
    
    drawText(scanText, 60, WHITE, 1, true, 0);
    drawText("Lutfen Bekleyin", 80, GREEN, 1, true, 0);
}

// WiFi listesi sayfasını çiz
void drawWiFiListPage(int networks) {
    tft.fillScreen(BLACK);
    drawText("WIFI", 5, CYAN, 2, true, 0);
    drawText("Lutfen Bekleyin", 80, GREEN, 1, true, 0);
}

void openWiFiScanAndListPages() {
    // WiFi'yi STA modunda başlat
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Tarama ekranını göster
    drawWiFiScanningPage();
    
    // Güvenli WiFi taraması
    Serial.println("WiFi taraması başlatılıyor...");
    int networks = WiFi.scanNetworks();
    
    // Tarama animasyonu
    for(int i = 0; i < 3; i++) {
        drawWiFiScanningPage();
        delay(1000);
    }
    
    Serial.println("Tarama tamamlandı.");
    
    // Hata kontrolü
    if (networks == -1) {
        Serial.println("WiFi taraması başarısız!");
        drawText("WiFi Tarama Hatasi!", 60, RED, 1, true, 0);
        delay(2000);
        networks = 0;
    } else if (networks == 0) {
        Serial.println("Hiçbir WiFi ağı bulunamadı.");
    } else {
        Serial.print(networks);
        Serial.println(" adet ağ bulundu:");
        
        // Maksimum ağ sayısını sınırla
        if (networks > 20) {
            networks = 20;
            Serial.println("Maksimum 20 ağ gösteriliyor.");
        }
    }

    // Sonuçları göster
    selectedWiFi = 0;
    wifiNetworkCount = networks;
    drawWiFiListPage(networks);
}

// WiFi listesi butonları
void handleWiFiListButtons() {
    static unsigned long lastPress = 0;
    unsigned long now = millis();
    if (now - lastPress < DEBOUNCE_DELAY) return;

    bool up = digitalRead(BUTTON_UP) == LOW;
    bool down = digitalRead(BUTTON_DOWN) == LOW;
    bool select = digitalRead(BUTTON_SELECT) == LOW;

    if (up || down || select) {
        lastPress = now;

        if (up) {
            if (selectedWiFi > 0) {
                selectedWiFi--;
                drawWiFiListPage(wifiNetworkCount);
            }
        }
        else if (down) {
            if (selectedWiFi < wifiNetworkCount - 1) {
                selectedWiFi++;
                drawWiFiListPage(wifiNetworkCount);
            }
        }
        else if (select) {
            isInWiFiPage = false;
            isInSettingsPage = true;
            drawSettingsPage();
            delay(200);
        }
    }
}

// Basit SD kart simgesi çizimi (sadece gösterim amaçlı)
void drawSDCardIcon(int x, int y) {
  // Kutu
  tft.drawRect(x, y, 40, 30, WHITE);
  // Üstte kısa çizgi (kartın üst kısmı)
  tft.drawLine(x + 5, y, x + 35, y, WHITE);
  // Çizgiler (kart detayları)
  for (int i = 0; i < 3; i++) {
    tft.drawLine(x + 10 + i*8, y + 10, x + 10 + i*8, y + 25, WHITE);
  }
}

uint64_t getSDCardSize() {
  uint64_t cardSize = 0;
  uint64_t blocks = SD.cardSize() / 512;
  cardSize = blocks * 512ULL;
  return cardSize;
}

String formatSize(uint64_t bytes) {
  float size = (float)bytes;
  String unit = "B";

  if (size > 1024) {
    size /= 1024;
    unit = "KB";
  }
  if (size > 1024) {
    size /= 1024;
    unit = "MB";
  }
  if (size > 1024) {
    size /= 1024;
    unit = "GB";
  }
  return String(size, 2) + " " + unit;
}

// Bluetooth sembolü çizimi (daha detaylı, orijinaline yakın)
void drawBluetoothSymbol(int x, int y, uint16_t color) {
  // Orta dikey çizgi
  tft.drawLine(x + 12, y + 2, x + 12, y + 28, color);
  // Üst sağ üçgen
  tft.drawLine(x + 12, y + 15, x + 22, y + 7, color);
  tft.drawLine(x + 12, y + 15, x + 22, y + 23, color);
  // Üst sol üçgen
  tft.drawLine(x + 12, y + 2, x + 22, y + 15, color);
  tft.drawLine(x + 12, y + 28, x + 22, y + 15, color);
  // Orta çarpı
  tft.drawLine(x + 12, y + 10, x + 18, y + 15, color);
  tft.drawLine(x + 12, y + 20, x + 18, y + 15, color);
}

void drawExitBox(bool selected) {
  int size = 10;
  int x = tft.width() - size - 2;
  int y = 2;
  uint16_t color = selected ? CYAN : WHITE;
  tft.drawRect(x, y, size, size, color);
  // Çarpı işareti
  tft.drawLine(x + 5, y + 5, x + size - 5, y + size - 5, color);
  tft.drawLine(x + size - 5, y + 5, x + 5, y + size - 5, color);
}

// Hakkında sayfası
void drawAboutPage() {
    tft.fillScreen(BLACK);
    drawText("Obsidibyte", 10, GREEN, 2, true, 0);
    drawText("v1.0.0", 35, CYAN, 1, true, 0);
    drawText("Yapimci: efe38ka", 60, WHITE, 1, true, 0);
    drawText("GitHub: github.com/efe38ka", 85, YELLOW, 1, true, 0);
    drawExitBox(true);
}

void drawText(const char* text, int y, uint16_t color, int size, bool centered = true, int speed = 0) {
  // Implementation of drawText function
}

void drawText(String text, int y, uint16_t color, int size, bool centered = true, int speed = 0) {
  // Implementation of drawText function
}

void drawSDCardMenuPage() {
  // Implementation of drawSDCardMenuPage function
}