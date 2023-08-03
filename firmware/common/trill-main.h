//----------------------------------------------------------------------------
// TRILL: Touch Sensing for Makers
// (c) 2020 Augmented Instruments Ltd
//
// These materials are licensed under the Creative Commons BY-SA-NC 4.0:
// Attribution, ShareAlike, Non-Commercial. For further information please
// contact info@bela.io.
//----------------------------------------------------------------------------

#include "trill.h"

// This file is supposed to be included after ID_DEVICE_TYPE has been defined, e.g.:
// #include "trill.h"
// #define ID_DEVICE_TYPE			ID_DEVICE_SQUARE
// #include "main.c"

#include <m8c.h>        // part specific constants and macros
#include "PSoCAPI.h"    // PSoC API definitions for all User Modules
#include "PSoCGPIOInt.h"

#define BUFFER_SWAP
#define TIMER16

#define BASE_I2C_ADDR_BAR		0x20
#define BASE_I2C_ADDR_SQUARE		0x28
#define BASE_I2C_ADDR_CRAFT		0x30
#define BASE_I2C_ADDR_RING		0x38
#define BASE_I2C_ADDR_HEX		0x40
#define BASE_I2C_ADDR_FLEX		0x48

#define ID_FIRMWARE_VERSION		0x02
#define ID_RESERVED			0x00

#if (ID_DEVICE_TYPE == ID_DEVICE_SQUARE) || (ID_DEVICE_TYPE == ID_DEVICE_HEX)
#define TWOD
#else
#define ONED
#endif


// This sets the range of I2C addresses for the sensors
// 7 addresses are possible, beginning at this address,
// depending on the settings of the 3 GPIO pins
const BYTE addresses[] = {
	0, // dummy
	BASE_I2C_ADDR_BAR,
	BASE_I2C_ADDR_SQUARE,
	BASE_I2C_ADDR_CRAFT,
	BASE_I2C_ADDR_RING,
	BASE_I2C_ADDR_HEX,
	BASE_I2C_ADDR_FLEX,
};
#define BASELINE_I2C_ADDRESS addresses[ID_DEVICE_TYPE]

// Pins for determining I2C address: P1[2], P1[4], P1[6]
#define I2C_ADDR_PORT_DATA		PRT1DR
#define I2C_ADDR_PORT_DIR0		PRT1DM0
#define I2C_ADDR_PORT_DIR1		PRT1DM1
#define I2C_ADDR_PIN0			(1 << 2)
#define I2C_ADDR_PIN1			(1 << 4)

// Pin for output EVENT signal
#if (ID_DEVICE_TYPE == ID_DEVICE_CRAFT)
#define EVENT_PORT_DATA			PRT2DR
#define EVENT_PORT_DIR0			PRT2DM0
#define EVENT_PORT_DIR1			PRT2DM1
#define EVENT_PIN			(1 << 7)
#else
#define EVENT_PORT_DATA			PRT1DR
#define EVENT_PORT_DIR0			PRT1DM0
#define EVENT_PORT_DIR1			PRT1DM1
#define EVENT_PIN			(1 << 6)
#endif

// Max number of touches to support
#ifdef TWOD
#define MAX_NUM_CENTROIDS	4
#else // TWOD
#define MAX_NUM_CENTROIDS	5
#endif // TWOD

// Size of the communication buffer, and byte where data begins
// within it (first part is reserved for commands)
#define COMM_BUFFER_SIZE_BYTES	64
#define COMM_DATA_OFFSET_BYTES	4
#define COMM_DATA_OFFSET_WORDS	2

#if (ID_DEVICE_TYPE == ID_DEVICE_BAR)
#define NUM_SENSORS 26				// How many sensors total: 26 on the Trill Bar
#else
#define NUM_SENSORS 30				// 30 sensors on all other devices
#endif
#define SLIDER_BITS 7				// Total slider range is 19*2^(SLIDER_BITS)
#define PACKED_CENTROID_LENGTH 8

