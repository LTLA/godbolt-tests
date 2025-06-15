/* Test a complex devirtualization scheme in godbolt.
 * This is based on usage of `tatami_chunked::CustomDenseChunkedMatrixManager` in the [tatami_chunked](https://github.com/tatami-inc/tatami_chunked) library.
 * However, it also applies to any other library where an instance of an abstract interface creates another instance of an abstract interface.
 *
 * The question here is whether the "child" instance can be successfully devirtualized through a pointer to the interface stored in a wrapper class.
 * The key part here is that the wrapper itself is an abstract interface and is often called in a context where devirtualization cannot occur.
 * This limits the optimization opportunities for the compiler, which needs to be able to spot the devirtualization within the concrete subclass of the wrapper.
 *
 * More concretely - in the call to `foo()`, no devirtualization occurs on the wrapper classes so we expect `wrapped_get()` to perform some virtual dispatch.
 * However, once it arrives at the methods for the `ActualWrappedChild<Core_>::wrapped_get()`, do those perform virtual dispatch?
 * We hope that devirtualization occurs in `ActualWrappedChild` as its constructor knows the exact `Core_` subclass for which `create()` was called.
 *  
 * Unfortunately, no devirtualization seems to be performed on either ARM clang or x86-64 GCC, under `--std=c++17 -O2` or even `-O3`.
 * Inspection of the `wrapped_get()` code indicates that there are still jumps/branches to variable memory addresses for the `BaseCoreChild::get()` methods.
 * Indeed, the assembly for the `wrapped_get()` methods are identical for all template specializations of `Core_`, indicating that no devirtualization has occurred.
 * It seems that the compiler is unwilling to change the definition of the `BaseCoreChild*` class member. 
 */

#include <memory>

class BaseCoreChild {
public:
    virtual ~BaseCoreChild() = default;
    virtual int get() = 0;
};

class BaseCoreParent {
public:
    virtual ~BaseCoreParent() = default;
    virtual std::unique_ptr<BaseCoreChild> create() const = 0; 
};

class ACoreChild final : public BaseCoreChild {
public:
    ACoreChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload;
    }
};

class ACoreParent final : public BaseCoreParent {
public:
    ACoreParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseCoreChild> create() const {
        return std::make_unique<ACoreChild>(my_payload);
    }
};

class BCoreChild final : public BaseCoreChild {
public:
    BCoreChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload + 20;
    }
};

class BCoreParent final : public BaseCoreParent {
public:
    BCoreParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseCoreChild> create() const {
        return std::make_unique<BCoreChild>(my_payload);
    }
};

class BaseWrapperChild {
public:
    virtual ~BaseWrapperChild() = default;
    virtual int wrapped_get() = 0;
};

class BaseWrapperParent {
public:
    virtual ~BaseWrapperParent() = default;
    virtual std::unique_ptr<BaseWrapperChild> initialize() const = 0; 
};

template<class Core_>
class ActualWrapperChild final : public BaseWrapperChild {
public:
    ActualWrapperChild(const Core_& core_parent) : my_core_child(core_parent.create()) {}
    int wrapped_get() {
        return my_core_child->get();
    }
private:
    std::unique_ptr<BaseCoreChild> my_core_child;
};

template<class Core_>
class ActualWrapperParent final : public BaseWrapperParent {
public:
    ActualWrapperParent(std::shared_ptr<Core_> core) : my_core_parent(std::move(core)) {}
    std::unique_ptr<BaseWrapperChild> initialize() const {
        return std::make_unique<ActualWrapperChild<Core_> >(*my_core_parent);
    }
private:
    std::shared_ptr<Core_> my_core_parent;
};

int foo(const BaseWrapperParent& wparent) {
    auto wchild = wparent.initialize();
    return wchild->wrapped_get();
}

