#ifndef LIBGEODECOMP_STORAGE_UNSTRUCTUREDSOANEIGHBORHOOD_H
#define LIBGEODECOMP_STORAGE_UNSTRUCTUREDSOANEIGHBORHOOD_H

#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_WITH_CPP14

#include <libflatarray/flat_array.hpp>
#include <libflatarray/soa_accessor.hpp>

#include <libgeodecomp/geometry/coord.h>
#include <libgeodecomp/storage/unstructuredneighborhood.h>
#include <libgeodecomp/storage/unstructuredsoagrid.h>

#include <iterator>
#include <utility>

namespace LibGeoDecomp {

/**
 * Neighborhood providing pointers for vectorization of UnstructuredSoAGrid.
 * weights(id) returns a pair of two pointers. One points to the array where
 * the indices for gather are stored and the seconds points the matrix values.
 * Both pointers can be used to load LFA short_vec classes accordingly.
 */
template<
    typename CELL, long DIM_X, long DIM_Y, long DIM_Z, long INDEX,
    std::size_t MATRICES = 1, typename VALUE_TYPE = double, int C = 64, int SIGMA = 1>
class UnstructuredSoANeighborhood
{
public:
    using Grid = UnstructuredSoAGrid<CELL, MATRICES, VALUE_TYPE, C, SIGMA>;
    using IteratorPair = std::pair<const unsigned *, const VALUE_TYPE *>;
    using SoAAccessor = LibFlatArray::soa_accessor<CELL, DIM_X, DIM_Y, DIM_Z, INDEX>;

    /**
     * This iterator returns objects/values needed to update
     * the current chunk. Iterator consists of a pair: indices pointer
     * and matrix values pointer.
     */
    class Iterator : public std::iterator<std::forward_iterator_tag,
                                          const IteratorPair>
    {
    public:
        using Matrix = SellCSigmaSparseMatrixContainer<VALUE_TYPE, C, SIGMA>;

        inline
        Iterator(const Matrix& matrix, int offset) :
            matrix(matrix), offset(offset)
        {}

        inline
        void operator++()
        {
            offset += C;
        }

        inline
        bool operator==(const Iterator& other) const
        {
            // offset is indicator here
            return offset == other.offset;
        }

        inline
        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        inline
        const IteratorPair operator*() const
        {
            // load indices and matrix values pointers
            const VALUE_TYPE *weights = matrix.valuesVec().data() + offset;
            const unsigned *indices =
                reinterpret_cast<const unsigned *>(matrix.columnVec().data() + offset);
            return std::make_pair(indices, weights);
        }

    private:
        const Matrix& matrix;   /**< matrix to use */
        int offset;             /**< Where are we right now inside chunk?  */
    };

    inline
    UnstructuredSoANeighborhood(const SoAAccessor& acc, const Grid& grid, long startX) :
        grid(grid),
        currentChunk(startX / C),
        accessor(acc)
    {}

    inline
    UnstructuredSoANeighborhood& operator++()
    {
        ++currentChunk;
        return *this;
    }

    inline
    UnstructuredSoANeighborhood operator++(int)
    {
        UnstructuredSoANeighborhood tmp(*this);
        operator++();
        return tmp;
    }

    inline
    int index() const
    {
        return currentChunk;
    }

    inline
    int index()
    {
        return currentChunk;
    }

    inline
    UnstructuredSoANeighborhood& weights()
    {
        // default neighborhood is for matrix 0
        return weights(0);
    }

    inline
    UnstructuredSoANeighborhood& weights(std::size_t matrixID)
    {
        currentMatrixID = matrixID;

        return *this;
    }

    inline
    Iterator begin() const
    {
        const auto& matrix = grid.getWeights(currentMatrixID);
        return Iterator(matrix, matrix.chunkOffsetVec()[currentChunk]);
    }

    inline
    const Iterator end() const
    {
        const auto& matrix = grid.getWeights(currentMatrixID);
        return Iterator(matrix, matrix.chunkOffsetVec()[currentChunk + 1]);
    }

    inline
    const SoAAccessor *operator->() const
    {
        return &accessor;
    }

private:
    const Grid& grid;            /**< old grid */
    int currentChunk;            /**< current chunk */
    int currentMatrixID;         /**< current id for matrices */
    const SoAAccessor& accessor; /**< accessor to old grid */
};

/**
 * Neighborhood which is used for hoodNew in updateLineX().
 * Provides access to member pointers of the new grid.
 */
template<typename CELL, long DIM_X, long DIM_Y, long DIM_Z, long INDEX>
class UnstructuredSoANeighborhoodNew
{
public:
    using SoAAccessor = LibFlatArray::soa_accessor<CELL, DIM_X, DIM_Y, DIM_Z, INDEX>;

    inline explicit
    UnstructuredSoANeighborhoodNew(SoAAccessor& acc) :
        accessor(acc)
    {}

    inline
    SoAAccessor *operator->() const
    {
        return &accessor;
    }

private:
    SoAAccessor& accessor;      /**< accessor to new grid */
};

/**
 * This neighborhood is used in SoA cells in update() function. Update()
 * may be called due to loop peeling. The only differences to
 * UnstructuredNeighborhood are the grid type and the []-operator.
 */
template<typename CELL, std::size_t MATRICES = 1,
         typename VALUE_TYPE = double, int C = 64, int SIGMA = 1>
class UnstructuredSoAScalarNeighborhood :
        public UnstructuredNeighborhoodHelpers::
        UnstructuredNeighborhoodBase<CELL, UnstructuredSoAGrid<CELL, MATRICES,
                                                               VALUE_TYPE, C, SIGMA>,
                                     MATRICES, VALUE_TYPE, C, SIGMA, false>
{
public:
    using Grid = UnstructuredSoAGrid<CELL, MATRICES, VALUE_TYPE, C, SIGMA>;
    using UnstructuredNeighborhoodHelpers::
    UnstructuredNeighborhoodBase<CELL, Grid, MATRICES, VALUE_TYPE, C, SIGMA, false>::grid;

    inline
    UnstructuredSoAScalarNeighborhood(const Grid& grid, long startX) :
        UnstructuredNeighborhoodHelpers::
        UnstructuredNeighborhoodBase<CELL, Grid, MATRICES, VALUE_TYPE, C, SIGMA, false>(grid, startX)
    {}

    CELL operator[](int index) const
    {
        return grid[index];
    }
};

}

#endif
#endif
