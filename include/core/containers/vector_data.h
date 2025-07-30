#ifndef SECRECY_VECTOR_DATA_H
#define SECRECY_VECTOR_DATA_H

#include <vector>
#include "../../debug/debug.h"
#include "vector_cache.h"

namespace secrecy{

    template<typename T> class VectorDataBase;
    template<typename T> class VectorData;
    template<typename T> class SimpleVectorData;
    template<typename T> class AlternatingVectorData;
    template<typename T> class ReversedAlternatingVectorData;
    template<typename T> class ReversedVectorData;
    template<typename T> class RepeatedVectorData;
    template<typename T> class CyclicVectorData;



    template<typename T>
    class VectorDataBase {
    private:
        VectorSizeType current_size;

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr() = 0;
    public:

        VectorDataBase(const VectorSizeType& _current_size):
                       current_size(_current_size){}

        VectorDataBase():
        current_size(0){}

        virtual ~VectorDataBase(){}

        /**
         * @return An iterator pointing to the first element.
         *
         * NOTE: This method is used by the communicator.
         */
        inline virtual typename std::vector<T>::iterator begin() = 0;

        /**
         * @return An iterator pointing to the last element.
         *
         * NOTE: This method is used by the communicator.
         */
        inline virtual typename std::vector<T>::iterator end() = 0;

        /**
         * Returns a mutable reference to the element at the given `index`.
         * @param index - The index of the target element.
         * @return A mutable reference to the element at the given `index`.
         */
        inline virtual T &operator[](const int &index) = 0;

        /**
         * Returns an immutable reference of the element at the given `index`.
         * @param index - The index of the target element.
         * @return Returns a read-only reference of the element at the given `index`.
         */
        inline virtual const T &operator[](const int &index) const = 0;

        SimpleVectorData<T> simple_subset_reference(const VectorSizeType &_start_index,
                                                                   const VectorSizeType &_end_index,
                                                                   const VectorSizeType &_step){
            return SimpleVectorData<T>(this->copy_shared_ptr(), _start_index, _end_index, _step);
        }

        AlternatingVectorData<T> alternating_subset_reference(const VectorSizeType &_included_size,
                                                                             const VectorSizeType &_excluded_size){
            return AlternatingVectorData<T>(this->copy_shared_ptr(), _included_size, _excluded_size);
        }

        ReversedAlternatingVectorData<T> reversed_alternating_subset_reference(const VectorSizeType &_included_size,
                                              const VectorSizeType &_excluded_size) {
            return ReversedAlternatingVectorData<T>(this->copy_shared_ptr(), _included_size, _excluded_size);
        }

        ReversedVectorData<T> directed_subset_reference(){
            return ReversedVectorData<T>(this->copy_shared_ptr());
        }

        RepeatedVectorData<T> repeated_subset_reference(const VectorSizeType &_repetition_number){
            return RepeatedVectorData<T>(this->copy_shared_ptr(), _repetition_number);
        }

        CyclicVectorData<T> cyclic_subset_reference(const VectorSizeType &_cycles_number){
            return CyclicVectorData<T>(this->copy_shared_ptr(), _cycles_number);
        }


        /**
         * @return The total number of elements in the vector.
         */
        inline VectorSizeType size() const {
            return current_size;
        }
    };


    template<typename T>
    class VectorData : public VectorDataBase<T>{
    private:
        std::vector<T>* data;

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new VectorData(*this));
        }
    public:

#if defined(RECYCLE_THREAD_MEMORY)
        static thread_local std::shared_ptr<VectorCache<T>> vectorCache;
        std::shared_ptr<VectorCache<T>> sourceVectorCachePtr;

        VectorData(const VectorSizeType &_size, T _init_val = 0) :
                data(vectorCache.get()->AllocateVector(_size)),
                VectorDataBase<T>(_size) {
            auto &ptr = *data;
            for (int i = 0; i < _size; ++i) {
                ptr[i] = _init_val;
            }
            sourceVectorCachePtr = vectorCache;
        }

        ~VectorData() {
            vectorCache.get()->deallocateVector(data);
        }
