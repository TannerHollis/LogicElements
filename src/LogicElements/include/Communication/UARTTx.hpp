#pragma once

#include <cstdint>
#include <cstring>

namespace LogicElements {

/**
 * @brief UARTTx class implements a circular buffer for UART transmission.
 */
class UARTTx {
public:
    /**
     * @brief Construct a new UARTTx object.
     *
     * @param bufferSize The size of the circular buffer.
     */
    UARTTx(uint16_t bufferSize);

    /**
     * @brief Destroy the UARTTx object.
     */
    ~UARTTx();

    /**
     * @brief Write data to the UART transmission buffer.
     *
     * @param data Pointer to the data to be written.
     * @param len Length of the data to be written.
     */
    void Write(const char* data, uint16_t len);

    /**
     * @brief Check if the buffer is full.
     *
     * @return true if the buffer is full, false otherwise.
     */
    bool IsBufferFull() const;

    /**
     * @brief Get the available space in the buffer.
     *
     * @return The available space in the buffer.
     */
    uint16_t AvailableSpace() const;

    /**
     * @brief Check if there is enough space in the buffer for the given length.
     *
     * @param len The length to check.
     * @return true if there is enough space, false otherwise.
     */
    bool HasSpace(uint16_t len) const;

    /**
     * @brief Get the amount of data ready to send.
     *
     * @return The amount of data ready to send.
     */
    uint16_t DataReadyToSend() const;

    /**
     * @brief Send the next data ready in the buffer.
     */
    void SendNextData();

    /**
     * @brief Weak function declaration for starting UART transmission.
     *        Users should provide their own implementation.
     *
     * @param data Pointer to the data buffer to transmit.
     * @param length Length of the data to transmit.
     */
    void UARTTx_writeUART(const char* data, uint16_t len);

    /**
     * @brief Weak function declaration for checking if UART is ready.
     *        Users should provide their own implementation.
     *
     * @return true if the UART is ready, false otherwise.
     */
    bool UARTTx_isUARTReady();

    /**
     * @brief Weak function declaration for checking if UART is not busy.
     *        Users should provide their own implementation.
     *
     * @return true if the UART is not busy, false otherwise.
     */
    bool UARTTx_isUARTBusy();

private:
    /**
     * @brief Advance a pointer by a given length, wrapping around if necessary.
     *
     * @param pointer The pointer to advance.
     * @param len The length to advance by.
     */
    inline void AdvancePointer(uint16_t& pointer, uint16_t len);

    /**
     * @brief Update the amount of data ready to send.
     */
    inline void UpdateDataReadyToSend();

    char* _buffer; ///< The circular buffer.
    uint16_t uBufferSize; ///< The size of the buffer.
    uint16_t uWrite; ///< The write pointer.
    uint16_t uRead; ///< The read pointer.
    bool bBufferFull; ///< Flag indicating if the buffer is full.
    uint16_t dataReadyToSend; ///< The amount of data ready to send.
};

} // namespace LogicElements
