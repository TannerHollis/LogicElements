#include "UARTTx.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR
#include <cstdio>
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif

/**
 * @brief Construct a new UARTTx object.
 *
 * @param bufferSize The size of the circular buffer.
 */
UARTTx::UARTTx(uint16_t bufferSize)
    : uBufferSize(bufferSize), uWrite(0), uRead(0), bBufferFull(false), dataReadyToSend(0)
{
    this->_buffer = new char[bufferSize];
}

/**
 * @brief Destroy the UARTTx object.
 */
UARTTx::~UARTTx() {
    delete[] this->_buffer;
}

/**
 * @brief Write data to the UART transmission buffer.
 *
 * @param data Pointer to the data to be written.
 * @param len Length of the data to be written.
 */
void UARTTx::Write(const char* data, uint16_t len) {
    if (len <= 0)
        return;

    uint16_t remaining = len;
    uint16_t chunk;

    while (remaining > 0) {
        chunk = (this->uWrite + remaining < this->uBufferSize) ? remaining : this->uBufferSize - this->uWrite;

        memcpy(&this->_buffer[this->uWrite], data, chunk);
        this->AdvancePointer(this->uWrite, chunk);
        data += chunk;
        remaining -= chunk;
    }

    this->bBufferFull = (this->uWrite == this->uRead);
    this->UpdateDataReadyToSend();

    this->SendNextData();
}

/**
 * @brief Check if the buffer is full.
 *
 * @return true if the buffer is full, false otherwise.
 */
bool UARTTx::IsBufferFull() const {
    return this->bBufferFull;
}

/**
 * @brief Get the available space in the buffer.
 *
 * @return The available space in the buffer.
 */
uint16_t UARTTx::AvailableSpace() const {
    if (this->bBufferFull) {
        return 0;
    }

    if (this->uWrite >= this->uRead) {
        return this->uBufferSize - (this->uWrite - this->uRead);
    }
    else {
        return this->uRead - this->uWrite;
    }
}

/**
 * @brief Check if there is enough space in the buffer for the given length.
 *
 * @param len The length to check.
 * @return true if there is enough space, false otherwise.
 */
bool UARTTx::HasSpace(uint16_t len) const {
    return this->AvailableSpace() >= len;
}

/**
 * @brief Get the amount of data ready to send.
 *
 * @return The amount of data ready to send.
 */
uint16_t UARTTx::DataReadyToSend() const {
    return this->dataReadyToSend;
}

/**
 * @brief Send the next data ready in the buffer.
 */
void UARTTx::SendNextData() {
    if (this->dataReadyToSend > 0 && this->UARTTx_isUARTReady() && !this->UARTTx_isUARTBusy()) {
        this->UARTTx_writeUART(&this->_buffer[this->uRead], this->dataReadyToSend);
        this->AdvancePointer(this->uRead, this->dataReadyToSend);
        this->UpdateDataReadyToSend();
    }
}

/**
 * @brief Advance a pointer by a given length, wrapping around if necessary.
 *
 * @param pointer The pointer to advance.
 * @param len The length to advance by.
 */
inline void UARTTx::AdvancePointer(uint16_t& pointer, uint16_t len) {
    pointer = (pointer + len) % this->uBufferSize;
}

/**
 * @brief Update the amount of data ready to send.
 */
inline void UARTTx::UpdateDataReadyToSend() {
    if (this->uWrite >= this->uRead) {
        this->dataReadyToSend = this->uWrite - this->uRead;
    }
    else {
        this->dataReadyToSend = this->uBufferSize - this->uRead;
    }
}

/**
 * @brief Weak function for starting UART transmission.
 *        Users should provide their own implementation.
 *
 * @param data Pointer to the data buffer to transmit.
 * @param len Length of the data to transmit.
 */
WEAK_ATTR void UARTTx::UARTTx_writeUART(const char* data, uint16_t len) {
    // Default implementation, can be overridden
    // For demonstration, print to standard output
    //printf("%.*s", len, data);
}

/**
 * @brief Weak function for checking if UART is ready.
 *        Users should provide their own implementation.
 *
 * @return true if the UART is ready, false otherwise.
 */
WEAK_ATTR bool UARTTx::UARTTx_isUARTReady() {
    // Default implementation, always returns true
    return true;
}

/**
 * @brief Weak function for checking if UART is not busy.
 *        Users should provide their own implementation.
 *
 * @return true if the UART is not busy, false otherwise.
 */
WEAK_ATTR bool UARTTx::UARTTx_isUARTBusy() {
    // Default implementation, always returns false
    return false;
}
