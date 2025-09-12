#ifndef MAPPED_ITERATOR_H
#define MAPPED_ITERATOR_H

#include <iterator>
#include <memory>
#include <vector>

namespace orq {
template <typename T, std::random_access_iterator D, std::random_access_iterator M>
    requires std::convertible_to<std::iter_value_t<D>, T> &&
             std::convertible_to<std::iter_value_t<M>, std::iter_difference_t<D>>
class MappedIterator {
   private:
    D dataIter;
    std::optional<M> mappingIter;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;

    MappedIterator() {}

    MappedIterator(std::vector<T>::iterator _dataIter,
                   std::vector<VectorSizeType>::iterator _mappingIter)
        : dataIter(_dataIter), mappingIter(_mappingIter) {}

    MappedIterator(std::vector<T>::iterator _dataIter) : dataIter(_dataIter) {}

    T& operator*() const {
        if (mappingIter) {
            return dataIter[*(*mappingIter)];
        } else {
            return *dataIter;
        }
    }

    T& operator[](difference_type i) const {
        if (mappingIter) {
            return dataIter[(*mappingIter)[i]];
        } else {
            return dataIter[i];
        }
    }

    MappedIterator& operator++() {
        if (mappingIter) {
            ++(*mappingIter);
        } else {
            ++dataIter;
        }
        return *this;
    }

    MappedIterator operator++(int) {
        MappedIterator tmp = *this;
        operator++();
        return tmp;
    }

    MappedIterator& operator--() {
        if (mappingIter) {
            --(*mappingIter);
        } else {
            --dataIter;
        }
        return *this;
    }

    MappedIterator operator--(int) {
        MappedIterator tmp = *this;
        operator--();
        return tmp;
    }

    MappedIterator& operator+=(difference_type n) {
        if (mappingIter) {
            *mappingIter += n;
        } else {
            dataIter += n;
        }
        return *this;
    }

    MappedIterator operator+(difference_type n) const {
        if (mappingIter) {
            return MappedIterator(dataIter, *mappingIter + n);
        } else {
            return MappedIterator(dataIter + n);
        }
    }

    friend MappedIterator operator+(difference_type n, const MappedIterator& other) {
        return other + n;
    }

    MappedIterator& operator-=(difference_type n) {
        if (mappingIter) {
            *mappingIter -= n;
        } else {
            dataIter -= n;
        }
        return *this;
    }

    MappedIterator operator-(difference_type n) const {
        if (mappingIter) {
            return MappedIterator(dataIter, *mappingIter - n);
        } else {
            return MappedIterator(dataIter - n);
        }
    }

    difference_type operator-(const MappedIterator& other) const {
        if (mappingIter && other.mappingIter) {
            return *mappingIter - *other.mappingIter;
        } else {
            return dataIter - other.dataIter;
        }
    }

    bool operator<(const MappedIterator& other) const {
        if (mappingIter && other.mappingIter) {
            return *mappingIter < *other.mappingIter;
        } else {
            return dataIter < other.dataIter;
        }
    }

    bool operator>(const MappedIterator& other) const { return other < *this; }

    bool operator<=(const MappedIterator& other) const { return !(other < *this); }

    bool operator>=(const MappedIterator& other) const { return !(*this < other); }

    bool operator==(const MappedIterator& other) const {
        if (mappingIter && other.mappingIter) {
            return *mappingIter == *other.mappingIter;
        } else {
            return dataIter == other.dataIter;
        }
    }
};
}  // namespace orq

static_assert(std::random_access_iterator<
              orq::MappedIterator<int, std::vector<int>::iterator, std::vector<int>::iterator>>);

#endif  // MAPPED_ITERATOR_H
