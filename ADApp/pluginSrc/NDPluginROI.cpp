/*
 * NDPluginROI.cpp
 *
 * Region-of-Interest (ROI) plugin
 * Author: Mark Rivers
 *
 * Created April 23, 2008
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <epicsString.h>
#include <epicsMutex.h>
#include <iocsh.h>

#include <epicsExport.h>
#include "NDPluginDriver.h"
#include "NDPluginROI.h"

#define MAX(A,B) (A)>(B)?(A):(B)
#define MIN(A,B) (A)<(B)?(A):(B)

using namespace std;
using namespace epics::pvData;
using namespace epics::pvDatabase;


/** Callback function that is called by the NDArray driver with new NDArray data.
  * Extracts the NDArray data into each of the ROIs that are being used.
  * \param[in] pArray  The NDArray from the callback.
  */
void NDPluginROI::processCallbacks(NDArrayPtr pArray)
{
    /* Get all parameters while we have the mutex */
    vector<NDDimension_t> dims(3);
    dims[0].binning = NDPluginROIDim0Bin->get();
    dims[1].binning = NDPluginROIDim1Bin->get();
    dims[2].binning = NDPluginROIDim2Bin->get();
    dims[0].reverse = NDPluginROIDim0Reverse->get();
    dims[1].reverse = NDPluginROIDim1Reverse->get();
    dims[2].reverse = NDPluginROIDim2Reverse->get();

    bool enableDim[3];
    enableDim[0] = NDPluginROIDim0Enable->get();
    enableDim[1] = NDPluginROIDim1Enable->get();
    enableDim[2] = NDPluginROIDim2Enable->get();

    bool autoSize[3];
    autoSize[0] = NDPluginROIDim0AutoSize->get();
    autoSize[1] = NDPluginROIDim1AutoSize->get();
    autoSize[2] = NDPluginROIDim2AutoSize->get();

    int dataTypeInt = NDPluginROIDataType->get();
    bool enableScale = NDPluginROIEnableScale->get();
    double scale = NDPluginROIScale->get();

    /* Call the base class method */
    NDPluginDriver::processCallbacks(pArray);
    
    /* Get information about the array */
    NDArrayInfo_t arrayInfo = pArray->getInfo();

    size_t userDims[3];
    userDims[0] = arrayInfo.x.dim;
    userDims[1] = arrayInfo.y.dim;
    userDims[2] = arrayInfo.color.dim;

    /* Make sure dimensions are valid, fix them if they are not */
    vector<NDDimension_t> arrayDims(pArray->getDimensions());
    for (size_t dim = 0; dim < arrayDims.size(); dim++) {
        NDDimension_t *pDim = &dims[dim];
        if (enableDim[dim]) {
            size_t newDimSize = arrayDims[userDims[dim]].size;
            pDim->offset  = requestedOffset_[dim];
            pDim->size    = requestedSize_[dim];
            pDim->offset  = MAX(pDim->offset,  0);
            pDim->offset  = MIN(pDim->offset,  newDimSize-1);
            if (autoSize[dim]) pDim->size = newDimSize;
            pDim->size    = MAX(pDim->size,    1);
            pDim->size    = MIN(pDim->size,    newDimSize - pDim->offset);
            pDim->binning = MAX(pDim->binning, 1);
            pDim->binning = MIN(pDim->binning, pDim->size);
        } else {
            pDim->offset  = 0;
            pDim->size    = arrayDims[userDims[dim]].size;
            pDim->binning = 1;
        }
    }

    /* Update the parameters that may have changed */
    NDPluginROIDim0MaxSize->put(0);
    NDPluginROIDim1MaxSize->put(0);
    NDPluginROIDim2MaxSize->put(0);

    if (arrayDims.size() > 0) {
        NDDimension_t *pDim = &dims[0];
        NDPluginROIDim0MaxSize->put(arrayDims[userDims[0]].size);
        if (enableDim[0]) {
            NDPluginROIDim0Min->put(pDim->offset);
            NDPluginROIDim0Size->put(pDim->size);
            NDPluginROIDim0Bin->put(pDim->binning);
        }
    }
    if (arrayDims.size() > 1) {
        NDDimension_t *pDim = &dims[1];
        NDPluginROIDim0MaxSize->put(arrayDims[userDims[1]].size);
        if (enableDim[1]) {
            NDPluginROIDim1Min->put(pDim->offset);
            NDPluginROIDim1Size->put(pDim->size);
            NDPluginROIDim1Bin->put(pDim->binning);
        }
    }
    if (arrayDims.size() > 2) {
        NDDimension_t *pDim = &dims[2];
        NDPluginROIDim0MaxSize->put(arrayDims[userDims[2]].size);
        if (enableDim[2]) {
            NDPluginROIDim2Min->put(pDim->offset);
            NDPluginROIDim2Size->put(pDim->size);
            NDPluginROIDim2Bin->put(pDim->binning);
        }
    }

    /* This function is called with the lock taken, and it must be set when we exit.
     * The following code can be exected without the mutex because we are not accessing memory
     * that other threads can access. */
    unlock();

    /* Extract this ROI from the input array.  The convert() function allocates
     * a new array and it is reserved (reference count = 1) */
    ScalarType dataType;
    if (dataTypeInt == -1)
        dataType = pArray->getDataType();
    else
        dataType = static_cast<ScalarType>(dataTypeInt);

    /* We treat the case of RGB1 data specially, so that NX and NY are the X and Y dimensions of the
     * image, not the first 2 dimensions.  This makes it much easier to switch back and forth between
     * RGB1 and mono mode when using an ROI. */
    if (arrayInfo.colorMode == NDColorModeRGB1) {
        NDDimension_t tempDim = dims[0];
        dims[0] = dims[2];
        dims[2] = dims[1];
        dims[1] = tempDim;
    }
    else if (arrayInfo.colorMode == NDColorModeRGB2) {
        NDDimension_t tempDim = dims[1];
        dims[1] = dims[2];
        dims[2] = tempDim;
    }
    
    // Trim dims
    dims.resize(arrayDims.size());

    NDArrayPtr pOutput;
    if (enableScale && (scale != 0) && (scale != 1)) {
        /* This is tricky.  We want to do the operation to avoid errors due to integer truncation.
         * For example, if an image with all pixels=1 is binned 3x3 with scale=9 (divide by 9), then
         * the output should also have all pixels=1. 
         * We do this by extracting the ROI and converting to double, do the scaling, then convert
         * to the desired data type. */
        NDArrayPtr pScratch(mNDArrayPool->convert(pArray, pvDouble, dims));
        NDArrayInfo_t scratchInfo = pScratch->getInfo();

        shared_vector<double> pData(pScratch->getData<double>());
        for (size_t i=0; i < scratchInfo.nElements; i++) pData[i] = pData[i]/scale;
        pScratch->setData(pData);
        pOutput = mNDArrayPool->convert(pScratch, dataType);
    } 
    else {        
        pOutput = mNDArrayPool->convert(pArray, dataType, dims);
    }

    vector<NDDimension_t> outputDims(pOutput->getDimensions());

    /* If we selected just one color from the array, then we need to change the
     * dimensions and the color mode */
    NDColorMode_t colorMode = NDColorModeMono;
    if ((outputDims.size() == 3) &&
        (arrayInfo.colorMode == NDColorModeRGB1) && 
        (outputDims[0].size == 1))
    {
        outputDims[0] = outputDims[1];
        outputDims[1] = outputDims[2];
        pOutput->setDimensions(outputDims);
        //pOutput->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);
    }
    else if ((outputDims.size() == 3) &&
        (arrayInfo.colorMode == NDColorModeRGB2) && 
        (outputDims[1].size == 1))
    {
        outputDims[1] = outputDims[2];
        outputDims.resize(2);
        pOutput->setDimensions(outputDims);
        //pOutput->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);
    }
    else if ((outputDims.size() == 3) &&
        (outputDims[2].size == 1))
    {
        outputDims.resize(2);
        pOutput->setDimensions(outputDims);
        //pOutput->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);
    }
    lock();

    /* Set the image size of the ROI image data */
    NDArraySizeX->put(outputDims.size() > 0 ? outputDims[userDims[0]].size : 0);
    NDArraySizeY->put(outputDims.size() > 1 ? outputDims[userDims[1]].size : 0);
    NDArraySizeZ->put(outputDims.size() > 2 ? outputDims[userDims[2]].size : 0);

    NDArrayData->put(pOutput->getArray());
    /* Get the attributes for this driver */
    //this->getAttributes(this->pArrays[0]->pAttributeList);
    /* Call any clients who have registered for NDArray callbacks */
    //this->unlock();
    //doCallbacksGenericPointer(this->pArrays[0], NDArrayData, 0);
    /* We must enter the loop and exit with the mutex locked */
    //this->lock();
    //callParamCallbacks();

}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including NDPluginDriverEnableCallbacks and
  * NDPluginDriverArrayAddr.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
