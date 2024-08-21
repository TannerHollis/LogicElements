#include "le_IRIGB.hpp"

/**
 * @brief Constructor to initialize the le_IRIGB object with specified timer frequencies and frame tolerance.
 * @param irigTimerFreq The frequency of the IRIG timer.
 * @param irigFrameTolerance The tolerance for IRIG frame timing.
 * @param updateTimerFreq The frequency of the update timer.
 */
le_IRIGB::le_IRIGB(uint32_t irigTimerFreq, float irigFrameTolerance, uint32_t updateTimerFreq) : le_Time(updateTimerFreq, 0, 0, 0, 0, 0, 0)
{
    // Set extrinsic variables
    this->uTimerFreq = irigTimerFreq;

    // Set intrinsic variables
    this->_irigFrameIn = new le_IRIGB_Frame[SIGNAL_LEN]; // Allocate buffer frame input data
    this->_irigFrameOut = new le_IRIGB_Frame[SIGNAL_LEN * 2]; // Allocate double-buffer frame output data
    this->uSignalStart = -1; // Declare start frame invalid
    this->uFrameDecodeWrite = 0; // Frame input write pointer
    this->uFrameWrite = 0; // Frame output write pointer

    // Initialize flags
    this->bValidSignal = false;
    this->bBufferFlip = false;

    // Initialize drift
    this->iDrift = 0;

    // Set tolerances to frame times
    this->SetFrameCountTolerances(irigTimerFreq, irigFrameTolerance);
}

/**
 * @brief Destructor to clean up the le_IRIGB object.
 */
le_IRIGB::~le_IRIGB()
{
    delete[] this->_irigFrameIn;
    delete[] this->_irigFrameOut;
}

/**
 * @brief Decodes a buffer of IRIG-B signals.
 * @param buffer The buffer containing the IRIG-B signal data.
 * @param length The length of the buffer.
 */
void le_IRIGB::Decode(uint16_t* buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        le_IRIGB_Frame decodedFrame = DecodeFrame(buffer[i]);
        this->_irigFrameIn[this->uFrameWrite] = decodedFrame;

        le_IRIGB_Frame lastDecodedFrame = this->_irigFrameIn[(this->uFrameWrite - 1 + SIGNAL_LEN) % SIGNAL_LEN];

        // Check for FRAME_REF to determine signal start
        if (decodedFrame == le_IRIGB_Frame::FRAME_REF && lastDecodedFrame == le_IRIGB_Frame::FRAME_REF)
        {
            // Second FRAME_REF found, indicating the start of a new frame
            this->uSignalStart = this->uFrameWrite;
            this->bValidSignal = true;
        }

        // Copy the new frame to _irigSignalOut aligned to signalStart
        if (this->bValidSignal)
        {
            // Calculate output-aligned index
            uint16_t alignedIndex = (this->uFrameWrite >= this->uSignalStart) ? (this->uFrameWrite - this->uSignalStart) : (SIGNAL_LEN - this->uSignalStart + this->uFrameWrite);
            uint16_t outIndex = this->bBufferFlip ? alignedIndex + SIGNAL_LEN : alignedIndex;

            // Write to output buffer
            this->_irigFrameOut[outIndex] = decodedFrame;

            // Check if all 100 frames are available
            if (this->uFrameDecodeWrite == SIGNAL_LEN - 1)
            {
                // Process the decoded frames in the valid half
                if (this->bBufferFlip)
                {
                    this->DecodeFrames(&this->_irigFrameOut[SIGNAL_LEN], SIGNAL_LEN);
                }
                else
                {
                    this->DecodeFrames(&this->_irigFrameOut[0], 0);
                }

                this->bBufferFlip = !this->bBufferFlip;

                // Reset write pointer
                this->uFrameDecodeWrite = 0;
            }
            else
            	this->uFrameDecodeWrite += 1;
        }

        this->uFrameWrite = (this->uFrameWrite + 1) % SIGNAL_LEN;
    }
}

/**
 * @brief Gets the current drift value.
 * @return The current drift value in microseconds.
 */
inline int32_t le_IRIGB::GetDrift() const
{
    return this->iDrift;
}

/**
 * @brief Decodes a single IRIG-B frame from a raw signal value.
 * @param frame The raw signal value representing a frame.
 * @return The decoded IRIG-B frame.
 */
inline le_IRIGB::le_IRIGB_Frame le_IRIGB::DecodeFrame(uint16_t frame)
{
    if (frame < this->uBit0Max)
        return le_IRIGB_Frame::BIT_0;
    else if (frame < this->uBit1Max)
        return le_IRIGB_Frame::BIT_1;
    else if (frame < this->uRefMax)
        return le_IRIGB_Frame::FRAME_REF;
    else
        return le_IRIGB_Frame::FRAME_INVALID;
}

/**
 * @brief Decodes multiple IRIG-B frames and processes them.
 * @param frames The array of IRIG-B frames to decode.
 * @param offset The offset at which to start decoding.
 */
