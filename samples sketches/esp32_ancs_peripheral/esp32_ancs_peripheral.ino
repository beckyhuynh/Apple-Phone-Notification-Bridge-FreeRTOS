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

// --- Callback: fires when Data Source sends back the actual notification text ---
static void dataSourceNotifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Data Source: ");
  for (int i = 0; i < length; i++) {
    if (i > 7) {
      Serial.print((char)pData[i]);   // after byte 7, it's readable text
    } else {
      Serial.printf("%02X ", pData[i]);  // header bytes, print as hex
    }
  }
  Serial.println();
}

// --- Callback: fires the moment a new/updated/removed notification event happens ---
static void notificationSourceCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (pData[0] == 0) {
    Serial.println("New notification!");
    latestMessageID[0] = pData[4];
    latestMessageID[1] = pData[5];
    latestMessageID[2] = pData[6];
    latestMessageID[3] = pData[7];

    const char* categories[] = {
      "Other", "Incoming Call", "Missed Call", "Voicemail", "Social",
      "Schedule", "Email", "News", "Health", "Business", "Location", "Entertainment"
    };
    if (pData[2] < 12) {
      Serial.printf("Category: %s\n", categories[pData[2]]);
    }
  } else if (pData[0] == 1) {
    Serial.println("Notification Modified!");
  } else if (pData[0] == 2) {
    Serial.println("Notification Removed!");
  }
  pendingNotification = true;
}

// --- Server callbacks: handles connect/disconnect on the peripheral side ---
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    // logs the connecting device's address
    Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());
    // KEY TRICK: wrap the EXISTING peripheral connection in a client object,
    // instead of creating a brand new one (which fails with "connection already exists").
    pClient = pServer->getClient(connInfo);
    needsDiscovery = true;
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("Client disconnected, reason: %d. Restarting advertising...\n", reason);
    pClient = nullptr;
    needsDiscovery = false;
    NimBLEDevice::startAdvertising();
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
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

  // Advertise the ANCS service UUID directly - helps iOS recognize this as
  // a legitimate notification-consuming accessory.
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  Serial.println("Step 7: getAdvertising done");

  pAdvertising->setName("ESP32-ANCS");
  Serial.println("Step 8: setName done");

  pAdvertising->addServiceUUID(ancsServiceUUID);
  Serial.println("Step 9: addServiceUUID done");

  pAdvertising->enableScanResponse(true);
  Serial.println("Step 10: enableScanResponse done");

  pAdvertising->start();
  Serial.println("Step 11: advertising started successfully!");

  Serial.println("=== SETUP COMPLETE - Advertising as ESP32-ANCS ===");
  Serial.println("Go to iPhone Settings > Bluetooth and look for ESP32-ANCS now.");
}

unsigned long lastHeartbeat = 0;

void loop() {
  // Heartbeat every 5 seconds so you can see the board is alive and what state it's in
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    if (pClient == nullptr) {
      Serial.println("[heartbeat] No client connected yet. Still advertising.");
    } else if (!pClient->isConnected()) {
      Serial.println("[heartbeat] Client object exists but not connected.");
    } else if (needsDiscovery) {
      Serial.println("[heartbeat] Connected, discovery pending...");
    } else {
      Serial.println("[heartbeat] Connected and subscribed. Waiting for notifications.");
    }
  }

  if (needsDiscovery && pClient != nullptr && pClient->isConnected()) {
    needsDiscovery = false;
    Serial.println("Discovering ANCS service...");

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

    uint8_t val[8] = {0x00, latestMessageID[0], latestMessageID[1], latestMessageID[2], latestMessageID[3], 0x00, 0x00, 0x10};
    pControlPointChar->writeValue(val, 6, true);  // request Title (AttributeID 0x01)
    val[5] = 0x01;
    pControlPointChar->writeValue(val, 8, true);
    val[5] = 0x03;  // request Message (AttributeID 0x03)
    pControlPointChar->writeValue(val, 8, true);
  }

  delay(200);
}