#else
        VectorData(const VectorSizeType &_size, T _init_val = 0) :
                data(new std::vector<T>(_size, _init_val)),
                VectorDataBase<T>(_size) {}

        ~VectorData() {
            delete data;
        }
#endif

        VectorData(std::initializer_list<T> &&elements) :
                VectorData(elements.size(), 0) {
            auto &ptr = *data;
            for (int i = 0; i < elements.size(); ++i) {
                ptr[i] = elements[i];
            }
        }

        VectorData(const std::vector<T> &_data) :
                VectorData(_data.size(), 0) {
            auto &ptr = *data;
            for (int i = 0; i < _data.size(); ++i) {
                ptr[i] = _data[i];
            }
        }

        VectorData(const std::vector<T> &&_data) :
                VectorData(_data.size(), 0) {
            auto &ptr = *data;
            for (int i = 0; i < _data.size(); ++i) {
                ptr[i] = _data[i];
            }
        }

        // VectorData(const std::shared_ptr<std::vector<T>> &_data) :
        //         data(_data), VectorDataBase<T>(_data.get()->size()) {}

        // VectorData(const std::shared_ptr<std::vector<T>> &&_data) :
        //         data(_data), VectorDataBase<T>(_data.get()->size()) {}

        inline typename std::vector<T>::iterator begin(){
            return data->begin();
        }

        inline typename std::vector<T>::iterator end(){
            return data->end();
        }

        inline T &operator[](const int &index){
            return (*data)[index];
        }

        inline const T &operator[](const int &index) const{
            return (*data)[index];
        }
    };

#if defined(RECYCLE_THREAD_MEMORY)
    template<typename T>
    thread_local std::shared_ptr<VectorCache<T>>
            VectorData<T>::vectorCache =
            std::shared_ptr<VectorCache<T>>(new VectorCache<T>());
