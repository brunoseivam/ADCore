#ifndef PVNDARRAYDRIVER_H
#define PVNDARRAYDRIVER_H

#include <pv/pvPortDriver.h>
#include <pv/pvData.h>

#include "NDArrayPool.h"

/** Maximum length of a filename or any of its components */
#define MAX_FILENAME_LEN 256

/** Enumeration of file saving modes */
typedef enum {
    NDFileModeSingle,       /**< Write 1 array per file */
    NDFileModeCapture,      /**< Capture NDNumCapture arrays into memory, write them out when capture is complete.
                              *  Write all captured arrays to a single file if the file format supports this */
    NDFileModeStream        /**< Stream arrays continuously to a single file if the file format supports this */
} NDFileMode_t;

typedef enum {
    NDFileWriteOK,
    NDFileWriteError
} NDFileWriteStatus_t;

/** Strings defining parameters that affect the behaviour of the detector. 
  * These are the values passed to drvUserCreate. 
  * The driver will place in pasynUser->reason an integer to be used when the
  * standard asyn interface methods are called. */
 /*                             String                       data type     access   Description  */
#define NDPortNameSelfString    "PortName_RBV"          /**< (string,       r/o) Asyn port name of this driver instance */

/** ADCore version string */
#define NDADCoreVersionString   "ADCoreVersion_RBV"     /**< (string,       r/o) Version of ADCore */

/* Parameters defining characteristics of the array data from the detector.
 * NDArraySizeX and NDArraySizeY are the actual dimensions of the array data,
 * including effects of the region definition and binning */
#define NDArraySizeXString      "ArraySizeX_RBV"        /**< (int32,        r/o) Size of the array data in the X direction */
#define NDArraySizeYString      "ArraySizeY_RBV"        /**< (int32,        r/o) Size of the array data in the Y direction */
#define NDArraySizeZString      "ArraySizeZ_RBV"        /**< (int32,        r/o) Size of the array data in the Z direction */
#define NDArraySizeString       "ArraySize_RBV"         /**< (int32,        r/o) Total size of array data in bytes */
#define NDNDimensionsString     "NDimensions_RBV"       /**< (int32,        r/o) Number of dimensions in array */
#define NDDimensionsString      "Dimensions_RBV"        /**< (int32Array,   r/o) Array dimensions */
#define NDDataTypeString        "DataType"              /**< (int32,        r/w) Data type (NDDataType_t) */
#define NDColorModeString       "ColorMode"             /**< (int32,        r/w) Color mode (NDColorMode_t) */
#define NDUniqueIdString        "UniqueId_RBV"          /**< (int32,        r/o) Unique ID number of array */
#define NDTimeStampString       "TimeStamp_RBV"         /**< (double,       r/o) Time stamp of array */
#define NDEpicsTSSecString      "EpicsTSSec_RBV"        /**< (int32,        r/o) EPICS time stamp secPastEpoch of array */
#define NDEpicsTSNsecString     "EpicsTSNsec_RBV"       /**< (int32,        r/o) EPICS time stamp nsec of array */
#define NDBayerPatternString    "BayerPattern_RBV"      /**< (int32,        r/o) Bayer pattern of array  (from bayerPattern array attribute if present) */

/* Statistics on number of arrays collected */
#define NDArrayCounterString    "ArrayCounter"          /**< (int32,        r/w) Number of arrays since last reset */

/* File name related parameters for saving data.
 * Drivers are not required to implement file saving, but if they do these parameters
 * should be used.
 * The driver will normally combine NDFilePath, NDFileName, and NDFileNumber into
 * a file name that order using the format specification in NDFileTemplate.
 * For example NDFileTemplate might be "%s%s_%d.tif" */
#define NDFilePathString        "FilePath"              /**< (string,       r/w) The file path */
#define NDFilePathExistsString  "FilePathExists"        /**< (int32,        r/w) File path exists? */
#define NDFileNameString        "FileName"              /**< (string,       r/w) The file name */
#define NDFileNumberString      "FileNumber"            /**< (int32,        r/w) The next file number */
#define NDFileTemplateString    "FileTemplate"          /**< (string,       r/w) The file format template; C-style format string */
#define NDAutoIncrementString   "AutoIncrement"         /**< (int32,        r/w) Autoincrement file number */
#define NDFullFileNameString    "FullFileName_RBV"      /**< (string,       r/o) The actual complete file name for the last file saved */
#define NDFileFormatString      "FileFormat"            /**< (int32,        r/w) The data format to use for saving the file.  */
#define NDAutoSaveString        "AutoSave"              /**< (int32,        r/w) Automatically save files */
#define NDWriteFileString       "WriteFile"             /**< (int32,        r/w) Manually save the most recent array to a file when value=1 */
#define NDReadFileString        "ReadFile"              /**< (int32,        r/w) Manually read file when value=1 */
#define NDFileWriteModeString   "FileWriteMode"         /**< (int32,        r/w) File saving mode (NDFileMode_t) */
#define NDFileWriteStatusString "WriteStatus"           /**< (int32,        r/w) File write status */
#define NDFileWriteMessageString "WriteMessage"         /**< (string,       r/w) File write message */
#define NDFileNumCaptureString  "NumCapture"            /**< (int32,        r/w) Number of arrays to capture */
#define NDFileNumCapturedString "NumCaptured_RBV"       /**< (int32,        r/o) Number of arrays already captured */
#define NDFileCaptureString     "Capture"               /**< (int32,        r/w) Start or stop capturing arrays */
#define NDFileDeleteDriverFileString  "DeleteDriverFile"/**< (int32,        r/w) Delete driver file */
#define NDFileLazyOpenString    "LazyOpen"              /**< (int32,        r/w) Don't open file until first frame arrives in Stream mode */
#define NDFileCreateDirString   "CreateDirectory"       /**< (int32,        r/w) Create the target directory up to this depth */
#define NDFileTempSuffixString  "TempSuffix"            /**< (string,       r/w) Temporary filename suffix while writing data to file. The file will be renamed (suffix removed) upon closing the file. */

