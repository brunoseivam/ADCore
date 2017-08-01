/** NDArrayPool.cpp
 *
 * Mark Rivers
 * University of Chicago
 * October 18, 2013
 *
 */

#include "NDArrayPool.h"
#include <epicsExport.h>

using std::vector;
using std::string;
using std::tr1::shared_ptr;

using namespace epics::pvData;
using namespace epics::nt;

static const char *driverName = "NDArrayPool";

/** eraseNDAttributes is a global flag the controls whether NDArray::clearAttributes() is called
  * each time a new array is allocated with NDArrayPool->alloc().
  * The default value is 0, meaning that clearAttributes() is not called.  This mode is efficient
  * because it saves lots of allocation/deallocation, and it is fine when the attributes for a driver
  * are set once and not changed.  If driver attributes are deleted however, the allocated arrays
  * will still have the old attributes if this flag is 0.  Set this flag to force attributes to be
  * removed each time an NDArray is allocated.
  */

volatile int eraseNDAttributes=0;
extern "C" {epicsExportAddress(int, eraseNDAttributes);}

template <typename dataTypeIn, typename dataTypeOut>
static int convertType(NDArrayConstPtr & pIn, NDArrayPtr & pOut)
{
    shared_vector<const dataTypeIn> pInData(pIn->viewData<dataTypeIn>());
    shared_vector<dataTypeOut> pOutData(pOut->getData<dataTypeOut>());

    for(size_t i = 0; i < pOutData.size(); ++i)
        pOutData[i] = (dataTypeOut) pInData[i];

    pOut->setData<dataTypeOut>(pOutData);
    return 0;
}

template <typename dataTypeOut>
static int convertType (NDArrayConstPtr & pIn, NDArrayPtr & pOut)
{
    switch(pIn->getDataType()) {
    case pvBoolean: return convertType<boolean, dataTypeOut> (pIn, pOut);
    case pvByte:    return convertType<int8,    dataTypeOut> (pIn, pOut);
    case pvUByte:   return convertType<uint8,   dataTypeOut> (pIn, pOut);
    case pvShort:   return convertType<int16,   dataTypeOut> (pIn, pOut);
    case pvUShort:  return convertType<uint16,  dataTypeOut> (pIn, pOut);
    case pvInt:     return convertType<int32,   dataTypeOut> (pIn, pOut);
    case pvUInt:    return convertType<uint32,  dataTypeOut> (pIn, pOut);
    case pvLong:    return convertType<int64,   dataTypeOut> (pIn, pOut);
    case pvULong:   return convertType<uint64,  dataTypeOut> (pIn, pOut);
    case pvFloat:   return convertType<float,   dataTypeOut> (pIn, pOut);
    case pvDouble:  return convertType<double,  dataTypeOut> (pIn, pOut);
    default:        return -1;
  }
}

template <typename dataTypeIn, typename dataTypeOut>
static void convertDimension (shared_vector<const dataTypeIn> const & inData,
        vector<NDDimension_t> const & inDims,
        shared_vector<dataTypeOut> & outData,
        vector<NDDimension_t> const & outDims,
        size_t dim)
{
    size_t inStep   = 1;
    size_t outStep  = 1;
    int inDir       = 1;
    size_t inOffset = outDims[dim].offset;

    for (size_t i = 0; i < dim; ++i) {
        inStep  *= inDims[i].size;
        outStep *= outDims[i].size;
    }

    if (outDims[dim].reverse) {
        inOffset += outDims[dim].size * outDims[dim].binning - 1;
        inDir     = -1;
    }

    int inc     = inDir * inStep;
    size_t iIn  = inOffset*inStep;
    size_t iOut = 0;

    for (size_t i = 0; i < outDims[dim].size; ++i)
    {
        if(dim > 0)
        {
            for (int bin = 0; bin < outDims[dim].binning; ++bin)
            {
                shared_vector<const dataTypeIn> newInData(inData);
                shared_vector<dataTypeOut> newOutData(outData);

                newInData.slice(iIn);
                newOutData.slice(iOut);

                convertDimension <dataTypeIn, dataTypeOut> (newInData, inDims, newOutData, outDims, dim-1);
                iIn += inc;
            }
        }
        else
        {
            for (int bin = 0; bin < outDims[dim].binning; ++bin)
            {
                outData[iOut] += (dataTypeOut)inData[iIn];
                iIn += inc;
            }
        }

        iOut += outStep;
    }
}

