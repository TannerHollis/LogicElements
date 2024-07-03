import numpy as np
import matplotlib.pyplot as plt

# Define the le_Analog1PWinding class with the specified methods and attributes
class le_Analog1PWinding:
    def __init__(self, samples_per_cycle):
        self.uSamplesPerCycle = samples_per_cycle
        self.uWrite = samples_per_cycle - 1
        self.uQuarterCycle = int(samples_per_cycle / 4.0) - 1
        self._rawValues = np.zeros(samples_per_cycle)
        self._rawFilteredValues = np.zeros(samples_per_cycle)
        self.coefficients = [2.0 / samples_per_cycle * np.cos(2.0 * np.pi / samples_per_cycle * i) for i in range(int(samples_per_cycle/1))]

    def ApplyCosineFilter(self):
        # Get the input element value
        e_value = self.input_value
        
        # Store raw input to raw values buffer
        self._rawValues[self.uWrite] = e_value

        sum_val = 0.0
        pos = self.uWrite

        for i in range(int(self.uSamplesPerCycle/1)):
            # Calculate Cosine Filter sum
            sum_val += self._rawValues[pos] * self.coefficients[i]

            # Decrement position
            pos = (pos + 1) % self.uSamplesPerCycle

        # Scale sum and store in filtered values buffer
        self._rawFilteredValues[self.uWrite] = sum_val

    def CalculatePhasor(self):
        real_component = self._rawFilteredValues[self.uWrite]
        imaginary_component = self._rawFilteredValues[self.uQuarterCycle]
        return real_component, imaginary_component

    def Update(self, timeStamp):
        self.ApplyCosineFilter()
        real_component, imaginary_component = self.CalculatePhasor()

        # Update the write indices
        self.uWrite = (self.uWrite - 1 + self.uSamplesPerCycle) % self.uSamplesPerCycle
        self.uQuarterCycle = (self.uQuarterCycle - 1 + self.uSamplesPerCycle) % self.uSamplesPerCycle
        
        return real_component, imaginary_component

    def SetValue(self, input_value):
        self.input_value = input_value

# Generate a noisy test signal with a step in magnitude and a decaying DC offset
test_signal_frequency = 60  # frequency of the test signal in Hz
samples_per_cycle = 8
sampling_rate = test_signal_frequency * samples_per_cycle  # samples per second
duration = 1  # duration of the test signal in seconds
t = np.linspace(0, duration, int(sampling_rate * duration), endpoint=False)

# Create test signal (sine wave)
test_signal = np.sin(2 * np.pi * test_signal_frequency * t)

# Create a step in the sinusoidal magnitude at 0.5 seconds with 5x magnitude
step_time = 0.5  # seconds
step_index = int(step_time * sampling_rate)
test_signal_with_step = test_signal.copy()
test_signal_with_step[step_index:] *= 5  # Increase the amplitude by 5 times after the step time

# Add a decaying DC offset starting at 0.5 seconds
decay_constant = 5
dc_offset_mag = 5
dc_offset = np.zeros_like(test_signal)
dc_offset[step_index:] = dc_offset_mag * np.exp(-decay_constant * (t[step_index:] - step_time))

# Combine the step change and the decaying DC offset
test_signal_with_step_and_dc = test_signal_with_step + dc_offset

# Add noise to the test signal with step and decaying DC offset
noise = 0.1 * np.random.normal(size=test_signal.shape)
noisy_test_signal_with_step_and_dc = test_signal_with_step_and_dc + noise

# Create an instance of the class and run the update method
winding = le_Analog1PWinding(samples_per_cycle)

# Simulate the signal processing over time
real_components = []
imaginary_components = []
filtered_values = []
for i in range(len(t)):
    winding.SetValue(noisy_test_signal_with_step_and_dc[i])
    real, imag = winding.Update(t[i])
    real_components.append(real)
    imaginary_components.append(imag)
    filtered_values.append(winding._rawFilteredValues[(winding.uWrite + 1) % winding.uSamplesPerCycle])

# Calculate the magnitude of the phasor
phasor_magnitude = np.sqrt(np.array(real_components)**2 + np.array(imaginary_components)**2)

# Plot the input signal, noisy signal, and filtered signal
plt.figure(figsize=(14, 7))

plt.subplot(2, 1, 1)
plt.plot(t, noisy_test_signal_with_step_and_dc, label='Noisy Test Signal with Step at 0.5s')
plt.plot(t, filtered_values, label='Filtered Signal', color='orange')
plt.plot(t, phasor_magnitude, label='Phasor Magnitude', color='green')
plt.title('Noisy Test Signal with Step at 0.5s')
plt.xlabel('Time [s]')
plt.ylabel('Amplitude')
plt.legend()

plt.subplot(2, 1, 2)
plt.plot(t, phasor_magnitude, label='Phasor Magnitude', color='green')
plt.title('Phasor Magnitude')
plt.xlabel('Time [s]')
plt.ylabel('Magnitude')
plt.legend()

plt.tight_layout()
plt.show()