#define NDAttributesFileString  "NDAttributesFile"      /**< (string,       r/w) Attributes file name */

/* The detector array data */
#define NDArrayDataString       "ArrayData"             /**< (NDArray,      r/w) NDArray data */
#define NDArrayCallbacksString  "ArrayCallbacks"        /**< (int32,        r/w) Do callbacks with array data (0=No, 1=Yes) */

/* NDArray Pool status */
#define NDPoolMaxBuffersString      "PoolMaxBuffers"
#define NDPoolAllocBuffersString    "PoolAllocBuffers"
#define NDPoolFreeBuffersString     "PoolFreeBuffers"
#define NDPoolMaxMemoryString       "PoolMaxMem"
#define NDPoolUsedMemoryString      "PoolUsedMem"

/** This is the class from which NDArray drivers are derived; implements the asynGenericPointer functions
  * for NDArray objects.
  * For areaDetector, both plugins and detector drivers are indirectly derived from this class.
  * PVNDArrayDriver inherits from PVPortDriver.
  */
class epicsShareFunc PVNDArrayDriver : public epics::pvPortDriver::PVPortDriver
{
public:
    PVNDArrayDriver (std::string const & prefix, int maxBuffers, size_t maxMemory);
    virtual ~PVNDArrayDriver() {}
    bool checkPath (void);
    int createFilePath (const char *path, int pathDepth);
    int createFileName (int maxChars, char *fullFileName);
    int createFileName (int maxChars, char *filePath, char *fileName);
    //virtual int readNDAttributesFile(const char *fileName);
    //virtual int getAttributes(NDAttributeList *pAttributeList);
    virtual void process (epics::pvDatabase::PVRecord const * record);
    virtual void report (FILE *fp, int details);

protected:
    epics::pvPortDriver::StringParamPtr  NDPortNameSelf;
    epics::pvPortDriver::StringParamPtr  NDADCoreVersion;
    epics::pvPortDriver::IntParamPtr     NDArraySizeX;
    epics::pvPortDriver::IntParamPtr     NDArraySizeY;
    epics::pvPortDriver::IntParamPtr     NDArraySizeZ;
    epics::pvPortDriver::IntParamPtr     NDArraySize;
    epics::pvPortDriver::IntParamPtr     NDNDimensions;
    epics::pvPortDriver::IntParamPtr     NDDimensions;
    epics::pvPortDriver::IntParamPtr     NDDataType;
    epics::pvPortDriver::IntParamPtr     NDColorMode;
    epics::pvPortDriver::IntParamPtr     NDUniqueId;
    epics::pvPortDriver::DoubleParamPtr  NDTimeStamp;
    epics::pvPortDriver::IntParamPtr     NDEpicsTSSec;
    epics::pvPortDriver::IntParamPtr     NDEpicsTSNsec;
    epics::pvPortDriver::IntParamPtr     NDBayerPattern;
    epics::pvPortDriver::IntParamPtr     NDArrayCounter;
    epics::pvPortDriver::StringParamPtr  NDFilePath;
    epics::pvPortDriver::BooleanParamPtr NDFilePathExists;
    epics::pvPortDriver::StringParamPtr  NDFileName;
    epics::pvPortDriver::IntParamPtr     NDFileNumber;
    epics::pvPortDriver::StringParamPtr  NDFileTemplate;
    epics::pvPortDriver::BooleanParamPtr NDAutoIncrement;
    epics::pvPortDriver::StringParamPtr  NDFullFileName;
    epics::pvPortDriver::IntParamPtr     NDFileFormat;
    epics::pvPortDriver::BooleanParamPtr NDAutoSave;
    epics::pvPortDriver::IntParamPtr     NDWriteFile;
    epics::pvPortDriver::IntParamPtr     NDReadFile;
    epics::pvPortDriver::IntParamPtr     NDFileWriteMode;
    epics::pvPortDriver::IntParamPtr     NDFileWriteStatus;
    epics::pvPortDriver::StringParamPtr  NDFileWriteMessage;
    epics::pvPortDriver::IntParamPtr     NDFileNumCapture;
    epics::pvPortDriver::IntParamPtr     NDFileNumCaptured;
    epics::pvPortDriver::IntParamPtr     NDFileCapture;
    epics::pvPortDriver::IntParamPtr     NDFileDeleteDriverFile;
    epics::pvPortDriver::IntParamPtr     NDFileLazyOpen;
    epics::pvPortDriver::IntParamPtr     NDFileCreateDir;
    epics::pvPortDriver::StringParamPtr  NDFileTempSuffix;
    epics::pvPortDriver::StringParamPtr  NDAttributesFile;
    epics::pvPortDriver::NDArrayParamPtr NDArrayData;
    epics::pvPortDriver::IntParamPtr     NDArrayCallbacks;
    epics::pvPortDriver::IntParamPtr     NDPoolMaxBuffers;
    epics::pvPortDriver::IntParamPtr     NDPoolAllocBuffers;
    epics::pvPortDriver::IntParamPtr     NDPoolFreeBuffers;
    epics::pvPortDriver::DoubleParamPtr  NDPoolMaxMemory;
    epics::pvPortDriver::DoubleParamPtr  NDPoolUsedMemory;

    NDArrayPoolPtr mNDArrayPool;
    //NDAttributeList *mAttributeList;
};


#endif
