/*
 * NDPluginDriver.cpp
 * 
 * Asyn driver for callbacks to save area detector data to files.
 *
 * Author: Mark Rivers
 *
 * Created April 5, 2008
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <epicsTypes.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <cantProceed.h>

#include <asynDriver.h>

#include <pv/clientFactory.h>

#include <epicsExport.h>
#include "NDPluginDriver.h"

using std::string;
using std::tr1::shared_ptr;
using std::vector;
using std::multiset;

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvDatabase;
using namespace epics::nt;

typedef enum {
    ToThreadMessageData,
    ToThreadMessageExit
} ToThreadMessageType_t;

typedef enum {
    FromThreadMessageEnter,
    FromThreadMessageExit
} FromThreadMessageType_t;

typedef struct {
    FromThreadMessageType_t messageType;
    epicsThreadId threadId;
} FromThreadMessage_t;

static const char *driverName="NDPluginDriver";

sortedListElement::sortedListElement(NDArrayConstPtr pArray, TimeStamp time)
    : pArray_(pArray), insertionTime_(time) {}

static void sortingTaskC(void *drvPvt)
{
    NDPluginDriver *pPvt = (NDPluginDriver *)drvPvt;

    pPvt->sortingTask();
}

/** Constructor for NDPluginDriver; most parameters are simply passed to asynNDArrayDriver::asynNDArrayDriver.
  * After calling the base class constructor this method creates a thread to execute the NDArray callbacks, 
  * and sets reasonable default values for all of the parameters defined in NDPluginDriver.h.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] queueSize The number of NDArrays that the input queue for this plugin can hold when 
  *            NDPluginDriverBlockingCallbacks=0.  Larger queues can decrease the number of dropped arrays,
  *            at the expense of more NDArray buffers being allocated from the underlying driver's NDArrayPool.
  * \param[in] blockingCallbacks Initial setting for the NDPluginDriverBlockingCallbacks flag.
  *            0=callbacks are queued and executed by the callback thread; 1 callbacks execute in the thread
  *            of the driver doing the callbacks.
  * \param[in] NDArrayPort Name of asyn port driver for initial source of NDArray callbacks.
  * \param[in] NDArrayAddr asyn port driver address for initial source of NDArray callbacks.
  * \param[in] maxAddr The maximum  number of asyn addr addresses this driver supports. 1 is minimum.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to 0 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to 0 to allow an unlimited amount of memory.
  * \param[in] interfaceMask Bit mask defining the asyn interfaces that this driver supports.
  * \param[in] interruptMask Bit mask definining the asyn interfaces that can generate interrupts (callbacks)
  * \param[in] asynFlags Flags when creating the asyn port driver; includes ASYN_CANBLOCK and ASYN_MULTIDEVICE.
  * \param[in] autoConnect The autoConnect flag for the asyn port driver.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  *            This value should also be used for any other threads this object creates.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  *            This value should also be used for any other threads this object creates.
  * \param[in] maxThreads The maximum number of threads this plugin is allowed to use.
  */
