#ifndef ADDRIVER_H
#define ADDRIVER_H

#include <epicsTypes.h>
#include <epicsMessageQueue.h>
#include <epicsTime.h>

#include "pvNDArrayDriver.h"

/** Success code; generally asyn status codes are used instead where possible */
#define AREA_DETECTOR_OK 0
/** Failure code; generally asyn status codes are used instead where possible */
#define AREA_DETECTOR_ERROR -1

/** Enumeration of shutter status */
typedef enum
{
    ADShutterClosed,    /**< Shutter closed */
    ADShutterOpen       /**< Shutter open */
} ADShutterStatus_t;

/** Enumeration of shutter modes */
typedef enum
{
    ADShutterModeNone,      /**< Don't use shutter */
    ADShutterModeEPICS,     /**< Shutter controlled by EPICS PVs */
    ADShutterModeDetector   /**< Shutter controlled directly by detector */
} ADShutterMode_t;

/** Enumeration of detector status */
typedef enum
{
    ADStatusIdle,         /**< Detector is idle */
    ADStatusAcquire,      /**< Detector is acquiring */
    ADStatusReadout,      /**< Detector is reading out */
    ADStatusCorrect,      /**< Detector is correcting data */
    ADStatusSaving,       /**< Detector is saving data */
    ADStatusAborting,     /**< Detector is aborting an operation */
    ADStatusError,        /**< Detector has encountered an error */
    ADStatusWaiting,      /**< Detector is waiting for something, typically for the acquire period to elapse */
    ADStatusInitializing, /**< Detector is initializing, typically at startup */
    ADStatusDisconnected, /**< Detector is not connected */
    ADStatusAborted       /**< Detector acquisition has been aborted.*/
} ADStatus_t;

/** Enumeration of image collection modes */
typedef enum
{
    ADImageSingle,      /**< Collect a single image per Acquire command */
    ADImageMultiple,    /**< Collect ADNumImages images per Acquire command */
    ADImageContinuous   /**< Collect images continuously until Acquire is set to 0 */
} ADImageMode_t;

/* Enumeration of frame types */
typedef enum
{
    ADFrameNormal,          /**< Normal frame type */
    ADFrameBackground,      /**< Background frame type */
    ADFrameFlatField,       /**< Flat field (no sample) frame type */
    ADFrameDoubleCorrelation      /**< Double correlation frame type, used to remove zingers */
} ADFrameType_t;

/* Enumeration of trigger modes */
typedef enum
{
    ADTriggerInternal,      /**< Internal trigger from detector */
    ADTriggerExternal       /**< External trigger input */
} ADTriggerMode_t;

/** Strings defining parameters that affect the behaviour of the detector. 
  * These are the values passed to drvUserCreate. 
  * The driver will place in pasynUser->reason an integer to be used when the
  * standard asyn interface methods are called. */
 /*                                 String                          data type  access   Description  */
#define ADManufacturerString        "Manufacturer_RBV"          /**< (string,   r/o) Detector manufacturer name */
#define ADModelString               "Model_RBV"                 /**< (string,   r/o) Detector model name */

#define ADGainString                "Gain"                      /**< (double,   r/w) Gain */

    /* Parameters that control the detector binning */
#define ADBinXString                "BinX"                      /**< (int32,    r/w) Binning in the X direction */
#define ADBinYString                "BinY"                      /**< (int32,    r/w) Binning in the Y direction */

    /* Parameters the control the region of the detector to be read out.
    * ADMinX, ADMinY, ADSizeX, and ADSizeY are in unbinned pixel units */
#define ADMinXString                "MinX"                      /**< (int32,    r/w) First pixel in the X direction; 0 is the first pixel on the detector */
#define ADMinYString                "MinY"                      /**< (int32,    r/w) First pixel in the Y direction; 0 is the first pixel on the detector */
#define ADSizeXString               "SizeX"                     /**< (int32,    r/w) Size of the region to read in the X direction */
#define ADSizeYString               "SizeY"                     /**< (int32,    r/w) Size of the region to read in the Y direction */
#define ADMaxSizeXString            "MaxSizeX"                  /**< (int32,    r/o) Maximum (sensor) size in the X direction */
#define ADMaxSizeYString            "MaxSizeY"                  /**< (int32,    r/o) Maximum (sensor) size in the Y direction */

    /* Parameters that control the orientation of the image */
#define ADReverseXString            "ReverseX"                  /**< (int32,    r/w) Reverse image in the X direction (0=No, 1=Yes) */
#define ADReverseYString            "ReverseY"                  /**< (int32,    r/w) Reverse image in the Y direction (0=No, 1=Yes) */

    /* Parameters defining the acquisition parameters. */
#define ADFrameTypeString           "FrameType"                 /**< (int32,    r/w) Frame type (ADFrameType_t) */
#define ADImageModeString           "ImageMOde"                 /**< (int32,    r/w) Image mode (ADImageMode_t) */
#define ADTriggerModeString         "TriggerMode"               /**< (int32,    r/w) Trigger mode (ADTriggerMode_t) */
#define ADNumExposuresString        "NumExposures"              /**< (int32,    r/w) Number of exposures per image to acquire */
#define ADNumImagesString           "NumImages"                 /**< (int32,    r/w) Number of images to acquire in one acquisition sequence */
#define ADAcquireTimeString         "AcquireTime"               /**< (double,   r/w) Acquisition time per image */
#define ADAcquirePeriodString       "AcquirePeriod"             /**< (double,   r/w) Acquisition period between images */
#define ADStatusString              "DetectorState"             /**< (enum,     r/o) Acquisition status (ADStatus_t) */
#define ADAcquireString             "Acquire"                   /**< (int32,    r/w) Start(1) or Stop(0) acquisition */

    /* Shutter parameters */
