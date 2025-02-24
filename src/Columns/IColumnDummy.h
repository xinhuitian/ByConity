/*
 * Copyright 2016-2023 ClickHouse, Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
 * This file may have been modified by Bytedance Ltd. and/or its affiliates (“ Bytedance's Modifications”).
 * All Bytedance's Modifications are Copyright (2023) Bytedance Ltd. and/or its affiliates.
 */

#pragma once

#include <Common/Arena.h>
#include <Common/PODArray.h>
#include <Columns/IColumn.h>
#include <Columns/ColumnsCommon.h>
#include <Core/Field.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int SIZES_OF_COLUMNS_DOESNT_MATCH;
    extern const int NOT_IMPLEMENTED;
}


/** Base class for columns-constants that contain a value that is not in the `Field`.
  * Not a full-fledged column and is used in a special way.
  */
class IColumnDummy : public IColumn
{
public:
    IColumnDummy() : s(0) {}
    IColumnDummy(size_t s_) : s(s_) {}

public:
    virtual MutableColumnPtr cloneDummy(size_t s_) const = 0;

    MutableColumnPtr cloneResized(size_t s_) const override { return cloneDummy(s_); }
    size_t size() const override { return s; }
    void insertDefault() override { ++s; }
    void popBack(size_t n) override { s -= n; }
    size_t byteSize() const override { return 0; }
    size_t byteSizeAt(size_t) const override { return 0; }
    size_t allocatedBytes() const override { return 0; }
    int compareAt(size_t, size_t, const IColumn &, int) const override { return 0; }
    void compareColumn(const IColumn &, size_t, PaddedPODArray<UInt64> *, PaddedPODArray<Int8> &, int, int) const override
    {
    }

    bool hasEqualValues() const override { return true; }

    Field operator[](size_t) const override { throw Exception("Cannot get value from " + getName(), ErrorCodes::NOT_IMPLEMENTED); }
    void get(size_t, Field &) const override { throw Exception("Cannot get value from " + getName(), ErrorCodes::NOT_IMPLEMENTED); }
    void insert(const Field &) override { throw Exception("Cannot insert element into " + getName(), ErrorCodes::NOT_IMPLEMENTED); }

    StringRef getDataAt(size_t) const override
    {
        return {};
    }

    void insertData(const char *, size_t) override
    {
        ++s;
    }

    StringRef serializeValueIntoArena(size_t /*n*/, Arena & arena, char const *& begin) const override
    {
        /// Has to put one useless byte into Arena, because serialization into zero number of bytes is ambiguous.
        char * res = arena.allocContinue(1, begin);
        *res = 0;
        return { res, 1 };
    }

    const char * deserializeAndInsertFromArena(const char * pos) override
    {
        ++s;
        return pos + 1;
    }

    const char * skipSerializedInArena(const char * pos) const override
    {
        return pos;
    }

    void updateHashWithValue(size_t /*n*/, SipHash & /*hash*/) const override
    {
    }

    void updateWeakHash32(WeakHash32 & /*hash*/) const override
    {
    }

    void updateHashFast(SipHash & /*hash*/) const override
    {
    }

    void insertFrom(const IColumn &, size_t) override
    {
        ++s;
    }

    void insertRangeFrom(const IColumn & /*src*/, size_t /*start*/, size_t length) override
    {
        s += length;
    }

    void insertRangeSelective(const IColumn & /*src*/, const Selector & /*selector*/, size_t /*selector_start*/, size_t length) override
    {
        s += length;
    }

    ColumnPtr filter(const Filter & filt, ssize_t /*result_size_hint*/) const override
    {
        return cloneDummy(countBytesInFilter(filt));
    }

    ColumnPtr permute(const Permutation & perm, size_t limit) const override
    {
        if (s != perm.size())
            throw Exception("Size of permutation doesn't match size of column.", ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH);

        return cloneDummy(limit ? std::min(s, limit) : s);
    }

    ColumnPtr index(const IColumn & indexes, size_t limit) const override
    {
        if (indexes.size() < limit)
            throw Exception("Size of indexes is less than required.", ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH);

        return cloneDummy(limit ? limit : s);
    }

    void getPermutation(IColumn::PermutationSortDirection /*direction*/, IColumn::PermutationSortStability /*stability*/,
                    size_t /*limit*/, int /*nan_direction_hint*/, Permutation & res) const override
    {
        res.resize(s);
        for (size_t i = 0; i < s; ++i)
            res[i] = i;
    }

    void updatePermutation(IColumn::PermutationSortDirection /*direction*/, IColumn::PermutationSortStability /*stability*/,
                    size_t, int, Permutation &, EqualRanges&) const override {}

    ColumnPtr replicate(const Offsets & offsets) const override
    {
        if (s != offsets.size())
            throw Exception("Size of offsets doesn't match size of column.", ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH);

        return cloneDummy(offsets.back());
    }

    MutableColumns scatter(ColumnIndex num_columns, const Selector & selector) const override
    {
        if (s != selector.size())
            throw Exception("Size of selector doesn't match size of column.", ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH);

        std::vector<size_t> counts(num_columns);
        for (auto idx : selector)
            ++counts[idx];

        MutableColumns res(num_columns);
        for (size_t i = 0; i < num_columns; ++i)
            res[i] = cloneResized(counts[i]);

        return res;
    }

    void gather(ColumnGathererStream &) override
    {
        throw Exception("Method gather is not supported for " + getName(), ErrorCodes::NOT_IMPLEMENTED);
    }

    void getExtremes(Field &, Field &) const override
    {
    }

    void addSize(size_t delta)
    {
        s += delta;
    }

    bool isDummy() const override
    {
        return true;
    }

protected:
    size_t s;
};

}
