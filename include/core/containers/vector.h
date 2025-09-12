namespace orq {
typedef size_t VectorSizeType;
}

#ifdef ZEROPC_DUMMY_VECTOR
#include "dummy_vector.h"
#else
#ifdef USE_INDEX_MAPPED_VECTOR
#include "mapping_access_vector.h"
#else
#include "class_access_vector.h"
#endif
#endif