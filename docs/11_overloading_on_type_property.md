* 基于模板实参属性重载函数模板是很自然的需求，比如

```cpp
template<typename Number> void f(Number); // only for numbers
template<typename Container> void f(Container); // only for containers
```

* 然而，C++ 现在还不提供任何直接表达基于类型属性重载的方法，两个 f 其实是相同的函数模板声明。幸运的是，有许多技术可以用于替代基于类型属性的重载

## 算法特化

* 函数模板重载的一个常见动机是提供更多特化版本的算法

```cpp
template<typename T>
void swap(T& x, T& y)
{
  T tmp(x);
  x = y;
  y = tmp;
}
```

* 这个实现涉及三个拷贝操作。对一些类型可以提供更高效的操作，为一个通用算法引入更专用（specialized）的变体称为算法特化

```cpp
template<typename T>
void swap(Array<T>& x, Array<T>& y)
{
  swap(x.ptr, y.ptr);
  swap(x.len, y.len);
}
```

* 算法特化实现的关键是更专用的变体的属性被自动选择，甚至不需要调用者意识到变体的存在。但不是所有概念上更专用的变体都可以像上面这样重载，比如基于迭代器类型实现不同的算法

```cpp
template<typename InputIterator, typename Distance>
void advanceIter(InputIterator& x, Distance n)
{
  while (n > 0)
  { // 线性时间
    ++x;
    --n;
  }
}

template<typename RandomAccessIterator, typename Distance>
void advanceIter(RandomAccessIterator& x, Distance n) // 错误：重定义
{
  x += n; // 常数时间
}
```

* 这两个模板本质上是相同的，为此需要引入一个用于区分的标签

## 标签分派（Tag Dispatching）

```cpp
template<typename Iterator, typename Distance>
void advanceIterImpl(Iterator& x, Distance n, std::input_iterator_tag)
{
  while (n > 0)
  { // 线性时间
    ++x;
    --n;
  }
}

template<typename Iterator, typename Distance>
void advanceIterImpl(Iterator& x, Distance n, std::random_access_iterator_tag)
{
  x += n; // 常数时间
}

template<typename Iterator, typename Distance>
void advanceIter(Iterator& x, Distance n)
{
  advanceIterImpl(x, n,
    typename std::iterator_traits<Iterator>::iterator_category());
}
```

