#pragma once

#include "le_Time.hpp"

#define SIGNAL_LEN 100
#define VALID_PULSES 4

class le_IRIGB : public le_Time
{
public:
	le_IRIGB(uint32_t timerFreq, float tolerance) : le_Time(timerFreq, 0, 0, 0, 0, 0, 0)
	{
		// Set extrinsic variables
		this->uTimerFreq = timerFreq;

		// Set intrinsic variables		
		//this->_irigFrameIn = new le_IRIGB_Frame[100];
		//this->_irigFrameOut = new le_IRIGB_Frame[100];
		this->uSignalStart = -1;	// Declare start frame invalid
		this->uFrameDecodeWrite = 0;
		this->uFrameWrite = 0;

		// Initialize flags
		this->bValidSignal = false;
		this->bBufferFlip = false;

		// Initialize next time stamp
		this->sNext.second = 0;
		this->sNext.minute = 0;
		this->sNext.hour = 0;
		this->sNext.day = 0;
		this->sNext.year = 0;

		// Initialize drift
		this->fDrift = 0.0f;

		// Set tolerances to frame times
		this->SetFrameCountTolerances(timerFreq, tolerance);
	}

	~le_IRIGB()
	{
		delete[] this->_irigFrameIn;
		delete[] this->_irigFrameOut;
	}

	void Decode(uint16_t* buffer, uint16_t length)
	{
	    for (uint16_t i = 0; i < length; i++)
	    {
	        le_IRIGB_Frame decodedFrame = DecodeFrame(buffer[i]);
	        this->_irigFrameIn[this->uFrameWrite] = decodedFrame;

			le_IRIGB_Frame lastDecodedFrame = this->_irigFrameIn[(this->uFrameWrite - 1 + SIGNAL_LEN) % SIGNAL_LEN];

	        // Check for FRAME_REF to determine signal start
	        if (decodedFrame == le_IRIGB_Frame::FRAME_REF && 
				lastDecodedFrame == le_IRIGB_Frame::FRAME_REF)
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
	            uint16_t outIndex = this->bBufferFlip ? alignedIndex : alignedIndex + SIGNAL_LEN;
				
				// Write to output buffer
	            this->_irigFrameOut[outIndex] = decodedFrame;

				// Check if all 100 frames are available
				if (alignedIndex == SIGNAL_LEN - 1)
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
				}

				// Save write index
				this->uFrameDecodeWrite = outIndex;
	        }

			this->uFrameWrite = (this->uFrameWrite + 1) % SIGNAL_LEN;
	    }
	}

	float GetDrift() const
	{
		return this->fDrift;
	}

private:
	enum class le_IRIGB_Frame : int8_t
	{
		BIT_0 = 0,
		BIT_1 = 1,
		FRAME_REF = 9,
		FRAME_INVALID = -1
	};

	const le_IRIGB_Frame DecodeFrame(uint16_t frame)
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

	void DecodeFrames(le_IRIGB_Frame* frames, uint16_t offset)
	{
		// Decode seconds (bits 1-8)
		if (frames[0] != le_IRIGB_Frame::FRAME_REF ||
			frames[5] != le_IRIGB_Frame::BIT_0 ||
			frames[9] != le_IRIGB_Frame::FRAME_REF)
		{
			this->InvalidateIRIG();
			return;
		}

		this->sNext.second = this->FromBCD(frames, 1, 4, 1);
		this->sNext.second += this->FromBCD(frames, 6, 8, 10);
		
		// Decode minutes (bits 10-17)
		if (frames[14] != le_IRIGB_Frame::BIT_0 ||
			frames[18] != le_IRIGB_Frame::BIT_0 ||
			frames[19] != le_IRIGB_Frame::FRAME_REF)
		{
			this->InvalidateIRIG();
			return;
		}

		this->sNext.minute = this->FromBCD(frames, 10, 13, 1);
		this->sNext.minute += this->FromBCD(frames, 15, 17, 10);

		// Decode hours (bits 20-28)
		if (frames[24] != le_IRIGB_Frame::BIT_0 ||
			frames[27] != le_IRIGB_Frame::BIT_0 ||
			frames[28] != le_IRIGB_Frame::BIT_0 ||
			frames[29] != le_IRIGB_Frame::FRAME_REF)
		{
			this->InvalidateIRIG();
			return;
		}

		this->sNext.hour = this->FromBCD(frames, 20, 23, 1);
		this->sNext.hour += this->FromBCD(frames, 25, 26, 10);

		// Decode days (bits 30-41)
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

		this->sNext.day = this->FromBCD(frames, 30, 33, 1);
		this->sNext.day += this->FromBCD(frames, 35, 38, 10);
		this->sNext.day += this->FromBCD(frames, 40, 41, 100);

		// Decode years (bits 50-58)
		if (frames[54] != le_IRIGB_Frame::BIT_0 ||
			frames[59] != le_IRIGB_Frame::FRAME_REF)
		{
			this->InvalidateIRIG();
			return;
		}

		this->sNext.year = this->FromBCD(frames, 50, 53, 1);
		this->sNext.year += this->FromBCD(frames, 55, 58, 10);

		this->fDrift = this->Align(0, this->sNext.second, this->sNext.minute, this->sNext.hour, this->sNext.day, this->sNext.year);

		char buffer[50];

		this->PrintShortTime(buffer, 50);
		printf("%s\r\n", buffer);
	}

	inline uint16_t FromBCD(le_IRIGB_Frame* buffer, uint16_t start, uint16_t stop, uint16_t multiplier)
	{
		uint16_t tmp = 0;

		for (int i = start; i <= stop; i++)
		{
			tmp += ((uint8_t)_irigFrameOut[i] << (i - start));
		}

		return tmp * multiplier;
	}

	uint8_t from_bcd(uint8_t bcd) {
		return ((bcd >> 4) * 10) + (bcd & 0x0F);
	}

	void SetFrameCountTolerances(uint32_t timerFreq, float tolerance)
	{
		this->uBit0Max = (uint32_t)(0.002f * (float)timerFreq * (1.0f + tolerance));
		this->uBit1Max = (uint32_t)(0.005f * (float)timerFreq * (1.0f + tolerance));
		this->uRefMax = (uint32_t)(0.008f * (float)timerFreq * (1.0f + tolerance));
	}

	void InvalidateIRIG()
	{
		this->uSignalStart = -1;
		this->bValidSignal = false;
	}

	le_IRIGB_Frame _irigFrameIn[100]; ///< Frame buffer input
	le_IRIGB_Frame _irigFrameOut[200]; ///< Frame buffer output, left-aligned
	bool bBufferFlip; ///< Double buffer marker

	// Signal offset for alignment
	int16_t uSignalStart; ///< Signals the offset start of the signal
	bool bValidSignal;

	// Write pointers
	uint8_t uFrameWrite;
	uint8_t uFrameDecodeWrite;
	
	// Extrinsic variables
	uint32_t uTimerFreq;
	uint32_t uBit0Max;
	uint32_t uBit1Max;
	uint32_t uRefMax;

	// Store next time
	struct
	{
		uint8_t second;
		uint8_t minute;
		uint8_t hour;
		uint16_t day;
		uint16_t year;
	} sNext;

	// Drift since last update
	float fDrift;
};