NDPluginDriver::NDPluginDriver(const char *portName, string const & pvName, int queueSize, int blockingCallbacks,
                               string const & ndArrayPv, int maxAddr, int maxBuffers, size_t maxMemory,
                               int interfaceMask, int interruptMask, int asynFlags, int autoConnect, int priority,
                               int stackSize, int maxThreads)

    : asynNDArrayDriver(portName, pvName, maxAddr, maxBuffers, maxMemory,
          interfaceMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynInt32ArrayMask | asynDrvUserMask,
          interruptMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynInt32ArrayMask,
          asynFlags, autoConnect, priority, stackSize),
    pluginStarted_(false),
    firstOutputArray_(true),
    connectedToArrayPort_(false),
    pToThreadMsgQ_(NULL),
    pFromThreadMsgQ_(NULL),
    prevUniqueId_(-1000),
    sortingThreadId_(0),
    gotFirst_(false),
    thisPtr_(shared_ptr<NDPluginDriver>(this))
{
    //static const char *functionName = "NDPluginDriver";
    
    lock();
    
    /* Initialize some members to 0 */
    //memset(&this->dimsPrev_, 0, sizeof(this->dimsPrev_));
    
    if (maxThreads < 1) maxThreads = 1;

    createParam(NDPluginDriverArrayPvString,           asynParamOctet, &NDPluginDriverArrayPv);
    createParam(NDPluginDriverArrayConnectedString,    asynParamInt32, &NDPluginDriverArrayConnected);
    createParam(NDPluginDriverArrayOverrunsString,     asynParamInt32, &NDPluginDriverArrayOverruns);
    createParam(NDPluginDriverPluginTypeString,        asynParamOctet, &NDPluginDriverPluginType);
    createParam(NDPluginDriverDroppedArraysString,     asynParamInt32, &NDPluginDriverDroppedArrays);
    createParam(NDPluginDriverQueueSizeString,         asynParamInt32, &NDPluginDriverQueueSize);
    createParam(NDPluginDriverQueueFreeString,         asynParamInt32, &NDPluginDriverQueueFree);
    createParam(NDPluginDriverMaxThreadsString,        asynParamInt32, &NDPluginDriverMaxThreads);
    createParam(NDPluginDriverNumThreadsString,        asynParamInt32, &NDPluginDriverNumThreads);
    createParam(NDPluginDriverSortModeString,          asynParamInt32, &NDPluginDriverSortMode);
    createParam(NDPluginDriverSortTimeString,          asynParamFloat64, &NDPluginDriverSortTime);
    createParam(NDPluginDriverSortSizeString,          asynParamInt32, &NDPluginDriverSortSize);
    createParam(NDPluginDriverSortFreeString,          asynParamInt32, &NDPluginDriverSortFree);
    createParam(NDPluginDriverDisorderedArraysString,  asynParamInt32, &NDPluginDriverDisorderedArrays);
    createParam(NDPluginDriverDroppedOutputArraysString,  asynParamInt32, &NDPluginDriverDroppedOutputArrays);
    createParam(NDPluginDriverEnableCallbacksString,   asynParamInt32, &NDPluginDriverEnableCallbacks);
    createParam(NDPluginDriverBlockingCallbacksString, asynParamInt32, &NDPluginDriverBlockingCallbacks);
    createParam(NDPluginDriverProcessPluginString,     asynParamInt32, &NDPluginDriverProcessPlugin);
    createParam(NDPluginDriverExecutionTimeString,     asynParamFloat64, &NDPluginDriverExecutionTime);
    createParam(NDPluginDriverMinCallbackTimeString,   asynParamFloat64, &NDPluginDriverMinCallbackTime);

    /* Here we set the values of read-only parameters and of read/write parameters that cannot
     * or should not get their values from the database.  Note that values set here will override
     * those in the database for output records because if asyn device support reads a value from 
     * the driver with no error during initialization then it sets the output record to that value.  
     * If a value is not set here then the read request will return an error (uninitialized).
     * Values set here will be overridden by values from save/restore if they exist. */
    setStringParam (NDPluginDriverArrayPv, ndArrayPv);
    setIntegerParam(NDPluginDriverDroppedArrays, 0);
    setIntegerParam(NDPluginDriverDroppedOutputArrays, 0);
    setIntegerParam(NDPluginDriverQueueSize, queueSize);
    setIntegerParam(NDPluginDriverQueueFree, queueSize);
    setIntegerParam(NDPluginDriverMaxThreads, maxThreads);
    setIntegerParam(NDPluginDriverNumThreads, 1);
    setIntegerParam(NDPluginDriverBlockingCallbacks, blockingCallbacks);

    // Start pvAccess client factory
    ClientFactory::start();
    provider_ = ChannelProviderRegistry::clients()->getProvider("pva");

    /* Create the callback threads, unless blocking callbacks are disabled with
     * the blockingCallbacks argument here. Even then, if they are enabled
     * subsequently, we will create the threads then. */
    if (!blockingCallbacks) {
        createCallbackThreads();
    }

    unlock();
}

NDPluginDriver::~NDPluginDriver()
{
  // Most methods in NDPluginDriver expect to be called with the asynPortDriver mutex locked.
  // The destructor does not, the mutex should be unlocked before calling the destructor.
  // We lock the mutex because deleteCallbackThreads expects it to be held, but then
  // unlocked it because the mutex is deleted in the asynPortDriver destructor and the
  // mutex must be unlocked before deleting it.
  this->lock();
  deleteCallbackThreads();
  this->unlock();
}

/** Method that is normally called at the beginning of the processCallbacks
  * method in derived classes.
  * \param[in] pArray  The NDArray from the callback.
  *
  * This method takes care of some bookkeeping for callbacks, updating parameters
  * from data in the class and in the NDArray.  It does asynInt32Array callbacks
  * for the dimensions array if the dimensions of the NDArray data have changed. */ 
void NDPluginDriver::beginProcessCallbacks(NDArrayConstPtr & pArray)
{
    //static const char *functionName="beginProcessCallbacks";
    int colorMode=NDColorModeMono, bayerPattern=NDBayerRGGB;

    NDAttributeConstPtr colorAttr(pArray->viewAttributeList()->find("ColorMode"));
    if(colorAttr)
        colorAttr->getValue(NDAttrInt32, &colorMode);

    NDAttributeConstPtr bayerAttr(pArray->viewAttributeList()->find("BayerPattern"));
    if(bayerAttr)
        bayerAttr->getValue(NDAttrInt32, &bayerPattern);

    int arrayCounter;
    getIntegerParam(NDArrayCounter, &arrayCounter);
    setIntegerParam(NDArrayCounter, arrayCounter + 1);
    setIntegerParam(NDNDimensions, pArray->getDimensions().size());
    setIntegerParam(NDDataType, pArray->getDataType());
    setIntegerParam(NDColorMode, colorMode);
    setIntegerParam(NDBayerPattern, bayerPattern);
    setIntegerParam(NDUniqueId, pArray->getUniqueId());

    epicsTimeStamp epicsTS;
    epicsTS.secPastEpoch = pArray->getEpicsTimeStamp().getSecondsPastEpoch();
    epicsTS.nsec = pArray->getEpicsTimeStamp().getNanoseconds();

    setTimeStamp(&epicsTS);
    setDoubleParam(NDTimeStamp, pArray->getTimeStamp().toSeconds());
    setIntegerParam(NDEpicsTSSec, epicsTS.secPastEpoch);
    setIntegerParam(NDEpicsTSNsec, epicsTS.nsec);

    /* See if the array dimensions have changed.  If so then do callbacks on them. */
    vector<NDDimension_t> dims = pArray->getDimensions();
    bool dimsChanged = false;
    for (size_t i = 0; i < ND_ARRAY_MAX_DIMS; i++)
    {
        int size = i < dims.size() ? dims[i].size : 0;

        if (size != this->dimsPrev_[i])
        {
            this->dimsPrev_[i] = size;
            dimsChanged = true;
        }
    }

    if (dimsChanged)
        doCallbacksInt32Array(this->dimsPrev_, ND_ARRAY_MAX_DIMS, NDDimensions, 0);

    // Save a pointer to the input array for use by ProcessPlugin
    pPrevInputArray_ = pArray;
}

