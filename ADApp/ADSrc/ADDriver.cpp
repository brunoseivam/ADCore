/** ADDriver.cpp
 *
 * This is the base class from which actual area detectors are derived.
 *
 * /author Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 20, 2008
 *
 */

#include "ADDriver.h"

using namespace std;
using namespace epics::pvData;
using namespace epics::pvDatabase;

/** Set the shutter position.
  * This method will open (1) or close (0) the shutter if
  * ADShutterMode==ADShutterModeEPICS. Drivers will implement setShutter if they
  * support ADShutterModeDetector. If ADShutterMode=ADShutterModeDetector they will
  * control the shutter directly, else they will call this method.
  * \param[in] open 1 (open) or 0 (closed)
  */
void ADDriver::setShutter (int open)
{
    ADShutterMode_t shutterMode = (ADShutterMode_t) ADShutterMode->get();
    double shutterOpenDelay     = ADShutterOpenDelay->get();
    double shutterCloseDelay    = ADShutterCloseDelay->get();
    double delay                = shutterOpenDelay - shutterCloseDelay;

    switch (shutterMode) {
        case ADShutterModeNone:
            break;
        case ADShutterModeEPICS:
            ADShutterControlEPICS->put(open);
            epicsThreadSleep(delay);
            break;
        case ADShutterModeDetector:
            break;
    }
}

void ADDriver::process (PVRecord const * record)
{
    if(record == ADShutterControl.get())
        setShutter((int)ADShutterControl->get());
    else
        PVNDArrayDriver::process(record);
}

/** All of the arguments are simply passed to the constructor for the
  * pvNDArrayDriver base class. After calling the base class constructor this
  * method sets reasonable default values for all of the parameters defined in
  * ADDriver.h.
  */
ADDriver::ADDriver(string const & prefix, int maxBuffers, size_t maxMemory)
    : PVNDArrayDriver(prefix, maxBuffers, maxMemory)

{
    ADManufacturer          = createParam<string>(ADManufacturerString);
    ADModel                 = createParam<string>(ADModelString);
    ADGain                  = createParam<double>(ADGainString);
    ADBinX                  = createParam<int32>(ADBinXString);
    ADBinY                  = createParam<int32>(ADBinYString);
    ADMinX                  = createParam<int32>(ADMinXString);
    ADMinY                  = createParam<int32>(ADMinYString);
    ADSizeX                 = createParam<int32>(ADSizeXString);
    ADSizeY                 = createParam<int32>(ADSizeYString);
    ADMaxSizeX              = createParam<int32>(ADMaxSizeXString, 1);
    ADMaxSizeY              = createParam<int32>(ADMaxSizeYString, 1);
    ADReverseX              = createParam<boolean>(ADReverseXString);
    ADReverseY              = createParam<boolean>(ADReverseYString);
    ADFrameType             = createParam<int32>(ADFrameTypeString);
    ADImageMode             = createParam<int32>(ADImageModeString);
    ADNumExposures          = createParam<int32>(ADNumExposuresString);
    ADNumExposuresCounter   = createParam<int32>(ADNumExposuresCounterString);
    ADNumImages             = createParam<int32>(ADNumImagesString);
    ADNumImagesCounter      = createParam<int32>(ADNumImagesCounterString, 0);
    ADAcquireTime           = createParam<double>(ADAcquireTimeString);
    ADAcquirePeriod         = createParam<double>(ADAcquirePeriodString);
    ADTimeRemaining         = createParam<double>(ADTimeRemainingString, 0.0);

    vector<string> ADStatusChoices(ADStatusAborted+1);
    ADStatusChoices[ADStatusIdle]           = "Idle";
    ADStatusChoices[ADStatusAcquire]        = "Acquire";
    ADStatusChoices[ADStatusReadout]        = "Readout";
    ADStatusChoices[ADStatusCorrect]        = "Correct";
    ADStatusChoices[ADStatusSaving]         = "Saving";
    ADStatusChoices[ADStatusAborting]       = "Aborting";
    ADStatusChoices[ADStatusError]          = "Error";
    ADStatusChoices[ADStatusWaiting]        = "Waiting";
    ADStatusChoices[ADStatusInitializing]   = "Initializing";
    ADStatusChoices[ADStatusDisconnected]   = "Disconnected";
    ADStatusChoices[ADStatusAborted]        = "Aborted";
    ADStatus                = createEnumParam(ADStatusString, ADStatusChoices);
    ADTriggerMode           = createParam<int32>(ADTriggerModeString);
    ADAcquire               = createParam<int32>(ADAcquireString, 0);
    ADShutterControl        = createParam<int32>(ADShutterControlString);
    ADShutterControlEPICS   = createParam<int32>(ADShutterControlEPICSString);
    ADShutterStatus         = createParam<int32>(ADShutterStatusString, 0);
    ADShutterMode           = createParam<int32>(ADShutterModeString);
    ADShutterOpenDelay      = createParam<double>(ADShutterOpenDelayString);
    ADShutterCloseDelay     = createParam<double>(ADShutterCloseDelayString);
    ADTemperature           = createParam<double>(ADTemperatureString);
    ADTemperatureActual     = createParam<double>(ADTemperatureActualString);
    ADReadStatus            = createParam<int32>(ADReadStatusString);
    ADStatusMessage         = createParam<string>(ADStatusMessageString, "");
    ADStringToServer        = createParam<string>(ADStringToServerString, "");
    ADStringFromServer      = createParam<string>(ADStringFromServerString, "");


    /* Here we set the values of read-only parameters and of read/write parameters that cannot
     * or should not get their values from the database.  Note that values set here will override
     * those in the database for output records because if asyn device support reads a value from 
     * the driver with no error during initialization then it sets the output record to that value.  
     * If a value is not set here then the read request will return an error (uninitialized).
     * Values set here will be overridden by values from save/restore if they exist. */
}
