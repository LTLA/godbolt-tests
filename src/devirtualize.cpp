/* Test devirtualization in godbolt, using a simplified setup of the knncolle/CppKmeans Matrix interface. 
 * Both of these libraries involve the creation of a Matrix subclass, which then creates an instance of a MatrixExtractor subclass.
 * The question is whether the compiler can successfully devirtualize the MatrixExtractor if it knows the exact Matrix class being used in a function.
 * 
 * In x86-64 GCC at -O2, we see that AChild::get() is called directly, along with its constructor and destructor. 
 * At -O3, this is even more dramatic whereby foo() skips the construction of AChild altogether and inlines the AChild::get() call.
 * By comparison, if we change foo() to accept a BaseParent, the get() call involves a dispatch to a variable address. 
 *
 * Clang (on Arm and x86-64) also skips the construction of AChild, even at -O2.
 */

#include <memory>

class BaseChild {
public:
    virtual ~BaseChild() = default;
    virtual int get() = 0;
};

class BaseParent {
public:
    virtual ~BaseParent() = default;
    virtual std::unique_ptr<BaseChild> create() const = 0; 
};

class AChild final : public BaseChild {
public:
    AChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload;
    }
};

class AParent final : public BaseParent {
public:
    AParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseChild> create() const {
        return std::make_unique<AChild>(my_payload);
    }
};

class BChild final : public BaseChild {
public:
    BChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload + 20;
    }
};

class BParent final : public BaseParent {
public:
    BParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseChild> create() const {
        return std::make_unique<AChild>(my_payload);
    }
};

int foo(const AParent& ap) {
    auto value = ap.create();
    return value->get();
}
