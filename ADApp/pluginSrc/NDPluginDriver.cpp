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

#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsMessageQueue.h>
#include <cantProceed.h>

#include <pv/clientFactory.h>

#include <epicsExport.h>
#include "NDPluginDriver.h"


using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvDatabase;
using namespace epics::nt;

static const char *driverName="NDPluginDriver";

/** Method that is normally called at the beginning of the processCallbacks
  * method in derived classes.
  * \param[in] pArray  The NDArray from the callback.
  *
  * This method takes care of some bookkeeping for callbacks, updating parameters
  * from data in the class and in the NDArray.  It does asynInt32Array callbacks
  * for the dimensions array if the dimensions of the NDArray data have changed. */ 
void NDPluginDriver::processCallbacks(NDArrayPtr pArray)
{
    //NDAttribute *pAttribute;
    int colorMode=NDColorModeMono, bayerPattern=NDBayerRGGB;
    
    /*pAttribute = pArray->pAttributeList->find("ColorMode");
    if (pAttribute) pAttribute->getValue(NDAttrInt32, &colorMode);
    pAttribute = pArray->pAttributeList->find("BayerPattern");
    if (pAttribute) pAttribute->getValue(NDAttrInt32, &bayerPattern);*/
    
    NDArrayCounter->put(NDArrayCounter->get()+1);
    NDNDimensions->put(pArray->getDimensions().size());
    NDDataType->put(pArray->getDataType());
    NDColorMode->put(colorMode);
    NDBayerPattern->put(bayerPattern);
    NDUniqueId->put(pArray->getUniqueId());
    NDTimeStamp->put(pArray->getTimeStamp().toSeconds());

    //setTimeStamp(&pArray->epicsTS);
    //TimeStamp epicsTS(pArray->getEpicsTimeStamp());
    //NDEpicsTSSec->put(epicsTS.getEpicsSecondsPastEpoch());
    //NDEpicsTSNsec->put(epicsTS.getNanoseconds());
}

/** Method that is called from the driver with a new NDArray.
  * It calls the processCallbacks function, which typically is implemented in the
  * derived class.
  * It can either do the callbacks directly (if NDPluginDriverBlockingCallbacks=1) or by queueing
  * the arrays to be processed by a background task (if NDPluginDriverBlockingCallbacks=0).
  * In the latter case arrays can be dropped if the queue is full.  This method should really
  * be private, but it must be called from a C-linkage callback function, so it must be public.
  * \param[in] pasynUser  The pasynUser from the asyn client.
  * \param[in] genericPointer The pointer to the NDArray */ 
void NDPluginDriver::driverCallback(NDArrayPtr array)
{
    lock();

    double minCallbackTime = NDPluginDriverMinCallbackTime->get();
    bool blockingCallbacks = NDPluginDriverBlockingCallbacks->get();
    int queueSize = NDPluginDriverQueueSize->get();
    
    TimeStamp tNow;
    tNow.getCurrent();
    double deltaTime = TimeStamp::diff(tNow, mLastProcessTime);

    if ((minCallbackTime == 0.) || (deltaTime > minCallbackTime)) {  
        /* Time to process the next array */
        
        /* The callbacks can operate in 2 modes: blocking or non-blocking.
         * If blocking we call processCallbacks directly, executing them
         * in the detector callback thread.
         * If non-blocking we put the array on the queue and it executes
         * in our background thread. */
        /* Update the time we last posted an array */
        mLastProcessTime.getCurrent();

        if (blockingCallbacks) {
            processCallbacks(array);
        } else {
            /* Try to put this array on the message queue.  If there is no room then return
             * immediately. */

            mQueue.push_back(array);
            int signal = 0; // actual value is irrelevant
            int status = epicsMessageQueueTrySend(this->mMsgQId, &signal, sizeof(signal));
            int queueFree = queueSize - epicsMessageQueuePending(this->mMsgQId);
            NDPluginDriverQueueFree->put(queueFree);
            if (status) {
                mQueue.pop_back();
                printf("NDPluginDriver:driverCallback message queue full, dropped array %d\n",
                        NDArrayCounter->get());

                NDPluginDriverDroppedArrays->put(NDPluginDriverDroppedArrays->get()+1);
            }
        }
    }
    unlock();
}

void processTask(void *drvPvt)
{
    NDPluginDriver *pPvt = (NDPluginDriver *)drvPvt;
    pPvt->processTask();
}

