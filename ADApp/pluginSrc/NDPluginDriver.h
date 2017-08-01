#ifndef NDPluginDriver_H
#define NDPluginDriver_H

#include <set>
#include <epicsTypes.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsTime.h>

#include "asynNDArrayDriver.h"


// This class defines the object that is contained in the std::multilist for sorting output NDArrays
// It contains a pointer to the NDArray and the time that the object was added to the list
// It defines the < operator to use the NDArray::uniqueId field as the sort key

// We would like to hide this class definition in NDPluginDriver.cpp and just forward reference it here.
// That works on Visual Studio, and on gcc if instantiating plugins as heap variables with "new", but fails on gcc
// if instantiating plugins as automatic variables.
//class sortedListElement;

class sortedListElement {
    public:
        sortedListElement(NDArrayConstPtr pArray, epics::pvData::TimeStamp time);
        friend bool operator<(const sortedListElement& lhs, const sortedListElement& rhs) {
            return (lhs.pArray_->getUniqueId() < rhs.pArray_->getUniqueId());
        }
        NDArrayConstPtr pArray_;
        epics::pvData::TimeStamp insertionTime_;
};

#define NDPluginDriverArrayPvString             "NTNDARRAY_PV"          /**< (asynOctet,    r/w) This plugin's source of NTNDArrays */
#define NDPluginDriverArrayConnectedString      "NTNDARRAY_CONNECTED"   /**< (asynInt32,    r/o) Is connected to source? */
#define NDPluginDriverArrayOverrunsString       "NTNDARRAY_OVERRUNS"    /**< (asynInt32,    r/o) Number of source overrruns */
#define NDPluginDriverPluginTypeString          "PLUGIN_TYPE"           /**< (asynOctet,    r/o) The type of plugin */
#define NDPluginDriverDroppedArraysString       "DROPPED_ARRAYS"        /**< (asynInt32,    r/w) Number of dropped input arrays */
#define NDPluginDriverQueueSizeString           "QUEUE_SIZE"            /**< (asynInt32,    r/w) Total queue elements */ 
#define NDPluginDriverQueueFreeString           "QUEUE_FREE"            /**< (asynInt32,    r/w) Free queue elements */
#define NDPluginDriverMaxThreadsString          "MAX_THREADS"           /**< (asynInt32,    r/w) Maximum number of threads */ 
#define NDPluginDriverNumThreadsString          "NUM_THREADS"           /**< (asynInt32,    r/w) Number of threads */
#define NDPluginDriverSortModeString            "SORT_MODE"             /**< (asynInt32,    r/w) sorted callback mode */
#define NDPluginDriverSortTimeString            "SORT_TIME"             /**< (asynFloat64,  r/w) sorted callback time */
#define NDPluginDriverSortSizeString            "SORT_SIZE"             /**< (asynInt32,    r/o) std::multiset maximum # elements */
#define NDPluginDriverSortFreeString            "SORT_FREE"             /**< (asynInt32,    r/o) std::multiset free elements */
#define NDPluginDriverDisorderedArraysString    "DISORDERED_ARRAYS"     /**< (asynInt32,    r/o) Number of out of order output arrays */
#define NDPluginDriverDroppedOutputArraysString "DROPPED_OUTPUT_ARRAYS" /**< (asynInt32,    r/o) Number of dropped output arrays */
#define NDPluginDriverEnableCallbacksString     "ENABLE_CALLBACKS"      /**< (asynInt32,    r/w) Enable callbacks from driver (1=Yes, 0=No) */
#define NDPluginDriverBlockingCallbacksString   "BLOCKING_CALLBACKS"    /**< (asynInt32,    r/w) Callbacks block (1=Yes, 0=No) */
#define NDPluginDriverProcessPluginString       "PROCESS_PLUGIN"        /**< (asynInt32,    r/w) Process plugin with last callback array */
#define NDPluginDriverExecutionTimeString       "EXECUTION_TIME"        /**< (asynFloat64,  r/o) The last execution time (milliseconds) */
#define NDPluginDriverMinCallbackTimeString     "MIN_CALLBACK_TIME"     /**< (asynFloat64,  r/w) Minimum time between calling processCallbacks 
                                                                         *  to execute plugin code */

