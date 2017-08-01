/** NDAttribute.h
 *
 * Mark Rivers
 * University of Chicago
 * October 18, 2013
 *
 */

#ifndef NDAttribute_H
#define NDAttribute_H

#include <string>

#include <stdio.h>
#include <string.h>

#include <ellLib.h>
#include <epicsTypes.h>
#include <pv/pvData.h>
#include <pv/ntndarray.h>
#include <pv/ntndarrayAttribute.h>

/** Success return code  */
#define ND_SUCCESS 0
/** Failure return code  */
#define ND_ERROR -1

/** Enumeration of NDAttribute attribute data types */
typedef enum
{
    NDAttrBool    = epics::pvData::pvBoolean,   /**< Boolean */
    NDAttrInt8    = epics::pvData::pvByte,      /**< Signed 8-bit integer */
    NDAttrUInt8   = epics::pvData::pvUByte,     /**< Unsigned 8-bit integer */
    NDAttrInt16   = epics::pvData::pvShort,     /**< Signed 16-bit integer */
    NDAttrUInt16  = epics::pvData::pvUShort,    /**< Unsigned 16-bit integer */
    NDAttrInt32   = epics::pvData::pvInt,       /**< Signed 32-bit integer */
    NDAttrUInt32  = epics::pvData::pvUInt,      /**< Unsigned 32-bit integer */
    NDAttrInt64   = epics::pvData::pvLong,      /**< Signed 64-bit integer */
    NDAttrUInt64  = epics::pvData::pvULong,     /**< Unsigned 64-bit integer */
    NDAttrFloat32 = epics::pvData::pvFloat,     /**< 32-bit float */
    NDAttrFloat64 = epics::pvData::pvDouble,    /**< 64-bit float */
    NDAttrString  = epics::pvData::pvString,    /**< Dynamic length string */
    NDAttrUndefined = -1                        /**< Undefined data type */
} NDAttrDataType_t;

/** Enumeration of NDAttibute source types */
typedef enum
{
    NDAttrSourceDriver,    /**< Attribute is obtained directly from driver */
    NDAttrSourceParam,     /**< Attribute is obtained from parameter library */
    NDAttrSourceEPICSPV,   /**< Attribute is obtained from an EPICS PV */
    NDAttrSourceFunct,     /**< Attribute is obtained from a user-specified function */
    NDAttrSourceUndefined  /**< Attribute source is undefined */
} NDAttrSource_t;

/** NDAttribute class; an attribute has a name, description, source type, source string,
  * data type, and value.
  */
class NDAttribute;
typedef std::tr1::shared_ptr<NDAttribute> NDAttributePtr;
typedef std::tr1::shared_ptr<const NDAttribute> NDAttributeConstPtr;

class epicsShareClass NDAttribute {
public:
    POINTER_DEFINITIONS(NDAttribute);

    static const epics::nt::NTNDArrayAttributeBuilderPtr builder;
    /* Methods */
    NDAttribute(std::string const & name, std::string const & description,
                NDAttrSource_t sourceType, std::string const & source,
                NDAttrDataType_t dataType, void *pValue);
    NDAttribute(NDAttribute const & attribute);
    NDAttribute(epics::nt::NTNDArrayAttributePtr & ntAttribute);
    static const char *attrSourceString(NDAttrSource_t type);
    virtual ~NDAttribute();
    virtual void copy(NDAttributePtr & to) const;
    virtual std::string getName() const;
    virtual std::string getDescription() const;
    virtual std::string getSource() const;
    virtual std::string getSourceInfo(NDAttrSource_t *pSourceType) const;
    virtual NDAttrDataType_t getDataType() const;
    virtual int getValueInfo(NDAttrDataType_t *pDataType, size_t *pDataSize) const;
    virtual int getValue(NDAttrDataType_t dataType, void *pValue, size_t dataSize=0) const;
    virtual int getValue(std::string& value) const;
    virtual int setValue(const void *pValue);
    virtual int setValue(const std::string&);
    virtual int setValue(const void *pValue, NDAttrDataType_t dataType);
    template <typename epicsType> int setValueT(epicsType value);
    virtual int updateValue();
    virtual epics::nt::NTNDArrayAttributePtr getNTAttribute();
    virtual int report(FILE *fp, int details) const;

    virtual void setName (std::string const & name);
    virtual void setDescription (std::string const & description);
    virtual void setSource (std::string const & source);
    virtual void setSourceType (NDAttrSource_t sourceType);
    virtual int setDataType (NDAttrDataType_t type);

    template <typename epicsType> int getValueT(void *pValue) const;

    epics::nt::NTNDArrayAttributePtr attribute_;
    std::string sourceTypeString_;
};

#endif