* [std::iterator_traits](https://en.cppreference.com/w/cpp/iterator/iterator_traits) 的成员类型 iterator_category 提供了迭代器类别，标准库中的 [iterator_tags](https://en.cppreference.com/w/cpp/iterator/iterator_tags) 定义如下

```cpp
namespace std {
struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};
}
```

* 两个 advanceIterImpl 变体分别标签为 std::input_iterator_tag 和 std::random_access_iterator_tag，后者派生自前者，因此对随机访问迭代器调用 advanceIterImpl 时就会选择更专用的使用后者的算法变体
* 标签分派适合有自然分层结构的属性，当算法特化依赖于特定类型属性（如 T 是否有一个平凡的拷贝赋值运算符）时，则需要更强力的技术

## 启用/禁用函数模板

* 标准库提供了 [std::enable_if](https://en.cppreference.com/w/cpp/types/enable_if) 用于启用或禁用特定条件下的特定函数模板，这里自定义一个简单实现

```cpp
template<bool, typename T = void>
struct EnableIfT {};

template<typename T>
struct EnableIfT<true, T> {
  using Type = T;
};

template<bool Cond, typename T = void>
using EnableIf = typename EnableIfT<Cond, T>::Type;
```

* 现在 advanceIter 算法实现如下

```cpp
template<typename Iterator>
constexpr bool IsRandomAccessIterator =
  IsConvertible<
    typename std::iterator_traits<Iterator>::iterator_category,
    std::random_access_iterator_tag>;

template<typename Iterator, typename Distance>
EnableIf<IsRandomAccessIterator<Iterator>>
advanceIter(Iterator& x, Distance n)
{
  x += n; // 常数时间
}

template<typename Iterator, typename Distance>
EnableIf<!IsRandomAccessIterator<Iterator>> // 必须指定此条件，否则两个模板一样特殊
advanceIter(Iterator& x, Distance n)
{
  while (n > 0)
  { // 线性时间
    ++x;
    --n;
  }
}
```

*  现在想实现负距离往回移动，这需要稍微复杂一些的逻辑，每个函数模板需要指定一个与其他变体互斥的条件

```cpp
// 随机访问迭代器
template<typename Iterator, typename Distance>
EnableIf<IsRandomAccessIterator<Iterator>>
advanceIter(Iterator& x, Distance n)
{
  x += n; // 常数时间
}

template<typename Iterator>
constexpr bool IsBidirectionalIterator =
  IsConvertible<
  typename std::iterator_traits<Iterator>::iterator_category,
  std::bidirectional_iterator_tag>;

// 双向迭代器且非随机访问迭代器
template<typename Iterator, typename Distance>
EnableIf<IsBidirectionalIterator<Iterator> &&
  !IsRandomAccessIterator<Iterator>>
advanceIter(Iterator& x, Distance n)
{
  if (n > 0)
  {
    for (; n > 0; ++x, --n) {} // 线性时间
  }
  else
  {
    for (; n < 0; --x, ++n) {} // 线性时间
  }
}

// 其他迭代器（输入迭代器且非双向迭代器）
template<typename Iterator, typename Distance>
EnableIf<!IsBidirectionalIterator<Iterator>>
advanceIter(Iterator& x, Distance n) {
  if (n < 0)
  {
    throw "advanceIter(): invalid iterator category for negative n";
  }
  while (n > 0)
  { // 线性时间
    ++x;
    --n;
  }
}
```

* EnableIf 常用于函数模板的返回类型，这会降低代码可读性，并且不能用于没有返回类型的构造函数模板或转换函数模板，为此可以把 EnableIf 写为一个默认模板实参

```cpp
template<typename Iterator>
constexpr bool IsInputIterator =
  IsConvertible<
    typename std::iterator_traits<Iterator>::iterator_category,
    std::input_iterator_tag>;

template<typename T>
class Container {
 public:
  // construct from an input iterator sequence:
  template<typename Iterator, typename = EnableIf<IsInputIterator<Iterator>>>
  Container(Iterator first, Iterator last);

  // convert to a container so long as the value types are convertible:
  template<typename U, typename = EnableIf<IsConvertible<T, U>>>
  operator Container<U>() const;
};
```

* 但使用 EnableIf 作为默认实参时，添加重载将导致错误，因为确定两个模板是否等价时不会考虑默认模板实参

```cpp
template<typename Iterator,
  typename = EnableIf<IsInputIterator<Iterator> &&
  !IsRandomAccessIterator<Iterator>>>
Container(Iterator first, Iterator last);

template<typename Iterator,
  typename = EnableIf<IsRandomAccessIterator<Iterator>>>
Container(Iterator first, Iterator last); // 错误：重复声明
```

* 为此可以添加一个额外的模板参数，以使两个模板有不同数量的模板参数

```cpp
// construct from an input iterator sequence:
template<typename Iterator,
  typename = EnableIf<IsInputIterator<Iterator> &&
  !IsRandomAccessIterator<Iterator>>>
Container(Iterator first, Iterator last);

template<typename Iterator,
  typename = EnableIf<IsRandomAccessIterator<Iterator>>,
  typename = int> // extra dummy parameter to enable both constructors
Container(Iterator first, Iterator last); // OK now
```

### if constexpr

* C++17 的 if constexpr 在许多情况下避免了对 EnableIf 的需求

```cpp
template<typename Iterator, typename Distance>
void advanceIter(Iterator& x, Distance n) {
  if constexpr (IsRandomAccessIterator<Iterator>)
  {
    x += n;
  }
  else if constexpr (IsBidirectionalIterator<Iterator>)
  {
    if (n > 0)
    {
      for (; n > 0; ++x, --n) {}
    }
    else
    {
      for (; n < 0; --x, ++n) {}
    }
  }
  else
  {
    if (n < 0)
    {
      throw "advanceIter(): invalid iterator category for negative n";
    }
    while (n > 0)
    {
      ++x;
      --n;
    }
  }
}
```

* 但 if constexpr 要求通用的组件能在函数模板体内完整表达。对于一些情况仍需要 EnableIf
  * 调用了不同的接口
  * 需要不同的类定义
  * 有效的实例化不应该存在于某个模板实参列表
* 如下实现最后一种情况看起来没问题，但它不支持 SFINAE，替换失败时模板不被丢弃可能导致抑制另一个重载版本，而使用 EnableIf 替换失败时则会移除模板

```cpp
template<typename T>
void f(T p)
{
  if constexpr (condition<T>::value)
  {
    // do something here...
  }
  else
  {
    // not a T for which f() makes sense:
    static_assert(condition<T>::value, "can't call f() for such a T");
  }
}
```

### [Concepts](https://en.cppreference.com/w/cpp/concepts)

* 以上技术比较笨拙，使用了大量编译器资源，出错时将造成复杂的诊断信息，C++20 可以使用更方便的 concepts

```cpp
template<typename T>
class Container {
 public:
  // construct from an input iterator sequence:
  template<typename Iterator>
    requires IsInputIterator<Iterator>
  Container(Iterator first, Iterator last);

  // construct from a random access iterator sequence:
  template<typename Iterator>
    requires IsRandomAccessIterator<Iterator>
  Container(Iterator first, Iterator last);

  // convert to a container so long as the value types are convertible:
  template<typename U>
    requires IsConvertible<T, U>
  operator Container<U>() const;
};
```

* requires 子句描述对模板的限制条件，相当于 EnableIf 的更直接表达，且带有顺序而不再需要标签分派，此外还能用于非模板

```cpp
template<typename T>
class Container {
 public:
  requires HasLess<T>
  void sort()
  {
    ...
  }
};
```

## 类特化

* 类模板偏特化就像为函数模板使用重载一样。对于如下的基于 [std::vector](https://en.cppreference.com/w/cpp/container/vector) 实现的类模板，如果 K 支持 `operator<` 就可以提供一个基于 [std::map](https://en.cppreference.com/w/cpp/container/map) 的更高效的实现，类似地，如果 K 支持哈希操作就可以提供基于 [std::unordered_map](https://en.cppreference.com/w/cpp/container/unordered_map) 的更高效的实现

```cpp
template<typename K, typename V>
class Dictionary {
 public:
  // subscripted access to the data:
  value& operator[](const K& k)
  {
    // search for the element with this k:
    for (auto& element : data)
    {
      if (element.first == k)
      {
        return element.second;
      }
    }
    // there is no element with this k; add one
    data.push_back(std::pair<const K, V>(k, V{}));
    return data.back().second;
  }
  ...
 private:
  std::vector<std::pair<const K, V>> data;
};
```

### 启用/禁用类模板

* 启用/禁用类模板的不同实现的方法就是启用/禁用类模板的偏特化。为了对类模板偏特化使用 EnableIf，先引入一个未命名的默认模板参数

```cpp
template<typename K, typename V, typename = void>
class Dictionary {
  ... // std::vector implementation as above
};
```

* 这个新模板参数相当于 EnableIf 的锚

```cpp
template<typename K, typename V>
class Dictionary<K, V, EnableIf<HasLess<K>>> {
 public:
  value& operator[](const K& k)
  {
    return data[k];
  }
  ...
 private:
  std::map<K, V> data;
};
```

* 类模板偏特化优先于原始模板，所以不需要像函数模板那样为原始模板禁用条件，但偏特化之间的条件必须互斥

```cpp
template<typename K, typename V, typename = void>
class Dictionary {
  ... // std::vector implementation as above
};

template<typename K, typename V>
class Dictionary<K, V, EnableIf<HasLess<K> && !HasHash<K>>> {
  ... // std::map implementation as above
};

template typename K, typename V>
class Dictionary K, V, EnableIf<HasHash<K>>> {
 public:
  value& operator[](const K& k)
  {
    return data[k];
  }
  ...
 private:
  unordered_std::map K, V> data;
};
```

### 用于类模板的标签分派

* 标签分派也能用于选择类模板偏特化

```cpp
// primary template (intentionally undefined):
template<typename Iterator,
  typename Tag =
    BestMatchInSet<
      typename std::iterator_traits<Iterator>::iterator_category,
      std::input_iterator_tag,
      std::bidirectional_iterator_tag,
      std::random_access_iterator_tag>>
class Advance;

// general, linear-time implementation for input iterators:
template<typename Iterator>
class Advance<Iterator, std::input_iterator_tag> {
 public:
  using DifferenceType =
    typename std::iterator_traits<Iterator>::difference_type;
  
  void operator()(Iterator& x, DifferenceType n) const
  {
    while (n > 0)
    {
      ++x;
      --n;
    }
  }
};

// bidirectional, linear-time algorithm for bidirectional iterators:
template<typename Iterator>
class Advance<Iterator, std::bidirectional_iterator_tag> {
 public:
  using DifferenceType =
    typename std::iterator_traits<Iterator>::difference_type;

  void operator()(Iterator& x, DifferenceType n) const
  {
    if (n > 0)
    {
      for (; n > 0; ++x, --n) {}
    }
    else
    {
      for (; n < 0; --x, ++n) {}
    }
  }
};

// bidirectional, constant-time algorithm for random access iterators:
template<typename Iterator>
class Advance<Iterator, std::random_access_iterator_tag> {
 public:
  using DifferenceType =
    typename std::iterator_traits<Iterator>::difference_type;
  
  void operator()(Iterator& x, DifferenceType n) const
  {
    x += n;
  }
};
```

* BestMatchInSet 的行为相当于对一个迭代器类型标签，在如下重载中进行选择，再给出参数类型

```cpp
void f(std::input_iterator_tag);
void f(std::bidirectional_iterator_tag);
void f(std::random_access_iterator_tag);
```

* 因此 BestMatchInSet 的实现方法就是借助重载解析

```cpp
// construct a set of f() overloads for the types in Types...:
template<typename... Types>
struct A;

// basis case: nothing matched:
template<>
struct A<> {
  static void f(...); // 如果下面对所有参数尝试匹配失败，则匹配此版本
};

// recursive case: introduce a new f() overload:
template<typename T1, typename... Rest>
struct A<T1, Rest...> : public A<Rest...> { // 递归继承
  static T1 f(T1); // 尝试匹配第一个参数，如果匹配则返回类型 T1 就是所需 Type
  using A<Rest...>::f; // 否则递归调用自身基类，尝试匹配下一个参数
};

// find the best f for T in Types...:
template<typename T, typename... Types>
struct BestMatchInSetT { // f 的返回类型就是所需要的 Type
  using Type = decltype(A<Types...>::f(std::declval<T>()));
};

template<typename T, typename... Types>
using BestMatchInSet = typename BestMatchInSetT<T, Types...>::Type;
```

## 实例化安全的（instantiation-safe）模板

* EnableIf 的本质是仅当模板实参满足某些条件时才启用模板，把模板在模板实参上执行的每个操作写进 EnableIf，模板实例化将永远不会失败，这样的模板称为实例化安全的模板
* 如下模板要求类型T有一个 `operator<` 操作符来比较两个T值，然后把比较结果转为布尔值

```cpp
template<typename T>
const T& min(const T& x, const T& y)
{
  if (y < x)
  {
    return y;
  }
  return x;
}
```

* 为了使上面的模板实例化安全，先实现一个检查 `operator<` 的 traits

```cpp
template<typename T1, typename T2>
class HasLess {
  template<typename T> struct Identity;
  template<typename U1, typename U2>
  static std::true_type
    test(Identity<decltype(std::declval<U1>() < std::declval<U2>())>*);
  
  template<typename U1, typename U2>
  static std::false_type test(...);
 public:
  static constexpr bool value = decltype(test<T1, T2>(nullptr))::value;
};

template<typename T1, typename T2, bool HasLess>
class LessResultImpl {
 public:
  using Type = decltype(std::declval<T1>() < std::declval<T2>());
};

template<typename T1, typename T2>
class LessResultImpl<T1, T2, false> {};

template<typename T1, typename T2>
class LessResultT
: public LessResultImpl<T1, T2, HasLess<T1, T2>::value> {};

template<typename T1, typename T2>
using LessResult = typename LessResultT<T1, T2>::Type;
```

* 再结合 [std::is_convertible](https://en.cppreference.com/w/cpp/types/is_convertible) 即可使得模板实例化安全

```cpp
template<typename T>
std::enable_if_t<std::is_convertible_v<LessResult<const T&, const T&>, bool>, const T&>
min(const T& x, const T& y)
{
  if (y < x)
  {
    return y;
  }
  return x;
}

struct X1 {};
bool operator< (const X1&, const X1&) { return true; }

struct X2 {};
bool operator<(X2, X2) { return true; }

struct X3 {};
bool operator<(X3&, X3&) { return true; }

struct X4 {};

struct BoolConvertible {
  operator bool() const { return true; } // implicit conversion to bool
};
struct X5 {};
BoolConvertible operator<(const X5&, const X5&)
{
  return BoolConvertible();
}

struct NotBoolConvertible {}; // no conversion to bool
struct X6 {};
NotBoolConvertible operator<(const X6&, const X6&)
{
  return NotBoolConvertible();
}

struct BoolLike { // BoolLike 不能隐式转换为 bool
  explicit operator bool() const { return true; } // explicit conversion to bool
};
struct X7 {};
BoolLike operator<(const X7&, const X7&) { return BoolLike(); }

int main()
{ // 下面的 ERROR 源于实例化被拒绝，没有可匹配的重载
  min(X1(), X1()); // X1 can be passed to min()
  min(X2(), X2()); // X2 can be passed to min()
  min(X3(), X3()); // ERROR: X3 cannot be passed to min()
  min(X4(), X4()); // ERROR: X4 can be passed to min()
  min(X5(), X5()); // X5 can be passed to min()
  min(X6(), X6()); // ERROR: X6 cannot be passed to min()
  min(X7(), X7()); // UNEXPECTED ERROR: X7 cannot be passed to min()
}
```

* X7 传给实例化安全的 min 失败，因为 BoolLike 不能隐式转换为 bool。如果想根据上下文决定 T 能否转换为 bool，则需要定义一个 traits

```cpp
template<typename T>
class IsContextualBoolT {
 private:
  template<typename T> struct Identity;
  template<typename U>
  static std::true_type
    test(Identity<decltype(std::declval<U>()? 0 : 1)>*);
  
  template<typename U>
  static std::false_type test(...);
 public:
  static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template<typename T>
constexpr bool IsContextualBool = IsContextualBoolT<T>::value;

template<typename T>
std::enable_if_t<IsContextualBool<LessResult<const T&, const T&>>, const T&>
min(const T& x, const T& y)
{
  if (y < x)
  {
    return y;
  }
  return x;
}

min(X7(), X7()); // OK
```
