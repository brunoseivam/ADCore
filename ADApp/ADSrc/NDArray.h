/** NDArray.h
 *
 * N-dimensional array definition
 *
 *
 * Mark Rivers
 * University of Chicago
 * May 10, 2008
 *
 */

#ifndef NDArray_H
#define NDArray_H

#include <epicsMutex.h>
#include <epicsTime.h>
#include <stdio.h>

#include <pv/ntndarray.h>
#include <pv/ntndarrayAttribute.h>

#include "NDAttribute.h"
#include "NDAttributeList.h"

/** The maximum number of dimensions in an NDArray */
#define ND_ARRAY_MAX_DIMS 10

/** Enumeration of color modes for NDArray attribute "colorMode" */
typedef enum
{
    NDColorModeMono,    /**< Monochromatic image */
    NDColorModeBayer,   /**< Bayer pattern image, 1 value per pixel but with color filter on detector */
    NDColorModeRGB1,    /**< RGB image with pixel color interleave, data array is [3, NX, NY] */
    NDColorModeRGB2,    /**< RGB image with row color interleave, data array is [NX, 3, NY]  */
    NDColorModeRGB3,    /**< RGB image with plane color interleave, data array is [NX, NY, 3]  */
    NDColorModeYUV444,  /**< YUV image, 3 bytes encodes 1 RGB pixel */
    NDColorModeYUV422,  /**< YUV image, 4 bytes encodes 2 RGB pixel */
    NDColorModeYUV411   /**< YUV image, 6 bytes encodes 4 RGB pixels */
} NDColorMode_t;

/** Enumeration of Bayer patterns for NDArray attribute "bayerPattern".
  * This value is only meaningful if colorMode is NDColorModeBayer. 
  * This value is needed because the Bayer pattern will change when reading out a 
  * subset of the chip, for example if the X or Y offset values are not even numbers */
typedef enum
{
    NDBayerRGGB        = 0,    /**< First line RGRG, second line GBGB... */
    NDBayerGBRG        = 1,    /**< First line GBGB, second line RGRG... */
    NDBayerGRBG        = 2,    /**< First line GRGR, second line BGBG... */
    NDBayerBGGR        = 3     /**< First line BGBG, second line GRGR... */
} NDBayerPattern_t;

/** Structure defining a dimension of an NDArray */
typedef struct NDDimension {
    size_t size;    /**< The number of elements in this dimension of the array */
    size_t offset;  /**< The offset relative to the origin of the original data source (detector, for example).
                      * If a selected region of the detector is being read, then this value may be > 0. 
                      * The offset value is cumulative, so if a plugin such as NDPluginROI further selects
                      * a subregion, the offset is relative to the first element in the detector, and not 
                      * to the first element of the region passed to NDPluginROI. */
    int binning;    /**< The binning (pixel summation, 1=no binning) relative to original data source (detector, for example)
                      * The offset value is cumulative, so if a plugin such as NDPluginROI performs binning,
                      * the binning is expressed relative to the pixels in the detector and not to the possibly
                      * binned pixels passed to NDPluginROI.*/
    int reverse;    /**< The orientation (0=normal, 1=reversed) relative to the original data source (detector, for example)
                      * This value is cumulative, so if a plugin such as NDPluginROI reverses the data, the value must
                      * reflect the orientation relative to the original detector, and not to the possibly
                      * reversed data passed to NDPluginROI. */
} NDDimension_t;

/** Structure returned by NDArray::getInfo */
typedef struct NDArrayInfo {
    size_t nElements;        /**< The total number of elements in the array */
    int bytesPerElement;     /**< The number of bytes per element in the array */
    size_t totalBytes;       /**< The total number of bytes required to hold the array;
                               *  this may be less than NDArray::dataSize. */
                             /**< The following are mostly useful for color images (RGB1, RGB2, RGB3) */
    NDColorMode_t colorMode; /**< The color mode */

    struct
    {
        int dim;            /**< The array index for this dimension */
        size_t size;        /**< Number of elements in this dimension */
        size_t stride;      /**< Number of elements between values in this dimension */
    }x, y, color;
} NDArrayInfo_t;

/** N-dimensional array class; each array has a set of dimensions, a data type, pointer to data, and optional attributes. 
  * An NDArray also has a uniqueId and timeStamp that to identify it. NDArray objects can be allocated
  * by an NDArrayPool object, which maintains a free list of NDArrays for efficient memory management. */