/** Method that is normally called at the end of the processCallbacks())
  * method in derived classes.  
  * \param[in] pArray  The NDArray from the callback. This must be an array controlled by the plugin.
  * \param[in] readAttributes This flag must be true if the derived class has not yet called readAttributes() for pArray.
  *
  * This method does NDArray callbacks to downstream plugins if NDArrayCallbacks is true and SortMode is Unsorted.
  * If SortMode is sorted it inserts the NDArray into the std::multilist for callbacks in SortThread(). 
  * It keeps track of DisorderedArrays and DroppedOutputArrays. 
  * It caches the most recent NDArray in pArrays[0]. */
asynStatus NDPluginDriver::endProcessCallbacks(NDArrayPtr & pArray, bool readAttributes)
{
    static const char *functionName = "endProcessCallbacks";

    int arrayCallbacks;
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    if (arrayCallbacks == 0) {
        // We don't do array callbacks but still want to cache the last array in pArrays[0]
        this->pArrays[0] = pArray;
        return asynSuccess;
    }

    int callbacksSorted;
    getIntegerParam(NDPluginDriverSortMode, &callbacksSorted);

    if (readAttributes) {
        NDAttributeListPtr pAttrs(pArray->getAttributeList());
        this->getAttributes(pAttrs);
    }
    this->pArrays[0] = pArray;

    if (callbacksSorted) {
        int sortSize;
        int listSize = (int)sortedNDArrayList_.size();
        getIntegerParam(NDPluginDriverSortSize, &sortSize);
        setIntegerParam(NDPluginDriverSortFree, sortSize-listSize);
        if (listSize >= sortSize) {
            int droppedOutputArrays;
            getIntegerParam(NDPluginDriverDroppedOutputArrays, &droppedOutputArrays);
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, 
                "%s::%s std::multilist size exceeded, dropped array uniqueId=%d\n",
                driverName, functionName, pArray->getUniqueId());
            droppedOutputArrays++;
            setIntegerParam(NDPluginDriverDroppedOutputArrays, droppedOutputArrays);
        } else {
            TimeStamp now;
            now.getCurrent();
            sortedListElement *pListElement = new sortedListElement(pArray, now);
            sortedNDArrayList_.insert(*pListElement);
            sortedNDArrayList_.insert(sortedListElement(pArray, now));
        }
    } else {
        // See the comments about releasing the lock when calling doCallbacksGenericPointer above
        NDArrayData->put(pArray);
        bool orderOK = (pArray->getUniqueId() == prevUniqueId_)   ||
                       (pArray->getUniqueId() == prevUniqueId_+1);
        if (!firstOutputArray_ && !orderOK) {
            int disorderedArrays;
            getIntegerParam(NDPluginDriverDisorderedArrays, &disorderedArrays);
            disorderedArrays++;
            setIntegerParam(NDPluginDriverDisorderedArrays, disorderedArrays);
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, 
                "%s::%s disordered array found uniqueId=%d, prevUniqueId_=%d, orderOK=%d, disorderedArrays=%d\n",
                driverName, functionName, pArray->getUniqueId(), prevUniqueId_, orderOK, disorderedArrays);
        }
        firstOutputArray_ = false;
        prevUniqueId_ = pArray->getUniqueId();
    }
    return asynSuccess;
}

/** Convenience method that copies the array to a modifiable version before
  * calling endProcessCallbacks */
asynStatus NDPluginDriver::endProcessCallbacks(NDArrayConstPtr & pArray, bool readAttributes)
{
    static const char *functionName = "endProcessCallbacks";

    NDArrayPtr pOut(NDArrayPool_.copy(pArray));
    if(!pOut) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s: Couldn't allocate output array. Further processing terminated.\n",
                    driverName, functionName);
                return asynError;
    }
    return endProcessCallbacks(pOut, readAttributes);
}