void le_IRIGB::DecodeFrames(le_IRIGB_Frame* frames, uint16_t offset)
{
    // Decode seconds (bits 1-8), if invalid frame format, quit
    if (frames[0] != le_IRIGB_Frame::FRAME_REF ||
        frames[5] != le_IRIGB_Frame::BIT_0 ||
        frames[9] != le_IRIGB_Frame::FRAME_REF)
    {
        this->InvalidateIRIG();
        return;
    }

    uint8_t second = (uint8_t)this->FromBCD(frames, 1, 4, 1);
    second += this->FromBCD(frames, 6, 8, 10);

    // Decode minutes (bits 10-17), if invalid frame format, quit
    if (frames[14] != le_IRIGB_Frame::BIT_0 ||
        frames[18] != le_IRIGB_Frame::BIT_0 ||
        frames[19] != le_IRIGB_Frame::FRAME_REF)
    {
        this->InvalidateIRIG();
        return;
    }

    uint8_t minute = (uint8_t)this->FromBCD(frames, 10, 13, 1);
    minute += this->FromBCD(frames, 15, 17, 10);

    // Decode hours (bits 20-28), if invalid frame format, quit
    if (frames[24] != le_IRIGB_Frame::BIT_0 ||
        frames[27] != le_IRIGB_Frame::BIT_0 ||
        frames[28] != le_IRIGB_Frame::BIT_0 ||
        frames[29] != le_IRIGB_Frame::FRAME_REF)
    {
        this->InvalidateIRIG();
        return;
    }

    uint8_t hour = (uint8_t)this->FromBCD(frames, 20, 23, 1);
    hour += this->FromBCD(frames, 25, 26, 10);

    // Decode days (bits 30-41), if invalid frame format, quit
    if (frames[34] != le_IRIGB_Frame::BIT_0 ||
        frames[39] != le_IRIGB_Frame::FRAME_REF ||
        frames[42] != le_IRIGB_Frame::BIT_0 ||
        frames[43] != le_IRIGB_Frame::BIT_0 ||
        frames[44] != le_IRIGB_Frame::BIT_0 ||
        frames[45] != le_IRIGB_Frame::BIT_0 ||
        frames[46] != le_IRIGB_Frame::BIT_0 ||
        frames[47] != le_IRIGB_Frame::BIT_0 ||
        frames[48] != le_IRIGB_Frame::BIT_0 ||
        frames[49] != le_IRIGB_Frame::FRAME_REF)
    {
        this->InvalidateIRIG();
        return;
    }

    uint16_t day = this->FromBCD(frames, 30, 33, 1);
    day += this->FromBCD(frames, 35, 38, 10);
    day += this->FromBCD(frames, 40, 41, 100);

    // Decode years (bits 50-58), if invalid frame format, quit
    if (frames[54] != le_IRIGB_Frame::BIT_0 ||
        frames[59] != le_IRIGB_Frame::FRAME_REF)
    {
        this->InvalidateIRIG();
        return;
    }

    uint8_t year = (uint8_t)this->FromBCD(frames, 50, 53, 1);
    year += this->FromBCD(frames, 55, 58, 10);

    this->iDrift = this->Align(0, second, minute, hour, day, year);

    if(true)
    {
		char buffer[100];
		this->PrintShortTime(&buffer[0], 100);
		printf("%s\r\n", buffer);
    }
}

/**
 * @brief Converts a Binary-Coded Decimal (BCD) value to a decimal value.
 * @param buffer The buffer containing the BCD value.
 * @param start The starting index of the BCD value in the buffer.
 * @param stop The stopping index of the BCD value in the buffer.
 * @param multiplier The multiplier to apply to the converted value (e.g., 1 for units, 10 for tens, 100 for hundreds).
 * @return The converted decimal value.
 */
inline uint16_t le_IRIGB::FromBCD(le_IRIGB_Frame* buffer, uint16_t start, uint16_t stop, uint16_t multiplier)
{
    uint16_t tmp = 0;

    for (int i = start; i <= stop; i++)
    {
        tmp += ((uint8_t)_irigFrameOut[i] << (i - start));
    }

    // Return with ones, tens, or hundreds multiplier
    return tmp * multiplier;
}

/**
 * @brief Sets the frame count tolerances based on the timer frequency and tolerance.
 * @param timerFreq The frequency of the timer.
 * @param tolerance The frame timing tolerance.
 */
inline void le_IRIGB::SetFrameCountTolerances(uint32_t timerFreq, float tolerance)
{
    this->uBit0Max = (uint32_t)(0.002f * (float)timerFreq * (1.0f + tolerance));
    this->uBit1Max = (uint32_t)(0.005f * (float)timerFreq * (1.0f + tolerance));
    this->uRefMax = (uint32_t)(0.008f * (float)timerFreq * (1.0f + tolerance));
}

/**
 * @brief Invalidates the current IRIG signal, resetting relevant flags and indices.
 */
inline void le_IRIGB::InvalidateIRIG()
{
    //this->uSignalStart = -1;
    //this->bValidSignal = false;
    this->uFrameDecodeWrite = 0;
}
