// ble related task on core 0
// display task on core 1
#include <NimBLEDevice.h>

// touch screen stuff
#include <SPI.h>
#include <TFT_eSPI.h>
// #include <XPT2046_Touchscreen.h>

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

#define SCREEN_HEIGHT 320
#define SCREEN_WIDTH 240
#define FONT_SIZE 1

// ANCS service and characteristic UUIDs (Apple-defined, public spec)
static NimBLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static NimBLEUUID notifSourceUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static NimBLEUUID controlPointUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static NimBLEUUID dataSourceUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

static NimBLEClient* pClient = nullptr;
static NimBLERemoteCharacteristic* pControlPointChar = nullptr;
static bool needsDiscovery = false;
static bool pendingNotification = false;
uint8_t latestMessageID[4];

// struct for display task
struct Notification{
  char appName[32];
  char title[64];
  char message[128];
  uint8_t uid[4];
  // bool isCleared; // just for rendering
};

// buffer data structure
#define MAX_NOTIFS 5 // also the queue size
Notification notifList[MAX_NOTIFS];
int notifCount = 0; // how many slots of notifList actually have non garbage data to render
int notifHead = 0; // where to insert the next data for wrap around buffer

TFT_eSPI tft = TFT_eSPI();
// Touchscreen coordinates
int x, y, z;
// code for touchscreen check here

QueueHandle_t notifQueue = NULL;


static Notification assemblingNotif = {};   // persists across calls, zero-initialized
static uint8_t attrsReceived = 0;           // bitmask/counter tracking which fields arrived

// when data source sends back notification message
// length is in bytes
// pData is pointer to array of bytes
static void dataSourceNotifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
   // read the header byte for info 
  // byte 5 is attribute id(title or message or app name) 
  // byte 6-7 is 16 bit integer for how many bytes long the text string is 
  uint16_t attrLen = pData[6] | (pData[7] << 8);
  memcpy(assemblingNotif.uid, &pData[0], 4);

  if (pData[5] == 0x00) {
    if (attrLen >= sizeof(assemblingNotif.appName)) attrLen = sizeof(assemblingNotif.appName) - 1;
    memcpy(assemblingNotif.appName, &pData[8], attrLen);
    assemblingNotif.appName[attrLen] = '\0';
    attrsReceived |= 0x01;
  } 
  
  else if (pData[5] == 0x01) {
    if (attrLen >= sizeof(assemblingNotif.title)) attrLen = sizeof(assemblingNotif.title) - 1;
    memcpy(assemblingNotif.title, &pData[8], attrLen);
    assemblingNotif.title[attrLen] = '\0';
    attrsReceived |= 0x02;
  } 
  
  else if (pData[5] == 0x03) {
    if (attrLen >= sizeof(assemblingNotif.message)) attrLen = sizeof(assemblingNotif.message) - 1;
    memcpy(assemblingNotif.message, &pData[8], attrLen);
    assemblingNotif.message[attrLen] = '\0';
    attrsReceived |= 0x04;
  }

  // Only send to queue once all three attributes have arrived
  if (attrsReceived == 0x07) {
    xQueueSend(notifQueue, &assemblingNotif, portMAX_DELAY);
    assemblingNotif = {};   // reset for the next notification
    attrsReceived = 0;
  }

   // for (int i = 0; i < length; i++) {
  //   if (i > 7) {
      
  //     // Serial.print((char)pData[i]);
  //   {
  // }
  // Serial.println();
}

// when new/updated/removed notification event happens
static void notificationSourceCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify){
  if (pData[0] == 0){
    Serial.println("New notification!");
    // save the notification id for later
    latestMessageID[0] = pData[4];
    latestMessageID[1] = pData[5];
    latestMessageID[2] = pData[6];
    latestMessageID[3] = pData[7];

    const char* categories[] = {
      "Other", "Incoming Call", "Missed Call", "Voicemail", "Social",
      "Schedule", "Email", "News", "Health", "Business", "Location", "Entertainment"
    };

    if (pData[2] < (sizeof(categories) / sizeof(categories[0]))) {
      Serial.printf("Category: %s\n", categories[pData[2]]);
    }
  }

  else if (pData[0] == 1) Serial.println("Notification Modified!");
  else if (pData[0] == 2) Serial.println("Notification Removed!");

  pendingNotification = true;
}

