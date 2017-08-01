/** NDAttribute.cpp
 *
 * Mark Rivers
 * University of Chicago
 * October 18, 2013
 *
 */

#include <string>
#include <stdlib.h>

#include <epicsString.h>

#include <epicsExport.h>

#include <pv/ntndarrayAttribute.h>

#include "NDAttribute.h"

using std::string;
using std::tr1::shared_ptr;
using std::tr1::static_pointer_cast;

using namespace epics::nt;
using namespace epics::pvData;

static const PVDataCreatePtr PVDC = getPVDataCreate();

const NTNDArrayAttributeBuilderPtr NDAttribute::builder(NTNDArrayAttribute::createBuilder());

/** Strings corresponding to the above enums */
static const char *NDAttrSourceStrings[] = {
    "DRIVER",
    "PARAM",
    "EPICS_PV",
    "FUNCTION"
};

const char *NDAttribute::attrSourceString(NDAttrSource_t type)
{
    return NDAttrSourceStrings[type];
}

/** NDAttribute constructor
  * \param[in] pName The name of the attribute to be created. 
  * \param[in] sourceType The source type of the attribute (NDAttrSource_t).
  * \param[in] pSource The source string for the attribute.
  * \param[in] pDescription The description of the attribute.
  * \param[in] dataType The data type of the attribute (NDAttrDataType_t).
  * \param[in] pValue A pointer to the value for this attribute.
  */
NDAttribute::NDAttribute(string const & name, string const & description,
                         NDAttrSource_t sourceType, string const & source,
                         NDAttrDataType_t dataType, void *pValue)
: attribute_(builder->create())
{
    setName(name);
    setDescription(description);
    setSource(source);
    setSourceType(sourceType);

    if (pValue)
        setValue(pValue, dataType);
}

/** NDAttribute copy constructor
  * \param[in] attribute The attribute to copy from
  */
NDAttribute::NDAttribute(NDAttribute const & attribute)
: attribute_(builder->create())
{
    attribute_->getPVStructure()->copy(*attribute.attribute_->getPVStructure());
}

/** NDAttribute constructor
  * \param[in] attribute The NTNDArrayAttribute to wrap
  */
NDAttribute::NDAttribute(epics::nt::NTNDArrayAttributePtr & attribute)
: attribute_(attribute)
{
}

/** NDAttribute destructor */
NDAttribute::~NDAttribute()
{
}

/** Copies properties from <b>this</b> to pOut.
  * \param[in] pOut A pointer to the output attribute
  * Only the value is copied, all other fields are assumed to already be the same in pOut
  */
void NDAttribute::copy(NDAttributePtr & pOut) const
{
    pOut->attribute_->getValue()->copy(*attribute_->getValue());
}

/** Returns the name of this attribute.
  */
string NDAttribute::getName() const
{
    return attribute_->getName()->get();
}

/** Returns the data type of this attribute.
  */
NDAttrDataType_t NDAttribute::getDataType() const
{
    PVScalarPtr unionScalar(attribute_->getValue()->get<PVScalar>());
    if(!unionScalar)
        return NDAttrUndefined;

    return static_cast<NDAttrDataType_t>(unionScalar->getScalar()->getScalarType());
}

/** Returns the description of this attribute.
  */
string NDAttribute::getDescription() const
{
    return attribute_->getDescriptor()->get();
}

/** Returns the source string of this attribute.
  */
string NDAttribute::getSource() const
{
    return attribute_->getSource()->get();
}

/** Returns the source information of this attribute.
  * \param[out] pSourceType Source type (NDAttrSource_t) of this attribute.
  * \return The source type string of this attribute
  */
string NDAttribute::getSourceInfo(NDAttrSource_t *pSourceType) const
{
    *pSourceType = static_cast<NDAttrSource_t>(attribute_->getSourceType()->get());
    return sourceTypeString_;
}

/** Sets the value for this attribute. Maintain same type.
  * \param[in] pValue Pointer to the value. */
int NDAttribute::setValue(const void *pValue)
{
    return setValue(pValue, getDataType());
}

/** Sets the value for this attribute.
  * \param[in] pValue Pointer to the value.
  * \param[in] dataType The type of the value */
