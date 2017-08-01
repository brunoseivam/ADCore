#include "NDArrayParam.h"

#include <pv/ntndarray.h>
//#include <pv/pvData.h>
#include <pv/pvDatabase.h>

using std::string;

using namespace epics::pvData;
using namespace epics::nt;

NDArrayParam::NDArrayParam (string const & name, PVStructurePtr const & pvStructure)
: PVRecord(name, pvStructure),
  array_(NTNDArray::wrap(pvStructure)) {}

NDArrayParam::~NDArrayParam (void) {}

NDArrayParamPtr NDArrayParam::create (string const & name)
{
    NTNDArrayPtr array(NDArray::builder->create());
    NDArrayParamPtr param(new NDArrayParam(name, array->getPVStructure()));

    if(!param->init())
        param.reset();

    return param;
}

void NDArrayParam::put (NDArray::const_shared_pointer array)
{
    lock();
    beginGroupPut();

    array_->getPVStructure()->copy(*array->getArray()->getPVStructure());

    endGroupPut();
    unlock();
}