// Server callbacks, for connect/disconnect
class ServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());

    // as soon as connect, switch esp32 to client
    pClient = pServer -> getClient(connInfo);

    // set discovery, need to find out the services/characteristics
    needsDiscovery = true;
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("Client disconnected, reason: %d. Restarting advertising...\n", reason);
    pClient = nullptr;
    needsDiscovery = false;
    pendingNotification = false;
    assemblingNotif = {};
    attrsReceived = 0;
    NimBLEDevice::startAdvertising(); // advertise again
  }

  void onAuthenticationComplete(NimBLEConnInfo & connInfo) override{
    Serial.printf("Auth complete. Encrypted: %d, Bonded: %d\n",
                  connInfo.isEncrypted(), connInfo.isBonded());
  }
};

unsigned long lastBeat= 0;

void BLETask(void *parameter){
  // setup server to advertise
  NimBLEServer* pServer = NimBLEDevice::createServer();
  Serial.println("step 1: createServer done");

  pServer->setCallbacks(new ServerCallbacks());
  Serial.println("Step 2: setCallbacks done");

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  Serial.println("Step 3: getAdvertising done");

  pAdvertising -> setName("ESP32-ANCS");
  pAdvertising->addServiceUUID(ancsServiceUUID);
  pAdvertising->enableScanResponse(true);

  pAdvertising->start();
  Serial.println("Step 4: advertising started successfully!");
  Serial.println("Setup Complete, Advertising as ESP32-ANCS");

  // connection and discovery
  for (;;) {
    // check every 5 seconds to see if board is still alive
    if (millis() - lastBeat > 5000) {
      lastBeat = millis();
      if (pClient == nullptr) {
        Serial.println("No Client connected yet. still advertising");
      }
      
      else if (!pClient->isConnected()) {
        Serial.println("Client object exists but not connected.");
      } 
      
      else if (needsDiscovery) {
        Serial.println("Connected, discovery pending...");
      } 
      
      else {
        Serial.println("Connected and subscribed. Waiting for notifications.");
      }
    }

    if (needsDiscovery && pClient != nullptr && pClient -> isConnected()) {
      needsDiscovery = false;
      Serial.println("Discovering ANCS service");

      NimBLERemoteService* pService = pClient->getService(ancsServiceUUID);
      if (pService == nullptr) {
        Serial.println("ANCS service not found.");
        continue;
      }
      Serial.println("ANCS service found!");

      NimBLERemoteCharacteristic* pNotifSourceChar = pService->getCharacteristic(notifSourceUUID);
      pControlPointChar = pService->getCharacteristic(controlPointUUID);
      NimBLERemoteCharacteristic* pDataSourceChar = pService->getCharacteristic(dataSourceUUID);

      if (pNotifSourceChar && pControlPointChar && pDataSourceChar) {
        Serial.println("All ANCS characteristics found. Subscribing...");
        pDataSourceChar->subscribe(true, dataSourceNotifyCallback); // call these functions whenever these characteristics change
        pNotifSourceChar->subscribe(true, notificationSourceCallback);
        Serial.println("Subscribed! Waiting for notifications...");
      } else {
        Serial.println("One or more ANCS characteristics missing.");
      }
    }

    // data source handling
    if (pendingNotification && pControlPointChar != nullptr) {
      pendingNotification = false;
      Serial.println("Requesting notification details...");

      // sending command packets
      // val[0] = 0x00 means to fetch details abt a notif
      // then attach the notif's id
      // then the attribute id to fetch
      uint8_t val[8] = {0x00, latestMessageID[0], latestMessageID[1], latestMessageID[2], latestMessageID[3], 0x00, 0x00, 0x10};
      
      // 0x00- app identifier, 0x01 is title, 0x03 is message- for attribute id
      // true means with response(wait for acknowledgement from phone)
      pControlPointChar->writeValue(val, 6, true);  
      val[5] = 0x01; // request Title (AttributeID 0x01)
      pControlPointChar->writeValue(val, 8, true);
      val[5] = 0x03;  // request Message (AttributeID 0x03)
      pControlPointChar->writeValue(val, 8, true);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS); 
  }
}

