/* NDArray.cpp
 *
 * NDArray classes
 *
 *
 * \author Mark Rivers
 *
 * \author University of Chicago
 *
 * \date May 11 2008
 *
 */

#include <vector>

#include <string.h>
#include <stdio.h>
#include <ellLib.h>

#include <epicsMutex.h>
#include <epicsTypes.h>
#include <epicsString.h>
#include <ellLib.h>
#include <cantProceed.h>

#include <epicsExport.h>

#include "NDArray.h"

using std::vector;
using std::string;
using std::tr1::static_pointer_cast;
using std::tr1::shared_ptr;

using namespace epics::nt;
using namespace epics::pvData;

const NTNDArrayBuilderPtr NDArray::builder(NTNDArray::createBuilder()->addTimeStamp());

void NDArray::initAttributes (void)
{
    PVStructureArrayPtr ntAttrs(array_->getAttribute());
    attributes_ = NDAttributeListPtr(new NDAttributeList(ntAttrs));
}

/** Initializes the dimension structure
  * \param[in] dim Reference to an NDDimension_t structure
  * \param[in] size The size of this dimension.
  * \param[in] offset The offset of this dimension.
  * \param[in] binning The binning of this dimension.
  * \param[in] reverse Is this dimension reversed? */
void NDArray::initDimension (NDDimension_t & dim, size_t size, size_t offset,
        int binning, int reverse)
{
    dim.size    = size;
    dim.offset  = offset;
    dim.binning = binning;
    dim.reverse = reverse;
}

/** NDArray constructor, no parameters.
  * Creates a brand new underlying NTNDArray */
NDArray::NDArray (void)
: array_(NDArray::builder->create())
{
    initAttributes();
}

/** NDArray constructor that accepts an existing NTNDArray
  * \param[in] array The existing NTNDArray that will be copied */
NDArray::NDArray (NTNDArray::const_shared_pointer & array)
  : array_(NDArray::builder->create())
{
    array_->getPVStructure()->copy(*array->getPVStructure());
    initAttributes();
}

void NDArray::copy (NDArrayConstPtr & from)
{
    array_->getPVStructure()->copy(*from->array_->getPVStructure());
    initAttributes();
}

NTNDArrayPtr NDArray::getArray (void)
{
    return array_;
}

NTNDArray::const_shared_pointer NDArray::getArray (void) const
{
    return array_;
}

const void *NDArray::viewRawData (void) const
{
    switch(getDataType())
    {
    case pvBoolean: return static_cast<const void*>(viewData<bool>().dataPtr().get());
    case pvByte:    return static_cast<const void*>(viewData<int8>().dataPtr().get());
    case pvUByte:   return static_cast<const void*>(viewData<uint8>().dataPtr().get());
    case pvShort:   return static_cast<const void*>(viewData<int16>().dataPtr().get());
    case pvUShort:  return static_cast<const void*>(viewData<uint16>().dataPtr().get());
    case pvInt:     return static_cast<const void*>(viewData<int32>().dataPtr().get());
    case pvUInt:    return static_cast<const void*>(viewData<uint32>().dataPtr().get());
    case pvLong:    return static_cast<const void*>(viewData<int64>().dataPtr().get());
    case pvULong:   return static_cast<const void*>(viewData<uint64>().dataPtr().get());
    case pvFloat:   return static_cast<const void*>(viewData<float>().dataPtr().get());
    case pvDouble:  return static_cast<const void*>(viewData<double>().dataPtr().get());
    default:
        return NULL;
    }
}

