using namespace std;
#include <bits/stdc++.h>

// Lock-free queue for order processing
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        shared_ptr<T> data;
        atomic<Node*> next;
        Node() : next(nullptr) {}
    };

    atomic<Node*> head;
    atomic<Node*> tail;

public:
    LockFreeQueue() {
        Node* dummy = new Node();
        head.store(dummy);
        tail.store(dummy);
    }

    void push(T item) {
        Node* new_node = new Node();
        new_node->data = make_shared<T>(move(item));

        while (true) {
            Node* last = tail.load();
            Node* next = last->next.load();

            if (last == tail.load()) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, new_node)) {
                        tail.compare_exchange_weak(last, new_node);
                        return;
                    }
                } else {
                    tail.compare_exchange_weak(last, next);
                }
            }
        }
    }

    bool pop(T& result) {
        while (true) {
            Node* first = head.load();
            Node* last = tail.load();
            Node* next = first->next.load();

            if (first == head.load()) {
                if (first == last) {
                    if (next == nullptr) {
                        return false;
                    }
                    tail.compare_exchange_weak(last, next);
                } else {
                    if (next) {
                        result = move(*next->data);
                        if (head.compare_exchange_weak(first, next)) {
                            delete first;
                            return true;
                        }
                    }
                }
            }
        }
    }
};

// Order structure
struct Order {
    int id;
    double price;
    int quantity;
};

// Order book using a sorted vector for quick binary search
class OrderBook {
private:
    vector<Order> buy_orders;
    vector<Order> sell_orders;

public:
    void addOrder(const Order& order) {
        auto& orders = (order.quantity > 0) ? buy_orders : sell_orders;
        auto it = lower_bound(orders.begin(), orders.end(), order,
            [](const Order& a, const Order& b) { return a.price > b.price; });
        orders.insert(it, order);
    }

    bool matchOrders() {
        if (buy_orders.empty() || sell_orders.empty()) return false;

        if (buy_orders.front().price >= sell_orders.front().price) {
            // Match found, process the trade
            cout << "Trade executed: " << buy_orders.front().id << " - " << sell_orders.front().id << endl;
            buy_orders.erase(buy_orders.begin());
            sell_orders.erase(sell_orders.begin());
            return true;
        }

        return false;
    }
};

// Main trading system
class TradingSystem {
private:
    LockFreeQueue<Order> order_queue;
    OrderBook order_book;
    atomic<bool> running{true};

    void processOrders() {
        while (running) {
            Order order;
            if (order_queue.pop(order)) {
                order_book.addOrder(order);
                while (order_book.matchOrders()) {}
            }
        }
    }

public:
    void submitOrder(const Order& order) {
        order_queue.push(order);
    }

    void run() {
        thread processor(&TradingSystem::processOrders, this);
        processor.join();
    }

    void stop() {
        running = false;
    }
};

int main() {
    TradingSystem trading_system;

    // Simulate order submission
    thread order_generator([&]() {
        for (int i = 0; i < 1000; ++i) {
            Order order{i, 100.0 + (rand() % 10), (rand() % 2 == 0) ? 100 : -100};
            trading_system.submitOrder(order);
            this_thread::sleep_for(chrono::milliseconds(1));
        }
        trading_system.stop();
    });

    trading_system.run();
    order_generator.join();

    return 0;
}