class NDArray;
typedef std::tr1::shared_ptr<NDArray> NDArrayPtr;
typedef std::tr1::shared_ptr<const NDArray> NDArrayConstPtr;

class epicsShareClass NDArray
{
private:
    void initAttributes (void);

    friend class NDArrayPool;
    epics::nt::NTNDArrayPtr array_;
    NDAttributeListPtr attributes_;

public:
    POINTER_DEFINITIONS(NDArray);

    static const epics::nt::NTNDArrayBuilderPtr builder;

    static void initDimension (NDDimension_t & dim, size_t size,
            size_t offset = 0, int binning = 1, int reverse = 0);

    NDArray(void);
    NDArray(epics::nt::NTNDArray::const_shared_pointer & array);

    void copy (NDArray::const_shared_pointer & from);

    epics::nt::NTNDArrayPtr getArray(void);
    epics::nt::NTNDArray::const_shared_pointer getArray (void) const;

    template<typename T>
    epics::pvData::shared_vector<T> getData (void)
    {
        return array_->getValue()->get< epics::pvData::PVValueArray<T> >()->reuse();
    }

    template<typename T>
    epics::pvData::shared_vector<const T> viewData (void) const
    {
        return array_->getValue()->get< epics::pvData::PVValueArray<T> >()->view();
    }

    const void * viewRawData (void) const;

    template<typename T>
    void setData (epics::pvData::shared_vector<T> & data)
    {
        setData<T>(freeze(data));
    }

    template<typename T>
    void setData (epics::pvData::shared_vector<const T> const & data)
    {
        typedef typename epics::pvData::PVValueArray<T> arrayType;

        std::string fieldName(
                std::string(epics::pvData::ScalarTypeFunc::name(arrayType::typeCode)) +
                std::string("Value")
        );

        epics::pvData::PVUnionPtr unionField(array_->getValue());
        unionField->select<arrayType>(fieldName)->replace(data);
    }

    template<typename T>
    void zeroData (void)
    {
        // TODO: this is potentially slow
        epics::pvData::shared_vector<T> data(getData<T>());
        typename epics::pvData::shared_vector<T>::iterator iter;
        for(iter = data.begin(); iter != data.end(); ++iter)
            *iter = 0;
        setData<T>(data);
    }

    void zeroData (void)
    {
        switch(getDataType()) {
        case epics::pvData::pvBoolean: zeroData<epics::pvData::boolean>(); break;
        case epics::pvData::pvByte:    zeroData<epics::pvData::int8>();    break;
        case epics::pvData::pvUByte:   zeroData<epics::pvData::uint8>();   break;
        case epics::pvData::pvShort:   zeroData<epics::pvData::int16>();   break;
        case epics::pvData::pvUShort:  zeroData<epics::pvData::uint16>();  break;
        case epics::pvData::pvInt:     zeroData<epics::pvData::int32>();   break;
        case epics::pvData::pvUInt:    zeroData<epics::pvData::uint32>();  break;
        case epics::pvData::pvLong:    zeroData<epics::pvData::int64>();   break;
        case epics::pvData::pvULong:   zeroData<epics::pvData::uint64>();  break;
        case epics::pvData::pvFloat:   zeroData<float>();  break;
        case epics::pvData::pvDouble:  zeroData<double>(); break;
        case epics::pvData::pvString:  break;
        }
    }

    std::vector<NDDimension_t> getDimensions (void) const;
    epics::pvData::ScalarType getDataType (void) const;
    NDColorMode_t getColorMode (void) const;
    NDArrayInfo_t getInfo (void) const;
    epics::pvData::int32 getUniqueId (void) const;
    epics::pvData::int64 getCompressedSize (void) const;
    epics::pvData::int64 getUncompressedSize (void) const;
    epics::pvData::TimeStamp getTimeStamp (void) const;
    epics::pvData::TimeStamp getEpicsTimeStamp (void) const;
    bool hasData (void) const;

    NDAttributeListPtr getAttributeList (void);
    NDAttributeListConstPtr viewAttributeList (void) const;
    void eraseAttributes (void);

    void setDimensions (std::vector<NDDimension_t> const & dims);
    void setUniqueId (epics::pvData::int32 value);
    void setCompressedSize (epics::pvData::int64 value);
    void setUncompressedSize (epics::pvData::int64 value);
    void setTimeStamp (epics::pvData::TimeStamp const & value);
    void setEpicsTimeStamp (epics::pvData::TimeStamp const & value);

    /* Methods */
    int report (FILE *fp, int details) const;
};

#endif
