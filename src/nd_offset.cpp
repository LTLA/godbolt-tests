/* This is code taken from the [sanisizer](https://github.com/LTLA/sanisizer) library,
 * where nd_offset() returns an offset for accessing an element of a high-dimensional array that has been flattened into a single dimension.
 * The question is, if one were to use nd_offset() in a tight loop, would the compiler be smart enough to hoist the multiplication out of the loop?
 *
 * We run this with '--std=c++17 -O2' on x86-64 GCC 11.4 and armv8-a clang 20.10.
 * Examination of the assembly indicates that the multiplication (imul in x86, mul in arm) is performed once (if at all) outside of the loop.
 * The loop itself increments by the stride (in this example, 'NC') and successfully avoids the more-expensive integer multiplication.
 * This is pretty much as good as the code we might have manually written.
 */

#include <cstddef>

template<typename Size_>
Size_ nd_offset_internal(Size_ extent, Size_ pos) {
    return extent * pos;
}

template<typename Size_, typename... MoreArgs_>
Size_ nd_offset_internal(Size_ extent, Size_ pos, MoreArgs_... more_args) {
    return (pos + nd_offset_internal<Size_>(more_args...)) * extent;
}

template<typename Size_, typename First_, typename Second_, typename... Remaining_>
Size_ nd_offset(First_ x1, First_ extent1, Second_ x2, Remaining_... remaining) {
    return static_cast<Size_>(x1) + nd_offset_internal<Size_>(extent1, x2, remaining...);
}

double sum(const double* mat, int NR, int NC, int r0, int c) {
    double val = 0;
    for (int r = r0; r < NR; ++r) {
        // Do something with the matrix element at (r, c)
        auto elmt = mat[nd_offset<std::size_t>(c, NC, r)];
        val += elmt;
    }
    return val;
}
