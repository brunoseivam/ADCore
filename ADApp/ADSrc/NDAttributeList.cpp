/** NDAttributeList.cpp
 *
 * Mark Rivers
 * University of Chicago
 * October 18, 2013
 *
 */
 
#include <stdlib.h>

#include <epicsExport.h>

#include "NDAttributeList.h"

#include <pv/pvIntrospect.h>
#include <pv/ntndarrayAttribute.h>

using std::string;
using std::pair;
using std::map;
using std::tr1::const_pointer_cast;

using namespace epics::nt;
using namespace epics::pvData;

static const PVDataCreatePtr PVDC = getPVDataCreate();

/** NDAttributeList constructor
 *  Creates empty backing array
  */
NDAttributeList::NDAttributeList()
: array_(PVDC->createPVStructureArray(NDAttribute::builder->create()->getPVStructure()->getStructure())),
  map_()
{
}

/** NDAttributeList constructor
  * Use existing array as backing array
  */
NDAttributeList::NDAttributeList(PVStructureArrayPtr const & array)
: array_(array), map_()
{
    Lock lock(mutex_);

    PVStructureArray::const_svector vec(array_->view());
    PVStructureArray::const_svector::const_iterator it;

    for(it = vec.cbegin(); it != vec.cend(); ++it)
    {
        NTNDArrayAttributePtr ntAttribute(NTNDArrayAttribute::wrap(*it));
        NDAttributePtr attribute(new NDAttribute(ntAttribute));
        map_.insert(std::pair<string, NDAttributePtr>(attribute->getName(), attribute));
    }
}

/** NDAttributeList destructor
  */
NDAttributeList::~NDAttributeList()
{
}

/** Adds an attribute to the list.
  * If an attribute of the same name already exists then
  * the existing attribute is replaced with the new one.
  * \param[in] pAttribute A pointer to the attribute to add.
  */
int NDAttributeList::add (NDAttributePtr const & pAttribute)
{
    //const char *functionName = "NDAttributeList::add";

    Lock lock(mutex_);

    NDAttributePtr pExisting(find(pAttribute->getName()));

    if(pExisting)
        pAttribute->copy(pExisting);
    else
    {
        PVStructureArray::svector vec(array_->reuse());
        vec.push_back(pAttribute->getNTAttribute()->getPVStructure());
        array_->replace(freeze(vec));
        map_.insert(pair<string, NDAttributePtr>(pAttribute->getName(), pAttribute));
    }

    return(ND_SUCCESS);
}

/** Adds an attribute to the list.
  * This is a convenience function for adding attributes to a list.  
  * It first searches the list to see if there is an existing attribute
  * with the same name.  If there is it just changes the properties of the
  * existing attribute.  If not, it creates a new attribute with the
  * specified properties. 
  * IMPORTANT: This method is only capable of creating attributes
  * of the NDAttribute base class type, not derived class attributes.
  * To add attributes of a derived class to a list the NDAttributeList::add(NDAttribute*)
  * method must be used.
  * \param[in] name The name of the attribute to be added.
  * \param[in] description The description of the attribute.
  * \param[in] dataType The data type of the attribute.
  * \param[in] pValue A pointer to the value for this attribute.
  *
  */
NDAttributePtr NDAttributeList::add(string const & name, string const & description,
        NDAttrDataType_t dataType, void *pValue)
{
    //const char *functionName = "NDAttributeList::add";
    NDAttributePtr pAttribute(new NDAttribute(name, description, NDAttrSourceDriver, "Driver", dataType, pValue));
    add(pAttribute);
    return pAttribute;
}

/** Finds an attribute by name; the search is now case sensitive (R1-10)
  * \param[in] name The name of the attribute to be found.
  * \return Returns a pointer to the attribute if found, NULL if not found. 
  */
NDAttributePtr NDAttributeList::find(string const & name)
{
    return const_pointer_cast<NDAttribute>(static_cast<const NDAttributeList*>(this)->find(name));
}

NDAttributeConstPtr NDAttributeList::find(string const & name) const
{
    Lock lock(mutex_);

    std::map<string, NDAttributePtr>::const_iterator it = map_.find(name);

    if(it == map_.end())
        return NDAttributeConstPtr();

    return it->second;
}

/** Returns the total number of attributes in the list of attributes.
  * \return Returns the number of attributes. */
size_t NDAttributeList::count() const
{
    //const char *functionName = "NDAttributeList::count";
    return array_->view().size();
}

/** Removes an attribute from the list.
  * \param[in] name The name of the attribute to be deleted.
  * \return Returns ND_SUCCESS if the attribute was found and deleted, ND_ERROR if the
  * attribute was not found. */
int NDAttributeList::remove(string const & name)
{
    //const char *functionName = "NDAttributeList::remove";

    Lock lock(mutex_);

    PVStructureArray::svector vec(array_->reuse());
    PVStructureArray::svector::iterator it;

    for(it = vec.begin(); it != vec.end(); ++it)
    {
        NTNDArrayAttributePtr ntAttribute(NTNDArrayAttribute::wrap(*it));
        NDAttributePtr attribute(new NDAttribute(ntAttribute));
        if(attribute->getName() == name)
        {
            // Shared vector doesn't implement erase... So swap with last (screws order, but it is efficient)
            std::swap(*it, vec.back());
            vec.pop_back();
            array_->replace(freeze(vec));
            map_.erase(map_.find(name));
            return ND_SUCCESS;
        }
    }
    return ND_ERROR;
}

/** Deletes all attributes from the list. */
int NDAttributeList::clear()
{
    //const char *functionName = "NDAttributeList::clear";

    Lock lock(mutex_);
    PVStructureArray::svector vec(array_->reuse());
    map_.clear();
    return ND_SUCCESS;
}

/** Copies all attributes from one attribute list to another.
  * It is efficient so that if the attribute already exists in the output
  * list it just copies the properties, and memory allocation is minimized.
  * The attributes are added to any existing attributes already present in the output list.
  * \param[out] pListOut A reference to the output attribute list to copy to.
  */
int NDAttributeList::copy(NDAttributeListPtr & pListOut) const
{
    //const char *functionName = "NDAttributeList::copy";

    Lock lock(mutex_);

    map<string, NDAttributePtr>::const_iterator it;

    for(it = map_.begin(); it != map_.end(); ++it)
        pListOut->add(it->second);

    return ND_SUCCESS;
}

/** Updates all attribute values in the list; calls NDAttribute::updateValue() for each attribute in the list.
  */
int NDAttributeList::updateValues()
{
    Lock lock(mutex_);
    map<string, NDAttributePtr>::iterator it;

    for(it = map_.begin(); it != map_.end(); ++it)
        it->second->updateValue();

    return ND_SUCCESS;
}

/** Reports on the properties of the attribute list.
  * \param[in] fp File pointer for the report output.
  * \param[in] details Level of report details desired; if >10 calls NDAttribute::report() for each attribute.
  */
int NDAttributeList::report(FILE *fp, int details) const
{
    Lock lock(mutex_);
    fprintf(fp, "\n");
    fprintf(fp, "NDAttributeList: address=%p:\n", this);
    fprintf(fp, "  number of attributes=%lu\n", this->count());
    if (details > 10)
    {
        map<string, NDAttributePtr>::const_iterator it;

        for(it = map_.begin(); it != map_.end(); ++it)
            it->second->report(fp, details);
    }
    return ND_SUCCESS;
}

map<string, NDAttributePtr> const & NDAttributeList::getMap() const
{
    return map_;
}
