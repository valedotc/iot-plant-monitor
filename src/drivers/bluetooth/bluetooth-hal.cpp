#include "bluetooth-hal.h"


// --- UUIDs (keep them stable) ---
static const NimBLEUUID SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const NimBLEUUID RX_UUID     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // phone -> ESP32 (write)
static const NimBLEUUID TX_UUID     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // ESP32 -> phone (notify)


namespace PlantMonitor{
namespace Drivers{

// -----------------------------
// Callback implementations
// -----------------------------

class ServerCallbacksImpl : public NimBLEServerCallbacks {
public:
  explicit ServerCallbacksImpl(BleUartHal* owner) : owner_(owner) {}
  void onConnect(NimBLEServer*) override {
    owner_->onConnect_();
  }
  void onDisconnect(NimBLEServer*) override {
    owner_->onDisconnect_();
  }
private:
  BleUartHal* owner_;
};

class RxCallbacksImpl : public NimBLECharacteristicCallbacks {
public:
  explicit RxCallbacksImpl(BleUartHal* owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* ch) override {
    std::string v = ch->getValue();
    if (v.empty()) return;
    owner_->onRxWrite_(reinterpret_cast<const uint8_t*>(v.data()), v.size());
  }
private:
  BleUartHal* owner_;
};

// -----------------------------
// BleUartHal public API
// -----------------------------

BleUartHal::BleUartHal(){}


BleUartHal::~BleUartHal(){
  this->end();
}


bool BleUartHal::begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);               // max TX power
  NimBLEDevice::setSecurityAuth(false, false, false);   // no bonding for now

  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(new ServerCallbacksImpl(this));

  NimBLEService* svc = server_->createService(SERVICE_UUID);

  // TX characteristic (notify)
  txChar_ = svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);

  // RX characteristic (write / write no response)
  NimBLECharacteristic* rxChar =
      svc->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacksImpl(this));

  svc->start();

  
  return true;
}

void BleUartHal::end() {
  // Many projects never need to deinit BLE.
  // If you do, be careful: deinit affects re-init and memory.
  if(server_) {
    server_->setCallbacks(nullptr);
  }
  NimBLEDevice::deinit();
}

bool BleUartHal::isConnected() const {
  return connected_;
}

bool BleUartHal::send(const uint8_t* data, size_t len) {
  if (!connected_ || txChar_ == nullptr || data == nullptr || len == 0){
    Serial.printf("[BLE] maybe the device is not connected");
    return false;
  }
  // NimBLECharacteristic takes std::string; safe for binary too
  std::string payload(reinterpret_cast<const char*>(data), len);
  txChar_->setValue(payload);
  txChar_->notify();
  return true;
}

bool BleUartHal::sendText(const std::string& text) {
  return send(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

bool BleUartHal::sendChunked(const uint8_t* data, size_t len, size_t chunkSize) {
  if (!connected_ || txChar_ == nullptr || data == nullptr || len == 0) {
    Serial.println("[BLE] sendChunked: not connected or invalid data");
    return false;
  }

  size_t offset = 0;
  int chunkNum = 0;

  while (offset < len) {
    size_t toSend = (len - offset > chunkSize) ? chunkSize : (len - offset);

    std::string payload(reinterpret_cast<const char*>(data + offset), toSend);
    txChar_->setValue(payload);
    txChar_->notify();

    offset += toSend;
    chunkNum++;

    // Small delay between chunks to let BLE stack process
    if (offset < len) {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }

  Serial.printf("[BLE] Sent %d bytes in %d chunks\n", len, chunkNum);
  return true;
}

bool BleUartHal::sendTextChunked(const std::string& text) {
  return sendChunked(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

void BleUartHal::setRxHandler(RxHandler cb) {
  rxHandler_ = std::move(cb);
}

// -----------------------------
// BleUartHal internal methods
// -----------------------------

bool BleUartHal::startAdvertising_() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);

  NimBLEAdvertisementData advData;
  uint8_t mfgData[] = { 0x47, 0xE9, 0xA7, 0x3B, 0x01 };
  advData.setManufacturerData(std::string((char*) mfgData , sizeof(mfgData)));
  adv->setAdvertisementData(advData);

  // same tuning as your function: formula -> x * 0.625ms
  adv->setMinInterval(32); // 20ms 
  adv->setMaxInterval(64); // 40ms

  return adv->start();
}

void BleUartHal::setAutoRestartingADV(bool enable){
  autoRestartAdv = enable;
}

void BleUartHal::onConnect_() {
  connected_ = true;
  Serial.println("[BLE] connected");
}

void BleUartHal::onDisconnect_() {
  connected_ = false;
  if(autoRestartAdv){
    Serial.println("[BLE] disconnected. Restarting advertising...");
    startAdvertising_(); // keeps your advertising config consistent
  }
}

void BleUartHal::onRxWrite_(const uint8_t* data, size_t len) {
  // Debug-friendly: safe for binary (doesn't assume null-terminated)
  Serial.print("RX from phone (");
  Serial.print(len);
  Serial.println(" bytes):");
  Serial.write(data, len);
  Serial.println();

  // 1) Deliver raw bytes to firmware/application layer
  if (rxHandler_) {
    Bytes copy(data, data + len);
    rxHandler_(copy);
  }

}


} // namespace Drivers
} // namespace PlantMonitor