int NDAttribute::setValue(const void *pValue, NDAttrDataType_t dataType)
{
    /* If any data type but undefined then pointer must be valid */
    if ((dataType != NDAttrUndefined) && !pValue) return ND_ERROR;

    switch(dataType)
    {
    case NDAttrBool:      return setValueT(*static_cast<const boolean*>(pValue));
    case NDAttrInt8:      return setValueT(*static_cast<const int8*>(pValue));
    case NDAttrUInt8:     return setValueT(*static_cast<const uint8*>(pValue));
    case NDAttrInt16:     return setValueT(*static_cast<const int16*>(pValue));
    case NDAttrUInt16:    return setValueT(*static_cast<const uint16*>(pValue));
    case NDAttrInt32:     return setValueT(*static_cast<const int32*>(pValue));
    case NDAttrUInt32:    return setValueT(*static_cast<const uint32*>(pValue));
    case NDAttrInt64:     return setValueT(*static_cast<const int64*>(pValue));
    case NDAttrUInt64:    return setValueT(*static_cast<const uint64*>(pValue));
    case NDAttrFloat32:   return setValueT(*static_cast<const float*>(pValue));
    case NDAttrFloat64:   return setValueT(*static_cast<const double*>(pValue));
    case NDAttrString:    return setValue(string(static_cast<const char *>(pValue)));
    case NDAttrUndefined:
        attribute_->getValue()->get().reset();
        return ND_SUCCESS;

    default:
        return ND_ERROR;
    }
}

/** Sets the string value for this attribute.
  * \param[in] value value of this attribute. */
int NDAttribute::setValue(const string& value)
{
    /* Data type must be string */
    NDAttrDataType_t dataType = getDataType();
    if(dataType != NDAttrUndefined && dataType != NDAttrString)
        return ND_ERROR;

    PVUnionPtr un(attribute_->getValue());

    if(!un->get())
        un->set(PVDC->createPVScalar<PVString>());

    static_pointer_cast<PVString>(un->get())->put(value);
    return ND_SUCCESS;
}

/** Sets the numeric value for this attribute.
  * \param[in] value value of this attribute. */
template <typename epicsType>
int NDAttribute::setValueT(epicsType value)
{
    NDAttrDataType_t dataType = getDataType();
    if(dataType != NDAttrUndefined && dataType != static_cast<NDAttrDataType_t>(PVScalarValue<epicsType>::typeCode))
        return ND_ERROR;

    PVUnionPtr un(attribute_->getValue());

    if(!un->get())
        un->set(PVDC->createPVScalar< PVScalarValue<epicsType> >());

    static_pointer_cast< PVScalarValue<epicsType> >(un->get())->put(value);
    return ND_SUCCESS;
}

/** Returns the data type and size of this attribute.
  * \param[out] pDataType Pointer to location to return the data type.
  * \param[out] pSize Pointer to location to return the data size; this is the
  *  data type size for all data types except NDAttrString, in which case it is the length of the
  * string including 0 terminator. */
int NDAttribute::getValueInfo(NDAttrDataType_t *pDataType, size_t *pSize) const
{
    *pDataType = getDataType();
    switch (*pDataType) {
        case NDAttrBool:        *pSize = sizeof(boolean); break;
        case NDAttrInt8:        *pSize = sizeof(int8);    break;
        case NDAttrUInt8:       *pSize = sizeof(uint8);   break;
        case NDAttrInt16:       *pSize = sizeof(int16);   break;
        case NDAttrUInt16:      *pSize = sizeof(uint16);  break;
        case NDAttrInt32:       *pSize = sizeof(int32);   break;
        case NDAttrUInt32:      *pSize = sizeof(uint32);  break;
        case NDAttrInt64:       *pSize = sizeof(int64);   break;
        case NDAttrUInt64:      *pSize = sizeof(uint64);  break;
        case NDAttrFloat32:     *pSize = sizeof(float);   break;
        case NDAttrFloat64:     *pSize = sizeof(double);  break;
        case NDAttrUndefined:   *pSize = 0;               break;
        case NDAttrString:
        {
            std::string s;
            getValue(s);
            *pSize = s.size()+1;
            break;
        }
        default:
            return ND_ERROR;
    }
    return ND_SUCCESS;
}