int bar(std::shared_ptr<BaseCoreParent> base, std::shared_ptr<ACoreParent> abase, std::shared_ptr<BCoreParent> bbase) {
    ActualWrapperParent<BaseCoreParent> parent(std::move(base));
    ActualWrapperParent<ACoreParent> aparent(std::move(abase));
    ActualWrapperParent<BCoreParent> bparent(std::move(bbase));
    return foo(parent) + foo(aparent) + foo(bparent);
}

/* To force devirtualization, it seems we need to be a bit more direct.
 * We add a non-virtual `create_exact()` function to each `Parent` class, which returns the exact type of `Child`.
 * This allows us to specify the exact type of the `ActualWrappedChild`'s pointer when the type of the parent (`Core_`) is also exactly known. 
 * Inspection of the assembly now reveals a difference in the various `wrapped_get()` methods,
 * where jumps are only performed for `Core_ = BaseCoreParent` while the calculation is directly inlined for `Core_ = ACoreParent` or `BCoreParent`. 
 */

#include <memory>

class BaseCoreChild {
public:
    virtual ~BaseCoreChild() = default;
    virtual int get() = 0;
};

class BaseCoreParent {
public:
    virtual ~BaseCoreParent() = default;
    virtual std::unique_ptr<BaseCoreChild> create() const = 0; 
    std::unique_ptr<BaseCoreChild> create_exact() const { return create(); }
};

class ACoreChild final : public BaseCoreChild {
public:
    ACoreChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload;
    }
};

class ACoreParent final : public BaseCoreParent {
public:
    ACoreParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseCoreChild> create() const {
        return create_exact();
    }

    std::unique_ptr<ACoreChild> create_exact() const {
        return std::make_unique<ACoreChild>(my_payload);
    }
};

class BCoreChild final : public BaseCoreChild {
public:
    BCoreChild(int p) : payload(p) {}

private:
    int payload;

public:
    int get() {
        return payload + 20;
    }
};

class BCoreParent final : public BaseCoreParent {
public:
    BCoreParent(int p) : my_payload(p) {}

private:
    int my_payload;

public:
    std::unique_ptr<BaseCoreChild> create() const {
        return create_exact();
    }

    std::unique_ptr<BCoreChild> create_exact() const {
        return std::make_unique<BCoreChild>(my_payload);
    }
};

class BaseWrapperChild {
public:
    virtual ~BaseWrapperChild() = default;
    virtual int wrapped_get() = 0;
};

class BaseWrapperParent {
public:
    virtual ~BaseWrapperParent() = default;
    virtual std::unique_ptr<BaseWrapperChild> initialize() const = 0; 
};

template<class Core_>
class ActualWrapperChild final : public BaseWrapperChild {
public:
    ActualWrapperChild(const Core_& core_parent) : my_core_child(core_parent.create_exact()) {}
    int wrapped_get() {
        return my_core_child->get();
    }
private:
    decltype(std::declval<Core_>().create_exact()) my_core_child;
};

template<class Core_>
class ActualWrapperParent final : public BaseWrapperParent {
public:
    ActualWrapperParent(std::shared_ptr<Core_> core) : my_core_parent(std::move(core)) {}
    std::unique_ptr<BaseWrapperChild> initialize() const {
        return std::make_unique<ActualWrapperChild<Core_> >(*my_core_parent);
    }
private:
    std::shared_ptr<BaseCoreParent> my_core_parent;
};

int foo(const BaseWrapperParent& wparent) {
    auto wchild = wparent.initialize();
    return wchild->wrapped_get();
}

int bar(std::shared_ptr<BaseCoreParent> base, std::shared_ptr<ACoreParent> abase, std::shared_ptr<BCoreParent> bbase) {
    ActualWrapperParent<BaseCoreParent> parent(std::move(base));
    ActualWrapperParent<ACoreParent> aparent(std::move(abase));
    ActualWrapperParent<BCoreParent> bparent(std::move(bbase));
    return foo(parent) + foo(aparent) + foo(bparent);
}
