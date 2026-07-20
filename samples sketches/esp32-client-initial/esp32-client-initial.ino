#include <NimBLEDevice.h>

static const NimBLEAdvertisedDevice* targetDevice = nullptr;
static bool doConnect = false;

static NimBLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static NimBLEUUID notifSourceUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    Serial.printf("Found device: %s\n", advertisedDevice->toString().c_str());

    std::string mfgData = advertisedDevice->getManufacturerData();
    if (mfgData.length() >= 2 && (uint8_t)mfgData[0] == 0x4C && (uint8_t)mfgData[1] == 0x00) {
      Serial.println("Apple device found! Stopping scan...");
      NimBLEDevice::getScan()->stop();
      targetDevice = advertisedDevice;
      doConnect = true;
    }
  }
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    Serial.println("Connected! Attempting to discover ANCS service...");
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    Serial.printf("Auth complete. Encrypted: %d, Bonded: %d\n",
                  connInfo.isEncrypted(), connInfo.isBonded());
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    Serial.printf("Disconnected! Reason code: %d (0x%02X)\n", reason, reason);
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting BLE client...");

  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEDevice::init("ESP32-Client");

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new ScanCallbacks());
  pScan->setActiveScan(true);
  pScan->start(10, false);
}
static bool isConnecting = false;

void loop() {
  if (doConnect) {
    doConnect = false;
    isConnecting = true;

    NimBLEClient* pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    Serial.println("Connecting...");

    if (pClient->connect(targetDevice)) {
      Serial.println("Connected successfully!");

      NimBLERemoteService* pService = pClient->getService(ancsServiceUUID);
      if (pService) {
        Serial.println("ANCS service found!");
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(notifSourceUUID);
        if (pChar) {
          Serial.println("Notification Source characteristic found - subscribing (should trigger pairing)...");
          pChar->subscribe(true);
          Serial.println("Subscribe requested. Check your phone NOW for a pairing prompt.");
        } else {
          Serial.println("Notification Source characteristic not found.");
        }
      } else {
        Serial.println("ANCS service not found on this device.");
      }
    } else {
      Serial.println("Connection failed");
      NimBLEDevice::deleteClient(pClient);
      isConnecting = false;
    }
  }

  // Only scan again if we're not currently connecting/connected
  if (!doConnect && !isConnecting) {
    Serial.println("Scanning again...");
    NimBLEDevice::getScan()->start(5, false);
  }

  delay(1000);
}

