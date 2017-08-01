/*
 * NDArrayPool.h
 *
 *  Created on: May 23, 2017
 *      Author: bmartins
 */

#ifndef NDArrayPool_H
#define NDArrayPool_H

#include <deque>
#include <shareLib.h>
#include "NDArray.h"

/** The NDArrayPool class manages a free list (pool) of NDArray objects.
  * Drivers allocate NDArray objects from the pool, and pass these objects to plugins.
  * Plugins increase the reference count on the object when they place the object on
  * their queue, and decrease the reference count when they are done processing the
  * array. When the reference count reaches 0 again the NDArray object is placed back
  * on the free list. This mechanism minimizes the copying of array data in plugins.
  */
class epicsShareClass NDArrayPool {

private:
    friend class NDArrayPoolDeleter;

    std::deque<NDArrayPtr> pool_;   /**< List of free NDArray objects that form the pool */
    epics::pvData::Mutex mutex_;    /**< Mutex to protect the free list */
    size_t maxBuffers_;             /**< Maximum number of buffers this object is allowed to allocate; 0=unlimited */
    size_t maxMemory_;              /**< Maximum bytes of memory this object is allowed to allocate; 0=unlimited */
    size_t numBuffers_;             /**< Number of buffers this object has currently allocated */
    size_t memorySize_;             /**< Number of bytes of memory this object has currently allocated */

    NDArrayPtr getBuffer (void);
    void giveBack (NDArray *array);
    size_t freeMemory (void);

public:
    NDArrayPool(size_t maxBuffers, size_t maxMemory);

    NDArrayPtr copy (NDArrayConstPtr & pArray);

    NDArrayPtr alloc (int ndims, size_t *dims, epics::pvData::ScalarType dataType,
            size_t dataSize = 0);

    NDArrayPtr alloc (std::vector<NDDimension_t> const & dims,
            epics::pvData::ScalarType dataType, size_t dataSize = 0);

    template<typename arrayType>
    NDArrayPtr alloc (std::vector<NDDimension_t> const & dims, size_t dataSize = 0);

    NDArrayPtr alloc (epics::nt::NTNDArrayPtr const & array);
    NDArrayPtr alloc (epics::pvData::PVStructurePtr const & pvStructure);

    NDArrayPtr convert (NDArrayConstPtr in, epics::pvData::ScalarType dataTypeOut);
    NDArrayPtr convert (NDArrayConstPtr in, epics::pvData::ScalarType dataTypeOut,
            std::vector<NDDimension_t> dimsOut);

    size_t maxBuffers (void) const;
    size_t maxMemory  (void) const;
    size_t numBuffers (void) const;
    size_t memorySize (void) const;
    size_t numFree    (void) const;

    void report (FILE *fp, int details);

};


#endif