/** Method that is called from monitorEvent with a new NDArray.
  * It calls the processCallbacks function, which typically is implemented in the
  * derived class.
  * It can either do the callbacks directly (if NDPluginDriverBlockingCallbacks=1) or by queueing
  * the arrays to be processed by a background task (if NDPluginDriverBlockingCallbacks=0).
  * In the latter case arrays can be dropped if the queue is full.  This method should really
  * be private, but it must be called from a C-linkage callback function, so it must be public.
  * \param[in] pArray The pointer to the NDArray */
void NDPluginDriver::driverCallback (asynUser *pasynUser, NDArrayConstPtr pArray)
{
    int status=0;
    double minCallbackTime;
    int blockingCallbacks, queueSize;
    bool ignoreQueueFull = false;
    static const char *functionName = "driverCallback";

    this->lock();

    status |= getDoubleParam(NDPluginDriverMinCallbackTime, &minCallbackTime);
    status |= getIntegerParam(NDPluginDriverBlockingCallbacks, &blockingCallbacks);
    status |= getIntegerParam(NDPluginDriverQueueSize, &queueSize);
    
    TimeStamp tNow;
    tNow.getCurrent();

    double deltaTime = TimeStamp::diff(tNow, lastProcessTime_);

    if ((minCallbackTime == 0.) || (deltaTime > minCallbackTime)) {
        if (pasynUser->auxStatus == asynOverflow) ignoreQueueFull = true;
        pasynUser->auxStatus = asynSuccess;
        
        /* Time to process the next array */
        
        /* The callbacks can operate in 2 modes: blocking or non-blocking.
         * If blocking we call processCallbacks directly, executing them
         * in the detector callback thread.
         * If non-blocking we put the array on the queue and it executes
         * in our background thread. */
        /* Update the time we last posted an array */
        tNow.getCurrent();
        lastProcessTime_ = tNow;

        if (blockingCallbacks) {
            processCallbacks(pArray);
            TimeStamp tEnd;
            tEnd.getCurrent();
            setDoubleParam(NDPluginDriverExecutionTime, TimeStamp::diff(tEnd, tNow)*1e3);
        } else {
            /* Try to put this array on the message queue.  If there is no room then return
             * immediately. */
            ToThreadMessageType_t msg = ToThreadMessageData;
            queue_.push_back(pArray);
            status = pToThreadMsgQ_->trySend(&msg, sizeof(msg));
            int queueFree = queueSize - pToThreadMsgQ_->pending();
            setIntegerParam(NDPluginDriverQueueFree, queueFree);

            if (status) {
                queue_.pop_back();
                pasynUser->auxStatus = asynOverflow;
                if (!ignoreQueueFull) {
                    int droppedArrays;
                    status |= getIntegerParam(NDPluginDriverDroppedArrays, &droppedArrays);
                    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                        "%s::%s message queue full, dropped array uniqueId=%d\n",
                        driverName, functionName, pArray->getUniqueId());
                    droppedArrays++;
                    status |= setIntegerParam(NDPluginDriverDroppedArrays, droppedArrays);
                }
            }
        }
    }
    callParamCallbacks();
    this->unlock();
}

/** Method runs as a separate thread, waiting for NDArrays to arrive in a message queue
  * and processing them.
  * This thread is used when NDPluginDriverBlockingCallbacks=0.
  * This method should really be private, but it must be called from a 
  * C-linkage callback function, so it must be public. */ 
void NDPluginDriver::processTask()
{
    /* This thread processes a new array when it arrives */
    int status;
    FromThreadMessage_t fromMsg = {FromThreadMessageEnter, epicsThreadGetIdSelf()};
    static const char *functionName = "processTask";

    // Send event indicating that the thread has started. Must do this before taking lock.
    status = pFromThreadMsgQ_->send(&fromMsg, sizeof(fromMsg));
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s error sending enter message thread %s\n",
            driverName, functionName, epicsThreadGetNameSelf());
    }

    lock();

    /* Loop forever */
    while(1) {
        /* Wait for an array to arrive from the queue. Release the lock while  waiting. */
        unlock();
        ToThreadMessageType_t toMsg;
        int numBytes = pToThreadMsgQ_->receive(&toMsg, sizeof(toMsg));

        if (numBytes != sizeof(toMsg)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                "%s::%s error reading message queue, expected size=%d, actual=%d\n",
                driverName, functionName, (int)sizeof(toMsg), numBytes);
        }
        switch (toMsg) {
            case ToThreadMessageExit:
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                    "%s::%s received exit message, thread=%s\n", 
                    driverName, functionName, epicsThreadGetNameSelf());
                fromMsg.messageType = FromThreadMessageExit;
                pFromThreadMsgQ_->send(&fromMsg, sizeof(fromMsg));
                return; // shutdown thread if special message
            case ToThreadMessageData:
                break;  // get array from deque further on, with lock taken
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s::%s unknown message type = %d\n",
                    driverName, functionName, toMsg);
                continue;
        }
        
        // Note: the lock must not be taken until after the thread exit logic above
        lock();
        NDArrayConstPtr pArray(queue_.front());
        queue_.pop_front();

        TimeStamp tStart;
        tStart.getCurrent();

        int queueSize;
        getIntegerParam(NDPluginDriverQueueSize, &queueSize);
        int queueFree = queueSize - pToThreadMsgQ_->pending();
        setIntegerParam(NDPluginDriverQueueFree, queueFree);

        /* Call the function that does the business of this callback.
         * This function should release the lock during time-consuming operations,
         * but of course it must not access any class data when the lock is released. */
        processCallbacks(pArray); 

        TimeStamp tEnd;
        tEnd.getCurrent();
        setDoubleParam(NDPluginDriverExecutionTime, TimeStamp::diff(tEnd, tStart)*1e3);
        callParamCallbacks();
    }
}

