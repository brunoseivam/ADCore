#ifndef NDARRAYPARAM_H
#define NDARRAYPARAM_H

#include <pv/ntndarray.h>
#include <pv/pvData.h>
#include <pv/pvDatabase.h>

#include "NDArray.h"

class NDArrayParam;
typedef std::tr1::shared_ptr<NDArrayParam> NDArrayParamPtr;

class epicsShareClass NDArrayParam : public epics::pvDatabase::PVRecord
{
private:
    epics::nt::NTNDArrayPtr array_;

    NDArrayParam (std::string const & name,
            epics::pvData::PVStructurePtr const & pvStructure);

public:
    POINTER_DEFINITIONS(NDArrayParam);

    ~NDArrayParam (void);

    static NDArrayParamPtr create (std::string const & name);

    //void put (epics::nt::NTNDArray::const_shared_pointer array);
    void put (NDArray::const_shared_pointer array);
    epics::nt::NTNDArrayPtr get (void);
};

#endif