/** Class from which actual plugin drivers are derived; derived from asynNDArrayDriver */
class epicsShareClass NDPluginDriver :
    public asynNDArrayDriver,
    public epicsThreadRunable,
    public virtual epics::pvAccess::ChannelRequester,
    public virtual epics::pvData::MonitorRequester
{
public:
    NDPluginDriver(const char *portName, std::string const & pvName, int queueSize, int blockingCallbacks,
                   std::string const & sourcePvName, int maxAddr, int maxBuffers, size_t maxMemory,
                   int interfaceMask, int interruptMask, int asynFlags, int autoConnect, int priority,
                   int stackSize, int maxThreads);
    ~NDPluginDriver();

    /* These are the methods that we override from asynNDArrayDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,
                          size_t *nActual);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                        size_t nElements, size_t *nIn);
                                     
    /* These are the methods that are new to this class */
    virtual void run(void);
    virtual asynStatus start(void);
    void sortingTask();

protected:
    virtual void processCallbacks(NDArrayConstPtr pArray) = 0;
    virtual void beginProcessCallbacks(NDArrayConstPtr pArray);
    virtual asynStatus endProcessCallbacks(NDArrayConstPtr pArray, bool copyArray=false, bool readAttributes=true);
    virtual asynStatus connectToArrayPort(void);
    virtual asynStatus setArrayInterrupt(bool enableCallbacks);

protected:
    int NDPluginDriverArrayPv;
    #define FIRST_NDPLUGIN_PARAM NDPluginDriverArrayPv
    int NDPluginDriverArrayConnected;
    int NDPluginDriverArrayOverruns;
    int NDPluginDriverPluginType;
    int NDPluginDriverDroppedArrays;
    int NDPluginDriverQueueSize;
    int NDPluginDriverQueueFree;
    int NDPluginDriverMaxThreads;
    int NDPluginDriverNumThreads;
    int NDPluginDriverSortMode;
    int NDPluginDriverSortTime;
    int NDPluginDriverSortSize;
    int NDPluginDriverSortFree;
    int NDPluginDriverDisorderedArrays;
    int NDPluginDriverDroppedOutputArrays;
    int NDPluginDriverEnableCallbacks;
    int NDPluginDriverBlockingCallbacks;
    int NDPluginDriverProcessPlugin;
    int NDPluginDriverExecutionTime;
    int NDPluginDriverMinCallbackTime;

    NDArrayConstPtr pPrevInputArray_;

private:
    virtual void driverCallback(asynUser *pasynUser, NDArrayConstPtr pArray);
    void processTask();
    asynStatus createCallbackThreads();
    asynStatus startCallbackThreads();
    asynStatus deleteCallbackThreads();
    asynStatus createSortingThread();

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

    /* Our data */
    int numThreads_;
    bool pluginStarted_;
    bool firstOutputArray_;
    bool connectedToArrayPort_;
    std::vector<epicsThread*> pThreads_;
    epicsMessageQueue *pToThreadMsgQ_;
    epicsMessageQueue *pFromThreadMsgQ_;
    std::multiset<sortedListElement> sortedNDArrayList_;
    int prevUniqueId_;
    epicsThreadId sortingThreadId_;
    epics::pvData::TimeStamp lastProcessTime_;
    int dimsPrev_[ND_ARRAY_MAX_DIMS];
    bool gotFirst_;
    epics::pvAccess::ChannelProvider::shared_pointer provider_;
    epics::pvAccess::Channel::shared_pointer channel_;
    epics::pvData::MonitorPtr monitor_;
    epicsMessageQueueId msgQId_;
    std::deque<NDArrayConstPtr> queue_;
    std::tr1::shared_ptr<NDPluginDriver> thisPtr_;
};

    
#endif
