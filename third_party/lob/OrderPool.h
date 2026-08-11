#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Order.h"

// Preallocated Order storage. Shared pointers use a custom deleter that returns
// objects to the pool instead of freeing them (reduces heap churn on hot paths).
class OrderPool
{
public:
    explicit OrderPool(std::size_t capacity)
    {
        storage_.resize(capacity);
        free_.reserve(capacity);
        for (auto& order : storage_)
            free_.push_back(&order);
    }

    OrderPointer Acquire(OrderType type, OrderId id, Side side, Price price, Quantity qty)
    {
        if (free_.empty())
            return std::make_shared<Order>(type, id, side, price, qty);

        Order* raw = free_.back();
        free_.pop_back();
        raw->Reset(type, id, side, price, qty);
        return OrderPointer(raw, [this](Order* p) {
            if (p)
                free_.push_back(p);
        });
    }

    std::size_t available() const { return free_.size(); }
    std::size_t capacity() const { return storage_.size(); }

private:
    std::vector<Order> storage_;
    std::vector<Order*> free_;
};