template <typename epicsType>
int NDAttribute::getValueT(void *pValueIn) const
{
    NDAttrDataType_t dataType = getDataType();

    if(dataType == NDAttrString || dataType == NDAttrUndefined)
        return ND_ERROR;

    epicsType *pValue = static_cast<epicsType *>(pValueIn);
    *pValue = attribute_->getValue()->get< PVScalarValue<epicsType> >()->get();
    return ND_SUCCESS ;
}

/** Returns the value of this attribute.
  * \param[in] dataType Data type for the value.
  * \param[out] pValue Pointer to location to return the value.
  * \param[in] dataSize Size of the input data location; only used when dataType is NDAttrString.
  *
  * Does data type conversions between numeric data types */
int NDAttribute::getValue(NDAttrDataType_t dataType, void *pValue, size_t dataSize) const
{
    if(getDataType() == NDAttrUndefined)
        return ND_ERROR;

    if(getDataType() == NDAttrString)
    {
        if (dataType != NDAttrString) return ND_ERROR;
        string value(attribute_->getValue()->get<PVString>()->get());
        if (dataSize == 0) dataSize = value.size()+1;
        strncpy(static_cast<char *>(pValue), value.c_str(), dataSize);
        return ND_SUCCESS;
    }

    switch (dataType) {
        case NDAttrBool:    return getValueT<boolean>(pValue);
        case NDAttrInt8:    return getValueT<int8>   (pValue);
        case NDAttrUInt8:   return getValueT<uint8>  (pValue);
        case NDAttrInt16:   return getValueT<int16>  (pValue);
        case NDAttrUInt16:  return getValueT<uint16> (pValue);
        case NDAttrInt32:   return getValueT<int32>  (pValue);
        case NDAttrUInt32:  return getValueT<uint32> (pValue);
        case NDAttrInt64:   return getValueT<int64>  (pValue);
        case NDAttrUInt64:  return getValueT<uint64> (pValue);
        case NDAttrFloat32: return getValueT<float>  (pValue);
        case NDAttrFloat64: return getValueT<double> (pValue);
        default:            return ND_ERROR;
    }
    return ND_SUCCESS ;
}

/** Returns the value of an NDAttrString attribute as an std::string.
  * \param[out] value Location to return the value.
  *
  * Does data type conversions between numeric data types */
int NDAttribute::getValue(std::string& value) const
{
  if(getDataType() != NDAttrString)
    return ND_ERROR;

  value = attribute_->getValue()->get<PVString>()->get();
  return ND_SUCCESS;
}

/** Updates the current value of this attribute.
  * The base class does nothing, but derived classes may fetch the current value of the attribute,
  * for example from an EPICS PV or driver parameter library.
 */
int NDAttribute::updateValue()
{
  return ND_SUCCESS;
}

/** Returns the underlying NTNDArrayAttribute
 */
NTNDArrayAttributePtr NDAttribute::getNTAttribute()
{
    return attribute_;
}

/** Reports on the properties of the attribute.
  * \param[in] fp File pointer for the report output.
  * \param[in] details Level of report details desired; currently does nothing
  */
