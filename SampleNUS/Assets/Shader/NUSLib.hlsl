#ifndef NUS_LIB_INCLUDED
#define NUS_LIB_INCLUDED

inline void Require16bitType() {}

inline void NUS_Require_SM66_Bindless() {}

#ifdef NUS_SM_6_6
//ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Derivatives.html
inline void NUS_Require_SM66_QuadShuffle() {}
//ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html
inline void NUS_Require_SM66_Atomics() {}

#define NUS_InterlockedAdd(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedAdd(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedAdd64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedAdd64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedAnd(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedAnd(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedAnd64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedAnd64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedOr(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedOr(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedOr64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedOr64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedXor(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedXor(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedXor64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedXor64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedMin(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedMin(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedMin64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedMin64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedMax(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedMax(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedMax64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedMax64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedExchange(DST_ARRAY, IDX, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedExchange(DST_ARRAY[IDX], VAL, OUT_VAL); \
}

#define NUS_InterlockedExchangeFloat(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedExchangeFloat(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedExchange64(DST_ADD_BUF, OFFSET, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedExchange64(OFFSET, VAL, OUT_VAL); \
}

#define NUS_InterlockedCompareStore(DST_ARRAY, IDX, COMP_VAL, VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedCompareStore(DST_ARRAY[IDX], COMP_VAL, VAL); \
}

#define NUS_InterlockedCompareStore64(DST_ADD_BUF, OFFSET, COMP_VAL, VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedCompareStore64(OFFSET, COMP_VAL, VAL); \
}

#define NUS_InterlockedCompareStoreFloatBitwise(DST_ARRAY, IDX, COMP_VAL, VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedCompareStoreFloatBitwise(DST_ARRAY[IDX], COMP_VAL, VAL); \
}

#define NUS_InterlockedCompareStoreFloatBitwiseBuf(DST_ADD_BUF, OFFSET, COMP_VAL, VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedCompareStoreFloatBitwise(OFFSET, COMP_VAL, VAL); \
}

#define NUS_InterlockedCompareExchange(DST_ARRAY, IDX, COMP_VAL, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedCompareExchange(DST_ARRAY[IDX], COMP_VAL, VAL, OUT_VAL); \
}

#define NUS_InterlockedCompareExchange64(DST_ADD_BUF, OFFSET, COMP_VAL, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedCompareExchange64(OFFSET, COMP_VAL, VAL, OUT_VAL); \
}

#define NUS_InterlockedCompareExchangeFloatBitwise(DST_ARRAY, IDX, COMP_VAL, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    InterlockedCompareExchangeFloatBitwise(DST_ARRAY[IDX], COMP_VAL, VAL, OUT_VAL); \
}

#define NUS_InterlockedCompareExchangeFloatBitwiseBuf(DST_ADD_BUF, OFFSET, COMP_VAL, VAL, OUT_VAL) \
{ \
    NUS_Require_SM66_Atomics(); \
    DST_ADD_BUF.InterlockedCompareExchangeFloatBitwise(OFFSET, COMP_VAL, VAL, OUT_VAL); \
}

#endif

#endif