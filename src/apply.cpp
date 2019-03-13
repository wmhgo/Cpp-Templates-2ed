#include <cstddef>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace jc {

template <std::size_t... Index>
struct IntegerSequence {
  using Type = IntegerSequence<Index...>;
};

template <typename List1, typename List2>
struct ConcatT;

template <std::size_t... List1, std::size_t... List2>
struct ConcatT<IntegerSequence<List1...>, IntegerSequence<List2...>>
    : IntegerSequence<List1..., (sizeof...(List1) + List2)...> {};

template <typename List1, typename List2>
using Concat = typename ConcatT<List1, List2>::Type;

template <std::size_t N>
struct MakeIntegerSequence
    : Concat<typename MakeIntegerSequence<N / 2>::Type,
             typename MakeIntegerSequence<N - N / 2>::Type> {};

template <>
struct MakeIntegerSequence<0> : IntegerSequence<> {};

template <>
struct MakeIntegerSequence<1> : IntegerSequence<0> {};

template <std::size_t N>
using MakeIndexSequence = typename MakeIntegerSequence<N>::Type;

template <typename F, typename... List, std::size_t... Index>
void apply_impl(F&& f, const std::tuple<List...>& t,
                IntegerSequence<Index...>) {
  std::invoke(std::forward<F>(f), std::get<Index>(t)...);
}

template <typename F, typename... List>
void apply(F&& f, const std::tuple<List...>& t) {
  apply_impl(std::forward<F>(f), t, MakeIndexSequence<sizeof...(List)>{});
}

}  // namespace jc

struct Print {
  template <typename... Args>
  void operator()(const Args&... args) {
    auto no_used = {(std::cout << args << " ", 0)...};
  }
};

int main() {
  auto t = std::make_tuple(3.14, 42, "hello world");
  jc::apply(Print{}, t);  // 3.14 42 hello world
}