#if (ID_DEVICE_TYPE == ID_DEVICE_SQUARE)
#define FIRST_SENSOR_V 15
#define LAST_SENSOR_V NUM_SENSORS
#define FIRST_SENSOR_H 0
#define LAST_SENSOR_H 15
#elif (ID_DEVICE_TYPE == ID_DEVICE_HEX)
#define FIRST_SENSOR_V 14
#define LAST_SENSOR_V NUM_SENSORS
#define FIRST_SENSOR_H 0
#define LAST_SENSOR_H 14
#elif (ID_DEVICE_TYPE == ID_DEVICE_RING)
#define FIRST_SENSOR_V 0
#define LAST_SENSOR_V (NUM_SENSORS-2)
#else
#define FIRST_SENSOR_V 0
#define LAST_SENSOR_V NUM_SENSORS
#endif

#ifdef TIMER16
// Timer. Here's how to compute the timer period:
//#define TIMER_CLOCK 32000 // Hz
//#define TIMER_DURATION 0.015 // seconds
//wTimerPeriod = (WORD)(TIMER_CLOCK * TIMER_DURATION); // Timer period (samples)
WORD wTimerPeriod = 0;
BYTE bTimerTicks;
#endif

#pragma interrupt_handler timer_ISR_Handler;
void timer_ISR_Handler(void);


enum {
	kCommandNone = 0,
	kCommandMode = 1,
	kCommandScanSettings = 2,
	kCommandPrescaler = 3,
	kCommandNoiseThreshold = 4,
	kCommandIdac = 5,
	kCommandBaselineUpdate = 6,
	kCommandMinimumSize = 7,
	kCommandAdjacentCentroidNoiseThreshold = 8,
	kCommandAutoScanInterval = 16,
	kCommandIdentify = 255
};

enum {
	kModeCentroid = 0,
	kModeRaw = 1,
	kModeBaseline = 2,
	kModeDiff = 3
};

#ifdef BUFFER_SWAP
#define NUM_BUFFERS 2
#else // BUFFER_SWAP
#define NUM_BUFFERS 1
#endif

BYTE bCurrentCommBuffer = 0;
WORD wCommBuffers[NUM_BUFFERS][COMM_BUFFER_SIZE_BYTES];	// Buffer for communication
WORD wVCentroid[MAX_NUM_CENTROIDS];			// Calculated (vertical) centroids
WORD wVCentroidSize[MAX_NUM_CENTROIDS];		// Size of (vertical) centroids
#ifdef TWOD
WORD wHCentroid[MAX_NUM_CENTROIDS];			// Calculated (horizontal) centroids
WORD wHCentroidSize[MAX_NUM_CENTROIDS];		// Size of (horizontal) centroids
#endif // TWOD

BYTE bMode = kModeCentroid;		// Mode in which to transmit data via I2C (set from host)
BYTE bCommand = kCommandNone;

WORD wMinimumCentroidSize = 0; 	// Size below which a centroid is ignored as noise
WORD wAdjacentCentroidNoiseThreshold = 400; // Trough between peaks needed to identify two centroids

extern BYTE CSD_bNoiseThreshold;

#ifdef BUFFER_SWAP
void enableInterrupts(void) {
	M8C_EnableGInt;
	/* Disable I2C Interrupt mask */
	//M8C_EnableIntMask(INT_MSK3, INT_MSK3_I2C);
}
void disableInterrupts(void) {
	M8C_DisableGInt;
	/* Disable I2C Interrupt mask */
	//M8C_DisableIntMask(INT_MSK3, INT_MSK3_I2C);
}
#endif // BUFFER_SWAP

WORD calculateCentroids(WORD *centroidBuffer, WORD *sizeBuffer, BYTE maxNumCentroids, BYTE minSensor, BYTE maxSensor, BYTE numSensors);