// Fills as much of text as fits within maxWidth, returns leftover text via remaining
String fitLine(String text, int maxWidth, int font, String &remaining) {
  int fitChars = text.length();
  while (fitChars > 0 && tft.textWidth(text.substring(0, fitChars), font) > maxWidth) {
    fitChars--;
  }
  // back off to the last space so we don't cut mid-word
  int lastSpace = text.substring(0, fitChars).lastIndexOf(' ');
  if (lastSpace > 0 && fitChars < text.length()) fitChars = lastSpace;

  remaining = text.substring(fitChars);
  remaining.trim();
  return text.substring(0, fitChars);
}

void wrapTextTwoLines(String text, int maxWidth, int font, String &line1, String &line2) {
  line1 = fitLine(text, maxWidth, font, text);
  line2 = fitLine(text, maxWidth - tft.textWidth("...", font), font, text);
  if (text.length() > 0) line2 += "...";
}

void DisplayTask(void *parameter){
  tft.init();
  tft.setRotation(2);

  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;

  for (;;) {
    Notification notifItem;
    // if receives a new notification
    // should rerender everytime theres a new notification
    if (xQueueReceive(notifQueue, &notifItem, portMAX_DELAY)){
      notifList[notifHead] = notifItem;
      notifHead = (notifHead + 1) % MAX_NOTIFS;
      if (notifCount != MAX_NOTIFS) {
        notifCount ++;
      }
      // draw all the notifications stored in the buffer wrap list
      // render notifList
      
      for (int i = 0; i < notifCount; i++) {
        if (i == 0) {
          tft.fillScreen(TFT_WHITE);
          tft.setTextColor(TFT_BLACK, TFT_WHITE);
        }
        
        // for every notification 
        // display the app name on left, author name on right same line
        // message at the bottom (move down one line)
        // move the drawn location up each time, (newest notif at the top)
        // if the length cuts out, add the 3 dots...
        // line height is 20

        int currY = SCREEN_HEIGHT - i*65 - 30;
        // tft.drawString(notifList[i].message, 10, currY, FONT_SIZE);

        String line1, line2;
        wrapTextTwoLines(String(notifList[i].message), SCREEN_WIDTH - 20, FONT_SIZE, line1, line2);

        tft.drawString(line1, 10, currY, FONT_SIZE);
        if (line2.length() > 0) {
          tft.drawString(line2, 10, currY + 10, FONT_SIZE);
        }

        tft.drawString(notifList[i].appName, 10, currY-15, FONT_SIZE);
        tft.drawString(notifList[i].title, SCREEN_WIDTH-60, currY-15, FONT_SIZE);
      }

      // tft.drawString("TESTINGGG", 10, SCREEN_HEIGHT-20, FONT_SIZE);

    }

  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Serial started");

  NimBLEDevice::setSecurityAuth(true, false, true);   // bonding=true, mitm=false (Just Works), secureConnections=true
  Serial.println("Step 0: setSecurityAuth done");

  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);  // Just Works - proven working today
  Serial.println("Step 0.1: setSecurityIOCap done");

  NimBLEDevice::init("ESP32-ANCS");
  Serial.println("Step 0.2: init done");

  // create queue
  notifQueue = xQueueCreate(MAX_NOTIFS, sizeof(Notification));
  if (notifQueue == NULL) {
    Serial.println("failed to create queue");
    while(1);
  }

  xTaskCreatePinnedToCore(
    BLETask,
    "BLETask",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    DisplayTask,
    "DisplayTask",
    4000,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
}