/** Method runs as a separate thread, waiting for NDArrays to arrive in a message queue
  * and processing them.
  * This thread is used when NDPluginDriverBlockingCallbacks=0.
  * This method should really be private, but it must be called from a 
  * C-linkage callback function, so it must be public. */ 
void NDPluginDriver::processTask(void)
{
    /* This thread processes a new array when it arrives */
    /* Loop forever */
    while (1) {
        /* Wait for an array to arrive from the queue */
        int signal; // value is irrelevant
        epicsMessageQueueReceive(mMsgQId, &signal, sizeof(signal));
        
        /* Take the lock.  The function we are calling must release the lock
         * during time-consuming operations when it does not need it. */
        this->lock();
        int queueSize = NDPluginDriverQueueSize->get();
        int queueFree = queueSize - epicsMessageQueuePending(mMsgQId);
        NDPluginDriverQueueFree->put(queueFree);

        /* Call the function that does the business of this callback */
        NDArrayPtr pArray(mQueue.front());
        mQueue.pop_front();
        processCallbacks(pArray);
        this->unlock();
    }
}

/** Register or unregister to receive asynGenericPointer (NDArray) callbacks from the driver.
  * Note: this function must be called with the lock released, otherwise a deadlock can occur
  * in the call to cancelInterruptUser.
  * \param[in] enableCallbacks 1 to enable callbacks, 0 to disable callbacks */ 
int NDPluginDriver::setArrayInterrupt(int enableCallbacks)
{
    if(!mMonitor)
        return -1;

    Status status = enableCallbacks ? mMonitor->start() : mMonitor->stop();

    if(!status.isOK())
    {
        printf("NDPluginDriver::setArrayInterrupt %s\n", status.getMessage().c_str());
        return -1;
    }

    return 0;
}

/** Connect this plugin to an NTNDArray PV */
int NDPluginDriver::connectToArrayPort(void)
{
    setArrayInterrupt(false);

    const string providerName(NDPluginDriverArrayProvider->get());
    mProvider = getChannelProviderRegistry()->getProvider(providerName);
    if(!mProvider)
    {
        printf("NDPluginDriver::connectToArrayPort couldn't get provider %s\n", providerName.c_str());
        return -1;
    }

    const string pvName(NDPluginDriverArrayPV->get());
    mChannel = mProvider->createChannel(pvName, mThisPtr);
    if(!mChannel)
    {
        printf("NDPluginDriver::connectToArrayPort couldn't create channel %s\n", pvName.c_str());
        return -1;
    }

    mMonitor = mChannel->createMonitor(mThisPtr,
            CreateRequest::create()->createRequest("field()"));
    if(!mMonitor)
    {
        printf("NDPluginDriver::connectToArrayPort couldn't create monitor\n");
        return -1;
    }

    mGotFirst = false;

    return 0;
}   

void NDPluginDriver::process (PVRecord const * record)
{
    if(record == NDPluginDriverEnableCallbacks.get() && this->mConnectedToArrayPort)
    {
        unlock();
        setArrayInterrupt(NDPluginDriverEnableCallbacks->get());
        lock();
    }
    else if(record == NDPluginDriverArrayProvider.get() ||
            record == NDPluginDriverArrayPV.get())
    {
        unlock();
        connectToArrayPort();
        lock();
    }
    else
        PVNDArrayDriver::process(record);
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
  * \param[in] numParams The number of parameters that the derived class supports.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] interfaceMask Bit mask defining the asyn interfaces that this driver supports.
  * \param[in] interruptMask Bit mask definining the asyn interfaces that can generate interrupts (callbacks)
  * \param[in] asynFlags Flags when creating the asyn port driver; includes ASYN_CANBLOCK and ASYN_MULTIDEVICE.
  * \param[in] autoConnect The autoConnect flag for the asyn port driver.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
