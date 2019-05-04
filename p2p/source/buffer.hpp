/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_BUFFER_HPP
#define ORCHID_BUFFER_HPP

#include <deque>
#include <functional>
#include <iostream>

#include <asio.hpp>

#include <boost/mp11/tuple.hpp>

#include "error.hpp"
#include "trace.hpp"

namespace orc {

class Region;
class Beam;

class Buffer {
  public:
    virtual bool each(const std::function<bool (const Region &)> &code) const = 0;

    virtual size_t size() const;
    std::string str() const;

    size_t copy(uint8_t *data, size_t size) const;

    size_t copy(char *data, size_t size) const {
        return copy(reinterpret_cast<uint8_t *>(data), size);
    }

    virtual bool empty() const {
        return size() == 0;
    }
};

std::ostream &operator <<(std::ostream &out, const Buffer &buffer);

class Region :
    public Buffer
{
  public:
    virtual const uint8_t *data() const = 0;
    size_t size() const override = 0;

    bool each(const std::function<bool (const Region &)> &code) const override {
        return code(*this);
    }

    operator asio::const_buffer() const {
        return asio::const_buffer(data(), size());
    }
};

class Subset final :
    public Region
{
  private:
    const uint8_t *const data_;
    const size_t size_;

  public:
    Subset(const uint8_t *data, size_t size) :
        data_(data),
        size_(size)
    {
    }

    const uint8_t *data() const override {
        return data_;
    }

    size_t size() const override {
        return size_;
    }
};

template <typename Data_>
class Strung final :
    public Region
{
  private:
    const Data_ data_;

  public:
    Strung(Data_ data) :
        data_(std::move(data))
    {
    }

    const uint8_t *data() const override {
        return reinterpret_cast<const uint8_t *>(data_.data());
    }

    size_t size() const override {
        return data_.size();
    }
};

template <size_t Size_>
class Block final :
    public Region
{
  public:
    static const size_t Size = Size_;

  private:
    std::array<uint8_t, Size_> data_;

  public:
    Block() {
    }

    Block(const void *data, size_t size) {
        _assert(size == Size_);
        memcpy(data_.data(), data, Size_);
    }

    Block(const std::string &data) :
        Block(data.data(), data.size())
    {
    }

    Block(std::initializer_list<uint8_t> list) {
        std::copy(list.begin(), list.end(), data_.begin());
    }

    Block(const Block &rhs) :
        data_(rhs.data_)
    {
    }

    uint8_t &operator [](size_t index) {
        return data_[index];
    }

    const uint8_t *data() const override {
        return data_.data();
    }

    uint8_t *data() {
        return data_.data();
    }

    size_t size() const override {
        return Size_;
    }

    bool operator <(const Block<Size_> &rhs) const {
        return data_ < rhs.data_;
    }
};

class Beam :
    public Region
{
  private:
    size_t size_;
    uint8_t *data_;

    uint8_t &count() {
        return data_[size_];
    }

    void subsume() {
        if (data_ != nullptr)
            ++count();
    }

    void destroy() {
        if (data_ != nullptr && --count() == 0)
            delete [] data_;
    }

  public:
    Beam() :
        size_(0),
        data_(NULL)
    {
    }

    Beam(size_t size) :
        size_(size),
        data_(new uint8_t[size_ + 1])
    {
        count() = 1;
    }

    Beam(const void *data, size_t size) :
        Beam(size)
    {
        memcpy(data_, data, size_);
    }

    Beam(const std::string &data) :
        Beam(data.data(), data.size())
    {
    }

    Beam(const Buffer &buffer);

    Beam(Beam &&rhs) noexcept :
        size_(rhs.size_),
        data_(rhs.data_)
    {
        rhs.size_ = 0;
        rhs.data_ = nullptr;
    }

    Beam(const Beam &rhs) :
        size_(rhs.size_),
        data_(rhs.data_)
    {
        subsume();
    }

    virtual ~Beam() {
        destroy();
    }

    Beam &operator =(const Beam &rhs) {
        destroy();
        size_ = rhs.size_;
        data_ = rhs.data_;
        subsume();
        return *this;
    }

    const uint8_t *data() const override {
        return data_;
    }

    uint8_t *data() {
        return data_;
    }

    size_t size() const override {
        return size_;
    }
};

template <typename Data_>
inline bool operator ==(const Beam &lhs, const std::string &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <typename Data_>
inline bool operator ==(const Beam &lhs, const Strung<Data_> &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <size_t Size_>
inline bool operator ==(const Beam &lhs, const Block<Size_> &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

inline bool operator ==(const Beam &lhs, const Beam &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

bool operator ==(const Beam &lhs, const Buffer &rhs);

template <typename Buffer_>
inline bool operator !=(const Beam &lhs, const Buffer_ &rhs) {
    return !(lhs == rhs);
}

class Nothing final :
    public Region
{
  public:
    const uint8_t *data() const override {
        return NULL;
    }

    size_t size() const override {
        return 0;
    }
};

template <typename... Buffer_>
class Knot final :
    public Buffer
{
  private:
    const std::tuple<Buffer_...> buffers_;

  public:
    Knot(const Buffer_ &...buffers) :
        buffers_(buffers...)
    {
    }

    // XXX: implement Cat (currently this is ambiguous)

    /*Knot(Buffer_ &&...buffers) :
        buffers_(std::forward<Buffer_>(buffers)...)
    {
    }*/

    bool each(const std::function<bool (const Region &)> &code) const override {
        bool value(true);
        boost::mp11::tuple_for_each(buffers_, [&](const auto &buffer) {
            value &= buffer.each(code);
        });
        return value;
    }
};

template <typename Type_>
struct Decay_ {
    typedef Type_ type;
};

template <typename Type_>
struct Decay_<std::reference_wrapper<Type_>> {
    typedef Type_ &type;
};

template <typename Type_>
struct Decay {
    typedef typename Decay_<typename std::decay<Type_>::type>::type type;
};

template <typename... Buffer_>
auto Cat(Buffer_ &&...buffers) {
    return Knot<typename Decay<Buffer_>::type...>(std::forward<Buffer_>(buffers)...);
}

template <typename... Buffer_>
auto Tie(Buffer_ &&...buffers) {
    return Knot<Buffer_...>(std::forward<Buffer_>(buffers)...);
}

class Sequence final :
    public Buffer
{
  private:
    size_t count_;
    std::unique_ptr<const Region *[]> regions_;

    class Iterator {
      private:
        const Region *const *region_;

      public:
        Iterator(const Region **region) :
            region_(region)
        {
        }

        const Region &operator *() const {
            return **region_;
        }

        Iterator &operator ++() {
            ++region_;
            return *this;
        }

        bool operator !=(const Iterator &rhs) const {
            return region_ != rhs.region_;
        }
    };

  public:
    Sequence(const Buffer &buffer) :
        count_([&]() {
            size_t count(0);
            buffer.each([&](const Region &region) {
                ++count;
                return true;
            });
            return count;
        }()),

        regions_(new const Region *[count_])
    {
        auto i(regions_.get());
        buffer.each([&](const Region &region) {
            *(i++) = &region;
            return true;
        });
    }

    Sequence(Sequence &&sequence) :
        count_(sequence.count_),
        regions_(std::move(sequence.regions_))
    {
    }

    Sequence(const Sequence &sequence) :
        count_(sequence.count_),
        regions_(new const Region *[count_])
    {
        auto old(sequence.regions_.get());
        std::copy(old, old + count_, regions_.get());
    }

    Iterator begin() const {
        return regions_.get();
    }

    Iterator end() const {
        return regions_.get() + count_;
    }

    bool each(const std::function<bool (const Region &)> &code) const override {
        for (auto i(begin()), e(end()); i != e; ++i)
            if (!code(*i))
                return false;
        return true;
    }
};

class Window final :
    public Buffer
{
  private:
    size_t count_;
    std::unique_ptr<const Region *[]> regions_;

    class Iterator final :
        public Region
    {
        friend class Window;

      private:
        const Region **region_;
        size_t offset_;

      public:
        Iterator() :
            region_(NULL),
            offset_(0)
        {
        }

        Iterator(const Region **region, size_t offset) :
            region_(region),
            offset_(offset)
        {
        }

        const uint8_t *data() const override {
            return (*region_)->data() + offset_;
        }

        size_t size() const override {
            return (*region_)->size() - offset_;
        }
    } index_;

  public:
    Window() :
        count_(0)
    {
    }

    Window(const Buffer &buffer) :
        count_([&]() {
            size_t count(0);
            buffer.each([&](const Region &region) {
                ++count;
                return true;
            });
            return count;
        }()),

        regions_(new const Region *[count_]),

        index_(regions_.get(), 0)
    {
        auto i(regions_.get());
        buffer.each([&](const Region &region) {
            *(i++) = &region;
            return true;
        });
    }

    Window(Window &&rhs) = default;
    Window &operator =(Window &&rhs) = default;

    bool each(const std::function<bool (const Region &)> &code) const override {
        auto here(index_.region_);
        auto rest(regions_.get() + count_ - here);
        if (rest == 0)
            return true;

        size_t i;
        if (index_.offset_ == 0)
            i = 0;
        else {
            i = 1;
            if (!code(index_))
                return false;
        }

        for (; i != rest; ++i)
            if (!code(*here[i]))
                return false;

        return true;
    }

    template <size_t Size_>
    void Take(Block<Size_> &value) {
        auto data(value.data());

        auto &here(index_.region_);
        auto &step(index_.offset_);

        auto rest(regions_.get() + count_ - here);

        for (auto need(Size_); need != 0; step = 0, ++here, --rest) {
            _assert(rest != 0);

            auto size((*here)->size() - step);
            if (size == 0)
                continue;

            if (need < size) {
                memcpy(data, (*here)->data() + step, need);
                step += need;
                break;
            }

            memcpy(data, (*here)->data() + step, size);
            data += size;
            need -= size;
        }
    }
};

template <size_t Size_>
struct Taken {
    typedef Block<Size_> type;
};

template <>
struct Taken<0> {
    typedef Window type;
};

template <size_t Index_, size_t... Size_>
struct Taker {};

template <size_t Index_, size_t Size_, size_t... Rest_>
struct Taker<Index_, Size_, Rest_...> {
template <typename Type_>
static void Take(Window &&window, Type_ &value) {
    window.Take(std::get<Index_>(value));
    Taker<Index_ + 1, Rest_...>::Take(std::move(window), value);
} };

template <size_t Index_>
struct Taker<Index_, 0> {
template <typename Type_>
static void Take(Window &&window, Type_ &value) {
    std::get<Index_>(value) = std::move(window);
} };

template <size_t Index_>
struct Taker<Index_> {
template <typename Type_>
static void Take(Window &&window, Type_ &value) {
    _assert(window.empty());
} };

template <size_t... Size_>
auto Take(const Buffer &buffer) {
    std::tuple<typename Taken<Size_>::type...> value;
    Taker<0, Size_...>::Take(buffer, value);
    return value;
}

}

#endif//ORCHID_BUFFER_HPP