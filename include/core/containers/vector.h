namespace secrecy {
typedef size_t VectorSizeType;
}

#ifdef ZEROPC_DUMMY_VECTOR
#include "_dummy_vector.h"
#else
#ifdef USE_INDEX_MAPPED_VECTOR
#include "_mapping_access_vector.h"
#else
#include "_class_access_vector.h"
#endif
#endif