NDPluginDriver::NDPluginDriver(string const & prefix, unsigned int queueSize,
                               bool blockingCallbacks, string const & NDArrayProvider,
                               string const & NDArrayPV, int maxBuffers, size_t maxMemory,
                               int stackSize)

    : PVNDArrayDriver(prefix, maxBuffers, maxMemory), mGotFirst(false),
          mConnectedToArrayPort(false), mLastProcessTime(),
          mThisPtr(tr1::shared_ptr<NDPluginDriver>(this)), mQueue()
{
    static const char *functionName = "NDPluginDriver";
    char taskName[256] = {};

    /* Create the message queue for the input arrays */
    this->mMsgQId = epicsMessageQueueCreate(queueSize, sizeof(int));
    if (!this->mMsgQId) {
        printf("%s:%s: epicsMessageQueueCreate failure\n", driverName, functionName);
        return;
    }
    
    /* We use the same stack size for our callback thread as for the port thread */
    if (stackSize <= 0) stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);

    strncpy(taskName, (prefix + string("_Plugin")).c_str(), sizeof(taskName)-1);
    /* Create the thread that handles the NDArray callbacks */
    int status = (epicsThreadCreate(taskName,
                          epicsThreadPriorityMedium,
                          stackSize,
                          (EPICSTHREADFUNC)::processTask,
                          this) == NULL);
    if (status) {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }

    NDPluginDriverArrayProvider     = createParam<string>(NDPluginDriverArrayProviderString);
    NDPluginDriverArrayPV           = createParam<string>(NDPluginDriverArrayPVString);
    NDPluginDriverArrayConnected    = createParam<boolean>(NDPluginDriverArrayConnectedString);
    NDPluginDriverArrayOverruns     = createParam<uint64>(NDPluginDriverArrayOverrunsString);
    NDPluginDriverPluginType        = createParam<string>(NDPluginDriverPluginTypeString);
    NDPluginDriverDroppedArrays     = createParam<int32>(NDPluginDriverDroppedArraysString);
    NDPluginDriverQueueSize         = createParam<int32>(NDPluginDriverQueueSizeString);
    NDPluginDriverQueueFree         = createParam<int32>(NDPluginDriverQueueFreeString);
    NDPluginDriverEnableCallbacks   = createParam<int32>(NDPluginDriverEnableCallbacksString);
    NDPluginDriverBlockingCallbacks = createParam<boolean>(NDPluginDriverBlockingCallbacksString);
    NDPluginDriverMinCallbackTime   = createParam<double>(NDPluginDriverMinCallbackTimeString);

    /* Here we set the values of read-only parameters and of read/write parameters that cannot
     * or should not get their values from the database.  Note that values set here will override
     * those in the database for output records because if asyn device support reads a value from 
     * the driver with no error during initialization then it sets the output record to that value.  
     * If a value is not set here then the read request will return an error (uninitialized).
     * Values set here will be overridden by values from save/restore if they exist. */
    NDPluginDriverArrayProvider->put(NDArrayProvider);
    NDPluginDriverArrayPV->put(NDArrayPV);
    NDPluginDriverDroppedArrays->put(0);
    NDPluginDriverQueueSize->put(queueSize);
    NDPluginDriverQueueFree->put(queueSize);
}

// Implemented for pvData::Requester
string NDPluginDriver::getRequesterName (void)
{
    return NDPortNameSelf->get();
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
    printf("PVPortClient::channelStateChange %s: %s\n",
            channel->getChannelName().c_str(),
            Channel::ConnectionStateNames[state]);
    NDPluginDriverArrayConnected->put(state == Channel::CONNECTED);
}

// Implemented for pvData::MonitorRequester
void NDPluginDriver::monitorConnect (Status const & status, MonitorPtr const & monitor,
        StructureConstPtr const & structure)
{
    printf("NDPluginDriver::monitorConnect monitor connects [type=%s]\n",
            Status::StatusTypeName[status.getType()]);

    if (status.isSuccess())
    {
        if(!NTNDArray::isCompatible(getPVDataCreate()->createPVStructure(structure)))
        {
            printf("NDPluginDriver::monitorConnect incompatible PVStructure (Not NTNDArray!). Not starting monitor\n");
            return;
        }

        printf("NDPluginDriver::monitorConnect starting monitor\n");
        mGotFirst = false;
        monitor->start();
    }
}

void NDPluginDriver::monitorEvent (MonitorPtr const & monitor)
{
    const char *functionName = "monitorEvent";

    lock();

    MonitorElementPtr update;
    while ((update = monitor->poll()))
    {
        if(!mGotFirst)
        {
            printf("NDPluginDriver::%s discarding first update\n", functionName);
            mGotFirst = true;
            monitor->release(update);
            continue;
        }

        if(!update->overrunBitSet->isEmpty())
            NDPluginDriverArrayOverruns->put(NDPluginDriverArrayOverruns->get()+1);

        NDArrayPtr pArray(new NDArray(NTNDArray::wrap(update->pvStructurePtr)));

        driverCallback(pArray);
        monitor->release(update);
    }

    unlock();
}

void NDPluginDriver::unlisten (MonitorPtr const & monitor)
{
    printf("NDPluginDriver::unlisten monitor unlistens\n");
}