void NDPluginROI::process (PVRecord const * record)
{
    if        (record == NDPluginROIDim0Min.get()) {
        requestedOffset_[0] = NDPluginROIDim0Min->get();
    } else if (record == NDPluginROIDim1Min.get()) {
        requestedOffset_[1] = NDPluginROIDim1Min->get();
    } else if (record == NDPluginROIDim2Min.get()) {
        requestedOffset_[2] = NDPluginROIDim2Min->get();
    } else if (record == NDPluginROIDim0Size.get()) {
        requestedSize_[0] = NDPluginROIDim0Size->get();
    } else if (record == NDPluginROIDim1Size.get()) {
        requestedSize_[1] = NDPluginROIDim1Size->get();
    } else if (record == NDPluginROIDim2Size.get()) {
        requestedSize_[2] = NDPluginROIDim2Size->get();
    } else {
        NDPluginDriver::process(record);
    }
}

/** Constructor for NDPluginROI; most parameters are simply passed to NDPluginDriver::NDPluginDriver.
  * After calling the base class constructor this method sets reasonable default values for all of the
  * ROI parameters.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] queueSize The number of NDArrays that the input queue for this plugin can hold when
  *            NDPluginDriverBlockingCallbacks=0.  Larger queues can decrease the number of dropped arrays,
  *            at the expense of more NDArray buffers being allocated from the underlying driver's NDArrayPool.
  * \param[in] blockingCallbacks Initial setting for the NDPluginDriverBlockingCallbacks flag.
  *            0=callbacks are queued and executed by the callback thread; 1 callbacks execute in the thread
  *            of the driver doing the callbacks.
  * \param[in] NDArrayPort Name of asyn port driver for initial source of NDArray callbacks.
  * \param[in] NDArrayAddr asyn port driver address for initial source of NDArray callbacks.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
NDPluginROI::NDPluginROI(string const & prefix, unsigned int queueSize,
                         bool blockingCallbacks, string const & NDArrayProvider,
                         string const & NDArrayPV, int maxBuffers, size_t maxMemory,
                         int stackSize)
    /* Invoke the base class constructor */
    : NDPluginDriver(prefix, queueSize, blockingCallbacks, NDArrayProvider, NDArrayPV,
                     maxBuffers, maxMemory, stackSize)
{
    //static const char *functionName = "NDPluginROI";

    /* ROI general parameters */
    NDPluginROIName              = createParam<string>(NDPluginROINameString);

     /* ROI definition */
    NDPluginROIDim0Min           = createParam<int32>(NDPluginROIDim0MinString, 0);
    NDPluginROIDim1Min           = createParam<int32>(NDPluginROIDim1MinString, 0);
    NDPluginROIDim2Min           = createParam<int32>(NDPluginROIDim2MinString, 0);
    NDPluginROIDim0Size          = createParam<int32>(NDPluginROIDim0SizeString, 1000000);
    NDPluginROIDim1Size          = createParam<int32>(NDPluginROIDim1SizeString, 1000000);
    NDPluginROIDim2Size          = createParam<int32>(NDPluginROIDim2SizeString, 1000000);
    NDPluginROIDim0MaxSize       = createParam<int32>(NDPluginROIDim0MaxSizeString);
    NDPluginROIDim1MaxSize       = createParam<int32>(NDPluginROIDim1MaxSizeString);
    NDPluginROIDim2MaxSize       = createParam<int32>(NDPluginROIDim2MaxSizeString);
    NDPluginROIDim0Bin           = createParam<int32>(NDPluginROIDim0BinString, 1);
    NDPluginROIDim1Bin           = createParam<int32>(NDPluginROIDim1BinString, 1);
    NDPluginROIDim2Bin           = createParam<int32>(NDPluginROIDim2BinString, 1);
    NDPluginROIDim0Reverse       = createParam<boolean>(NDPluginROIDim0ReverseString, false);
    NDPluginROIDim1Reverse       = createParam<boolean>(NDPluginROIDim1ReverseString, false);
    NDPluginROIDim2Reverse       = createParam<boolean>(NDPluginROIDim2ReverseString, false);
    NDPluginROIDim0Enable        = createParam<boolean>(NDPluginROIDim0EnableString, true);
    NDPluginROIDim1Enable        = createParam<boolean>(NDPluginROIDim1EnableString, true);
    NDPluginROIDim2Enable        = createParam<boolean>(NDPluginROIDim2EnableString, true);
    NDPluginROIDim0AutoSize      = createParam<boolean>(NDPluginROIDim0AutoSizeString, false);
    NDPluginROIDim1AutoSize      = createParam<boolean>(NDPluginROIDim1AutoSizeString, false);
    NDPluginROIDim2AutoSize      = createParam<boolean>(NDPluginROIDim2AutoSizeString, false);
    NDPluginROIDataType          = createParam<int32>(NDPluginROIDataTypeString, -1);
    NDPluginROIEnableScale       = createParam<boolean>(NDPluginROIEnableScaleString, false);
    NDPluginROIScale             = createParam<double>(NDPluginROIScaleString, 1.0);

    /* Set the plugin type string */
    NDPluginDriverPluginType->put("NDPluginROI");

    // Enable ArrayCallbacks.  
    // This plugin currently ignores this setting and always does callbacks, so make the setting reflect the behavior
    NDArrayCallbacks->put(1);

    requestedOffset_[0] = NDPluginROIDim0Min->get();
    requestedOffset_[1] = NDPluginROIDim1Min->get();
    requestedOffset_[2] = NDPluginROIDim2Min->get();

    requestedSize_[0] = NDPluginROIDim0Size->get();
    requestedSize_[1] = NDPluginROIDim1Size->get();
    requestedSize_[2] = NDPluginROIDim2Size->get();

    /* Try to connect to the array port */
    connectToArrayPort();
}

/** Configuration command */
extern "C" int NDROIConfigure(const char *prefix, int queueSize, int blockingCallbacks,
                              const char *NDArrayProvider, const char *NDArrayPV,
                              int maxBuffers, size_t maxMemory, int stackSize)
{
    new NDPluginROI(prefix, queueSize, blockingCallbacks, NDArrayProvider, NDArrayPV,
                    maxBuffers, maxMemory, stackSize);
    return(0);
}

/* EPICS iocsh shell commands */
static const iocshArg initArg0 = { "prefix",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArrayProvider",iocshArgString};
static const iocshArg initArg4 = { "NDArrayPV",iocshArgString};
static const iocshArg initArg5 = { "maxBuffers",iocshArgInt};
static const iocshArg initArg6 = { "maxMemory",iocshArgInt};
static const iocshArg initArg7 = { "stackSize",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6,
                                            &initArg7};
static const iocshFuncDef initFuncDef = {"NDROIConfigure",7,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    NDROIConfigure(args[0].sval, args[1].ival, args[2].ival,
                   args[3].sval, args[4].sval, args[5].ival,
                   args[6].ival, args[7].ival);
}

extern "C" void NDROIRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

extern "C" {
epicsExportRegistrar(NDROIRegister);
}
