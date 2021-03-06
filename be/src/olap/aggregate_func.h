// Copyright (c) 2017, Baidu.com, Inc. All Rights Reserved

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef BDG_PALO_BE_SRC_OLAP_AGGREGATE_FUNC_H
#define BDG_PALO_BE_SRC_OLAP_AGGREGATE_FUNC_H

#include "olap/field_info.h"
#include "olap/hll.h"
#include "olap/types.h"

namespace palo {

using AggregateFunc = void (*)(char* left, char* right);
using FinalizeFunc = void (*)(char* data);

template<FieldAggregationMethod agg_method,
        FieldType field_type> struct AggregateFuncTraits {};

template<FieldType field_type>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_NONE, field_type> {
    static void aggregate(char* left, char* right) {}
};

template <FieldType field_type>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_MIN, field_type> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<field_type>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (l_null) {
            return;
        } else if (r_null) {
            *reinterpret_cast<bool*>(left) = true;
        } else {
            CppType* l_val = reinterpret_cast<CppType*>(left + 1);
            CppType* r_val = reinterpret_cast<CppType*>(right + 1);
            if (*r_val < *l_val) { *l_val = *r_val; }
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_MIN, OLAP_FIELD_TYPE_LARGEINT> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<OLAP_FIELD_TYPE_LARGEINT>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (l_null) {
            return;
        } else if (r_null) {
            *reinterpret_cast<bool*>(left) = true;
        } else {
            CppType l_val, r_val;
            memcpy(&l_val, left + 1, sizeof(CppType));
            memcpy(&r_val, right + 1, sizeof(CppType));
            if (r_val < l_val) {
                memcpy(left + 1, right + 1, sizeof(CppType));
            }
        }
    }
};

template <FieldType field_type>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_MAX, field_type> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<field_type>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (r_null) {
            return;
        }

        CppType* l_val = reinterpret_cast<CppType*>(left + 1);
        CppType* r_val = reinterpret_cast<CppType*>(right + 1);
        if (l_null) {
            *reinterpret_cast<bool*>(left) = false;
            *l_val = *r_val;
        } else {
            if (*r_val > *l_val) { *l_val = *r_val; }
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_MAX, OLAP_FIELD_TYPE_LARGEINT> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<OLAP_FIELD_TYPE_LARGEINT>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (r_null) {
            return;
        }

        if (l_null) {
            *reinterpret_cast<bool*>(left) = false;
            memcpy(left + 1, right + 1, sizeof(CppType));
        } else {
            CppType l_val, r_val;
            memcpy(&l_val, left + 1, sizeof(CppType));
            memcpy(&r_val, right + 1, sizeof(CppType));
            if (r_val > l_val) {
                memcpy(left + 1, right + 1, sizeof(CppType));
            }
        }
    }
};

template <FieldType field_type>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_SUM, field_type> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<field_type>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (r_null) {
            return;
        }

        CppType* l_val = reinterpret_cast<CppType*>(left + 1);
        CppType* r_val = reinterpret_cast<CppType*>(right + 1);
        if (l_null) {
            *reinterpret_cast<bool*>(left) = false;
            *l_val = *r_val;
        } else {
            *l_val += *r_val;
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_SUM, OLAP_FIELD_TYPE_LARGEINT> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<OLAP_FIELD_TYPE_LARGEINT>::CppType CppType;
        bool l_null = *reinterpret_cast<bool*>(left);
        bool r_null = *reinterpret_cast<bool*>(right);
        if (r_null) {
            return;
        }

        if (l_null) {
            *reinterpret_cast<bool*>(left) = false;
            memcpy(left + 1, right + 1, sizeof(CppType));
        } else {
            CppType l_val, r_val;
            memcpy(&l_val, left + 1, sizeof(CppType));
            memcpy(&r_val, right + 1, sizeof(CppType));
            l_val += r_val;
            memcpy(left + 1, &l_val, sizeof(CppType));
        }
    }
};

