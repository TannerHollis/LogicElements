#pragma once

#include "le_Time.hpp"

#define SIGNAL_LEN 100
#define VALID_PULSES 4

/**
 * @brief Class representing an IRIG-B time signal decoder.
 */
class le_IRIGB : public le_Time
{
public:
    /**
     * @brief Constructor to initialize the le_IRIGB object with specified timer frequencies and frame tolerance.
     * @param irigTimerFreq The frequency of the IRIG timer.
     * @param irigFrameTolerance The tolerance for IRIG frame timing.
     * @param updateTimerFreq The frequency of the update timer.
     */
    le_IRIGB(uint32_t irigTimerFreq, float irigFrameTolerance, uint32_t updateTimerFreq);

    /**
     * @brief Destructor to clean up the le_IRIGB object.
     */
    ~le_IRIGB();

    /**
     * @brief Decodes a buffer of IRIG-B signals.
     * @param buffer The buffer containing the IRIG-B signal data.
     * @param length The length of the buffer.
     */
    void Decode(uint16_t* buffer, uint16_t length);

    /**
     * @brief Gets the current drift value.
     * @return The current drift value in microseconds.
     */
    int32_t GetDrift() const;

private:
    /**
     * @brief Enumeration for the types of IRIG-B frames.
     */
    enum class le_IRIGB_Frame : int8_t
    {
        BIT_0 = 0,
        BIT_1 = 1,
        FRAME_REF = 10,
        FRAME_INVALID = -1
    };

    /**
     * @brief Decodes a single IRIG-B frame from a raw signal value.
     * @param frame The raw signal value representing a frame.
     * @return The decoded IRIG-B frame.
     */
    inline le_IRIGB_Frame DecodeFrame(uint16_t frame);

    /**
     * @brief Decodes multiple IRIG-B frames and processes them.
     * @param frames The array of IRIG-B frames to decode.
     * @param offset The offset at which to start decoding.
     */
    void DecodeFrames(le_IRIGB_Frame* frames, uint16_t offset);

    /**
     * @brief Converts a Binary-Coded Decimal (BCD) value to a decimal value.
     * @param buffer The buffer containing the BCD value.
     * @param start The starting index of the BCD value in the buffer.
     * @param stop The stopping index of the BCD value in the buffer.
     * @param multiplier The multiplier to apply to the converted value.
     * @return The converted decimal value.
     */
    inline uint16_t FromBCD(le_IRIGB_Frame* buffer, uint16_t start, uint16_t stop, uint16_t multiplier);

    /**
     * @brief Sets the frame count tolerances based on the timer frequency and tolerance.
     * @param timerFreq The frequency of the timer.
     * @param tolerance The frame timing tolerance.
     */
    inline void SetFrameCountTolerances(uint32_t timerFreq, float tolerance);

    /**
     * @brief Invalidates the current IRIG signal, resetting relevant flags and indices.
     */
    inline void InvalidateIRIG();

    le_IRIGB_Frame* _irigFrameIn; ///< Frame buffer input
    le_IRIGB_Frame* _irigFrameOut; ///< Frame buffer output, left-aligned
    bool bBufferFlip; ///< Double buffer marker

    int16_t uSignalStart; ///< Signals the offset start of the signal
    bool bValidSignal; ///< Valid signal flag

    uint8_t uFrameWrite; ///< Frame write pointer
    uint8_t uFrameDecodeWrite; ///< Frame decode write pointer

    uint32_t uTimerFreq; ///< Timer frequency
    uint32_t uBit0Max; ///< Maximum value for BIT_0
    uint32_t uBit1Max; ///< Maximum value for BIT_1
    uint32_t uRefMax; ///< Maximum value for FRAME_REF

    int32_t iDrift; ///< Drift since last update
};