BYTE detectI2cAddress(void)
{
	BYTE pinstatus = 0;
	BYTE address = BASELINE_I2C_ADDRESS;
	BYTE counter;

	// I2C address is determined based on status of ADDR0 and ADDR1 pins
	// which could be in three states: open, tied high, tied low. We first
	// read with a pullup resistor to distinguish between open and tied low;
	// then we read with high-Z to distinguish between open and tied high.
	// An external resistor pulls down more weakly than the internal pullup.

	// Address configuration, relative to baseline:
	// ADDR1  ADDR0  Address
	// open   open   +0
	// open   low    +1
	// open   high   +2
	// low    open   +3
	// low    low    +4
	// low    high   +5
	// high   open   +6
	// high   low    +7
	// high   high   +8

	// Enable pullup on input pins: DIR1:0 = 00, DR = 1
	I2C_ADDR_PORT_DIR0 &= ~(I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	I2C_ADDR_PORT_DIR1 &= ~(I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	I2C_ADDR_PORT_DATA |= (I2C_ADDR_PIN0 | I2C_ADDR_PIN1);

	// Wait a short while for pin to settle
	for(counter = 0; counter < 255; counter++) {}

	// If pin reads high, it is either open or tied high
	if((I2C_ADDR_PORT_DATA & I2C_ADDR_PIN0))
		pinstatus |= 0x01;
	if((I2C_ADDR_PORT_DATA & I2C_ADDR_PIN1))
		pinstatus |= 0x02;

	// Now turn pins to high-Z mode (no pullup): DIR1:0 = 11, DR = 1
	I2C_ADDR_PORT_DIR1 |= (I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	I2C_ADDR_PORT_DIR0 |= (I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	I2C_ADDR_PORT_DATA |= (I2C_ADDR_PIN0 | I2C_ADDR_PIN1);

	// Wait a short while for pin to settle
	for(counter = 0; counter < 255; counter++) {}

	// Now read pin again to disambiguate the three states
	if((I2C_ADDR_PORT_DATA & I2C_ADDR_PIN0)) {
		// Pin reads HIGH in high-Z mode; must be tied high
		address += 2;
	}
	else if(!(pinstatus & 0x01)) {
		// Pin read LOW in high-Z mode, and...
		// Pin read LOW with pullup: must be tied low
		address += 1;
	}
	// else pin is open; no change to address

	// Same idea with address pin 1
	if((I2C_ADDR_PORT_DATA & I2C_ADDR_PIN1)) {
		// Pin reads HIGH in high-Z mode; must be tied high
		address += 6;
	}
	else if(!(pinstatus & 0x02)) {
		// Pin read LOW in high-Z mode, and...
		// Pin read LOW with pullup: must be tied low
		address += 3;
	}
	// else pin is open; no change to address

	return address;
}

void setEventPin(void) {
	EVENT_PORT_DATA |= EVENT_PIN;
}
void clearEventPin(void) {
	EVENT_PORT_DATA &= ~EVENT_PIN;
}
// for debugging purposes, make it easy to use the address pins as additional
// outputs
#ifdef ADDRESS_PINS_DEBUG
void setAddrPin0(void) {
	I2C_ADDR_PORT_DATA |= I2C_ADDR_PIN0;
}
void clearAddrPin0(void) {
	I2C_ADDR_PORT_DATA &= ~I2C_ADDR_PIN0;
}
void setAddrPin1(void) {
	I2C_ADDR_PORT_DATA |= I2C_ADDR_PIN1;
}
void clearAddrPin1(void) {
	I2C_ADDR_PORT_DATA &= ~I2C_ADDR_PIN1;
}
#endif // ADDRESS_PINS_DEBUG

void main(void)
{
	BYTE address;
	BYTE counter;
#if ID_DEVICE_TYPE == ID_DEVICE_RING
	BYTE numSensors = LAST_SENSOR_V - FIRST_SENSOR_V + 5; // we wrap around
#else // ID_DEVICE_RING
	BYTE numSensors = LAST_SENSOR_V - FIRST_SENSOR_V;
#endif // ID_DEVICE_RING
	WORD posEndOfLoop = (LAST_SENSOR_V - FIRST_SENSOR_V) << SLIDER_BITS;

	// Detect the I2C address from the ADDR0 and ADDR1 pins
	address = detectI2cAddress();

	// Set event pin to strong output (low): DIR1:0 = 01
	EVENT_PORT_DIR1 &= ~EVENT_PIN;
	EVENT_PORT_DIR0 |= EVENT_PIN;
	clearEventPin();

#ifdef ADDRESS_PINS_DEBUG
	I2C_ADDR_PORT_DIR1 &= ~(I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	I2C_ADDR_PORT_DIR0 |= ~(I2C_ADDR_PIN0 | I2C_ADDR_PIN1);
	clearAddrPin0();
	clearAddrPin1();
#endif // ADDRESS_PINS_DEBUG

	// I2C init, part 1
	EzI2Cs_SetRamBuffer(COMM_BUFFER_SIZE_BYTES, COMM_DATA_OFFSET_BYTES, (BYTE *)wCommBuffers[bCurrentCommBuffer]);

	// Interrupts on
	M8C_EnableGInt;

	// CapSense init
	CSD_Start();
	CSD_InitializeBaselines();

	// I2C init, using address discovered above
	EzI2Cs_Start();
	EzI2Cs_SetAddr(address);

#ifdef TIMER16
	// Timer setup
	Timer16_1_EnableInt();
	Timer16_1_SetMode(1); // one shot
#endif // TIMER16
	
	// -------------- main scanning routine -----------------------------

	while (1)
	{
		BOOL bActivityDetected = 0;
		WORD temp;
		signed char firstActiveSensor;
		signed char lastActiveSensor;
		WORD* wCommBuffer = wCommBuffers[bCurrentCommBuffer];
#ifdef BUFFER_SWAP
		WORD* wCommBufferInactive = wCommBuffers[!bCurrentCommBuffer];
#else // BUFFER_SWAP
		// In the interest of minimising the #ifdefs below, when not using
		// BUFFER_SWAP, we simply have both buffers be the same:
		WORD* wCommBufferInactive = wCommBuffer;
#endif // BUFFER_SWAP
#ifdef TIMER16
		bTimerTicks = 0;
		// only start the auto-scanning timer if the period is positive.
		// Otherwise, auto-scanning is disabled
		if(wTimerPeriod) {
			Timer16_1_SetPeriod(wTimerPeriod);
			Timer16_1_Start();
		}
#endif // TIMER16

		CSD_ScanAllSensors(); 		// Scan all sensors in array, takes 6.24ms
		CSD_UpdateAllBaselines();   // Update all baseline levels, takes 430us

		clearEventPin(); // this is set below after the available data is made available via I2C

#ifndef BUFFER_SWAP
		// If we only have one buffer, wait for it to be free before writing to
		// it (and hope that no new transaction comes in in the meantime)
		while (EzI2Cs_bBusy_Flag != EzI2Cs_I2C_FREE);
#endif // BUFFER_SWAP

		switch(bMode) {
			case kModeCentroid:
				temp = calculateCentroids(wVCentroid, wVCentroidSize, MAX_NUM_CENTROIDS, FIRST_SENSOR_V, LAST_SENSOR_V, numSensors); // Vertical centroids
				firstActiveSensor = temp & 0xFF;
				lastActiveSensor = temp >> 8;
				bActivityDetected = lastActiveSensor >= 0;

				temp = lastActiveSensor - (LAST_SENSOR_V - FIRST_SENSOR_V );// retrieve the (wrapped) index
				//check for activity in the wraparound area
				// IF the last centroid ended after wrapping around ...
				// AND the first centroid was located before the end of the last ...
				if(lastActiveSensor >= LAST_SENSOR_V - FIRST_SENSOR_V
					&& (((BYTE)temp) << SLIDER_BITS) >= wVCentroid[0] )
				{
					// THEN the last touch is used to replace the first one
					for(counter = MAX_NUM_CENTROIDS - 1; counter >= 1; counter--) {
						if(0xFFFF == wVCentroid[counter])
							continue;
						// replace the first centroid
						wVCentroidSize[0] = wVCentroidSize[counter];
						wVCentroid[0] = wVCentroid[counter];
						// wrap around the position if needed
						if(wVCentroid[0] >= posEndOfLoop)
							wVCentroid[0] -= posEndOfLoop;
						// discard the last centroid
						wVCentroid[counter] = 0xFFFF;
						wVCentroidSize[counter] = 0x0;
						break;
					}
				}
				for(counter = 0; counter < MAX_NUM_CENTROIDS; counter++) {
					// Copy centroids and sizes to data buffer: locations first, then sizes, 16 bits each
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter] = wVCentroid[counter];
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter + MAX_NUM_CENTROIDS] = wVCentroidSize[counter];
				}
#ifdef TWOD
				temp = calculateCentroids(wHCentroid, wHCentroidSize, MAX_NUM_CENTROIDS, FIRST_SENSOR_H, LAST_SENSOR_H, numSensors); // Horizontal centroids
				lastActiveSensor = temp >> 8;
				bActivityDetected &&= lastActiveSensor >= 0;
				for(counter = 0; counter < MAX_NUM_CENTROIDS; counter++) {
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter + 2*MAX_NUM_CENTROIDS] = wHCentroid[counter];
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter + 3*MAX_NUM_CENTROIDS] = wHCentroidSize[counter];
				}
#endif // TWOD
#if ID_DEVICE_TYPE == ID_DEVICE_RING
				// Copy the two button readings to the end
				wCommBufferInactive[COMM_DATA_OFFSET_WORDS + 2*MAX_NUM_CENTROIDS] = CSD_waSnsDiff[28];
				wCommBufferInactive[COMM_DATA_OFFSET_WORDS + 2*MAX_NUM_CENTROIDS + 1] = CSD_waSnsDiff[29];
				// also set the event pin if any of the "button" pins is active
				bActivityDetected = bActivityDetected || CSD_waSnsDiff[28] || CSD_waSnsDiff[29];
#endif // RING
				break;
			case kModeRaw:
				for(counter = 0; counter < NUM_SENSORS; counter++) {
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter] = CSD_waSnsResult[counter];
					bActivityDetected = bActivityDetected || CSD_waSnsResult[counter];
				}
				break;
			case kModeBaseline:
				for(counter = 0; counter < NUM_SENSORS; counter++) {
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter] = CSD_waSnsBaseline[counter];
					bActivityDetected = bActivityDetected || CSD_waSnsBaseline[counter];
				}
				break;
			case kModeDiff:
				for(counter = 0; counter < NUM_SENSORS; counter++) {
					wCommBufferInactive[COMM_DATA_OFFSET_WORDS + counter] = CSD_waSnsDiff[counter];
					bActivityDetected = bActivityDetected || CSD_waSnsDiff[counter];
				}
				break;
			default:
				// Don't write anything...
				break;
		}

#ifdef BUFFER_SWAP
		// from here to enableInterrupts() it takes about 20us when we don't
		// have to wait
		while(1) {
			// Wait for current I2C transaction (if any) to complete
			while (EzI2Cs_bBusy_Flag != EzI2Cs_I2C_FREE)
				;
			// with interrupts disabled, swap the buffer that is to be read via
			// I2C transactions
			disableInterrupts();
			if (EzI2Cs_bBusy_Flag != EzI2Cs_I2C_FREE) {
				// in the unlikely event that another transaction has started
				// in the meantime, re-enable interrupts and try again
				enableInterrupts();
			}
			else {
				// otherwise, we are good to go.
				break;
			}
		}
		++bCurrentCommBuffer;
		if(bCurrentCommBuffer >= NUM_BUFFERS)
			bCurrentCommBuffer = 0;
		EzI2Cs_SetRamBuffer(COMM_BUFFER_SIZE_BYTES, COMM_DATA_OFFSET_BYTES, (BYTE *)wCommBuffers[bCurrentCommBuffer]);
		enableInterrupts();
#endif // BUFFER_SWAP
#ifdef BUFFER_SWAP
		// update pointers for the rest of the loop
		wCommBuffer = wCommBuffers[bCurrentCommBuffer];
		wCommBufferInactive = wCommBuffers[!bCurrentCommBuffer];
#endif // BUFFER_SWAP

		// If any activity was detected, turn on the event pin, which will
		// be unconditionally cleared elsewhere.
		// An external device monitoring this pin should be looking for a
		// rising edge denoting new data available.
		if(bActivityDetected)
			setEventPin();

		// Check for a command from the host
		bCommand = ((BYTE *)wCommBufferInactive)[0];

		switch(bCommand) {
			case kCommandMode:
				bMode = ((BYTE *)wCommBufferInactive)[1];
				break;
			case kCommandScanSettings:
				// Byte 1 is speed, byte 2 is number of bits
				// (reusing existing temp variables)
				bCommand = ((BYTE *)wCommBufferInactive)[1];
				counter = ((BYTE *)wCommBufferInactive)[2];
				CSD_SetScanMode(bCommand, counter);
				break;
			case kCommandPrescaler:
				bCommand = ((BYTE *)wCommBufferInactive)[1];
				CSD_SetPrescaler(bCommand);
				break;
			case kCommandNoiseThreshold:
				bCommand = ((BYTE *)wCommBufferInactive)[1];
				CSD_bNoiseThreshold = bCommand;
				break;
			case kCommandIdac:
				bCommand = ((BYTE *)wCommBufferInactive)[1];
				CSD_SetIdacValue(bCommand);
				break;
			case kCommandBaselineUpdate:
				CSD_InitializeBaselines();
				break;
			case kCommandMinimumSize:
				wMinimumCentroidSize = (((BYTE *)wCommBufferInactive)[1] << 8) + ((BYTE *)wCommBufferInactive)[2];
				break;
			case kCommandAdjacentCentroidNoiseThreshold:
				wAdjacentCentroidNoiseThreshold = (((BYTE *)wCommBufferInactive)[1]<< 8) + ((BYTE *)wCommBufferInactive)[2];
				break;
#ifdef TIMER16
			case kCommandAutoScanInterval:
				wTimerPeriod = (((BYTE *)wCommBufferInactive)[1]<< 8) + ((BYTE *)wCommBufferInactive)[2];
				break;
#endif // TIMER16
			case kCommandIdentify:
				((BYTE *)wCommBufferInactive)[1] = ID_DEVICE_TYPE;
				((BYTE *)wCommBufferInactive)[2] = ID_FIRMWARE_VERSION;
				((BYTE *)wCommBufferInactive)[3] = ID_RESERVED;
#ifdef BUFFER_SWAP
				// we don't know when the master is going to read our response, so we write it to both buffers
				((BYTE *)wCommBuffer)[1] = ID_DEVICE_TYPE;
				((BYTE *)wCommBuffer)[2] = ID_FIRMWARE_VERSION;
				((BYTE *)wCommBuffer)[3] = ID_RESERVED;
#endif // BUFFER_SWAP
				break;
			case kCommandNone:
				break;
			default:
				break;
		}

		// Clear command buffer
		((BYTE *)wCommBufferInactive)[0] = kCommandNone;
#ifdef TIMER16

		while(bTimerTicks <= 0 && EzI2Cs_bBusy_Flag == EzI2Cs_I2C_FREE)
			;
		// BUG: when not in one-shot mode, calling Timer16_1_Stop() here does
		// not produce the desired effect when the duration of the while(1)
		// loop exceeds that of the timer. Setting it to one shot works kind of fine
		Timer16_1_Stop();
#else // TIMER16
		while(EzI2Cs_bBusy_Flag == EzI2Cs_I2C_FREE);	// Wait for master to send another query
#endif // TIMER16
	}
}