/** Register or unregister to receive monitor callbacks from the PV.
  * Note: this function must be called with the lock released, otherwise a deadlock can occur
  * in the call to cancelInterruptUser.
  * \param[in] enableCallbacks true to enable callbacks, false to disable callbacks */
asynStatus NDPluginDriver::setArrayInterrupt(bool enableCallbacks)
{
    const char *functionName = "setArrayInterrupt";

    if(!monitor_)
        return asynError;

    gotFirst_ = false;
    Status status = enableCallbacks ? monitor_->start() : monitor_->stop();

    if(!status.isOK())
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s(%d) ERROR: Failed to %s monitor: %s\n",
                driverName, functionName, enableCallbacks,
                enableCallbacks ? "start" : "stop", status.getMessage().c_str());
        return asynError;
    }

    return asynSuccess;
}

/** Connect this plugin to an NTNDArray PV */
asynStatus NDPluginDriver::connectToArrayPort(void)
{
    const char *functionName = "connectToArrayPort";

    string pvName;
    getStringParam(NDPluginDriverArrayPv, pvName);

    int enableCallbacks;
    getIntegerParam(NDPluginDriverEnableCallbacks, &enableCallbacks);
    if(channel_ && channel_->getChannelName() == pvName)
        return asynSuccess;

    setArrayInterrupt(false);

    /* Connect to the array port driver */
    Channel::shared_pointer channel = provider_->createChannel(pvName, thisPtr_);
    if(!channel)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s ERROR: Couldn't create channel for: %s\n",
                driverName, functionName, pvName.c_str());
        return asynError;
    }

    MonitorPtr monitor(channel->createMonitor(thisPtr_,
            CreateRequest::create()->createRequest("field()")));
    if(!monitor)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s ERROR: Couldn't create monitor\n",
                driverName, functionName);
        return asynError;
    }

    if(channel_)
        channel_->destroy();

    gotFirst_ = false;
    channel_ = channel;
    monitor_ = monitor;

    return asynSuccess;
}

/** Method runs as a separate thread, periodically doing NDArray callbacks to downstream plugins.
  * This thread is used when SortMode=1.
  * This method should really be private, but it must be called from a 
  * C-linkage callback function, so it must be public. */ 
