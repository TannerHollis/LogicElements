import numpy as np
from datetime import datetime, timedelta

SIGNAL_LEN = 100
VALID_PULSES = 4

class le_Time:
    def __init__(self, sub_second_fraction, sub_second, second, minute, hour, day, year):
        self.uSubSecondFraction = sub_second_fraction
        self.uSubSecond = sub_second
        self.uSecond = second
        self.uMinute = minute
        self.uHour = hour
        self.uDay = day
        self.uYear = year

    def Align(self, sub_second, second, minute, hour, day, year):
        self.uSubSecond = sub_second
        self.uSecond = second
        self.uMinute = minute
        self.uHour = hour
        self.uDay = day
        self.uYear = year
        return 0

    def PrintShortTime(self):
        dt = datetime(self.uYear + 2000, 1, 1) + timedelta(days=self.uDay - 1, hours=self.uHour, minutes=self.uMinute, seconds=self.uSecond)
        print(dt, self.uDay)

class le_IRIGB(le_Time):
    class le_IRIGB_Frame:
        BIT_0 = 0
        BIT_1 = 1
        FRAME_REF = 10
        FRAME_INVALID = -1

    def __init__(self, irig_timer_freq, irig_frame_tolerance, update_timer_freq):
        super().__init__(update_timer_freq, 0, 0, 0, 0, 0, 0)
        
        # Set extrinsic variables
        self.uTimerFreq = irig_timer_freq

        # Set intrinsic variables
        self._irigFrameIn = [self.le_IRIGB_Frame.FRAME_INVALID] * SIGNAL_LEN  # Allocate buffer frame input data
        self._irigFrameOut = [self.le_IRIGB_Frame.FRAME_INVALID] * (SIGNAL_LEN * 2)  # Allocate double-buffer frame output data
        self.uSignalStart = -1  # Declare start frame invalid
        self.uFrameDecodeWrite = 0  # Frame input write pointer
        self.uFrameWrite = 0  # Frame output write pointer

        # Initialize flags
        self.bValidSignal = False
        self.bBufferFlip = False

        # Initialize drift
        self.iDrift = 0

        # Set tolerances to frame times
        self.SetFrameCountTolerances(irig_timer_freq, irig_frame_tolerance)

    def Decode(self, buffer, length):
        decoded_frames = np.zeros(length)
        for i in range(length):
            decoded_frame = self.DecodeFrame(buffer[i])
            self._irigFrameIn[self.uFrameWrite] = decoded_frame
            decoded_frames[i] = decoded_frame

            last_decoded_frame = self._irigFrameIn[(self.uFrameWrite - 1 + SIGNAL_LEN) % SIGNAL_LEN]

            # Check for FRAME_REF to determine signal start
            if decoded_frame == self.le_IRIGB_Frame.FRAME_REF and last_decoded_frame == self.le_IRIGB_Frame.FRAME_REF:
                # Second FRAME_REF found, indicating the start of a new frame
                self.uSignalStart = self.uFrameWrite
                self.bValidSignal = True
                print("Found Frame start @", self.uSignalStart)

            # Copy the new frame to _irigFrameOut aligned to signalStart
            if self.bValidSignal:
                # Calculate output-aligned index
                aligned_index = (self.uFrameWrite - self.uSignalStart + SIGNAL_LEN) % SIGNAL_LEN
                out_index = aligned_index if self.bBufferFlip else aligned_index + SIGNAL_LEN

                # Write to output buffer
                self._irigFrameOut[out_index] = decoded_frame

                # Check if all 100 frames are available
                if self.uFrameDecodeWrite == SIGNAL_LEN - 1:
                    # Process the decoded frames in the valid half
                    print("Decoding...")
                    if self.bBufferFlip:
                        self.DecodeFrames(self._irigFrameOut[SIGNAL_LEN:], SIGNAL_LEN)
                    else:
                        self.DecodeFrames(self._irigFrameOut[:SIGNAL_LEN], 0)

                    self.bBufferFlip = not self.bBufferFlip
                    self.uFrameDecodeWrite = 0
                else:
                    # Save write index
                    self.uFrameDecodeWrite += 1

            self.uFrameWrite = (self.uFrameWrite + 1) % SIGNAL_LEN

    def GetDrift(self):
        return self.iDrift

    def DecodeFrame(self, frame):
        if frame < self.uBit0Max:
            return self.le_IRIGB_Frame.BIT_0
        elif frame < self.uBit1Max:
            return self.le_IRIGB_Frame.BIT_1
        elif frame < self.uRefMax:
            return self.le_IRIGB_Frame.FRAME_REF
        else:
            return self.le_IRIGB_Frame.FRAME_INVALID

    def DecodeFrames(self, frames, offset):
        # Decode seconds (bits 1-8), if invalid frame format, quit
        if frames[0] != self.le_IRIGB_Frame.FRAME_REF or \
           frames[5] != self.le_IRIGB_Frame.BIT_0 or \
           frames[9] != self.le_IRIGB_Frame.FRAME_REF:
            self.InvalidateIRIG()
            return

        second = self.FromBCD(frames, 1, 4, 1) + self.FromBCD(frames, 6, 8, 10)

        # Decode minutes (bits 10-17), if invalid frame format, quit
        if frames[14] != self.le_IRIGB_Frame.BIT_0 or \
           frames[18] != self.le_IRIGB_Frame.BIT_0 or \
           frames[19] != self.le_IRIGB_Frame.FRAME_REF:
            self.InvalidateIRIG()
            return

        minute = self.FromBCD(frames, 10, 13, 1) + self.FromBCD(frames, 15, 17, 10)

        # Decode hours (bits 20-28), if invalid frame format, quit
        if frames[24] != self.le_IRIGB_Frame.BIT_0 or \
           frames[27] != self.le_IRIGB_Frame.BIT_0 or \
           frames[28] != self.le_IRIGB_Frame.BIT_0 or \
           frames[29] != self.le_IRIGB_Frame.FRAME_REF:
            self.InvalidateIRIG()
            return

        hour = self.FromBCD(frames, 20, 23, 1) + self.FromBCD(frames, 25, 26, 10)

        # Decode days (bits 30-41), if invalid frame format, quit
        if frames[34] != self.le_IRIGB_Frame.BIT_0 or \
           frames[39] != self.le_IRIGB_Frame.FRAME_REF or \
           frames[42] != self.le_IRIGB_Frame.BIT_0 or \
           frames[43] != self.le_IRIGB_Frame.BIT_0 or \
           frames[44] != self.le_IRIGB_Frame.BIT_0 or \
           frames[45] != self.le_IRIGB_Frame.BIT_0 or \
           frames[46] != self.le_IRIGB_Frame.BIT_0 or \
           frames[47] != self.le_IRIGB_Frame.BIT_0 or \
           frames[48] != self.le_IRIGB_Frame.BIT_0 or \
           frames[49] != self.le_IRIGB_Frame.FRAME_REF:
            self.InvalidateIRIG()
            return

        day = self.FromBCD(frames, 30, 33, 1) + self.FromBCD(frames, 35, 38, 10) + self.FromBCD(frames, 40, 41, 100)

        # Decode years (bits 50-58), if invalid frame format, quit
        if frames[54] != self.le_IRIGB_Frame.BIT_0 or \
           frames[59] != self.le_IRIGB_Frame.FRAME_REF:
            self.InvalidateIRIG()
            return

        year = self.FromBCD(frames, 50, 53, 1) + self.FromBCD(frames, 55, 58, 10)

        self.iDrift = self.Align(0, second, minute, hour, day, year)

        if True:
            self.PrintShortTime()

    def FromBCD(self, buffer, start, stop, multiplier):
        tmp = 0

        for i in range(start, stop + 1):
            tmp += (buffer[i] << (i - start))

        # Return with ones, tens, or hundreds multiplier
        return tmp * multiplier

    def SetFrameCountTolerances(self, timer_freq, tolerance):
        self.uBit0Max = int(0.002 * timer_freq * (1.0 + tolerance))
        self.uBit1Max = int(0.005 * timer_freq * (1.0 + tolerance))
        self.uRefMax = int(0.008 * timer_freq * (1.0 + tolerance))
        print(self.uBit0Max,self.uBit1Max, self.uRefMax)

    def InvalidateIRIG(self):
        print("Invalid Signal, ignoring frames...")
        #self.uSignalStart = -1
        #self.bValidSignal = False
        self.uFrameDecodeWrite = 0