// returns a WORD packing two signed chars. The high bytes is the last active sensor in the last centroid,
// while the low byte is the first active sensor of the last centroid
WORD calculateCentroids(WORD *centroidBuffer, WORD *sizeBuffer, BYTE maxNumCentroids, BYTE minSensor, BYTE maxSensor, BYTE numSensors) {
	signed char lastActiveSensor = -1;
	BYTE centroidIndex = 0, sensorIndex, actualHardwareIndex;
	BYTE wrappedAround = 0;
	BYTE inCentroid = 0;
	WORD peakValue = 0, troughDepth = 0;
	BYTE counter;
	long temp;

	WORD lastSensorVal, currentSensorVal, currentWeightedSum, currentUnweightedSum;
	BYTE currentStart, currentLength;

	for(sensorIndex = 0; sensorIndex < maxNumCentroids; sensorIndex++) {
		centroidBuffer[sensorIndex] = 0xFFFF;
		sizeBuffer[sensorIndex] = 0;
	}

	currentSensorVal = 0;

	for(sensorIndex = 0, actualHardwareIndex = minSensor; sensorIndex < numSensors; sensorIndex++)
	{
		lastSensorVal = currentSensorVal;

		currentSensorVal = CSD_waSnsDiff[actualHardwareIndex++];
		if(currentSensorVal > 0) {
			lastActiveSensor = sensorIndex;
		}
		// if we get to the end, and there is more to go, wrap around
		if(actualHardwareIndex == maxSensor)
		{
			actualHardwareIndex = minSensor;
			// once we wrap around, if we find ourselves out of a centroid,
			// any centroids detected after the then current point onwards
			// would be equal or worse than the ones we already got earlier for
			// the same sensors, so we will have to break
			wrappedAround = 1;
		}

		if(inCentroid) {
			// Currently in the middle of a group of sensors constituting a centroid.  Use a zero sample
			// or a spike above a certain magnitude to indicate the end of the centroid.

			if(currentSensorVal == 0) {
				if(currentUnweightedSum > wMinimumCentroidSize)
				{
					temp = ((long)currentWeightedSum << SLIDER_BITS) / currentUnweightedSum;
					centroidBuffer[centroidIndex] = (currentStart << SLIDER_BITS) + (WORD)temp;
					sizeBuffer[centroidIndex] = currentUnweightedSum;
					centroidIndex++;
				}

				inCentroid = 0;
				if(wrappedAround) {
					break;
				}
				if(centroidIndex >= maxNumCentroids)
					break;
				continue;
			}

			if(currentSensorVal > peakValue)  // Keep tabs on max and min values
				peakValue = currentSensorVal;
			if(peakValue - currentSensorVal > troughDepth)
				troughDepth = peakValue - currentSensorVal;

			// If this sensor value is a significant increase over the last one, AND the last one was decreasing, then start a new centroid.
			// In other words, identify a trough in the values and use it to segment into two centroids.

			if(sensorIndex >= 2) {
				if(troughDepth > wAdjacentCentroidNoiseThreshold && currentSensorVal > lastSensorVal + wAdjacentCentroidNoiseThreshold) {
					if(currentUnweightedSum > wMinimumCentroidSize)
					{
						temp = ((long)currentWeightedSum << SLIDER_BITS) / currentUnweightedSum;
						centroidBuffer[centroidIndex] = (currentStart << SLIDER_BITS) + (WORD)temp;
						sizeBuffer[centroidIndex] = currentUnweightedSum;
						centroidIndex++;
					}
					inCentroid = 0;
					if(wrappedAround){
						break;
					}
					if(centroidIndex >= maxNumCentroids)
						break;
					inCentroid = 1;
					currentStart = sensorIndex;
					currentUnweightedSum = peakValue = currentSensorVal;
					currentLength = 1;
					currentWeightedSum = 0;
					troughDepth = 0;
					continue;
				}
			}

			currentUnweightedSum += currentSensorVal;
			currentWeightedSum += currentLength * currentSensorVal;
			currentLength++;
		}
		else {
			// Currently not in a centroid (zeros between centroids).  Look for a new sample to initiate centroid.
			if(currentSensorVal > 0) {
				currentStart = sensorIndex;
				currentUnweightedSum = peakValue = currentSensorVal;
				currentLength = 1;
				currentWeightedSum = 0;
				troughDepth = 0;
				inCentroid = 1;
			}
		}
		if(!inCentroid && wrappedAround){
			break;
		}
  	}

	// Finish up the calculation on the last centroid, if necessary
	if(inCentroid && currentUnweightedSum > wMinimumCentroidSize)
	{
		temp = ((long)currentWeightedSum << SLIDER_BITS) / currentUnweightedSum;
		centroidBuffer[centroidIndex] = (currentStart << SLIDER_BITS) + (WORD)temp;
		sizeBuffer[centroidIndex] = currentUnweightedSum;
		centroidIndex++;
	}

	return (lastActiveSensor << 8) | currentStart;
}


// Handler for timer 16 ISR
void timer_ISR_Handler(void)
{
#ifdef TIMER16
	bTimerTicks++;
#endif
}

