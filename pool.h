#pragma once
#include <cstddef>
template <typename T>
class Pool{
    T* buffer_;
    size_t* next_free_;
    size_t capacity_;
    size_t free_head_;
    size_t used_;

  public:
    explicit Pool(size_t cap)
        : capacity_(cap), free_head_(0),used_(0)
    {
        buffer_ = static_cast<T*>(::operator new(capacity_ * sizeof(T)));
        next_free_ = static_cast<size_t*>(::operator new(capacity_ * sizeof(size_t)));
        for(size_t i =0; i < capacity_;++i) next_free_[i] = i+1;
    }

    ~Pool(){
        ::operator delete(buffer_);
        ::operator delete(next_free_);
    }

    T* allocate(){
        if(free_head_ >= capacity_) return nullptr;
        size_t slot = free_head_;
        free_head_ = next_free_[slot];
        ++used_;
        return &buffer_[slot];
    }
    void deallocate(T* p){
        size_t slot = static_cast<size_t>(p-buffer_);
        next_free_[slot] = free_head_;
        free_head_ = slot;
        --used_;
    }
    size_t used() const {return used_;}
    size_t capacity() const {return capacity_;}
};