int NDAttribute::report(FILE *fp, int details) const
{
    fprintf(fp, "\n");
    fprintf(fp, "NDAttribute, address=%p:\n", this);
    fprintf(fp, "  name=%s\n", getName().c_str());
    fprintf(fp, "  description=%s\n", getDescription().c_str());

    NDAttrSource_t sourceType;
    string sourceTypeString = getSourceInfo(&sourceType);

    fprintf(fp, "  source type=%d\n", sourceType);
    fprintf(fp, "  source type string=%s\n", sourceTypeString.c_str());
    fprintf(fp, "  source=%s\n", getSource().c_str());
    switch (getDataType()) {
    case NDAttrBool:
    {
        boolean value;
        getValueT<boolean>(&value);
        fprintf(fp, "  dataType=NDAttrBool\n");
        fprintf(fp, "  value=%d\n", value);
        break;
    }
    case NDAttrInt8:
    {
        int8 value;
        getValueT<int8>(&value);
        fprintf(fp, "  dataType=NDAttrInt8\n");
        fprintf(fp, "  value=%d\n", value);
        break;
    }
    case NDAttrUInt8:
    {
        uint8 value;
        getValueT<uint8>(&value);
        fprintf(fp, "  dataType=NDAttrUInt8\n");
        fprintf(fp, "  value=%u\n", value);
        break;
    }
    case NDAttrInt16:
    {
        int16 value;
        getValueT<int16>(&value);
        fprintf(fp, "  dataType=NDAttrInt16\n");
        fprintf(fp, "  value=%d\n", value);
        break;
    }
    case NDAttrUInt16:
    {
        uint16 value;
        getValueT<uint16>(&value);
        fprintf(fp, "  dataType=NDAttrUInt16\n");
        fprintf(fp, "  value=%u\n", value);
        break;
    }
    case NDAttrInt32:
    {
        int32 value;
        getValueT<int32>(&value);
        fprintf(fp, "  dataType=NDAttrInt32\n");
        fprintf(fp, "  value=%d\n", value);
        break;
    }
    case NDAttrUInt32:
    {
        uint32 value;
        getValueT<uint32>(&value);
        fprintf(fp, "  dataType=NDAttrUInt32\n");
        fprintf(fp, "  value=%u\n", value);
        break;
    }
    case NDAttrInt64:
    {
        int64 value;
        getValueT<int64>(&value);
        fprintf(fp, "  dataType=NDAttrInt64\n");
        fprintf(fp, "  value=%ld\n", value);
        break;
    }
    case NDAttrUInt64:
    {
        uint64 value;
        getValueT<uint64>(&value);
        fprintf(fp, "  dataType=NDAttrUInt64\n");
        fprintf(fp, "  value=%lu\n", value);
        break;
    }
    case NDAttrFloat32:
    {
        float value;
        getValueT<float>(&value);
        fprintf(fp, "  dataType=NDAttrFloat32\n");
        fprintf(fp, "  value=%f\n", value);
        break;
    }
    case NDAttrFloat64:
    {
        double value;
        getValueT<double>(&value);
        fprintf(fp, "  dataType=NDAttrFloat64\n");
        fprintf(fp, "  value=%lf\n", value);
        break;
    }
    case NDAttrString:
    {
        string value;
        getValue(value);
        fprintf(fp, "  dataType=NDAttrString\n");
        fprintf(fp, "  value=%s\n", value.c_str());
        break;
    }
    case NDAttrUndefined:
        fprintf(fp, "  dataType=NDAttrUndefined\n");
        break;
    default:
        fprintf(fp, "  dataType=UNKNOWN\n");
        return ND_ERROR;
        break;
    }
    return ND_SUCCESS;
}

void NDAttribute::setName (std::string const & name)
{
    attribute_->getName()->put(name);
}

void NDAttribute::setDescription (std::string const & description)
{
    attribute_->getDescriptor()->put(description);
}

void NDAttribute::setSource (std::string const & source)
{
    attribute_->getSource()->put(source);
}

void NDAttribute::setSourceType (NDAttrSource_t sourceType)
{
    switch (sourceType) {
        case NDAttrSourceDriver:
            this->sourceTypeString_ = "NDAttrSourceDriver";
            break;
        case NDAttrSourceEPICSPV:
            this->sourceTypeString_ = "NDAttrSourceEPICSPV";
            break;
        case NDAttrSourceParam:
            this->sourceTypeString_ = "NDAttrSourceParam";
            break;
        case NDAttrSourceFunct:
            this->sourceTypeString_ = "NDAttrSourceFunct";
            break;
        default:
            sourceType = NDAttrSourceUndefined;
            this->sourceTypeString_ = "Undefined";
    }
    attribute_->getSourceType()->put(static_cast<int32>(sourceType));
}

int NDAttribute::setDataType (NDAttrDataType_t type)
{
    NDAttrDataType_t currentDataType = getDataType();
    if(type == currentDataType)
        return ND_SUCCESS;

    if(currentDataType != NDAttrUndefined)
    {
        fprintf(stderr, "NDAttribute::setDataType, data type already defined = %d\n", static_cast<int>(currentDataType));
        return ND_ERROR;
    }

    if ((type < NDAttrBool) || (type > NDAttrString)) {
        fprintf(stderr, "NDAttribute::setDataType, invalid data type = %d\n", static_cast<int>(type));
        return ND_ERROR;
    }

    attribute_->getValue()->set(PVDC->createPVScalar(static_cast<ScalarType>(type)));
    return ND_SUCCESS;
}
