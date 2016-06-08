#ifndef NDPLUGINROI_H
#define NDPLUGINROI_H

#include "NDPluginDriver.h"

/* ROI general parameters */
#define NDPluginROINameString               "Name"          /* (string,     r/w) Name of this ROI */

/* ROI definition */
#define NDPluginROIDim0MinString            "MinX"          /* (int32,      r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim1MinString            "MinY"          /* (int32,      r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim2MinString            "MinZ"          /* (int32,      r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim0SizeString           "SizeX"         /* (int32,      r/w) Size of ROI in each dimension */
#define NDPluginROIDim1SizeString           "SizeY"         /* (int32,      r/w) Size of ROI in each dimension */
#define NDPluginROIDim2SizeString           "SizeZ"         /* (int32,      r/w) Size of ROI in each dimension */
#define NDPluginROIDim0MaxSizeString        "MaxSizeX_RBV"  /* (int32,      r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim1MaxSizeString        "MaxSizeY_RBV"  /* (int32,      r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim2MaxSizeString        "MaxSizeZ_RBV"  /* (int32,      r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim0BinString            "BinX"          /* (int32,      r/w) Binning of ROI in each dimension */
#define NDPluginROIDim1BinString            "BinY"          /* (int32,      r/w) Binning of ROI in each dimension */
#define NDPluginROIDim2BinString            "BinZ"          /* (int32,      r/w) Binning of ROI in each dimension */
#define NDPluginROIDim0ReverseString        "ReverseX"      /* (boolean,    r/w) Reversal of ROI in each dimension */
#define NDPluginROIDim1ReverseString        "ReverseY"      /* (boolean,    r/w) Reversal of ROI in each dimension */
#define NDPluginROIDim2ReverseString        "ReverseZ"      /* (boolean,    r/w) Reversal of ROI in each dimension */
#define NDPluginROIDim0EnableString         "EnableX"       /* (boolean,    r/w) If set then do ROI in this dimension */
#define NDPluginROIDim1EnableString         "EnableY"       /* (boolean,    r/w) If set then do ROI in this dimension */
#define NDPluginROIDim2EnableString         "EnableZ"       /* (boolean,    r/w) If set then do ROI in this dimension */
#define NDPluginROIDim0AutoSizeString       "AutoSizeX"     /* (boolean,    r/w) Automatically set size to max */
#define NDPluginROIDim1AutoSizeString       "AutoSizeY"     /* (boolean,    r/w) Automatically  set size to max */
#define NDPluginROIDim2AutoSizeString       "AutoSizeZ"     /* (boolean,    r/w) Automatically  set size to max */
#define NDPluginROIDataTypeString           "DataTypeOut"   /* (int32,      r/w) Data type for ROI.  -1 means automatic. */
#define NDPluginROIEnableScaleString        "EnableScale"   /* (boolean,    r/w) Disable/Enable scaling */
#define NDPluginROIScaleString              "Scale"         /* (double,     r/w) Scaling value, used as divisor */

/** Extract Regions-Of-Interest (ROI) from NDArray data; the plugin can be a source of NDArray callbacks for
  * other plugins, passing these sub-arrays. 
  * The plugin also optionally computes a statistics on the ROI. */
class epicsShareClass NDPluginROI : public NDPluginDriver {
public:
    NDPluginROI(std::string const & prefix, unsigned int queueSize,
                bool blockingCallbacks, std::string const & NDArrayProvider,
                std::string const & NDArrayPV, int maxBuffers, size_t maxMemory,
                int stackSize);
    /* These methods override the virtual methods in the base class */
    void processCallbacks(NDArrayPtr pArray);
    void process (epics::pvDatabase::PVRecord const * record);

protected:
    /* ROI general parameters */
    epics::pvPortDriver::StringParamPtr     NDPluginROIName;

    /* ROI definition */
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim0Min;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim1Min;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim2Min;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim0Size;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim1Size;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim2Size;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim0MaxSize;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim1MaxSize;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim2MaxSize;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim0Bin;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim1Bin;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDim2Bin;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim0Reverse;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim1Reverse;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim2Reverse;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim0Enable;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim1Enable;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim2Enable;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim0AutoSize;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim1AutoSize;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIDim2AutoSize;
    epics::pvPortDriver::IntParamPtr        NDPluginROIDataType;
    epics::pvPortDriver::BooleanParamPtr    NDPluginROIEnableScale;
    epics::pvPortDriver::DoubleParamPtr     NDPluginROIScale;
                                
private:
    int requestedSize_[3];
    int requestedOffset_[3];
};
    
#endif