void NDPluginDriver::sortingTask()
{
    static const char *functionName = "sortingTask";

    lock();
    while (1) {
        double sortTime;
        getDoubleParam(NDPluginDriverSortTime, &sortTime);
        unlock();
        epicsThreadSleep(sortTime);
        lock();

        TimeStamp now;
        now.getCurrent();

        int sortSize;
        getIntegerParam(NDPluginDriverSortSize, &sortSize);

        int listSize;
        while ((listSize=(int)sortedNDArrayList_.size()) > 0) {
            multiset<sortedListElement>::iterator pListElement = sortedNDArrayList_.begin();
            double deltaTime = TimeStamp::diff(now, pListElement->insertionTime_);

            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                "%s::%s, deltaTime=%f, list size=%d, uniqueId=%d\n", 
                driverName, functionName, deltaTime, listSize, pListElement->pArray_->getUniqueId());

            bool orderOK = (pListElement->pArray_->getUniqueId() == prevUniqueId_) ||
                      (pListElement->pArray_->getUniqueId() == prevUniqueId_+1);
            if ((!firstOutputArray_ && orderOK) || (deltaTime > sortTime)) {
                // NOTE: we have been releasing the lock before calling doCallbacksGenericPointer because
                // sometime in the distant past I thought I was getting deadlocks without doing so.
                // However, releasing the lock here does not work when NumThreads>1 because other threads
                // can get access to pArrays[0] when the lock is released and cause problems with the NDArrayPool.
                // Keep the lock for now unless we find deadlock problems.
                //this->unlock();
                NDArrayData->put(pListElement->pArray_);
                //this->lock();

                prevUniqueId_ = pListElement->pArray_->getUniqueId();
                sortedNDArrayList_.erase(pListElement);
                if (!firstOutputArray_ && !orderOK) {
                    int disorderedArrays;
                    getIntegerParam(NDPluginDriverDisorderedArrays, &disorderedArrays);
                    disorderedArrays++;
                    setIntegerParam(NDPluginDriverDisorderedArrays, disorderedArrays);
                    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, 
                        "%s::%s disordered array found uniqueId=%d, prevUniqueId_=%d, orderOK=%d, disorderedArrays=%d\n",
                        driverName, functionName, pListElement->pArray_->getUniqueId(), prevUniqueId_,
                        orderOK, disorderedArrays);
                }
                prevUniqueId_ = pListElement->pArray_->getUniqueId();
                sortedNDArrayList_.erase(pListElement);
                firstOutputArray_ = false;
            } else  {
                break;
            }
        }
        listSize=(int)sortedNDArrayList_.size();
        setIntegerParam(NDPluginDriverSortFree, sortSize-listSize);
        callParamCallbacks();
    }    
}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including NDPluginDriverEnableCallbacks and
  * NDPluginDriverArrayAddr.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus NDPluginDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int addr=0;
    asynStatus status = asynSuccess;
    static const char* functionName = "writeInt32";

    /* If this parameter belongs to a base class call its method */
    if (function < FIRST_NDPLUGIN_PARAM) {
        return asynNDArrayDriver::writeInt32(pasynUser, value);
    }

    status = getAddress(pasynUser, &addr); 
    if (status != asynSuccess) goto done;

    /* Set the parameter in the parameter library. */
    status = (asynStatus) setIntegerParam(addr, function, value);
    if (status != asynSuccess) goto done;

    /* If blocking callbacks are being disabled but the callback threads have
     * not been created yet, create them here. */
    if (function == NDPluginDriverBlockingCallbacks && !value && pThreads_.size() == 0) {
         createCallbackThreads();
     }
    
    if (function == NDPluginDriverEnableCallbacks) {
        this->unlock();
        status = setArrayInterrupt(value);
        this->lock();
        if (status != asynSuccess) goto done;
        if(!value)
            pPrevInputArray_.reset();   // Release the input NDArray
    } else if ((function == NDPluginDriverQueueSize) ||
               (function == NDPluginDriverNumThreads)) {
        if ((status = deleteCallbackThreads())) goto done;
        if ((status = createCallbackThreads())) goto done;

    } else if ((function == NDPluginDriverSortMode) && 
               (value == 1)) {
        status = createSortingThread();

    } else if (function == NDPluginDriverProcessPlugin) {
        if (pPrevInputArray_) {
            driverCallback(pasynUserSelf, pPrevInputArray_);
        } else {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                "%s::%s cannot do ProcessPlugin, no input array cached\n", 
                driverName, functionName);
            status = asynError;
            goto done;
        }
    }
    
done:
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks(addr);
    
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s::%s ERROR, status=%d, function=%d, value=%d, connectedToArrayPort_=%d\n", 
              driverName, functionName, status, function, value, this->connectedToArrayPort_);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s::%s function=%d, value=%d, connectedToArrayPort_=%d\n", 
              driverName, functionName, function, value, connectedToArrayPort_);
    return status;
}

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including NDPluginDriverArrayPort.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus NDPluginDriver::writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual)
{
    int addr=0;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    static const char *functionName = "writeOctet";

    status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(addr, function, (char *)value);

    if (function == NDPluginDriverArrayPv) {
        this->unlock();
        connectToArrayPort();
        this->lock();
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_NDPLUGIN_PARAM) 
            status = asynNDArrayDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr);

    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s::%s: status=%d, function=%d, value=%s", 
                  driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s::%s: function=%d, value=%s\n", 
              driverName, functionName, function, value);
    *nActual = nChars;
    return status;
}

/** Called when asyn clients call pasynInt32Array->read().
  * Returns the value of the array dimensions for the last NDArray.  
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus NDPluginDriver::readInt32Array(asynUser *pasynUser, epicsInt32 *value, 
                                         size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    int addr=0;
    size_t ncopy;
    asynStatus status = asynSuccess;
    static const char *functionName = "readInt32Array";

    status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
    if (function == NDDimensions) {
            ncopy = ND_ARRAY_MAX_DIMS;
            if (nElements < ncopy) ncopy = nElements;
            memcpy(value, this->dimsPrev_, ncopy*sizeof(*this->dimsPrev_));
            *nIn = ncopy;
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_NDPLUGIN_PARAM) 
            status = asynNDArrayDriver::readInt32Array(pasynUser, value, nElements, nIn);
    }
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s::%s: status=%d, function=%d", 
                  driverName, functionName, status, function);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s::%s: function=%d\n", 
              driverName, functionName, function);
    return status;
}

/** Starts the plugin threads.  This method must be called after the derived class object is fully constructed. */ 
asynStatus NDPluginDriver::start(void)
{
    assert(!this->pluginStarted_); 
    //static const char *functionName = "start";
  
    this->pluginStarted_ = true;
    // If the plugin was started with BlockingCallbacks=Yes then pThreads_.size() will be 0
    if (pThreads_.size() == 0) return asynSuccess;
  
    return startCallbackThreads();
}

/** Starts the plugin threads. 
  * This method is called from NDPluginDriver::start and whenever the number of threads is changed. */ 
