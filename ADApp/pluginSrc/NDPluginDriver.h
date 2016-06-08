#ifndef NDPluginDriver_H
#define NDPluginDriver_H

#include <pv/pvAccess.h>

#include <epicsTypes.h>
#include <epicsMessageQueue.h>
#include <epicsTime.h>
#include <deque>

#include "pvNDArrayDriver.h"

#define NDPluginDriverArrayProviderString       "NDArrayProvider"       /**< (string,   r/w) The NDArray PV provider name */
#define NDPluginDriverArrayPVString             "NDArrayPV"             /**< (string,   r/w) The NDArray PV name */
#define NDPluginDriverArrayConnectedString      "NDArrayConnected"      /**< (boolean,  r/o) Is connected to the NDArray PV? */
#define NDPluginDriverArrayOverrunsString       "NDArrayOverruns"       /**< (uint32,   r/o) Number of overruns */
#define NDPluginDriverPluginTypeString          "PluginType_RBV"        /**< (string,   r/o) The type of plugin */
#define NDPluginDriverDroppedArraysString       "DroppedArrays"         /**< (uint32,   r/w) Number of dropped arrays */
#define NDPluginDriverQueueSizeString           "QueueSize"             /**< (uint32,   r/w) Total queue elements */
#define NDPluginDriverQueueFreeString           "QueueFree"             /**< (uint32,   r/w) Free queue elements */
#define NDPluginDriverEnableCallbacksString     "EnableCallbacks"       /**< (int32  ,  r/w) Enable callbacks from driver */
#define NDPluginDriverBlockingCallbacksString   "BlockingCallbacks"     /**< (boolean,  r/w) Callbacks block */
#define NDPluginDriverMinCallbackTimeString     "MinCallbackTime"       /**< (double,   r/w) Minimum time between calling processCallbacks
                                                                         *  to execute plugin code */

/** Class from which actual plugin drivers are derived; derived from pvNDArrayDriver */
class epicsShareClass NDPluginDriver :
    public PVNDArrayDriver,
    public virtual epics::pvAccess::ChannelRequester,
    public virtual epics::pvData::MonitorRequester
{
public:
    NDPluginDriver(std::string const & prefix, unsigned int queueSize,
                   bool blockingCallbacks, std::string const & provider,
                   std::string const & NDArrayPV,
                   int maxBuffers, size_t maxMemory, int stackSize);

    /* These are the methods that we override from pvNDArrayDriver */
    virtual void process (epics::pvDatabase::PVRecord const * record);
                                     
    /* These are the methods that are new to this class */
    virtual void driverCallback(NDArrayPtr pArray);
    virtual void processTask(void);
    virtual ~NDPluginDriver(){}

protected:
    virtual void processCallbacks(NDArrayPtr pArray);
    virtual int connectToArrayPort(void);

    epics::pvPortDriver::StringParamPtr  NDPluginDriverArrayProvider;
    epics::pvPortDriver::StringParamPtr  NDPluginDriverArrayPV;
    epics::pvPortDriver::BooleanParamPtr NDPluginDriverArrayConnected;
    epics::pvPortDriver::ULongParamPtr   NDPluginDriverArrayOverruns;
    epics::pvPortDriver::StringParamPtr  NDPluginDriverPluginType;
    epics::pvPortDriver::IntParamPtr     NDPluginDriverDroppedArrays;
    epics::pvPortDriver::IntParamPtr     NDPluginDriverQueueSize;
    epics::pvPortDriver::IntParamPtr     NDPluginDriverQueueFree;
    epics::pvPortDriver::IntParamPtr     NDPluginDriverEnableCallbacks;
    epics::pvPortDriver::BooleanParamPtr NDPluginDriverBlockingCallbacks;
    epics::pvPortDriver::DoubleParamPtr  NDPluginDriverMinCallbackTime;

private:
    virtual int setArrayInterrupt(int connect);

    /* Our data */
    bool mGotFirst;
    bool mConnectedToArrayPort;
    epics::pvData::TimeStamp mLastProcessTime;
    epics::pvAccess::ChannelProvider::shared_pointer mProvider;
    epics::pvAccess::Channel::shared_pointer mChannel;
    epics::pvData::MonitorPtr mMonitor;
    epicsMessageQueueId mMsgQId;
    std::deque<NDArrayPtr> mQueue;
    std::tr1::shared_ptr<NDPluginDriver> mThisPtr;

    // Implemented for pvData::Requester
    std::string getRequesterName (void);
    void message (std::string const & message, epics::pvData::MessageType messageType);

    // Implemented for pvAccess::ChannelRequester
    void channelCreated (const epics::pvData::Status& status,
            epics::pvAccess::Channel::shared_pointer const & channel);
    void channelStateChange (epics::pvAccess::Channel::shared_pointer const & channel,
            epics::pvAccess::Channel::ConnectionState state);

    // Implemented for pvData::MonitorRequester
    void monitorConnect (epics::pvData::Status const & status,
            epics::pvData::MonitorPtr const & monitor,
            epics::pvData::StructureConstPtr const & structure);
    void monitorEvent (epics::pvData::MonitorPtr const & monitor);
    void unlisten (epics::pvData::MonitorPtr const & monitor);
};

    
#endif
