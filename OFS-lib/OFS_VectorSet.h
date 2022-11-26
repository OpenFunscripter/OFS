#pragma once

#include <algorithm>

template<typename T>
struct DefaultComparison {
    inline bool operator()(const T& a, const T& b) const noexcept
    {
        return a < b;
    }
};

template<typename T, typename Comparison = DefaultComparison<T>, typename Allocator = std::allocator<T>>
class vector_set: public std::vector<T, Allocator> {
private:
public:
    inline void sort() noexcept
    {
        std::sort(this->begin(), this->end());
    }

    template<typename... Args>
    inline bool emplace(Args&&... args) noexcept
    {
        T obj(std::forward<Args>(args)...);
        auto it = this->lower_bound(obj);

        bool areEqual = false;
        if (it != this->end()) {
            Comparison comp;
            areEqual = !comp(*it, obj) && !comp(obj, *it);
        }

        if (!areEqual) {
            this->insert(it, std::move(obj));
            return true;
        }
        return false;
    }

    inline void emplace_back_unsorted(const T& a) noexcept
    {
        this->emplace_back(a);
    }

    inline auto find(const T& a) noexcept
    {
        auto it = lower_bound(a);
        if (it != this->end() && *it == a) {
            return it;
        }
        return this->end();
    }

    inline auto find(const T& a) const noexcept
    {
        auto it = lower_bound(a);
        if (it != this->cend() && *it == a) {
            return it;
        }
        return this->cend();
    }

    inline auto lower_bound(const T& a) noexcept
    {
        auto it = std::lower_bound(this->begin(), this->end(), a,
            [](auto& a, auto& b) noexcept {
                Comparison comp;
                return comp(a, b);
            });
        return it;
    }

    inline auto lower_bound(const T& a) const noexcept
    {
        auto it = std::lower_bound(this->cbegin(), this->cend(), a,
            [](auto& a, auto& b) noexcept {
                Comparison comp;
                return comp(a, b);
            });
        return it;
    }

    inline auto upper_bound(const T& a) noexcept
    {
        auto it = std::upper_bound(this->begin(), this->end(), a,
            [](auto& a, auto& b) noexcept {
                Comparison comp;
                return comp(a, b);
            });
        return it;
    }

    inline auto upper_bound(const T& a) const noexcept
    {
        auto it = std::upper_bound(this->cbegin(), this->cend(), a,
            [](auto& a, auto& b) noexcept {
                Comparison comp;
                return comp(a, b);
            });
        return it;
    }
};