template <typename dataTypeIn, typename dataTypeOut>
static int convertDimension (NDArrayConstPtr & pIn, NDArrayPtr & pOut, size_t dim)
{
    shared_vector<const dataTypeIn> pInData(pIn->viewData<dataTypeIn>());
    shared_vector<dataTypeOut> pOutData(pOut->getData<dataTypeOut>());
    const vector<NDDimension_t> pInDims(pIn->getDimensions());
    vector<NDDimension_t> pOutDims(pOut->getDimensions());

    convertDimension<dataTypeIn, dataTypeOut>(pInData, pInDims, pOutData, pOutDims, dim);

    pOut->setData(pOutData);
    return 0;
}

template <typename dataTypeOut>
static int convertDimension(NDArrayConstPtr & pIn, NDArrayPtr & pOut, size_t dim)
{
    switch(pIn->getDataType()) {
    case pvBoolean: return convertDimension <boolean, dataTypeOut> (pIn, pOut, dim);
    case pvByte:    return convertDimension <int8,    dataTypeOut> (pIn, pOut, dim);
    case pvUByte:   return convertDimension <uint8,   dataTypeOut> (pIn, pOut, dim);
    case pvShort:   return convertDimension <int16,   dataTypeOut> (pIn, pOut, dim);
    case pvUShort:  return convertDimension <uint16,  dataTypeOut> (pIn, pOut, dim);
    case pvInt:     return convertDimension <int32,   dataTypeOut> (pIn, pOut, dim);
    case pvUInt:    return convertDimension <uint32,  dataTypeOut> (pIn, pOut, dim);
    case pvLong:    return convertDimension <int64,   dataTypeOut> (pIn, pOut, dim);
    case pvULong:   return convertDimension <uint64,  dataTypeOut> (pIn, pOut, dim);
    case pvFloat:   return convertDimension <float,   dataTypeOut> (pIn, pOut, dim);
    case pvDouble:  return convertDimension <double,  dataTypeOut> (pIn, pOut, dim);
    default:        return -1;
    }
}

static int convertDimension(NDArrayConstPtr & pIn, NDArrayPtr & pOut, size_t dim)
{
    /* This routine is passed:
     * A reference to the input array
     * A reference to the output array
     * A dimension index */
    switch(pOut->getDataType()) {
    case pvBoolean: return convertDimension <boolean> (pIn, pOut, dim);
    case pvByte:    return convertDimension <int8>    (pIn, pOut, dim);
    case pvUByte:   return convertDimension <uint8>   (pIn, pOut, dim);
    case pvShort:   return convertDimension <int16>   (pIn, pOut, dim);
    case pvUShort:  return convertDimension <uint16>  (pIn, pOut, dim);
    case pvInt:     return convertDimension <int32>   (pIn, pOut, dim);
    case pvUInt:    return convertDimension <uint32>  (pIn, pOut, dim);
    case pvLong:    return convertDimension <int64>   (pIn, pOut, dim);
    case pvULong:   return convertDimension <uint64>  (pIn, pOut, dim);
    case pvFloat:   return convertDimension <float>   (pIn, pOut, dim);
    case pvDouble:  return convertDimension <double>  (pIn, pOut, dim);
    default:        return -1;
    }
}

class NDArrayPoolDeleter
{
private:
    NDArrayPool * pool_;
public:
    NDArrayPoolDeleter (NDArrayPool * pool) : pool_(pool) {};
    void operator()(NDArray *array) { pool_->giveBack(array); }
};

