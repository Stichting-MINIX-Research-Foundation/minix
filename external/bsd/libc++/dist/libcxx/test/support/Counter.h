//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef COUNTER_H
#define COUNTER_H

#include <functional> // for std::hash

struct Counter_base { static int gConstructed; };
    
template <typename T>
class Counter : public Counter_base
{
public:
    Counter() : data_()                             { ++gConstructed; }
    Counter(const T &data) : data_(data)            { ++gConstructed; }
    Counter(const Counter& rhs) : data_(rhs.data_)  { ++gConstructed; }
    Counter& operator=(const Counter& rhs)          { ++gConstructed; data_ = rhs.data_; return *this; }
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    Counter(Counter&& rhs) : data_(std::move(rhs.data_))  { ++gConstructed; }
    Counter& operator=(Counter&& rhs) { ++gConstructed; data_ = std::move(rhs.data_); return *this; }
#endif
    ~Counter() { --gConstructed; }
    
    const T& get() const {return data_;}

    bool operator==(const Counter& x) const {return data_ == x.data_;}
    bool operator< (const Counter& x) const {return data_ <  x.data_;}

private:
    T data_;
};

int Counter_base::gConstructed = 0;

namespace std {

template <class T>
struct hash<Counter<T> >
    : public std::unary_function<Counter<T>, std::size_t>
{
    std::size_t operator()(const Counter<T>& x) const {return std::hash<T>(x.get());}
};
}

#endif
