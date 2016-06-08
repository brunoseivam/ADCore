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

#include "NDArray.h"

#include <vector>
#include <pv/pvData.h>
#include <pv/ntndarray.h>

using namespace std;
using namespace epics::nt;
using namespace epics::pvData;

void NDArray::initDimension (NDDimension_t & dim, size_t size, size_t offset,
        int binning, int reverse)
{
    dim.size    = size;
    dim.offset  = offset;
    dim.binning = binning;
    dim.reverse = reverse;
}

void NDArray::initDimensions (vector<size_t> const & dimSizes)
{
    PVStructureArrayPtr dimension(mArray->getDimension());
    PVStructureArray::svector dimensionVec(dimension->reuse());
    StructureConstPtr dimensionStruct(dimension->getStructureArray()->getStructure());

    dimensionVec.resize(dimSizes.size());
    for(size_t i = 0; i < dimSizes.size(); ++i)
    {
        if(!dimensionVec[i])
            dimensionVec[i] = getPVDataCreate()->createPVStructure(dimensionStruct);

        dimensionVec[i]->getSubField<PVInt>("size")->put(dimSizes[i]);
        dimensionVec[i]->getSubField<PVInt>("offset")->put(0);
        dimensionVec[i]->getSubField<PVInt>("fullSize")->put(dimSizes[i]);
        dimensionVec[i]->getSubField<PVInt>("binning")->put(1);
        dimensionVec[i]->getSubField<PVBoolean>("reverse")->put(false);
    }
    dimension->replace(freeze(dimensionVec));
}

void NDArray::eraseAttributes (void)
{
    mArray->getAttribute().reset();
}

NDArray::NDArray (void) :
    mArray(NTNDArray::createBuilder()->addTimeStamp()->create())/*,
    mAttributes(NDAttributeList::create(mArray->getAttribute()))*/
{
}

NDArray::NDArray (NTNDArrayPtr array) :
    mArray(array)
{
}

NTNDArrayPtr NDArray::getArray (void) const
{
    return mArray;
}

vector<NDDimension_t> NDArray::getDimensions (void) const
{
    PVStructureArrayPtr dimsArray(mArray->getDimension());
    PVStructureArray::const_svector dimsVector(dimsArray->view());

    size_t ndims = dimsVector.size();
    vector<NDDimension_t> dims(ndims);

    for(size_t i = 0; i < ndims; ++i)
    {
        dims[i].size     = dimsVector[i]->getSubField<PVInt>("size")->get();
        dims[i].fullSize = dimsVector[i]->getSubField<PVInt>("fullSize")->get();
        dims[i].offset   = dimsVector[i]->getSubField<PVInt>("offset")->get();
        dims[i].binning  = dimsVector[i]->getSubField<PVInt>("binning")->get();
        dims[i].reverse  = dimsVector[i]->getSubField<PVBoolean>("reverse")->get();
    }

    return dims;
}

ScalarType NDArray::getDataType (void) const
{
    string name(mArray->getValue()->getSelectedFieldName());

    if(name.empty())
        throw std::runtime_error("no union field selected");

    return ScalarTypeFunc::getScalarType(name.substr(0,name.find("Value")));
}

NDColorMode_t NDArray::getColorMode (void) const
{
    NDColorMode_t colorMode = NDColorModeMono;
    PVStructureArray::const_svector attrs(mArray->getAttribute()->view());

    for(PVStructureArray::const_svector::iterator it(attrs.cbegin());
            it != attrs.cend(); ++it)
    {
        if((*it)->getSubField<PVString>("name")->get() == "ColorMode")
        {
            PVUnionPtr field((*it)->getSubField<PVUnion>("value"));
            colorMode = (NDColorMode_t) 0;//static_pointer_cast<PVInt>(field->get())->get();
            break;
        }
    }

    return colorMode;
}

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
    return mArray->getUniqueId()->get();
}

int64 NDArray::getCompressedSize (void) const
{
    return mArray->getCompressedDataSize()->get();
}

int64 NDArray::getUncompressedSize (void) const
{
    return mArray->getUncompressedDataSize()->get();
}

TimeStamp NDArray::getTimeStamp (void) const
{

    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(mArray->getDataTimeStamp());

    TimeStamp timeStamp;
    pvTimeStamp.get(timeStamp);

    return timeStamp;
}

TimeStamp NDArray::getEpicsTimeStamp (void) const
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(mArray->getTimeStamp());

    TimeStamp timeStamp;
    pvTimeStamp.get(timeStamp);

    return timeStamp;
}

/*
NDAttributeListPtr NDArray::getAttributeList (void)
{
    return mAttributes;
}*/

int NDArray::report (FILE *fp, int details)
{
    fprintf(fp, "\n");
    fprintf(fp, "NDArray: address=%p:\n", this);

    vector<NDDimension_t> dims(getDimensions());
    size_t ndims = dims.size();

    fprintf(fp, "  ndims=%lu dims=[", ndims);
    for (size_t i = 0; i < ndims; i++)
        fprintf(fp, "%lu ", dims[i].size);
    fprintf(fp, "]\n");

    fprintf(fp, "  dataType=%s\n", ScalarTypeFunc::name(getDataType()));
    fprintf(fp, "  compressedSize=%lu, uncompressedSize=%lu",
            getCompressedSize(), getUncompressedSize());
    fprintf(fp, "  uniqueId=%d, timeStamp=%f\n",
        getUniqueId(), getTimeStamp().toSeconds());

    /*fprintf(fp, "  number of attributes=%d\n", mAttributes->count());

    if (details > 5)
        mAttributes->report(fp, details);*/

    return 0;
}

void NDArray::setDimensions (vector<NDDimension_t> const & dims)
{
    PVStructureArrayPtr dimsArray(mArray->getDimension());
    PVStructureArray::svector dimsVector(dimsArray->reuse());
    StructureConstPtr dimStructure(dimsArray->getStructureArray()->getStructure());

    size_t ndims = dims.size();
    dimsVector.resize(ndims);

    for (size_t i = 0; i < ndims; i++)
    {
        if (!dimsVector[i] || !dimsVector[i].unique())
            dimsVector[i] = getPVDataCreate()->createPVStructure(dimStructure);

        dimsVector[i]->getSubField<PVInt>("size")->put(dims[i].size);
        dimsVector[i]->getSubField<PVInt>("fullSize")->put(dims[i].fullSize);
        dimsVector[i]->getSubField<PVInt>("offset")->put(dims[i].offset);
        dimsVector[i]->getSubField<PVInt>("binning")->put(dims[i].binning);
        dimsVector[i]->getSubField<PVBoolean>("reverse")->put(dims[i].reverse);
    }
    dimsArray->replace(freeze(dimsVector));
}

void NDArray::setUniqueId (int32 value)
{
    mArray->getUniqueId()->put(value);
}

void NDArray::setCompressedSize (int64 value)
{
    return mArray->getCompressedDataSize()->put(value);
}

void NDArray::setUncompressedSize (int64 value)
{
    return mArray->getUncompressedDataSize()->put(value);
}

void NDArray::setTimeStamp (TimeStamp const & value)
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(mArray->getDataTimeStamp());

    pvTimeStamp.set(value);
}

void NDArray::setEpicsTimeStamp (TimeStamp const & value)
{
    PVTimeStamp pvTimeStamp;
    pvTimeStamp.attach(mArray->getTimeStamp());

    pvTimeStamp.set(value);
}