// This needs to be called with mutex_ taken!
NDArrayPtr NDArrayPool::getBuffer (void)
{
    const char *functionName = "getBuffer";
    NDArrayPtr nullArray, pArray;

    if(!pool_.empty()) // Get last array from pool
    {
        pArray = pool_.back();
        pool_.pop_back();
    }
    else // No array in the pool, create one
    {
        if (maxBuffers_ && numBuffers_ >= maxBuffers_)
        {
            fprintf(stderr, "%s::%s: error: reached limit of %lu buffers (memory use=%lu/%lu bytes)\n",
                    driverName, functionName, maxBuffers_, memorySize_, maxMemory_);
            return nullArray;
        }

        pArray = NDArrayPtr(new NDArray(), NDArrayPoolDeleter(this));

        if(!pArray.get())
        {
            fprintf(stderr, "%s::%s: error: failed to allocate buffer\n",
                    driverName, functionName);
            return nullArray;
        }

        ++numBuffers_;
    }

    if(eraseNDAttributes)
        pArray->eraseAttributes();

    return pArray;
}

void NDArrayPool::giveBack (NDArray *array)
{
    Lock lock(mutex_);
    pool_.push_back(NDArrayPtr(array, NDArrayPoolDeleter(this)));
}

// Must be called with mutex_ taken!
size_t NDArrayPool::freeMemory (void)
{
    return 0;
}

NDArrayPool::NDArrayPool (size_t maxBuffers, size_t maxMemory):
        pool_(), mutex_(), maxBuffers_(maxBuffers), maxMemory_(maxMemory),
        numBuffers_(0), memorySize_(0)
{}

NDArrayPtr NDArrayPool::copy (NDArrayConstPtr & pArrayIn)
{
    //const char *functionName = "copy";

    Lock lock(mutex_);
    NDArrayPtr nullArray, pArrayOut(getBuffer());

    if(!pArrayOut)
        return nullArray;

    memorySize_ -= pArrayIn->getInfo().totalBytes;
    pArrayOut->copy(pArrayIn);

    return pArrayOut;
}

NDArrayPtr NDArrayPool::alloc (int ndims, size_t *dims, ScalarType dataType, size_t dataSize)
{
    vector<NDDimension_t> dimensions(ndims);
    vector<NDDimension_t>::iterator it;

    for(int i = 0; i < ndims; ++i)
    {
        dimensions[i].size    = dims[i];
        dimensions[i].offset  = 0;
        dimensions[i].binning = 1;
        dimensions[i].reverse = false;
    }

    return alloc(dimensions, dataType, dataSize);
}

NDArrayPtr NDArrayPool::alloc (vector<NDDimension_t> const & dims, ScalarType dataType,
                  size_t dataSize)
{
    NDArrayPtr nullArray;
    switch(dataType)
    {
    case pvBoolean: return alloc<boolean>(dims, dataSize);
    case pvByte:    return alloc<int8>   (dims, dataSize);
    case pvUByte:   return alloc<uint8>  (dims, dataSize);
    case pvShort:   return alloc<int16>  (dims, dataSize);
    case pvUShort:  return alloc<uint16> (dims, dataSize);
    case pvInt:     return alloc<int32>  (dims, dataSize);
    case pvUInt:    return alloc<uint32> (dims, dataSize);
    case pvLong:    return alloc<int64>  (dims, dataSize);
    case pvULong:   return alloc<uint64> (dims, dataSize);
    case pvFloat:   return alloc<float>  (dims, dataSize);
    case pvDouble:  return alloc<double> (dims, dataSize);
    default:        return nullArray;
    }
}

