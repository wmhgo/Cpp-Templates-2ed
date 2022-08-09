## Typelist 解析

* Typelist 是类型元编程的核心数据结构，不同于大多数运行期数据结构，Typelist 不允许改变。比如添加一个元素到 [std::list](https://en.cppreference.com/w/cpp/container/list)会改变其本身，而添加一个元素到 Typelist 则是创建一个新的 Typelist
* 一个 Typelist 通常实现为一个类模板特化

```cpp
template<typename... Elements>
class Typelist {};
```

* Typelist 的元素直接写为模板实参，比如空的 Typelist 写为 `Typelist<>`，只包含 int 的 Typelist 写为 `Typelist<int>`

```cpp
using SignedIntegralTypes = // 包含所有有符号整型的 Typelist
  Typelist<signed char, short, int, long, long long>;
```

* 操作这个 Typelist 通常需要将它拆分开，一般把第一个元素分离在一个列表（头部），剩下的在另一个列表（尾部）

```cpp
template<typename List>
class FrontT;

template<typename Head, typename... Tail>
class FrontT<Typelist<Head, Tail...>> {
 public:
  using Type = Head;
};

template<typename List>
using Front = typename FrontT<List>::Type;

Front<SignedIntegralTypes> // 生成 signed char
```

* 类似地，实现 PopFront，从 Typelist 移除第一个元素

```cpp
template<typename List>
class PopFrontT;

template<typename Head, typename... Tail>
class PopFrontT<Typelist<Head, Tail...>> {
 public:
  using Type = Typelist<Tail...>;
};

template<typename List>
using PopFront = typename PopFrontT<List>::Type;

PopFront<SignedIntegralTypes> // 生成 Typelist<short, int, long, long long>
```

* 实现 PushFront，把一个新元素添加到 Typelist 之前

```cpp
template<typename List, typename NewElement>
class PushFrontT;

template<typename... Elements, typename NewElement>
class PushFrontT<Typelist<Elements...>, NewElement> {
 public:
  using Type = Typelist<NewElement, Elements...>;
};

template<typename List, typename NewElement>
using PushFront = typename PushFrontT<List, NewElement>::Type;

PushFront<SignedIntegralTypes, bool> // 生成 Typelist<bool, signed char, short, int, long, long long>
```

## Typelist算法

* 前面的操作可以结合起来实现更复杂的算法，比如 PopFront 再 PushFront 来替代 Typelist 的第一个元素

```cpp
using Type = PushFront<PopFront<SignedIntegralTypes>, bool>;
// equivalent to Typelist<bool, short, int, long, long long>
```

### 索引

* 最基本的算法操作之一是提取一个特定的元素

```cpp
using TL = NthElement<Typelist<short, int, long>, 2>; // TL 相当于 long
```

* 这个操作使用递归元编程实现，它将遍历 Typelist 直到找到需要的元素

```cpp
// recursive case:
template<typename List, unsigned N>
class NthElementT : public NthElementT<PopFront<List>, N - 1>
{};

// basis case:
template<typename List>
class NthElementT<List, 0> : public FrontT<List>
{};

template<typename List, unsigned N>
using NthElement = typename NthElementT<List, N>::Type;
```

* 上例中

```cpp
NthElementT<Typelist<short, int, long>, 2>
// 派生自
NthElementT<Typelist<int, long>, 1>
// 派生自
NthElementT<Typelist<long>, 0>
// 派生自
FrontT<Typelist<long>>
```

### 查找最佳匹配

* 下面有几次要用到 IfThenElseT 模板，也可以使用 [std::conditional](https://en.cppreference.com/w/cpp/types/conditional)

```cpp
template<bool COND, typename TrueType, typename FalseType>
struct IfThenElseT {
  using Type = TrueType;
};

// partial specialization: false yields third argument
template<typename TrueType, typename FalseType>
struct IfThenElseT<false, TrueType, FalseType> {
  using Type = FalseType;
};

template<bool COND, typename TrueType, typename FalseType>
using IfThenElse = typename IfThenElseT<COND, TrueType, FalseType>::Type;
```

* 查找 Typelist 中最大的类型（有多个一样大的类型则返回第一个）

```cpp
template<typename List>
class LargestTypeT;

// recursive case:
template<typename List>
class LargestTypeT {
 private:
  using First = Front<List>;
  using Rest = typename LargestTypeT<PopFront<List>>::Type;
 public:
  using Type = IfThenElse<(sizeof(First) >= sizeof(Rest)), First, Rest>;
};

// basis case:
template<>
class LargestTypeT<Typelist<>> {
 public:
  using Type = char;
};

template<typename List>
using LargestType = typename LargestTypeT<List>::Type;
```

* 基本情况显式指出空的 Typelist 为 `Typelist<>`，这阻碍了使用其他类型的 Typelist，为此引入一个检查 Typelist 是否为空的元函数 IsEmpty

```cpp
template<typename List>
class IsEmpty {
 public:
  static constexpr bool value = false;
};

template<>
class IsEmpty<Typelist<>> {
 public:
  static constexpr bool value = true;
};

template<typename List, bool Empty = IsEmpty<List>::value>
class LargestTypeT;

// recursive case:
template<typename List>
class LargestTypeT<List, false> {
 private:
  using Contender = Front<List>;
  using Best = typename LargestTypeT<PopFront<List>>::Type;
 public:
  using Type = IfThenElse<(sizeof(Contender) >= sizeof(Best)), Contender, Best>;
};

// basis case:
template<typename List>
class LargestTypeT<List, true> {
 public:
  using Type = char;
};

template<typename List>
using LargestType = typename LargestTypeT<List>::Type;
```

### Append

* 略微改动 PushFront 就能得到 PushBack

```cpp
template<typename List, typename NewElement>
class PushBackT;

template<typename... Elements, typename NewElement>
class PushBackT<Typelist<Elements...>, NewElement> {
 public:
  using Type = Typelist<Elements..., NewElement>;
};

template<typename List, typename NewElement>
using PushBack = typename PushBackT<List, NewElement>::Type;
```

* 也能为 PushBack 实现一个通用的算法，只需要使用 Front、PushFront、PopFront 和 IsEmpty

```cpp
template<typename List, typename NewElement, bool = IsEmpty<List>::value>
class PushBackRecT;

// recursive case:
template<typename List, typename NewElement>
class PushBackRecT<List, NewElement, false> {
  using Head = Front<List>;
  using Tail = PopFront<List>;
  using NewTail = typename PushBackRecT<Tail, NewElement>::Type;
 public:
  using Type = PushFront<NewTail, Head>;
};

// basis case: PushFront 添加 NewElement 到空列表
// 因为空列表中 PushFront 的作用等价于 PushBack
template<typename List, typename NewElement>
class PushBackRecT<List, NewElement, true> {
 public:
  using Type = PushFront<List, NewElement>;
};

// generic push-back operation:
template<typename List, typename NewElement>
class PushBackT : public PushBackRecT<List, NewElement> {};

template<typename List, typename NewElement>
using PushBack = typename PushBackT<List, NewElement>::Type;
```

* 对一个例子展开递归

```cpp
PushBackRecT<Typelist<short, int>, long>
// Head 是 short，Tail 是 Typelist<int>，NewTail 为
PushBackRecT<Typelist<int>, long>
// Head 是 int，Tail 是 Typelist<>，NewTail 为
PushBackRecT<Typelist<>, long>
// 触发基本情况，Type 求值为
PushFront<Typelist<>, long>
// 即
Typelist<long>
// 随后依次把之前的 Head 压入列表前，Type 为
PushFront<Typelist<long>, int>
// 即
Typelist<int, long>
// 再次压入 Head，Type 为
PushFront<Typelist<int, long>, short>
// 最终 Type 即为
Typelist<short, int, long>
```

* 通用的 PushBackRecT 实现能用于任何类型的 Typelist，就像之前的算法一样，它的计算需要线性数量的模板实例化，因为对于长度为 N 的 Typelist，将有 N+1 个 PushBackRect 和 PushFrontT 的实例化，以及 N 个 FrontT 和 PopFrontT 的实例化。模板实例化本身相当于一个编译器的调用进程，因此知道模板实例化的数量能粗略估计编译时间
* 对于巨大的模板元程序，编译时间是一个问题，因此尝试减少通过执行这些算法产生的模板实例化数量是合理的。事实上，第一个 PushBack 的实现采用了一个 Typelist 的偏特化，只需要常数数量的模板实例化，这使其在编译期比泛型版本更高效。此外，由于它被描述为一个偏特化，对一个 Typelist 执行 PushBack 时将自动选择这个偏特化，从而实现算法特化

### Reverse

* 翻转所有元素的顺序

```cpp
template<typename List, bool Empty = IsEmpty<List>::value>
class ReverseT;

template<typename List>
using Reverse = typename ReverseT<List>::Type;

// recursive case:
template<typename List>
class ReverseT<List, false>
: public PushBackT<Reverse<PopFront<List>>, Front<List>> {};

// basis case:
template<typename List>
class ReverseT<List, true> {
 public:
  using Type = List;
};
```

* 对一个例子展开递归

```cpp
Reverse<Typelist<short, int, long>>
// 即
PushBackT<Reverse<Typelist<int, long>>, short>::Type
// 即
PushBackT<PushBackT<Typelist<long>, int>::type>, short>::Type
// 即
PushBackT<Typelist<long, int>, short>::Type
// 即
Typelist<long, int, short>
```

* Reverse 结合 PopFront 即可实现移除尾元素的操作 PopBack

```cpp
template<typename List>
class PopBackT {
 public:
  using Type = Reverse<PopFront<Reverse<List>>>;
};

template<typename List>
using PopBack = typename PopBackT<List>::Type;
```

### Transform

* 要对每个元素类型进行某种操作，比如对所有元素使用如下元函数

```cpp
template<typename T>
struct AddConstT {
  using Type = const T;
};
```

* 这需要实现一个 Transform 算法

```cpp
template<typename List,
  template<typename T> class MetaFun,
  bool Empty = IsEmpty<List>::value>
class TransformT;

// recursive case:
template<typename List,
  template<typename T> class MetaFun>
class TransformT<List, MetaFun, false>
: public PushFrontT<
  typename TransformT<PopFront<List>, MetaFun>::Type,
  typename MetaFun<Front<List>>::Type
  >
{};

// basis case:
template<typename List,
  template<typename T> class MetaFun>
class TransformT<List, MetaFun, true> {
 public:
  using Type = List;
};

template<typename List,
  template<typename T> class MetaFun>
using Transform = typename TransformT<List, MetaFun>::Type;
```

* 对一个例子展开递归

```cpp
Transform<Typelist<short, int, long>, AddConstT>
// 即
PushFront<
  Transform<Typelist<int, long>, AddConstT>,
  typename AddConst<short>::Type
  >
// 即
PushFront<
  PushFront<
    Transform<Typelist<long>, AddConstT>,
    typename AddConst<int>::Type
    >,
  const short>
// 即
PushFront<
  PushFront<
    PushFront<
      Transform<Typelist<>, AddConstT>,
      typename AddConst<long>::Type
    const int
    >,
  const short>
// 即
PushFront<
  PushFront<
    PushFront<
      Typelist<>,
      const long
    const int
    >,
  const short>
// 即
Typelist<const short, const int, const long>
```

### Accumulate

* Accumulate 的参数为一个 Typelist，一个初始化类型 I，一个接受两个类型返回一个类型的元函数 F。设 Typelist 的元素依次表示为 T1~TN，则 Accumulate 返回 `F(F(...F(F(I, T1), T2), ..., TN−1), TN)`

```cpp
template<typename List,
  template<typename X, typename Y> class F, // 元函数
  typename I, // 初始类型
  bool = IsEmpty<List>::value>
class AccumulateT;

// recursive case:
template<typename List,
  template<typename X, typename Y> class F,
  typename I>
class AccumulateT<List, F, I, false>
: public AccumulateT<
  PopFront<List>,
  F,
  typename F<I, Front<List>>::Type
  >
{};

// basis case:
template<typename List,
  template<typename X, typename Y> class F,
  typename I>
class AccumulateT<List, F, I, true> {
 public:
  using Type = I;
};

template<typename List,
  template<typename X, typename Y> class F,
  typename I>
using Accumulate = typename AccumulateT<List, F, I>::Type;
```

* 对 Accumulate 使用 PushFrontT 作为元函数 F，空的 Typelist 作为初始类型 I，即可实现 Reverse 算法

```cpp
Accumulate<Typelist<short, int, long>, PushFrontT, Typelist<>>
// 生成 Typelist<long, int, short>
```

* 同理也可以用 Accumulate 实现 LargestType，这需要一个返回两个类型中较大者的元函数

```cpp
template<typename T, typename U>
class LargerTypeT
: public IfThenElseT<sizeof(T) >= sizeof(U), T, U>
{};

template<typename Typelist>
class LargestTypeAccT
: public AccumulateT<
  PopFront<Typelist>,
  LargerTypeT,  
  Front<Typelist>>
{};

template<typename Typelist>
using LargestTypeAcc = typename LargestTypeAccT<Typelist>::Type;
```

* 但注意这个版本的 LargestType 把 Typelist 的首元素作为初始类型，因此它需要一个非空的 Typelist，这需要显式指定

```cpp
template<typename T, typename U>
class LargerTypeT
: public IfThenElseT<sizeof(T) >= sizeof(U), T, U>
{};

template<typename Typelist, bool = IsEmpty<Typelist>::value>
class LargestTypeAccT;

template<typename Typelist>
class LargestTypeAccT<Typelist, false>
: public AccumulateT<
  PopFront<Typelist>,
  LargerTypeT,
  Front<Typelist>>
{};

template<typename Typelist>
class LargestTypeAccT<Typelist, true>
{};

template<typename Typelist>
using LargestTypeAcc = typename LargestTypeAccT<Typelist>::Type;

LargestTypeAcc<Typelist<short, int, char>> // int
```

### 插入排序

* 实现插入排序和其他算法一样，递归地把列表拆分为首元素和尾元素，尾部随后递归地排序，再把头部插入排序后的列表中

```cpp
template<typename List,
  template<typename T, typename U> class Compare,
  bool = IsEmpty<List>::value>
class InsertionSortT;

template<typename List,
  template<typename T, typename U> class Compare>
using InsertionSort = typename InsertionSortT<List, Compare>::Type;

// recursive case (insert first element into sorted list):
template<typename List,
  template<typename T, typename U> class Compare>
class InsertionSortT<List, Compare, false>
: public InsertSortedT< // 关键在于实现此元函数
  InsertionSort<PopFront<List>, Compare>,
  Front<List>,
  Compare>
{};

// basis case (an empty list is sorted):
template<typename List,
  template<typename T, typename U> class Compare>
class InsertionSortT<List, Compare, true> {
 public:
  using Type = List;
};
```

* 下面实现 InsertSortedT 元函数，它把一个值插入一个已排序的列表中

```cpp
template<typename T>
struct IdentityT {
  using Type = T;
};

template<typename List, typename Element,
  template<typename T, typename U> class Compare,
  bool = IsEmpty<List>::value>
class InsertSortedT;

// recursive case:
template<typename List, typename Element,
  template<typename T, typename U> class Compare>
class InsertSortedT<List, Element, Compare, false> {
  // compute the tail of the resulting list:
  using NewTail =
    typename IfThenElse<Compare<Element, Front<List>>::value,
      IdentityT<List>,
      InsertSortedT<PopFront<List>, Element, Compare>
    >::Type;

  // compute the head of the resulting list:
  using NewHead = IfThenElse<Compare<Element, Front<List>>::value,
    Element,
    Front<List>>;
 public:
  using Type = PushFront<NewTail, NewHead>;
};

// basis case:
template<typename List, typename Element,
  template<typename T, typename U> class Compare>
class InsertSortedT<List, Element, Compare, true>
: public PushFrontT<List, Element>
{};

template<typename List, typename Element,
  template<typename T, typename U> class Compare>
using InsertSorted = typename InsertSortedT<List, Element, Compare>::Type;
```

* 这个实现避免了初始化不使用的类型。将递归情况改为下面的实现，技术上也是正确的，但它计算了 IfThenElseT 所有分支的模板实参，这里 then 分支的 PushFront 是低开销的，但 else 分支的 InsertSorted 则不是

```cpp
// recursive case:
template<typename List, typename Element,
  template<typename T, typename U> class Compare>
class InsertSortedT<List, Element, Compare, false>
: public IfThenElseT<Compare<Element, Front<List>>::value,
  PushFront<List, Element>,
  PushFront<InsertSorted<PopFront<List>, Element, Compare>, Front<List>>>
{};
```

* 下面是对一个 Typelist 使用 InsertionSort 的例子

```cpp
template<typename T, typename U>
struct SmallerThanT {
  static constexpr bool value = sizeof(T) < sizeof(U);
};

using Types = Typelist<int, char, short, double>;
using ST = InsertionSort<Types, SmallerThanT>;
std::cout << std::is_same_v<ST,Typelist<char, short, int, double>>; // true
```

## 非类型Typelist

* Typelist 也能用于编译期值的序列，一个简单的方法是定义一个表示特定类型值的类模板

```cpp
template<typename T, T Value>
struct CTValue {
  static constexpr T value = Value;
};
```

* 使用这个模板即可表达一个包含质数整型值的 Typelist

```cpp
using Primes = Typelist<CTValue<int, 2>, CTValue<int, 3>,
  CTValue<int, 5>, CTValue<int, 7>, CTValue<int, 11>>;
```

* 下面在一个非类型 Typelist 上进行数值计算

```cpp
template<typename T, typename U>
struct MultiplyT;

template<typename T, T Value1, T Value2>
struct MultiplyT<CTValue<T, Value1>, CTValue<T, Value2>> {
  using Type = CTValue<T, Value1 * Value2>;
};

template<typename T, typename U>
using Multiply = typename MultiplyT<T, U>::Type;
```

* 下面的表达式的结果为 Typelist 中所有数的乘积

```cpp
Accumulate<Primes, MultiplyT, CTValue<int, 1>>::value
```

* 但这样指定元素十分繁琐，尤其是对于所有值类型相同的情况。引入一个别名模板能简化这种情况

```cpp
template<typename T, T... Values>
using CTTypelist = Typelist<CTValue<T, Values>...>;

using Primes = CTTypelist<int, 2, 3, 5, 7, 11>;
```

* 这个方法唯一的缺点是，别名模板只是别名，出现时的诊断信息还是下层的 CTValueType 的 Typelist，而这可能造成更冗长的问题。为此可以创建一个新的 Typelist 类 Valuelist 来直接存储值，提供 IsEmpty、FrontT、PopFrontT 和 PushFrontT 使 Valuelist 更合适地用于算法，提供 PushBackT 为一个减少编译期操作开销的算法特化

```cpp
template<typename T, T... Values>
struct Valuelist {};

template<typename T, T... Values>
struct IsEmpty<Valuelist<T, Values...>> {
  static constexpr bool value = sizeof...(Values) == 0;
};

template<typename T, T Head, T... Tail>
struct FrontT<Valuelist<T, Head, Tail...>> {
  using Type = CTValue<T, Head>;
  static constexpr T value = Head;
};

template<typename T, T Head, T... Tail>
  struct PopFrontT<Valuelist<T, Head, Tail...>> {
  using Type = Valuelist<T, Tail...>;
};

template<typename T, T... Values, T New>
struct PushFrontT<Valuelist<T, Values...>, CTValue<T, New>> {
  using Type = Valuelist<T, New, Values...>;
};

template<typename T, T... Values, T New>
struct PushBackT<Valuelist<T, Values...>, CTValue<T, New>> {
  using Type = Valuelist<T, Values..., New>;
};
```

* 对 Valuelist 使用于之前的定义 InsertionSort

```cpp
template<typename T, typename U>
struct GreaterThanT;

template<typename T, T First, T Second>
struct GreaterThanT<CTValue<T, First>, CTValue<T, Second>> {
  static constexpr bool value = First > Second;
};

void valuelisttest()
{
  using Integers = Valuelist<int, 6, 2, 4, 9, 5, 2, 1, 7>;
  using SortedIntegers = InsertionSort<Integers, GreaterThanT>;
  static_assert(std::is_same_v<SortedIntegers,
    Valuelist<int, 9, 7, 6, 5, 4, 2, 2, 1>>, "insertion sort failed");
}
```

* 另外可以提供使用字面值操作符初始化 CTValue 的能力

```cpp
auto a = 42_c; // a 为 CTValue<int, 42>
auto b = 0x815_c; // b 为 CTValue<int, 2069>
auto c = 0b1111'1111_c; // c 为 CTValue<int, 255>
```

* 实现如下

```cpp
#include <cassert>
#include <cstddef>

// convert single char to corresponding int value at compile time:
constexpr int toInt(char c) {
  // hexadecimal letters:
  if (c >= 'A' && c <= 'F')
  {
    return static_cast<int>(c) - static_cast<int>('A') + 10;
  }
  if (c >= 'a' && c <= 'f')
  {
    return static_cast<int>(c) - static_cast<int>('a') + 10;
  }
  // other (disable '.' for floating-point literals):
  assert(c >= '0' && c <= '9');
  return static_cast<int>(c) - static_cast<int>('0');
}

// parse array of chars to corresponding int value at compile time:
template<std::size_t N>
constexpr int parseInt(const char (&arr)[N]) {
  int base = 10; // to handle base (default: decimal)
  int offset = 0; // to skip prefixes like 0x
  if (N > 2 && arr[0] == '0')
  {
    switch (arr[1]) {
      case 'x': //prefix 0x or 0X, so hexadecimal
      case 'X':
        base = 16;
        offset = 2;
        break;
      case 'b': //prefix 0b or 0B (since C++14), so binary
      case 'B':
        base = 2;
        offset = 2;
        break;
      default: // prefix 0, so octal
        base = 8;
        offset = 1;
        break;
    }
  }
  // iterate over all digits and compute resulting value:
  int value = 0;
  int multiplier = 1;
  for (std::size_t i = 0; i < N - offset; ++i)
  {
    if (arr[N-1-i] != '\'')
    { // ignore separating single quotes (e.g. in 1'000)
      value += toInt(arr[N-1-i]) * multiplier;
      multiplier *= base;
    }
  }
  return value;
}

// literal operator: parse integral literals with suffix _c as sequence of chars:
template<char... cs>
constexpr auto operator"" _c()
{
  return CTValue<int, parseInt<sizeof...(cs)>({cs...})>{};
}
```

* 注意对于浮点字面值，断言会引发一个编译期错误，因为它是一个运行期特性

### 可推断的非类型参数

* C++17 中，CTValue 能通过使用单个可推断的非类型参数改进

```cpp
template<auto Value>
struct CTValue {
  static constexpr auto value = Value;
};
```

* 这就省去了对每个 CTValue 的使用指定特定类型的需要

```cpp
using Primes = Typelist<CTValue<2>, CTValue<3>,
  CTValue<5>, CTValue<7>, CTValue<11>>;
```

* 这也能用于 C++17 的 Valuelist，结果不一定更好。一个带可推断类型的非类型参数包允许每个实参类型不同，不同于之前的 Valuelist 要求所有元素有相同类型，这样混杂的值列表可能是有用的，

```cpp
template<auto... Values>
class Valuelist {};
int x;
using MyValueList = Valuelist<1,'a', true, &x>;
```

## 使用包扩展来优化算法

* Transform 算法可以很自然地使用包扩展，因为它对每个列表中的元素进行了相同的操作

```cpp
// recursive case:
template<typename... Elements,
  template<typename T> class MetaFun>
class TransformT<Typelist<Elements...>, MetaFun, false> {
 public:
  using Type = Typelist<typename MetaFun<Elements>::Type...>;
};

// 之前的写法
template<typename List,
  template<typename T> class MetaFun>
class TransformT<List, MetaFun, false>
: public PushFrontT<
  typename TransformT<PopFront<List>, MetaFun>::Type,
  typename MetaFun<Front<List>>::Type>
{};
```

* 这种实现更简单，不需要递归，并且十分直接地使用语言特性。此外，它需要更少的模板实例化，因为只有一个 Transform 模板需要实例化。算法仍然要求线性数量的 MeraFun 实例化，但那些实例化对算法来说是基本的
* 其他算法可以间接受益于使用包扩展。比如 Reverse 算法需要一个线性数量的 PushBack 的实例化，对 PushBack 使用包扩展则 Reverse 是线性的，但 Reverse 的递归实现本身是线性实例化的，因此 Reverse 是二次的（quadratic）
* 包扩展也可以用于选择给定索引列表中的元素以生成新的 Typelist。下面的 Selecty 元函数使用一个 Typelist 和一个包含该 Typelist 索引的 Valuelist，生成一个新的 Typelist

```cpp
template<typename T, auto... Values>
class Valuelist {};

template<typename Types, typename Indices>
class SelectT;

template<typename Types, unsigned... Indices>
class SelectT<Types, Valuelist<unsigned, Indices...>> {
 public:
  using Type = Typelist<NthElement<Types, Indices>...>;
};

template<typename Types, typename Indices>
using Select = typename SelectT<Types, Indices>::Type;
```

* 下面使用 Select 翻转 Typelist

```cpp
using SignedIntegralTypes =
  Typelist<signed char, short, int, long, long long>;

using ReversedSignedIntegralTypes =
  Select<SignedIntegralTypes, Valuelist<unsigned, 4, 3, 2, 1, 0>>;
  // 生成Typelist<long long, long, int, short, signed char>
```

* 包含另一个 Typelist 索引的非类型 Typelist 通常称为 index list 或 index sequence

## Cons（LISP 的核心数据结构）风格的 Typelist

* 引入变参模板之前，常根据 LISP 的 cons 建模的数据结构来表达 Typelist，每个 cons 单元包含一个值（列表的头）和一个嵌套列表（另一个 cons 或空列表 nil）

```cpp
class Nil {};
template<typename HeadT, typename TailT = Nil>
class Cons {
 public:
  using Head = HeadT;
  using Tail = TailT;
};
```

* 空的 Typelist 写为 `Nil`，包含单个 int 元素的 Typelist 写为 `Cons<int, Nil>` 或 `Cons<int>`，更长的列表需要嵌套

```cpp
using TwoShort = Cons<short, Cons<unsigned short>>;
```

* 递归嵌套可以构造任意长的 Typelist，尽管手写这样的长列表十分笨拙

```cpp
using SignedIntegralTypes =
  Cons<signed char,
    Cons<short,
      Cons<int,
        Cons<long,
          Cons<long long, Nil>>>>>;
```

* 实现提取首元素的 Front

```cpp
template<typename List>
class FrontT {
 public:
  using Type = typename List::Head;
};

template<typename List>
using Front = typename FrontT<List>::Type;
```

* 实现 PushFront

```cpp
template<typename List, typename Element>
class PushFrontT {
 public:
  using Type = Cons<Element, List>;
};

template<typename List, typename Element>
using PushFront = typename PushFrontT<List, Element>::Type;
```

* 实现 PopFront

```cpp
template<typename List>
class PopFrontT {
 public:
  using Type = typename List::Tail;  
};

template<typename List>
using PopFront = typename PopFrontT<List>::Type;
```

* 实现对 Nil 的 IsEmpty 特化

```cpp
template<typename List>
struct IsEmpty {
  static constexpr bool value = false;
};

template<>
struct IsEmpty<Nil> {
  static constexpr bool value = true;
};
```

* 为 cons 风格的 Typelist 提供了这些操作后，即可对其使用之前的 InsertionSort 算法

```cpp
template<typename T, typename U>
struct SmallerThanT {
  static constexpr bool value = sizeof(T) < sizeof(U);
};

void conslisttest()
{
  using ConsList = Cons<int, Cons<char, Cons<short, Cons<double>>>>;
  using SortedTypes = InsertionSort<ConsList, SmallerThanT>;
  using Expected = Cons<char, Cons<short, Cons<int, Cons<double>>>>;
  std::cout << std::is_same<SortedTypes, Expected>::value; // true
}
```