asynStatus NDPluginDriver::startCallbackThreads(void)
{
    asynStatus status = asynSuccess;
    int i;
    int numBytes;
    FromThreadMessage_t fromMsg;
    static const char *functionName = "startCallbackThreads";

    for (i=0; i<numThreads_; i++) {
        pThreads_[i]->start();
  
        // Wait for the thread to say its running
        numBytes = pFromThreadMsgQ_->receive(&fromMsg, sizeof(fromMsg), 2.0);
        if (numBytes != sizeof(fromMsg)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s timeout waiting for plugin thread %d start message\n",
                driverName, functionName, i);
            status = asynError;
        } else if (fromMsg.messageType != FromThreadMessageEnter) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s incorrect response from plugin thread %d = %d\n",
                driverName, functionName, i, fromMsg.messageType);
            status = asynError;
                  
        } else if (fromMsg.threadId != pThreads_[i]->getId()) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s incorrect thread ID from plugin thread %d = %p should be %p\n",
                driverName, functionName, i, fromMsg.threadId, pThreads_[i]->getId());
            status = asynError;
        }        
    }
    return status;
}
    
/** Starts the thread that receives NDArrays from the epicsMessageQueue. */ 
void NDPluginDriver::run()
{
    this->processTask();
}

/** Creates the plugin threads.  
  * This method is called when BlockingCallbacks is 0, and whenever QueueSize or NumThreads is changed. */ 
asynStatus NDPluginDriver::createCallbackThreads()
{
    assert(this->pThreads_.size() == 0);
    assert(this->pToThreadMsgQ_ == 0);
    assert(this->pFromThreadMsgQ_ == 0);
    
    int queueSize;
    int numThreads;
    int maxThreads;
    int enableCallbacks;
    int i;
    int status = asynSuccess;
    static const char *functionName = "createCallbackThreads";

    getIntegerParam(NDPluginDriverMaxThreads, &maxThreads);
    getIntegerParam(NDPluginDriverNumThreads, &numThreads);
    getIntegerParam(NDPluginDriverQueueSize, &queueSize);
    if (numThreads > maxThreads) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s error, numThreads=%d must be <= maxThreads=%d, setting to %d\n",
            driverName, functionName, numThreads, maxThreads, maxThreads);
        status = asynError;
        numThreads = maxThreads;
    }
    if (numThreads < 1) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s error, numThreads=%d must be >= 1, setting to 1\n",
            driverName, functionName, numThreads);
        status = asynError;
        numThreads = 1;
    }
    setIntegerParam(NDPluginDriverNumThreads, numThreads);
    numThreads_ = numThreads;
  
    pThreads_.resize(numThreads);

    /* Create the message queue for the input arrays */
    pToThreadMsgQ_ = new epicsMessageQueue(queueSize, sizeof(ToThreadMessageType_t));
    if (!pToThreadMsgQ_) {
        /* We don't handle memory errors above, so no point in handling this. */
        cantProceed("NDPluginDriver::createCallbackThreads epicsMessageQueueCreate failure\n");
    }
    pFromThreadMsgQ_ = new epicsMessageQueue(numThreads, sizeof(FromThreadMessage_t));
    if (!pFromThreadMsgQ_) {
        /* We don't handle memory errors above, so no point in handling this. */
        cantProceed("NDPluginDriver::createCallbackThreads epicsMessageQueueCreate failure\n");
    }

    for (i=0; i<numThreads; i++) {
        /* Create the thread (but not start). */
        char taskName[256];
        epicsSnprintf(taskName, sizeof(taskName)-1, "%s_Plugin_%d", portName, i+1);
        pThreads_[i] = new epicsThread(*this, taskName, this->threadStackSize_, this->threadPriority_);
    }

    /* If start() was already run, we also need to start the threads. */
    if (this->pluginStarted_) {
        status |= startCallbackThreads();
    }
    getIntegerParam(NDPluginDriverEnableCallbacks, &enableCallbacks);
    setIntegerParam(NDPluginDriverQueueFree, queueSize);
    if (enableCallbacks) this->setArrayInterrupt(1);
    return (asynStatus) status;
}

/** Deletes the plugin threads.  
  * This method is called from the destructor and whenever QueueSize or NumThreads is changed. */ 