template<typename valueType>
NDArrayPtr NDArrayPool::alloc (vector<NDDimension_t> const & dims, size_t dataSize)
{
    typedef typename PVValueArray<valueType>::shared_pointer arrayTypePtr;

    const char *functionName = "alloc";
    NDArrayPtr nullArray;

    Lock lock(mutex_);

    NDArrayPtr pArray(getBuffer());

    if(!pArray)
        return nullArray;

    // Calculate number of elements and total needed size
    size_t elementSize = sizeof(valueType);
    size_t nElements   = 1;

    vector<NDDimension_t>::const_iterator it;
    for(it = dims.begin(); it != dims.end(); ++it)
        nElements *= (*it).size;

    size_t totalBytes = nElements*elementSize;

    // Check dataSize
    if(!dataSize)
        dataSize = totalBytes;
    else if(totalBytes > dataSize)
    {
        fprintf(stderr, "%s::%s: ERROR: required size=%lu passed size=%lu is too small\n",
                driverName, functionName, totalBytes, dataSize);
        return nullArray;
    }

    string selected(pArray->getArray()->getValue()->getSelectedFieldName());
    size_t prevSize = selected.empty() ? 0 : pArray->getInfo().totalBytes;
    if(prevSize < dataSize)
    {
        // Check if we will exceed allowed memory
        if(maxMemory_ && memorySize_ + dataSize - prevSize > maxMemory_)
        {
            size_t freed = freeMemory();
            if(freed < dataSize - prevSize)
            {
                fprintf(stderr, "%s::%s: ERROR: no memory available\n",
                        driverName, functionName);
                return nullArray;
            }
        }
    }

    // We have enough room, resize array
    string unionField(string(ScalarTypeFunc::name(PVValueArray<valueType>::typeCode)) + string("Value"));
    arrayTypePtr value(pArray->getArray()->getValue()->select< PVValueArray<valueType> >(unionField));
    value->setLength(nElements);

    // Update used memory
    memorySize_ += dataSize - prevSize;

    // Set the new dimensions
    pArray->setDimensions(dims);

    return pArray;
}

NDArrayPtr NDArrayPool::alloc (NTNDArrayPtr const & array)
{
    PVStructurePtr pvStructure(array->getPVStructure());
    return alloc(pvStructure);
}

NDArrayPtr NDArrayPool::alloc (PVStructurePtr const & pvStructure)
{
    NDArrayPtr pArray(getBuffer());

    if(pArray)
        pArray->getArray()->getPVStructure()->copy(*pvStructure);

    return pArray;
}

size_t NDArrayPool::maxBuffers (void) const
{
    return maxBuffers_;
}

size_t NDArrayPool::maxMemory (void) const
{
    return maxMemory_;
}

size_t NDArrayPool::numBuffers (void) const
{
    return numBuffers_;
}

size_t NDArrayPool::memorySize (void) const
{
    return memorySize_;
}

size_t NDArrayPool::numFree (void) const
{
    return pool_.size();
}

void NDArrayPool::report (FILE *fp, int details)
{
    fprintf(fp, "\n");
    fprintf(fp, "NDArrayPool:\n");
    fprintf(fp, "  numBuffers=%lu, maxBuffers=%lu\n", numBuffers(), maxBuffers());
    fprintf(fp, "  memorySize=%lu, maxMemory=%lu\n", memorySize(), maxMemory());
    fprintf(fp, "  numFree=%lu\n", numFree());
}

NDArrayPtr NDArrayPool::convert (NDArrayConstPtr in, ScalarType dataTypeOut)
{
    vector<NDDimension_t> dimsOut(in->getDimensions());
    vector<NDDimension_t>::iterator it;

    for(it = dimsOut.begin(); it != dimsOut.end(); ++it)
    {
        (*it).offset  = 0;
        (*it).binning = 1;
        (*it).reverse = false;
    }

    return convert(in, dataTypeOut, dimsOut);
}

