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

#ifndef NDARRAY_H
#define NDARRAY_H

#include <pv/pvData.h>
#include <pv/pvDatabase.h>
#include <pv/ntndarray.h>
#include <pv/ntndarrayAttribute.h>

#include <vector>

#include <shareLib.h>

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

typedef struct NDArrayInfo
{
    size_t nElements, totalBytes;
    int bytesPerElement;
    NDColorMode_t colorMode;

    struct
    {
        int dim;
        size_t size, stride;
    }x, y, color;
}NDArrayInfo_t;

/** Structure defining a dimension of an NDArray */
typedef struct NDDimension {
    size_t size;    /**< The number of elements in this dimension of the array */
    size_t fullSize;/**< The whole number of elements in this dimension of the array */
    size_t offset;  /**< The offset relative to the origin of the original data source (detector, for example).
                      * If a selected region of the detector is being read, then this value may be > 0.
                      * The offset value is cumulative, so if a plugin such as NDPluginROI further selects
                      * a subregion, the offset is relative to the first element in the detector, and not
                      * to the first element of the region passed to NDPluginROI. */
    size_t binning; /**< The binning (pixel summation, 1=no binning) relative to original data source (detector, for example)
                      * The offset value is cumulative, so if a plugin such as NDPluginROI performs binning,
                      * the binning is expressed relative to the pixels in the detector and not to the possibly
                      * binned pixels passed to NDPluginROI.*/
    bool reverse;   /**< The orientation (0=normal, 1=reversed) relative to the original data source (detector, for example)
                      * This value is cumulative, so if a plugin such as NDPluginROI reverses the data, the value must
                      * reflect the orientation relative to the original detector, and not to the possibly
                      * reversed data passed to NDPluginROI. */
} NDDimension_t;

class NDArray;
typedef std::tr1::shared_ptr<NDArray> NDArrayPtr;

class epicsShareClass NDArray
{
private:
    friend class NDArrayPool;
    epics::nt::NTNDArrayPtr mArray;

    NDArray (void);
    void eraseAttributes (void);
    void initDimensions (std::vector<size_t> const & dimSizes);

public:
    POINTER_DEFINITIONS(NDArray);

    NDArray (epics::nt::NTNDArrayPtr array);
    ~NDArray (void) {}

    static void initDimension (NDDimension_t & dim, size_t size, size_t offset = 0,
            int binning = 1, int reverse = 0);

    template<typename T>
    epics::pvData::shared_vector<T> getData (void)
    {
        return mArray->getValue()->get< epics::pvData::PVValueArray<T> >()->reuse();
    }

    template<typename T>
    epics::pvData::shared_vector<const T> viewData (void)
    {
        return mArray->getValue()->get< epics::pvData::PVValueArray<T> >()->view();
    }

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

        epics::pvData::PVUnionPtr unionField(mArray->getValue());
        unionField->select<arrayType>(fieldName)->replace(data);
    }

    template<typename T>
    void zeroData (void)
    {
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
        case epics::pvData::pvByte: zeroData<epics::pvData::int8>(); break;
        case epics::pvData::pvUByte: zeroData<epics::pvData::uint8>(); break;
        case epics::pvData::pvShort: zeroData<epics::pvData::int16>(); break;
        case epics::pvData::pvUShort: zeroData<epics::pvData::uint16>(); break;
        case epics::pvData::pvInt: zeroData<epics::pvData::int32>(); break;
        case epics::pvData::pvUInt: zeroData<epics::pvData::uint32>(); break;
        case epics::pvData::pvLong: zeroData<epics::pvData::int64>(); break;
        case epics::pvData::pvULong: zeroData<epics::pvData::uint64>(); break;
        case epics::pvData::pvFloat: zeroData<float>(); break;
        case epics::pvData::pvDouble: zeroData<double>(); break;
        case epics::pvData::pvString: break;
        }
    }

    epics::nt::NTNDArrayPtr getArray (void) const;
    std::vector<NDDimension_t> getDimensions (void) const;
    epics::pvData::ScalarType getDataType (void) const;
    NDColorMode_t getColorMode (void) const;
    NDArrayInfo_t getInfo (void) const;
    epics::pvData::int32 getUniqueId (void) const;
    epics::pvData::int64 getCompressedSize (void) const;
    epics::pvData::int64 getUncompressedSize (void) const;
    epics::pvData::TimeStamp getTimeStamp (void) const;
    epics::pvData::TimeStamp getEpicsTimeStamp (void) const;
    //NDAttributeListPtr getAttributeList (void);

    void setDimensions (std::vector<NDDimension_t> const & dimensions);
    void setUniqueId (epics::pvData::int32 value);
    void setCompressedSize (epics::pvData::int64 value);
    void setUncompressedSize (epics::pvData::int64 value);
    void setTimeStamp (epics::pvData::TimeStamp const & value);
    void setEpicsTimeStamp (epics::pvData::TimeStamp const & value);

    int report (FILE *fp, int details);

};

#endif