template <FieldType field_type>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_REPLACE, field_type> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<field_type>::CppType CppType;
        bool r_null = *reinterpret_cast<bool*>(right);
        *reinterpret_cast<bool*>(left) = r_null;

        if (!r_null) {
            CppType* l_val = reinterpret_cast<CppType*>(left + 1);
            CppType* r_val = reinterpret_cast<CppType*>(right + 1);
            *l_val = *r_val;
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_REPLACE, OLAP_FIELD_TYPE_LARGEINT> {
    static void aggregate(char* left, char* right) {
        typedef typename FieldTypeTraits<OLAP_FIELD_TYPE_LARGEINT>::CppType CppType;
        bool r_null = *reinterpret_cast<bool*>(right);
        *reinterpret_cast<bool*>(left) = r_null;

        if (!r_null) {
            memcpy(left + 1, right + 1, sizeof(CppType));
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_REPLACE, OLAP_FIELD_TYPE_CHAR> {
    static void aggregate(char* left, char* right) {
        bool r_null = *reinterpret_cast<bool*>(right);
        *reinterpret_cast<bool*>(left) = r_null;
        if (!r_null) {
            StringSlice* l_slice = reinterpret_cast<StringSlice*>(left + 1);
            StringSlice* r_slice = reinterpret_cast<StringSlice*>(right + 1);
            memory_copy(l_slice->data, r_slice->data, r_slice->size);
            l_slice->size = r_slice->size;
        }
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_REPLACE, OLAP_FIELD_TYPE_VARCHAR> {
    static void aggregate(char* left, char* right) {
        //same with char aggregate
        AggregateFuncTraits<OLAP_FIELD_AGGREGATION_REPLACE,
                OLAP_FIELD_TYPE_CHAR>::aggregate(left, right);
    }
};

template <>
struct AggregateFuncTraits<OLAP_FIELD_AGGREGATION_HLL_UNION, OLAP_FIELD_TYPE_HLL> {
    static void aggregate(char* left, char* right) {
        StringSlice* l_slice = reinterpret_cast<StringSlice*>(left + 1);
        size_t hll_ptr = *(size_t*)(l_slice->data - sizeof(HllContext*));
        HllContext* context = (reinterpret_cast<HllContext*>(hll_ptr));
        HllSetHelper::fill_set(right + 1, context);
    }
    static void finalize(char* data) {
        StringSlice* slice = reinterpret_cast<StringSlice*>(data);
        size_t hll_ptr = *(size_t*)(slice->data - sizeof(HllContext*));
        HllContext* context = (reinterpret_cast<HllContext*>(hll_ptr));
        std::map<int, uint8_t> index_to_value;
        if (context->has_sparse_or_full ||
                context->hash64_set.size() > HLL_EXPLICLIT_INT64_NUM) {
            HllSetHelper::set_max_register(context->registers, HLL_REGISTERS_COUNT,
                                           context->hash64_set);
            for (int i = 0; i < HLL_REGISTERS_COUNT; i++) {
                if (context->registers[i] != 0) {
                    index_to_value[i] = context->registers[i];
                }
            }
        }
        int sparse_set_len = index_to_value.size() *
            (sizeof(HllSetResolver::SparseIndexType)
             + sizeof(HllSetResolver::SparseValueType))
            + sizeof(HllSetResolver::SparseLengthValueType);
        int result_len = 0;

        if (sparse_set_len >= HLL_COLUMN_DEFAULT_LEN) {
            // full set
            HllSetHelper::set_full(slice->data, context->registers,
                                   HLL_REGISTERS_COUNT, result_len);
        } else if (index_to_value.size() > 0) {
            // sparse set
            HllSetHelper::set_sparse(slice->data, index_to_value, result_len);
        } else if (context->hash64_set.size() > 0) {
            // expliclit set
            HllSetHelper::set_expliclit(slice->data, context->hash64_set, result_len);
        }

        slice->size = result_len & 0xffff;

        HllSetHelper::init_context(context);
    }
};

extern AggregateFunc get_aggregate_func(const FieldAggregationMethod agg_method,
                                        const FieldType field_type);
extern FinalizeFunc get_finalize_func(const FieldAggregationMethod agg_method,
                                      const FieldType field_type);

} // namespace palo

#endif // BDG_PALO_BE_SRC_OLAP_AGGREGATE_FUNC_H