NDArrayPtr NDArrayPool::convert (NDArrayConstPtr pIn, ScalarType dataTypeOut,
        vector<NDDimension_t> dimsOut)
{
    //const char *functionName = "convert";
    int colorMode, colorModeMono = NDColorModeMono;

    vector<NDDimension_t> dimsIn(pIn->getDimensions());

    // Compute the dimensions of the output array
    bool dimsUnchanged = true;
    for (size_t i = 0; i < dimsOut.size(); ++i)
    {
        dimsOut[i].size = dimsOut[i].size / dimsOut[i].binning;

        if(dimsOut[i].size    != dimsIn[i].size ||
           dimsOut[i].offset  != 0 ||
           dimsOut[i].binning != 1 ||
           dimsOut[i].reverse)
            dimsUnchanged = false;
    }

    // We know the datatype and dimensions of the output array. Allocate it.
    NDArrayPtr pOut(alloc(dimsOut, dataTypeOut));

    // Check if allocation failed
    if(!pOut)
        return pOut;

    // Copy fields from input to output
    pOut->setTimeStamp(pIn->getTimeStamp());
    pOut->setEpicsTimeStamp(pIn->getEpicsTimeStamp());
    pOut->setUniqueId(pIn->getUniqueId());

    // Replace the dimensions with those passed to this function
    pOut->setDimensions(dimsOut);

    // Copy attributes
    NDAttributeListPtr pOutAttrs(pOut->getAttributeList());
    pIn->viewAttributeList()->copy(pOutAttrs);

    if (dimsUnchanged)
    {
        if (pIn->getDataType() == pOut->getDataType())
        {
            /* The dimensions are the same and the data type is the same,
             * then just copy the input image to the output image */
            switch(pOut->getDataType())
            {
            case pvBoolean: pOut->setData<boolean> (pIn->viewData<boolean>()); break;
            case pvByte:    pOut->setData<int8>    (pIn->viewData<int8>());    break;
            case pvUByte:   pOut->setData<uint8>   (pIn->viewData<uint8>());   break;
            case pvShort:   pOut->setData<int16>   (pIn->viewData<int16>());   break;
            case pvUShort:  pOut->setData<uint16>  (pIn->viewData<uint16>());  break;
            case pvInt:     pOut->setData<int32>   (pIn->viewData<int32>());   break;
            case pvUInt:    pOut->setData<uint32>  (pIn->viewData<uint32>());  break;
            case pvLong:    pOut->setData<int64>   (pIn->viewData<int64>());   break;
            case pvULong:   pOut->setData<uint64>  (pIn->viewData<uint64>());  break;
            case pvFloat:   pOut->setData<float>   (pIn->viewData<float>());   break;
            case pvDouble:  pOut->setData<double>  (pIn->viewData<double>());  break;
            default:
                //status = ND_ERROR;
                pOut.reset();
            }

            return pOut;
        }
        else
        {
            // We need to convert data types
            switch(pOut->getDataType())
            {
            case pvBoolean: convertType <boolean> (pIn, pOut); break;
            case pvByte:    convertType <int8>    (pIn, pOut); break;
            case pvUByte:   convertType <uint8>   (pIn, pOut); break;
            case pvShort:   convertType <int16>   (pIn, pOut); break;
            case pvUShort:  convertType <uint16>  (pIn, pOut); break;
            case pvInt:     convertType <int32>   (pIn, pOut); break;
            case pvUInt:    convertType <uint32>  (pIn, pOut); break;
            case pvLong:    convertType <int64>   (pIn, pOut); break;
            case pvULong:   convertType <uint64>  (pIn, pOut); break;
            case pvFloat:   convertType <float>   (pIn, pOut); break;
            case pvDouble:  convertType <double>  (pIn, pOut); break;
            default:
                //status = ND_ERROR;
                pOut.reset();
                return pOut;
            }
        }
    }
    else
    {
        /* The input and output dimensions are not the same, so we are extracting a region
        * and/or binning */
        // Clear entire output array
        pOut->zeroData();
        convertDimension(pIn, pOut, dimsIn.size()-1);
    }

    /* Set fields in the output array */
    for (size_t i = 0; i < dimsOut.size(); ++i) {
        dimsOut[i].offset += dimsIn[i].offset;
        dimsOut[i].binning *= dimsIn[i].binning;
        if (dimsIn[i].reverse)
            dimsOut[i].reverse = !dimsOut[i].reverse;
    }

    /* If the frame is an RGBx frame and we have collapsed that dimension then change the colorMode */
    NDAttributePtr pAttribute(pOut->getAttributeList()->find("ColorMode"));
    if (pAttribute && pAttribute->getValue(NDAttrInt32, &colorMode)) {
        if      ((colorMode == NDColorModeRGB1) && (dimsOut[0].size != 3))
            pAttribute->setValue(&colorModeMono);
        else if ((colorMode == NDColorModeRGB2) && (dimsOut[1].size != 3))
            pAttribute->setValue(&colorModeMono);
        else if ((colorMode == NDColorModeRGB3) && (dimsOut[2].size != 3))
            pAttribute->setValue(&colorModeMono);
    }
    return pOut;
}