vector<NDDimension_t> NDArray::getDimensions (void) const
{
    PVStructureArrayPtr dimsArray(array_->getDimension());
    PVStructureArray::const_svector dimsVector(dimsArray->view());

    size_t ndims = dimsVector.size();
    vector<NDDimension_t> dims(ndims);

    for(size_t i = 0; i < ndims; ++i)
    {
        dims[i].size     = dimsVector[i]->getSubField<PVInt>("size")->get();
        //dims[i].fullSize = dimsVector[i]->getSubField<PVInt>("fullSize")->get();
        dims[i].offset   = dimsVector[i]->getSubField<PVInt>("offset")->get();
        dims[i].binning  = dimsVector[i]->getSubField<PVInt>("binning")->get();
        dims[i].reverse  = dimsVector[i]->getSubField<PVBoolean>("reverse")->get();
    }

    return dims;
}

ScalarType NDArray::getDataType (void) const
{
    string name(array_->getValue()->getSelectedFieldName());

    if(name.empty())
        throw std::runtime_error("no union field selected");

    return ScalarTypeFunc::getScalarType(name.substr(0,name.find("Value")));
}

NDColorMode_t NDArray::getColorMode (void) const
{
    NDColorMode_t colorMode = NDColorModeMono;

    NDAttributeConstPtr colorModeAttr(attributes_->find("ColorMode"));
    if(colorModeAttr)
        colorModeAttr->getValue(NDAttrInt32, &colorMode);

    return colorMode;
}

/** Convenience method returns information about an NDArray, including the total number of elements, 
  * the number of bytes per element, and the total number of bytes in the array.
  * \returns An NDArrayInfo_t structure instance. */
NDArrayInfo_t NDArray::getInfo (void) const
{
    NDArrayInfo_t info = {};

    vector<NDDimension_t> dims(getDimensions());
    size_t ndims = dims.size();

    info.nElements = 1;

    for(size_t i = 0; i < ndims; ++i)
        info.nElements *= dims[i].size;

    info.bytesPerElement = ScalarTypeFunc::elementSize(getDataType());
    info.totalBytes      = info.nElements*info.bytesPerElement;
    info.colorMode       = getColorMode();

    if(ndims > 0)
    {
        info.x.dim    = 0;
        info.x.stride = 1;
        info.x.size   = dims[0].size;
    }

    if(ndims > 1)
    {
        info.y.dim    = 1;
        info.y.stride = 1;
        info.y.size   = dims[1].size;
    }

    if(ndims == 3)
    {
        switch(info.colorMode)
        {
        case NDColorModeRGB1:
            info.x.dim        = 1;
            info.y.dim        = 2;
            info.color.dim    = 0;
            info.x.stride     = dims[0].size;
            info.y.stride     = dims[0].size*dims[1].size;
            info.color.stride = 1;
            break;

        case NDColorModeRGB2:
            info.x.dim        = 0;
            info.y.dim        = 2;
            info.color.dim    = 1;
            info.x.stride     = 1;
            info.y.stride     = dims[0].size*dims[1].size;
            info.color.stride = dims[0].size;
            break;

        case NDColorModeRGB3:
            info.x.dim        = 1;
            info.y.dim        = 2;
            info.color.dim    = 0;
            info.x.stride     = dims[0].size;
            info.y.stride     = dims[0].size*dims[1].size;
            info.color.stride = 1;
            break;

        default:
            info.x.dim        = 0;
            info.y.dim        = 1;
            info.color.dim    = 2;
            info.x.stride     = 1;
            info.y.stride     = dims[0].size;
            info.color.stride = dims[0].size*dims[1].size;
            break;
        }

        info.x.size     = dims[info.x.dim].size;
        info.y.size     = dims[info.y.dim].size;
        info.color.size = dims[info.color.dim].size;
    }

    return info;
}

int32 NDArray::getUniqueId (void) const
{
    return array_->getUniqueId()->get();
}

int64 NDArray::getCompressedSize (void) const
{
    return array_->getCompressedDataSize()->get();
}

int64 NDArray::getUncompressedSize (void) const
{
    return array_->getUncompressedDataSize()->get();
}

TimeStamp NDArray::getTimeStamp (void) const
{

    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(array_->getDataTimeStamp());

    TimeStamp timeStamp;
    pvTimeStamp.get(timeStamp);

    return timeStamp;
}

