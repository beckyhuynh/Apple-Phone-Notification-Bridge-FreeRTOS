#include <NimBLEDevice.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class ServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("Client disconnected, reason: %d. Restarting advertising...\n", reason);
    NimBLEDevice::startAdvertising();
  }

  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.println("Client connected (link not yet necessarily encrypted).");
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    Serial.printf("Auth complete. Encrypted: %d, Bonded: %d\n",
                  connInfo.isEncrypted(), connInfo.isBonded());
  }
};

void setup() {
    // Initialize the primary serial interface for debugging output
    Serial.begin(115200);
    Serial.println("Starting NimBLE Server Configuration...");


    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    // Phase 1: Initialize the BLE stack with a unique device name
    NimBLEDevice::init("MyESP32");

    // Phase 2: Instantiate the Server and create the Service
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Phase 3: Create the Characteristic and define its properties
    NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC
    );

    // Assign an initial payload to the characteristic
    pCharacteristic->setValue("Hello World!");

    // Phase 4: Start the service to make it accessible
    pService->start();

    // Phase 5: Configure and initiate advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("MyESP32");
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    Serial.println("NimBLE Server is successfully advertising.");
}

void loop() {
    // The FreeRTOS scheduler handles BLE tasks in the background; 
    // the main loop can remain empty or handle other sensor logic.
    delay(2000);
}
