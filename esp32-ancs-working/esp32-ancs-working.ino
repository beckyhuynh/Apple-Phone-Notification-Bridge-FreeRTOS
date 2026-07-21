#include <NimBLEDevice.h>

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

// when data source sends back notification message
// length is in bytes
// pData is pointer to array of bytes
static void dataSourceNotifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify){
  for (int i = 0; i < length; i++) {
    if (i > 7) {
      Serial.print((char)pData[i]);
    }
  }
  Serial.println();
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
    NimBLEDevice::startAdvertising(); // advertise again
  }

  void onAuthenticationComplete(NimBLEConnInfo & connInfo) override{
    Serial.printf("Auth complete. Encrypted: %d, Bonded: %d\n",
                  connInfo.isEncrypted(), connInfo.isBonded());
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Step 1: Serial started");

  NimBLEDevice::setSecurityAuth(true, false, true);   // bonding=true, mitm=false (Just Works), secureConnections=true
  Serial.println("Step 2: setSecurityAuth done");

  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);  // Just Works - proven working today
  Serial.println("Step 3: setSecurityIOCap done");

  NimBLEDevice::init("ESP32-ANCS");
  Serial.println("Step 4: init done");

  NimBLEServer* pServer = NimBLEDevice::createServer();
  Serial.println("Step 5: createServer done");

  pServer->setCallbacks(new ServerCallbacks());
  Serial.println("Step 6: setCallbacks done");

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  Serial.println("Step 7: getAdvertising done");

  pAdvertising -> setName("ESP32-ANCS");
  pAdvertising->addServiceUUID(ancsServiceUUID);
  pAdvertising->enableScanResponse(true);

  pAdvertising->start();
  Serial.println("Step 8: advertising started successfully!");
  Serial.println("Setup Complete, Advertising as ESP32-ANCS");
}

unsigned long lastBeat= 0;

void loop() {
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
      return;
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
    pControlPointChar->writeValue(val, 6, true);  // request Title (AttributeID 0x01)
    val[5] = 0x01;
    pControlPointChar->writeValue(val, 8, true);
    val[5] = 0x03;  // request Message (AttributeID 0x03)
    pControlPointChar->writeValue(val, 8, true);
  }
  delay(200);
}