asynStatus NDPluginDriver::deleteCallbackThreads()
{
    ToThreadMessageType_t toMsg = ToThreadMessageExit;
    FromThreadMessage_t fromMsg;
    asynStatus status = asynSuccess;
    int i;
    int pending;
    int numBytes;
    static const char *functionName = "deleteCallbackThreads";
    
    //  Disable callbacks from driver so the threads will empty the message queue
    if (pToThreadMsgQ_ != 0) {
        this->unlock();
        this->setArrayInterrupt(0);
        while ((pending=pToThreadMsgQ_->pending()) > 0) {
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s::%s waiting for queue to empty, pending=%d\n", 
                driverName, functionName, pending);
            epicsThreadSleep(0.05);
        }
        // Send a kill message to the threads and wait for reply.
        // Must do this with lock released else the threads may not be able to receive the message
        for (i=0; i<numThreads_; i++) {
            if (pToThreadMsgQ_->send(&toMsg, sizeof(toMsg)) != 0) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error sending plugin thread %d exit message\n",
                    driverName, functionName, i);
                status = asynError;
            }
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s::%s sent exit message %d\n", 
                driverName, functionName, i);
            numBytes = pFromThreadMsgQ_->receive(&fromMsg, sizeof(fromMsg), 2.0);
            if (numBytes != sizeof(fromMsg)) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s timeout waiting for plugin thread %d exit message\n",
                    driverName, functionName, i);
                status = asynError;
            } else if (fromMsg.messageType != FromThreadMessageExit) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s incorrect response from plugin thread %d = %d\n",
                    driverName, functionName, i, fromMsg.messageType);
                status = asynError;
            }
        }
        this->lock();
        // All threads have now been stopped.  Delete them.
        for (i=0; i<numThreads_; i++) {
            delete pThreads_[i]; // The epicsThread destructor waits for the thread to return
        }
        pThreads_.resize(0);
        delete pToThreadMsgQ_;
        pToThreadMsgQ_ = 0;
    }
    if (pFromThreadMsgQ_) {
        delete pFromThreadMsgQ_;
        pFromThreadMsgQ_ = 0;
    }
    
    return status;
}

/** Creates the sorting thread.  
  * This method is called when SortMode is set to Sorted. */ 
asynStatus NDPluginDriver::createSortingThread()
{
    char taskName[256];
    static const char *functionName = "createSortingThread";
   
    // If the thread already exists return
    if (sortingThreadId_ != 0) return asynSuccess;
    
    /* Create the thread that outputs sorted NDArrays */
    epicsSnprintf(taskName, sizeof(taskName)-1, "%s_Plugin_Sort", portName);
    sortingThreadId_ = epicsThreadCreate(taskName,
                                         this->threadPriority_,
                                         this->threadStackSize_,
                                         (EPICSTHREADFUNC)sortingTaskC, this);
    if (sortingThreadId_ == 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error creating sortingTask thread\n", 
            driverName, functionName);
        return asynError;
    }
    return asynSuccess;
}

// Implemented for pvData::Requester
string NDPluginDriver::getRequesterName (void)
{
    string value;
    getStringParam(NDPortNameSelf, value);
    return value;
}

void NDPluginDriver::message (string const & message, MessageType messageType)
{
    printf("NDPluginDriver::message [type=%s] %s\n", getMessageTypeName(messageType).c_str(),
            message.c_str());
}

// Implemented for pvAccess::ChannelRequester
void NDPluginDriver::channelCreated (const Status& status, Channel::shared_pointer const & channel)
{
    if(!status.isOK())
        printf("NDPluginDriver::channelCreated: failed to create channel\n");
    else
        printf("NDPluginDriver::channelCreated: %s created\n", channel->getChannelName().c_str());
}

void NDPluginDriver::channelStateChange (Channel::shared_pointer const & channel,
        Channel::ConnectionState state)
{
    printf("NDPluginDriver::channelStateChange %s: %s\n",
            channel->getChannelName().c_str(),
            Channel::ConnectionStateNames[state]);
    setIntegerParam(NDPluginDriverArrayConnected, state == Channel::CONNECTED);
}

// Implemented for pvData::MonitorRequester
void NDPluginDriver::monitorConnect (Status const & status, MonitorPtr const & monitor,
        StructureConstPtr const & structure)
{
    const char *functionName = "monitorConnect";
    printf("NDPluginDriver::monitorConnect monitor connects [type=%s]\n",
            Status::StatusTypeName[status.getType()]);

    if (status.isSuccess())
    {
        if(!NTNDArray::isCompatible(structure))
        {
            // TODO: better error handling here
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                                    "%s::%s incompatible Structure (Not NTNDArray!)\n",
                                    driverName, functionName);
            return;
        }
        gotFirst_ = false;
    }
}

void NDPluginDriver::monitorEvent (MonitorPtr const & monitor)
{
    const char *functionName = "monitorEvent";

    MonitorElementPtr update;
    while ((update = monitor->poll()))
    {
        if(!gotFirst_)
        {
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                        "%s::%s discarding first update\n",
                        driverName, functionName);
            gotFirst_ = true;
            monitor->release(update);
            continue;
        }

        if(!update->overrunBitSet->isEmpty())
        {
            lock();
            int overruns;
            getIntegerParam(NDPluginDriverArrayOverruns, &overruns);
            setIntegerParam(NDPluginDriverArrayOverruns, overruns + 1);
            unlock();
        }

        /* At this point, 'update' is ours. After the call to 'release', however,
         * pvAccess can do whatever it wishes with 'update'. Therefore, in a
         * multithreaded environment like this one, we need to copy
         * update->pvStructure to another structure to prevent data races.
         * The pool allocator takes care of that.
         */
        driverCallback(pasynUserSelf, NDArrayPool_.alloc(update->pvStructurePtr));
        monitor->release(update);
    }
}

void NDPluginDriver::unlisten (MonitorPtr const & monitor)
{
    const char *functionName = "unlisten";
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s monitor unlistens\n",
        driverName, functionName);
    printf("NDPluginDriver::unlisten monitor unlistens\n");
}
