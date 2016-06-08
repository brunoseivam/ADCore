#ifndef NDARRAYPOOL_H
#define NDARRAYPOOL_H

#include "NDArray.h"
#include <shareLib.h>

class NDArrayPool;
typedef std::tr1::shared_ptr<NDArrayPool> NDArrayPoolPtr;

class epicsShareClass NDArrayPool
{
private:
    friend class NDArrayPoolDeleter;

    std::deque<NDArrayPtr> mPool;   /**< Vector of NTNDArrayPtr objects that form the pool */
    epics::pvData::Mutex  mMutex;   /**< Mutex to protect the pool */
    size_t mMaxBuffers;             /**< Maximum number of buffers this object is allowed to allocate; 0=unlimited */
    size_t mMaxMemory;              /**< Maximum bytes of memory this object is allowed to allocate; 0=unlimited */
    size_t mNumBuffers;             /**< Number of buffers this object has currently allocated */
    size_t mMemorySize;             /**< Number of bytes of memory this object has currently allocated */

    void giveBack (NDArray *array);
    size_t freeMemory (void);

public:
    NDArrayPool(size_t maxBuffers, size_t maxMemory);

    NDArrayPtr alloc (int ndims, size_t *dims, epics::pvData::ScalarType dataType,
            size_t dataSize = 0, void *pData = NULL);

    NDArrayPtr alloc (std::vector<NDDimension_t> const & dims,
            epics::pvData::ScalarType dataType, size_t dataSize = 0,
            void *pData = NULL);

    template<typename arrayType>
    NDArrayPtr alloc (std::vector<NDDimension_t> const & dims, size_t dataSize = 0,
            void *pData = NULL);

    NDArrayPtr convert (NDArrayPtr & in, epics::pvData::ScalarType dataTypeOut);
    NDArrayPtr convert (NDArrayPtr & in, epics::pvData::ScalarType dataTypeOut,
            std::vector<NDDimension_t> dimsOut);

    size_t maxBuffers (void) const;
    size_t maxMemory  (void) const;
    size_t numBuffers (void) const;
    size_t memorySize (void) const;
    size_t numFree    (void) const;

    void report (FILE *fp, int details);

};

#endif