TimeStamp NDArray::getEpicsTimeStamp (void) const
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(array_->getTimeStamp());

    TimeStamp timeStamp;
    pvTimeStamp.get(timeStamp);

    return timeStamp;
}

bool NDArray::hasData (void) const
{
    string name(array_->getValue()->getSelectedFieldName());
    return !name.empty();
}

NDAttributeListPtr NDArray::getAttributeList (void)
{
    return attributes_;
}

NDAttributeListConstPtr NDArray::viewAttributeList (void) const
{
    return attributes_;
}

void NDArray::eraseAttributes (void)
{
    array_->getAttribute()->reuse();
    initAttributes();
}

void NDArray::setDimensions (vector<NDDimension_t> const & dims)
{
    PVStructureArrayPtr dimsArray(array_->getDimension());
    PVStructureArray::svector dimsVector(dimsArray->reuse());
    StructureConstPtr dimStructure(dimsArray->getStructureArray()->getStructure());

    size_t ndims = dims.size();
    dimsVector.resize(ndims);

    for (size_t i = 0; i < ndims; i++)
    {
        if (!dimsVector[i] || !dimsVector[i].unique())
            dimsVector[i] = getPVDataCreate()->createPVStructure(dimStructure);

        dimsVector[i]->getSubField<PVInt>("size")->put(dims[i].size);
        dimsVector[i]->getSubField<PVInt>("fullSize")->put(dims[i].size);
        dimsVector[i]->getSubField<PVInt>("offset")->put(dims[i].offset);
        dimsVector[i]->getSubField<PVInt>("binning")->put(dims[i].binning);
        dimsVector[i]->getSubField<PVBoolean>("reverse")->put(dims[i].reverse);
    }
    dimsArray->replace(freeze(dimsVector));
}

void NDArray::setUniqueId (int32 value)
{
    array_->getUniqueId()->put(value);
}

void NDArray::setCompressedSize (int64 value)
{
    return array_->getCompressedDataSize()->put(value);
}

void NDArray::setUncompressedSize (int64 value)
{
    return array_->getUncompressedDataSize()->put(value);
}

void NDArray::setTimeStamp (TimeStamp const & value)
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(array_->getDataTimeStamp());

    pvTimeStamp.set(value);
}

void NDArray::setEpicsTimeStamp (TimeStamp const & value)
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(array_->getTimeStamp());

    pvTimeStamp.set(value);
}

/** Reports on the properties of the array.
  * \param[in] fp File pointer for the report output.
  * \param[in] details Level of report details desired; if >5 calls NDAttributeList::report().
  */
int NDArray::report (FILE *fp, int details) const
{
    fprintf(fp, "\n");
    fprintf(fp, "NDArray: address=%p:\n", this);
    fprintf(fp, "  NTNDArray address=%p\n", array_.get());

    vector<NDDimension_t> dims(getDimensions());
    size_t ndims = dims.size();

    fprintf(fp, "  ndims=%lu dims=[", ndims);
    for (size_t i = 0; i < ndims; i++)
        fprintf(fp, "%lu ", dims[i].size);
    fprintf(fp, "]\n");

    if(hasData())
    {
        fprintf(fp, "  dataType=%s\n", ScalarTypeFunc::name(getDataType()));
        fprintf(fp, "  data address=%p\n", viewRawData());
    }
    else
        fprintf(fp, "  dataType=(no data)\n");
    fprintf(fp, "  compressedSize=%lu, uncompressedSize=%lu",
            getCompressedSize(), getUncompressedSize());
    fprintf(fp, "  uniqueId=%d, timeStamp=%f\n",
        getUniqueId(), getTimeStamp().toSeconds());

    fprintf(fp, "  number of attributes=%lu\n", attributes_->count());

    if (details > 5)
        attributes_->report(fp, details);

    return 0;
}
