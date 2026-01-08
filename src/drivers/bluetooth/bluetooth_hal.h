#pragma once
#include <functional>
#include <string>
#include <vector>
#include <NimBLEDevice.h>

class NimBLEServer;
class NimBLECharacteristic;

namespace PlantMonitor {
namespace Drivers {

    /*!
    * \file bluetooth_hal.h   
    * \brief BLE UART-like HAL built on top of NimBLE-Arduino.
    *
    * \details
    * This HAL exposes a minimal "UART over BLE" interface:
    * - A custom BLE service with RX (write) and TX (notify) characteristics
    * - A receive callback delivering raw bytes to the upper layer
    * - Simple send helpers for raw bytes and text
    *
    * The implementation uses a Nordic UART Service (NUS)-style UUID set
    * (commonly used by many BLE UART apps).
    *
    * \note Message reception is event-driven: the RX handler is invoked when the
    *       RX characteristic is written by the peer (phone/app).
    * \note This class is designed as a lightweight HAL; higher-level protocols
    *       (framing, JSON, commands, ACKs, etc.) should live above it.
    */
    class BleUartHal {
    
        public:
            BleUartHal();
            ~BleUartHal();

            using Bytes = std::vector<uint8_t>;
            using RxHandler = std::function<void(const Bytes&)>;

            /**
             * \brief Starts advertising, meaning it becomes visible by others devices
             * \param deviceName This is the name seen by other devices
             * \return
             */
            bool begin(const char* deviceName);

            /**
             *  \warning NOT IMPLEMENTED
             *  \brief Should end the connection peacefully and managing potential issues
             */
            void end();

            /**
             * \brief Checks if the device is connected 
             * \return Connection status 
             */
            bool isConnected() const;

            /**
             * \brief Simple function that handles message sending
             * \param data a pointer to data
             * \param len size of data
             * \return Success of message's delivery 
             */
            bool send(const uint8_t* data, size_t len);

            /**
             * \brief A wrapper for strings of send()
             */
            bool sendText(const std::string& text);

            /**
             * \brief Setter for a function that should handle RX receiving
             * \param cb void function(uint8_t* data)
             */
            void setRxHandler(RxHandler cb);
            
            // Internal event handlers called by callbacks

            /**
             * \brief Connected status turned on
             */
            void onConnect_();

            /**
             * \brief Connected status turned off, going back to advertising
             */
            void onDisconnect_();

            /**
             * \brief Handles receiving data safely
             * \param data characteristic value
             * \param len size of data
             */
            void onRxWrite_(const uint8_t* data, size_t len);

            /**
             * \brief Starts advertising (making him visible and ready for a connection)
             */
            void startAdvertising_();
    
        private:
            // Internal state
            bool connected_ = false;
            RxHandler rxHandler_;

            // Forward declare NimBLE types (keep NimBLE includes in .cpp)
            ::NimBLEServer* server_ = nullptr;
            ::NimBLECharacteristic* txChar_ = nullptr;

            // Prevent copying the object
            BleUartHal(const BleUartHal&) = delete;
            BleUartHal& operator=(const BleUartHal&) = delete;
    };


} // namespace Drivers
} // namespace PlantMonitor