#endif

    template<typename T>
    class SimpleVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType start_index;
        VectorSizeType end_index;
        VectorSizeType step;

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new SimpleVectorData(*this));
        }
    public:
        SimpleVectorData(const std::shared_ptr<VectorDataBase<T>> &_data,
                         const VectorSizeType &_start_index,
                         const VectorSizeType &_end_index,
                         const VectorSizeType &_step) :
                data(_data), start_index(_start_index), end_index(_end_index), step(_step),
                VectorDataBase<T>(std::min(_data.get()->size(), (_end_index - _start_index) / _step + 1)) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin() + start_index;
        }

        inline typename std::vector<T>::iterator end(){
            return data.get()->begin() + end_index;
        }

        inline T &operator[](const int &index){
            return (*data.get())[start_index + index * step];
        }

        inline const T &operator[](const int &index) const{
            return (*data.get())[start_index + index * step];
        }
    };


    template<typename T>
    class AlternatingVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType included_size;
        VectorSizeType excluded_size;

        VectorSizeType chunk_size;          // (included_size + excluded_size)

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new AlternatingVectorData(*this));
        }
    public:
        AlternatingVectorData(const std::shared_ptr<VectorDataBase<T>> &_data,
                              const VectorSizeType &_included_size,
                              const VectorSizeType &_excluded_size) :
                data(_data), included_size(_included_size), excluded_size(_excluded_size),
                chunk_size((_included_size + _excluded_size)),
                VectorDataBase<T>(_data.get()->size() / (_included_size + _excluded_size)
                                         * (_included_size)
                                         + std::min(_included_size,
                                                    _data.get()->size() % (_included_size + _excluded_size))) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin();
        }

        // TODO: optimize this?
        inline typename std::vector<T>::iterator end(){
            return data.get()->begin() + (((this->size() - 1) / included_size) * chunk_size + ((this->size() - 1) % included_size));
        }

        inline T &operator[](const int &index){
            return (*data.get())[(index / included_size) * chunk_size + (index % included_size)];
        }

        inline const T &operator[](const int &index) const{
            return (*data.get())[(index / included_size) * chunk_size + (index % included_size)];
        }
    };

    template<typename T>
    class ReversedAlternatingVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType included_size;
        VectorSizeType excluded_size;

        VectorSizeType chunk_size;          // (included_size + excluded_size)

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new ReversedAlternatingVectorData(*this));
        }
    public:
        ReversedAlternatingVectorData(const std::shared_ptr<VectorDataBase<T>> &_data,
                              const VectorSizeType &_included_size,
                              const VectorSizeType &_excluded_size) :
                data(_data), included_size(_included_size), excluded_size(_excluded_size),
                chunk_size((_included_size + _excluded_size)),
                VectorDataBase<T>(_data.get()->size() / (_included_size + _excluded_size)
                                         * (_included_size)
                                         + std::min(_included_size,
                                                    _data.get()->size() % (_included_size + _excluded_size))) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin();
        }

        // TODO: optimize this?
        inline typename std::vector<T>::iterator end(){
            return data.get()->begin() + (((this->size() - 1) / included_size) * chunk_size + ((this->size() - 1) % included_size));
        }

        inline T &operator[](const int &index){
            const VectorSizeType chunk_number = (index / included_size);
            const VectorSizeType chunk_index = (index % included_size);
            const VectorSizeType chunk_offset = std::min(chunk_number * chunk_size + included_size -1, data.get()->size()-1);

            return (*data.get())[chunk_offset - chunk_index];
        }

        inline const T &operator[](const int &index) const{
            const VectorSizeType chunk_number = (index / included_size);
            const VectorSizeType chunk_index = (index % included_size);
            const VectorSizeType chunk_offset = std::min(chunk_number * chunk_size + included_size -1, data.get()->size()-1);
            return (*data.get())[chunk_offset - chunk_index];
        }
    };


    template<typename T>
    class ReversedVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType start_index;         // (data_size - 1)

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new ReversedVectorData(*this));
        }
    public:
        ReversedVectorData(const std::shared_ptr<VectorDataBase<T>> &_data) :
                data(_data), start_index(std::max(_data.get()->size() - 1, 0)),
                VectorDataBase<T>(_data.get()->size()) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin() + start_index;
        }

        inline typename std::vector<T>::iterator end(){
            return data.get()->begin();
        }

        inline T &operator[](const int &index){
            return (*data.get())[start_index - index];
        }

        inline const T &operator[](const int &index) const{
            return (*data.get())[start_index - index];
        }
    };

    template<typename T>
    class RepeatedVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType data_size;
        VectorSizeType repetition_number;

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new RepeatedVectorData(*this));
        }
    public:
        RepeatedVectorData(const std::shared_ptr<VectorDataBase<T>> &_data,
                           const VectorSizeType& _repetition_number) :
                data(_data), data_size(_data.get()->size()), repetition_number(_repetition_number),
                VectorDataBase<T>(_data.get()->size() * _repetition_number) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin();
        }

        inline typename std::vector<T>::iterator end(){
            return data.get()->begin() + (data_size - 1);
        }

        inline T &operator[](const int &index){
            return (*data.get())[index / repetition_number];
        }

        inline const T &operator[](const int &index) const{
            return (*data.get())[index / repetition_number];
        }
    };

    template<typename T>
    class CyclicVectorData : public VectorDataBase<T>{
    private:
        std::shared_ptr<VectorDataBase<T>> data;

        VectorSizeType data_size;
        VectorSizeType cycles_number;

        inline virtual std::shared_ptr<VectorDataBase<T>> copy_shared_ptr(){
            return std::shared_ptr<VectorDataBase<T>>(new CyclicVectorData(*this));
        }
    public:
        CyclicVectorData(const std::shared_ptr<VectorDataBase<T>> &_data,
                           const VectorSizeType& _cycles_number) :
                data(_data), data_size(_data.get()->size()), cycles_number(_cycles_number),
                VectorDataBase<T>(_data.get()->size() * _cycles_number) {}

        inline typename std::vector<T>::iterator begin(){
            return data.get()->begin();
        }

        inline typename std::vector<T>::iterator end(){
            return data.get()->begin() + (data_size - 1);
        }

        inline T &operator[](const int &index){
            return (*data.get())[index % data_size];
        }

        inline const T &operator[](const int &index) const{
            return (*data.get())[index % data_size];
        }
    };





}

#endif //SECRECY_VECTOR_DATA_H
