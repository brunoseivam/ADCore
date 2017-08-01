/** NDAttributeList.h
 *
 * Mark Rivers
 * University of Chicago
 * October 18, 2013
 *
 */

#ifndef NDAttributeList_H
#define NDAttributeList_H

#include <stdio.h>
#include <ellLib.h>
#include <epicsMutex.h>
#include <map>

#include "NDAttribute.h"

/** NDAttributeList class; this is a vector of attributes.
  */
class NDAttributeList;
typedef std::tr1::shared_ptr<NDAttributeList> NDAttributeListPtr;
typedef std::tr1::shared_ptr<const NDAttributeList> NDAttributeListConstPtr;

class epicsShareClass NDAttributeList {
public:
    NDAttributeList();
    NDAttributeList(epics::pvData::PVStructureArrayPtr const & array);
    ~NDAttributeList();
    int            add(NDAttributePtr const & pAttribute);
    NDAttributePtr add(std::string const & name, std::string const & description="",
                     NDAttrDataType_t dataType=NDAttrUndefined, void *pValue=NULL);
    NDAttributePtr find(std::string const & name);
    NDAttributeConstPtr find(std::string const & name) const;
    size_t       count() const;
    int          remove(std::string const & name);
    int          clear();
    int          copy(NDAttributeListPtr & pOut) const;
    int          updateValues();
    int          report(FILE *fp, int details) const;
    std::map<std::string, NDAttributePtr> const & getMap() const;

private:
    epics::pvData::PVStructureArrayPtr array_;  /**< The array of NTNDArrayAttributes */
    std::map<std::string, NDAttributePtr> map_; /**< The map of NDAttributes */
    mutable epics::pvData::Mutex mutex_;        /**< Mutex to protect the array and the map */
};

#endif