#define ADShutterControlString      "ShutterControl"            /**< (int32,    r/w) (ADShutterStatus_t) Open (1) or Close(0) shutter */
#define ADShutterControlEPICSString "ShutterControlEPICS"       /**< (int32,    r/o) (ADShutterStatus_t) Open (1) or Close(0) EPICS shutter */
#define ADShutterStatusString       "ShutterStatus_RBV"         /**< (int32,    r/o) (ADShutterStatus_t) Shutter Open (1) or Closed(0) */
#define ADShutterModeString         "ShutterMode"               /**< (int32,    r/w) (ADShutterMode_t) Use EPICS or detector shutter */
#define ADShutterOpenDelayString    "ShutterOpenDelay"          /**< (double,   r/w) Time for shutter to open */
#define ADShutterCloseDelayString   "ShutterCloseDelay"         /**< (double,   r/w) Time for shutter to close */

    /* Temperature parameters */
#define ADTemperatureString         "Temperature"               /**< (double,   r/w) Detector temperature */
#define ADTemperatureActualString   "TemperatureActual"         /**< (double,   r/o) Actual detector temperature */

    /* Statistics on number of images collected and the image rate */
#define ADNumImagesCounterString    "NumImagesCounter_RBV"      /**< (int32,    r/o) Number of images collected in current acquisition sequence */
#define ADNumExposuresCounterString "NumExposuresCounter_RBV"   /**< (int32,    r/o) Number of exposures collected for current image */
#define ADTimeRemainingString       "TimeRemaining_RBV"         /**< (double,   r/o) Acquisition time remaining */

    /* Status reading */
#define ADReadStatusString          "ReadStatus"                /**< (int32,    r/w) Write 1 to force a read of detector status */

    /* Status message strings */
#define ADStatusMessageString       "StatusMessage_RBV"         /**< (string,   r/o) Status message */
#define ADStringToServerString      "StringToServer_RBV"        /**< (string,   r/o) String sent to server for message-based drivers */
#define ADStringFromServerString    "StringFromServer_RBV"      /**< (string,   r/o) String received from server for message-based drivers */

/** Class from which areaDetector drivers are directly derived. */
class epicsShareClass ADDriver : public PVNDArrayDriver
{
public:
    /* This is the constructor for the class. */
    ADDriver(std::string const & prefix, int maxBuffers, size_t maxMemory);

    /* These are the methods that we override from pvPortDriver */
    virtual void process (epics::pvDatabase::PVRecord const * record);

    /* These are the methods that are new to this class */
    virtual void setShutter (int open);

    virtual ~ADDriver(){}

protected:
    epics::pvPortDriver::StringParamPtr  ADManufacturer;
    epics::pvPortDriver::StringParamPtr  ADModel;
    epics::pvPortDriver::DoubleParamPtr  ADGain;
    epics::pvPortDriver::IntParamPtr     ADBinX;
    epics::pvPortDriver::IntParamPtr     ADBinY;
    epics::pvPortDriver::IntParamPtr     ADMinX;
    epics::pvPortDriver::IntParamPtr     ADMinY;
    epics::pvPortDriver::IntParamPtr     ADSizeX;
    epics::pvPortDriver::IntParamPtr     ADSizeY;
    epics::pvPortDriver::IntParamPtr     ADMaxSizeX;
    epics::pvPortDriver::IntParamPtr     ADMaxSizeY;
    epics::pvPortDriver::BooleanParamPtr ADReverseX;
    epics::pvPortDriver::BooleanParamPtr ADReverseY;
    epics::pvPortDriver::IntParamPtr     ADFrameType;
    epics::pvPortDriver::IntParamPtr     ADImageMode;
    epics::pvPortDriver::IntParamPtr     ADNumExposures;
    epics::pvPortDriver::IntParamPtr     ADNumExposuresCounter;
    epics::pvPortDriver::IntParamPtr     ADNumImages;
    epics::pvPortDriver::IntParamPtr     ADNumImagesCounter;
    epics::pvPortDriver::DoubleParamPtr  ADAcquireTime;
    epics::pvPortDriver::DoubleParamPtr  ADAcquirePeriod;
    epics::pvPortDriver::DoubleParamPtr  ADTimeRemaining;
    epics::pvPortDriver::EnumParamPtr    ADStatus;
    epics::pvPortDriver::IntParamPtr     ADTriggerMode;
    epics::pvPortDriver::IntParamPtr     ADAcquire;
    epics::pvPortDriver::IntParamPtr     ADShutterControl;
    epics::pvPortDriver::IntParamPtr     ADShutterControlEPICS;
    epics::pvPortDriver::IntParamPtr     ADShutterStatus;
    epics::pvPortDriver::IntParamPtr     ADShutterMode;
    epics::pvPortDriver::DoubleParamPtr  ADShutterOpenDelay;
    epics::pvPortDriver::DoubleParamPtr  ADShutterCloseDelay;
    epics::pvPortDriver::DoubleParamPtr  ADTemperature;
    epics::pvPortDriver::DoubleParamPtr  ADTemperatureActual;
    epics::pvPortDriver::IntParamPtr     ADReadStatus;
    epics::pvPortDriver::StringParamPtr  ADStatusMessage;
    epics::pvPortDriver::StringParamPtr  ADStringToServer;
    epics::pvPortDriver::StringParamPtr  ADStringFromServer;
};

#endif