# Constants for pulse widths (assuming some values for demonstration)
PULSE_WIDTH_0 = 200
PULSE_WIDTH_1 = 500
PULSE_WIDTH_REF = 800

def to_bcd(value):
    """Converts a decimal value to BCD."""
    return (int(value / 1000) << 12) | ((int(value / 100) % 10) << 8) | ((int(value / 10) % 10) << 4) | (value % 10)

def generate_irigb_signal(irigb_signal, current_time):
    """Generates an IRIG-B signal based on the current time."""
    # Clear the array
    for i in range(100):
        irigb_signal[i] = PULSE_WIDTH_0

    # Reference bits at specific positions
    irigb_signal[0] = PULSE_WIDTH_REF
    for pos in range(9, 100, 10):
        irigb_signal[pos] = PULSE_WIDTH_REF

    # Encode seconds (BCD)
    sec = to_bcd(current_time.tm_sec)
    irigb_signal[1] = PULSE_WIDTH_1 if sec & 0x01 else PULSE_WIDTH_0
    irigb_signal[2] = PULSE_WIDTH_1 if sec & 0x02 else PULSE_WIDTH_0
    irigb_signal[3] = PULSE_WIDTH_1 if sec & 0x04 else PULSE_WIDTH_0
    irigb_signal[4] = PULSE_WIDTH_1 if sec & 0x08 else PULSE_WIDTH_0
    irigb_signal[6] = PULSE_WIDTH_1 if sec & 0x10 else PULSE_WIDTH_0
    irigb_signal[7] = PULSE_WIDTH_1 if sec & 0x20 else PULSE_WIDTH_0
    irigb_signal[8] = PULSE_WIDTH_1 if sec & 0x40 else PULSE_WIDTH_0

    # Encode minutes (BCD)
    min = to_bcd(current_time.tm_min)
    irigb_signal[10] = PULSE_WIDTH_1 if min & 0x01 else PULSE_WIDTH_0
    irigb_signal[11] = PULSE_WIDTH_1 if min & 0x02 else PULSE_WIDTH_0
    irigb_signal[12] = PULSE_WIDTH_1 if min & 0x04 else PULSE_WIDTH_0
    irigb_signal[13] = PULSE_WIDTH_1 if min & 0x08 else PULSE_WIDTH_0
    irigb_signal[15] = PULSE_WIDTH_1 if min & 0x10 else PULSE_WIDTH_0
    irigb_signal[16] = PULSE_WIDTH_1 if min & 0x20 else PULSE_WIDTH_0
    irigb_signal[17] = PULSE_WIDTH_1 if min & 0x40 else PULSE_WIDTH_0

    # Encode hours (BCD)
    hour = to_bcd(current_time.tm_hour)
    irigb_signal[20] = PULSE_WIDTH_1 if hour & 0x01 else PULSE_WIDTH_0
    irigb_signal[21] = PULSE_WIDTH_1 if hour & 0x02 else PULSE_WIDTH_0
    irigb_signal[22] = PULSE_WIDTH_1 if hour & 0x04 else PULSE_WIDTH_0
    irigb_signal[23] = PULSE_WIDTH_1 if hour & 0x08 else PULSE_WIDTH_0
    irigb_signal[25] = PULSE_WIDTH_1 if hour & 0x10 else PULSE_WIDTH_0
    irigb_signal[26] = PULSE_WIDTH_1 if hour & 0x20 else PULSE_WIDTH_0

    # Encode day of year (BCD)
    day = to_bcd(current_time.tm_yday)
    irigb_signal[30] = PULSE_WIDTH_1 if day & 0x01 else PULSE_WIDTH_0
    irigb_signal[31] = PULSE_WIDTH_1 if day & 0x02 else PULSE_WIDTH_0
    irigb_signal[32] = PULSE_WIDTH_1 if day & 0x04 else PULSE_WIDTH_0
    irigb_signal[33] = PULSE_WIDTH_1 if day & 0x08 else PULSE_WIDTH_0
    irigb_signal[35] = PULSE_WIDTH_1 if day & 0x10 else PULSE_WIDTH_0
    irigb_signal[36] = PULSE_WIDTH_1 if day & 0x20 else PULSE_WIDTH_0
    irigb_signal[37] = PULSE_WIDTH_1 if day & 0x40 else PULSE_WIDTH_0
    irigb_signal[38] = PULSE_WIDTH_1 if day & 0x80 else PULSE_WIDTH_0
    irigb_signal[40] = PULSE_WIDTH_1 if day & 0x100 else PULSE_WIDTH_0
    irigb_signal[41] = PULSE_WIDTH_1 if day & 0x200 else PULSE_WIDTH_0

    # Encode year (BCD)
    year = to_bcd(current_time.tm_year % 100)  # tm_year is years since 1900
    irigb_signal[50] = PULSE_WIDTH_1 if year & 0x01 else PULSE_WIDTH_0
    irigb_signal[51] = PULSE_WIDTH_1 if year & 0x02 else PULSE_WIDTH_0
    irigb_signal[52] = PULSE_WIDTH_1 if year & 0x04 else PULSE_WIDTH_0
    irigb_signal[53] = PULSE_WIDTH_1 if year & 0x08 else PULSE_WIDTH_0
    irigb_signal[55] = PULSE_WIDTH_1 if year & 0x10 else PULSE_WIDTH_0
    irigb_signal[56] = PULSE_WIDTH_1 if year & 0x20 else PULSE_WIDTH_0
    irigb_signal[57] = PULSE_WIDTH_1 if year & 0x40 else PULSE_WIDTH_0
    irigb_signal[58] = PULSE_WIDTH_1 if year & 0x80 else PULSE_WIDTH_0

# Test script
if __name__ == "__main__":
    import time

    # Create an instance of le_IRIGB
    irig_decoder = le_IRIGB(irig_timer_freq=100000, irig_frame_tolerance=0.1, update_timer_freq=1)
    
    for j  in range(5):
        # Get the current time
        current_time = time.localtime()

        # Generate IRIG-B signal
        irigb_signal = np.zeros(SIGNAL_LEN, dtype=np.uint16)
        generate_irigb_signal(irigb_signal, current_time)

        # Decode the IRIG-B signal
        chunk_size = int(SIGNAL_LEN / 10)

        if(j == 3):
            print("Inserting invalid frame")
            irigb_signal[0] = PULSE_WIDTH_0
        
        pos = 0
        print("IRIG Pass {0}".format(j))
        for i in range(chunk_size):
            irig_chunk = irigb_signal[pos : pos + chunk_size]
            irig_decoder.Decode(irig_chunk, chunk_size)
            pos = (pos + chunk_size) % SIGNAL_LEN
        

    # Print the drift value
    print("Drift:", irig_decoder.GetDrift())
    
