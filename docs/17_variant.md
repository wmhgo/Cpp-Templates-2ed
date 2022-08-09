* 这里开发一个类似于 [std::variant](https://en.cppreference.com/w/cpp/utility/variant) 的类模板 Variant

```cpp
#include <cassert>
#include <string>

#include "variant.hpp"

int main() {
  Variant<int, double, std::string> v(42);
  if (v.is<int>()) {
    assert(v.get<int>() == 42);
  }
  v = "hello";
  assert(v.get<std::string>() == "hello");
}
```

## 存储

* Variant 的首要设计是管理活动值的存储，不同类型可能有不同的大小和对齐要考虑，一个简单（尽管低效）的存储机制是直接使用一个 tuple。此外还需要存储一个discriminator 作为 tuple 的索引

```cpp
template <typename... Types>
class Variant {
 public:
  Tuple<Types...> storage;
  unsigned char discriminator;
};
```

* 但这种做法导致 Variant 需要所有类型大小总和的空间，更好的做法是重叠每个可能类型的空间，这可以通过对其递归拆分为首部和尾部来实现

```cpp
template <typename... Types>
union VariantStorage;

template <typename Head, typename... Tail>
union VariantStorage<Head, Tail...> {
  Head head;
  VariantStorage<Tail...> tail;
};

template <>
union VariantStorage<> {};
```

* union 保证足够的大小和对齐来存储任意类型，但大多用来实现 Variant 的技术将使用继承，而 union 不支持继承。取代做法是用字符数组来存储，通过一个计算最大类型的 traits 得出数组大小，通过 [alignas](https://en.cppreference.com/w/cpp/language/alignas) 对类型做对齐

```cpp
#include <new>  // for std::launder()

template <typename... Types>
class VariantStorage {
 public:
  unsigned char getDiscriminator() const { return discriminator; }
  void setDiscriminator(unsigned char d) { discriminator = d; }
  void* getRawBuffer() { return buffer; }
  const void* getRawBuffer() const { return buffer; }

  template <typename T>
  T* getBufferAs() {
    return std::launder(reinterpret_cast<T*>(buffer));
  }

  template <typename T>
  const T* getBufferAs() const {
    return std::launder(reinterpret_cast<const T*>(buffer));
  }

 private:
  using LargestT = LargestType<Typelist<Types...>>;
  alignas(Types...) unsigned char buffer[sizeof(LargestT)];
  unsigned char discriminator = 0;
};
```

## 设计

* 接着设计 Variant 类型本身。和 Tuple 一样，使用继承对每个 Types 中的类型提供行为。不同于 Tuple 的是，这些基类没有存储。相反，每个基类使用 CRTP 通过最派生的类型访问共享的 Variant 存储。定义如下VariantChoice 类模板，它提供活动值为类型 T 时字符数组上所需的核心操作

```cpp
template<typename T, typename... Types>
class VariantChoice {
  using Derived = Variant<Types...>;
  Derived& getDerived() { return *static_cast<Derived*>(this); }
  const Derived& getDerived() const
  {
    return *static_cast<const Derived*>(this);
  }
 protected:
  constexpr static unsigned Discriminator =
    FindIndexOfT<Typelist<Types...>, T>::value + 1;
 public:
  VariantChoice() {}
  // see variantchoiceinit.hpp
  VariantChoice(const T& value);
  VariantChoice(T&& value);

  bool destroy(); // see variantchoicedestroy.hpp
  // see variantchoiceassign.hpp
  Derived& operator=(const T& value);
  Derived& operator=(T&& value);
};
```

* 元函数 FindIndexOfT 用来找出 Types 中类型T的位置，实现如下

```cpp
template<typename List, typename T, unsigned N = 0,
  bool Empty = IsEmpty<List>::value>
struct FindIndexOfT;

// recursive case:
template<typename List, typename T, unsigned N>
struct FindIndexOfT<List, T, N, false>
: public IfThenElse<std::is_same<Front<List>, T>::value,
  std::integral_constant<unsigned, N>,
  FindIndexOfT<PopFront<List>, T, N+1>>
{};

// basis case:
template<typename List, typename T, unsigned N>
struct FindIndexOfT<List, T, N, true>
{};
```

* Variant 继承于 VariantStorage 和 VariantChoice

```cpp
template<typename... Types>
class Variant : private VariantStorage<Types...>,
  private VariantChoice<Types, Types...>...
{
  template<typename T, typename... OtherTypes>
  friend class VariantChoice; // enable CRTP
  ...
};
```

* 对于一个

```cpp
Variant<int, double, std::string>
```

* 生成下列 VariantChoice 基类

```cpp
VariantChoice<int, int, double, std::string>,
VariantChoice<double, int, double, std::string>,
VariantChoice<std::string, int, double, std::string>
```

* 这三个基类对应的 discriminator 值将为1、2、3。当 Variant 的存储成员 discriminator 匹配一个特定基类的 VariantChoice::Discriminator 时，基类负责管理活动值
* discriminator 值为 0 被保留用于 Variant 不包含值的情况，这是一种奇态（odd state），只有在分配期间抛出异常时才能观察到
* Variant 完整定义如下

```cpp
template<typename... Types>
class Variant : private VariantStorage<Types...>,
  private VariantChoice<Types, Types...>...
{
  template<typename T, typename... OtherTypes>
  friend class VariantChoice;
 public:
  template<typename T> bool is() const; // see variantis.hpp

  // see variantget.hpp
  template<typename T> T& get() &;
  template<typename T> const T& get() const&;
  template<typename T> T&& get() &&;

  // see variantvisit.hpp:
  template<typename R = ComputedResultType, typename Visitor>
  VisitResult<R, Visitor, Types&...> visit(Visitor&& vis) &;

  template<typename R = ComputedResultType, typename Visitor>
  VisitResult<R, Visitor, const Types&...> visit(Visitor&& vis) const&;

  template<typename R = ComputedResultType, typename Visitor>
  VisitResult<R, Visitor, Types&&...> visit(Visitor&& vis) &&;

  using VariantChoice<Types, Types...>::VariantChoice...;

  Variant(); // see variantdefaultctor.hpp
  Variant(const Variant& source); // see variantcopyctor.hpp
  Variant(Variant&& source); // see variantmovector.hpp

  template<typename... SourceTypes>
  Variant(const Variant<SourceTypes...>& source); // see variantcopyctortmpl.hpp

  template<typename... SourceTypes>
  Variant(Variant<SourceTypes...>&& source); // see variantmovectortmpl.hpp

  using VariantChoice<Types, Types...>::operator=...;

  Variant& operator= (const Variant& source); // see variantcopyassign.hpp
  Variant& operator= (Variant&& source); // see variantmoveassign.hpp

  template<typename... SourceTypes>
  Variant& operator= (const Variant<SourceTypes...>& source); // see variantcopyassigntmpl.hpp

  template<typename... SourceTypes>
  Variant& operator= (Variant<SourceTypes...>&& source); // see variantmoveassigntmpl.hpp

  bool empty() const; // see variantempty.hpp

  ~Variant() { destroy(); }
  void destroy(); // see variantdestroy.hpp
};
```

## 查询

* is() 成员函数定义如下，用于确定 Variant 当前是否存储一个T类型值

```cpp
// variant/variantis.hpp

template<typename... Types>
  template<typename T>
bool Variant<Types...>::is() const
{
  return this->getDiscriminator() ==
    VariantChoice<T, Types...>::Discriminator;
}

// v.is<int>()将确定v的活动值是否为int
```

* 如果查找的类型未在列表中在找到，FindIndexOfT 将不会包含一个 value 成员，于是 VariantChoice 基类将实例化失败，从而造成 is() 的编译错误，这样就可以防止用户请求无法存储的类型

## 提取

* get() 成员函数通过接受一个类型，提取一个该类型存储值的引用，并且仅当 Variant 的活动值是该类型时才有效。当 Variant 未保存一个值（即 discriminator 为 0）时，get() 抛出一个 EmptyVariant 异常

```cpp
// variant/variantget.hpp

#include <exception>
class EmptyVariant : public std::exception {};

template<typename... Types>
  template<typename T>
T& Variant<Types...>::get() &
{
  if (empty()) throw EmptyVariant();
  assert(is<T>());
  return *this->template getBufferAs<T>();
}
```

## 初始化

* 从一个类型的值初始化一个 Variant，通过一个 VariantChoice 构造函数实现，这个构造函数接受一个 T 类型的值

```cpp
// variant/variantchoiceinit.hpp

template<typename T, typename... Types>
VariantChoice<T, Types...>::VariantChoice(const T& value)
{
  // getDerived()使用CRTP来操作派生类
  new(getDerived().getRawBuffer()) T(value); // 用 placement new 将 value 置入 buffer
  // 设置派生类的 discriminator 为基类的 Discriminator 来表示存储的类型
  getDerived().setDiscriminator(Discriminator);
}

template<typename T, typename... Types>
VariantChoice<T, Types...>::VariantChoice(T&& value)
{
  new(getDerived().getRawBuffer()) T(std::move(value));
  getDerived().setDiscriminator(Discriminator);
}
```

* 最终的目标是能从一个任意类型的值初始化 Variant，甚至可以隐式转换

```cpp
Variant<int, double, string> v("hello"); // 隐式转换为 string
```

* 为此使用 using 声明把 VariantChoice 的构造函数引入到 Variant 中

```cpp
using VariantChoice<Types, Types...>::VariantChoice...;
```

* 这个 using 声明为 Types 中的每个类型生成一个 Variant 的构造函数

```cpp
// Variant<int, double, string> 的构造函数实际是
Variant(const int&);
Variant(int&&);
Variant(const double&);
Variant(double&&);
Variant(const string&);
Variant(string&&);
```

## 析构

* 当 Variant 初始化时，一个值构造到它的 buffer 中，destroy 处理那个值的析构

```cpp
// variant/variantchoicedestroy.hpp

template<typename T, typename... Types>
bool VariantChoice<T, Types...>::destroy()
{
  if (getDerived().getDiscriminator() == Discriminator)
  {
    // 如果匹配则调用 placement delete
    getDerived().template getBufferAs<T>()->~T();
    return true;
  }
  return false;
}
```

* 只有当 discriminator 匹配时，VariantChoice::destroy() 操作是有用的。但通常希望不考虑当前活动的类型，直接销毁存储在 Variant 中的值，因此 Variant::destroy() 将调用基类中所有的 VariantChoice::destroy()

```cpp
// variant/variantdestroy.hpp

template<typename... Types>
void Variant<Types...>::destroy()
{
  // 调用所有的 VariantChoice::destroy()，至多一个成功
  bool results[] = { VariantChoice<Types, Types...>::destroy()...};
  this->setDiscriminator(0); // 表示不再存储一个值，Variant 为空
}
```

* 数组 results 的作用是提供一个能使用初始化列表的上下文，C++17 中可以使用折叠表达式来消除对数组 results 的需要

```cpp
// variant/variantdestroy17.hpp

template<typename... Types>
void Variant<Types...>::destroy()
{
  (VariantChoice<Types, Types...>::destroy(), ...);
  this->setDiscriminator(0);
}
```

## 赋值

* 对于同类型直接赋值，对于不同类型则需要析构现有值，再用 placement new 重新初始化新类型的值

```cpp
// variant/variantchoiceassign.hpp

template<typename T, typename... Types>
auto VariantChoice<T, Types...>::operator=(const T& value) -> Derived&
{
  if (getDerived().getDiscriminator() == Discriminator) // 同类型的值
  {
    *getDerived().template getBufferAs<T>() = value;
  }
  else // 不同类型的值
  {
    getDerived().destroy(); // 用 Variant::destroy() 析构现有值
    new(getDerived().getRawBuffer()) T(value); // 用 placement new 初始化 T 类型新值
    getDerived().setDiscriminator(Discriminator);
  }
  return getDerived();
}

template<typename T, typename... Types>
auto VariantChoice<T, Types...>::operator=(T&& value) -> Derived&
{
  if (getDerived().getDiscriminator() == Discriminator) // 同类型的值
  {
    *getDerived().template getBufferAs<T>() = std::move(value);
  }
  else // 不同类型的值
  {
    getDerived().destroy();
    new(getDerived().getRawBuffer()) T(std::move(value));
    getDerived().setDiscriminator(Discriminator);
  }
  return getDerived();
}
```

* 和初始化一样，可以在 Variant 中使用 using 声明来继承赋值操作

```cpp
using VariantChoice<Types, Types...>::operator=...;
```

* 对于不同类型，赋值有两步，先析构再初始化，这将有三个需要考虑的问题：自赋值、异常、[std::launder](https://en.cppreference.com/w/cpp/utility/launder)
* 自赋值发生情况如下。如果使用两步赋值，源值将在拷贝前会被析构，导致内存崩溃。但好在自赋值意味着discriminator匹配，所以会使用同类型赋值，因此不会出问题

```cpp
v = v.get<T>()
```

* 如果现有值的销毁已经完成，但是新值的初始化抛出异常，Variant::destroy() 将 discriminator 值重置为 0，表示 Variant 未存储值

```cpp
// variant/variantexception.cpp

class CopiedNonCopyable : public std::exception {};

class NonCopyable {
 public:
  NonCopyable() = default;

  NonCopyable(const NonCopyable&)
  {
    throw CopiedNonCopyable();
  }

  NonCopyable(NonCopyable&&) = default;

  NonCopyable& operator=(const NonCopyable&)
  {
    throw CopiedNonCopyable();
  }

  NonCopyable& operator=(NonCopyable&&) = default;
};

int main()
{
  Variant<int, NonCopyable> v(42);
  try
  {
    NonCopyable nc;
    v = nc;
  }
  catch (CopiedNonCopyable)
  {
    std::cout << "Copy assignment of NonCopyable failed." << '\n';
    if (!v.is<int>() && !v.is<NonCopyable>())
    {
      std::cout << "Variant has no value.";
    }
  }
}

// 程序输出为
Copy assignment of NonCopyable failed.
Variant has no value.
```

* 访问一个没有值的 variant 将抛出 EmptyVariant 异常，以允许程序从这个异常条件修复
* empty() 成员函数用于检查 Variant 是否为空

```cpp
// variant/variantempty.hpp

template<typename... Types>
bool Variant<Types...>::empty() const
{
  return this->getDiscriminator() == 0;
}
```

* 第三个问题是标准化委员会在 C++17 标准化过程结束时才意识到的一个微妙问题。编译器通常希望产生高性能代码，而提高生成代码性能的主要机制是避免从内存到寄存器的重复的数据复制。为此编译器必须做一些假设，其中一种假设是某些类型的数据在其生命周期内是不可变的，这包括 const 数据、引用（可以初始化，但之后不修改）以及存储在多态对象中的一些 bookkeeping data（用于调度虚拟函数、定位虚拟基类、以及处理 typeid 和 dynamic_cast 操作符）
* 上面两步分配过程的问题是，它以一种编译器可能无法识别的方式悄悄地结束一个对象的生命周期，并在同一位置开始另一个对象的生命周期。因此，编译器可能假定它从 Variant 对象的先前状态获取的值仍然有效，而实际上 placement new 的初始化会使其无效，这就导致编译具有不可变数据成员的 Variant 时偶尔会产生无效结果。这种 bug 通常很难追踪（一部分原因是它们很少发生，另一部分原因是它们在源代码中并不真正可见）
* C++17 中，这个问题的解决方案是使用 [std::launder](https://en.cppreference.com/w/cpp/utility/launder)，它返回实参所表示地址的对象的指针，使得编译器不再假设原有值有效，从而起到修复的效果
* 还有一些方法可以做得更好，比如额外添加一个指向 buffer 的指针成员，并在每次使用 placement new 赋值一个新值时获得已清洗的地址，但这些方法会使代码变得复杂，难以维护

## 访问者（Visitor）

* 检查一个 Variant 中的所有可能类型会造成一个冗长的 if 语句链

```cpp
if (v.is<int>())
{
  std::cout << v.get<int>();
}
else if (v.is<double>())
{
  std::cout << v.get<double>();
}
else
{
  std::cout << v.get<string>();
}
```

* 为此需要引入一个函数模板

```cpp
template<typename V, typename Head, typename... Tail>
void printImpl(const V& v)
{
  if (v.template is<Head>())
  {
    std::cout << v.template get<Head>();
  }
  else if constexpr (sizeof...(Tail) > 0)
  {
    printImpl<V, Tail...>(v);
  }
}

template<typename... Types>
void print(const Variant<Types...>& v)
{
  printImpl<Variant<Types...>, Types...>(v);
}

int main()
{
  Variant<int, short, float, double> v(3.14);
  print(v);
}
```

* 但对于一个相对简单的操作来说，这是一个相当大的代码量。为了简化这点，可以用 visit() 来扩展 Variant，它接受一个仿函数或者 lambda，接受的参数就相当于访问者的角色

```cpp
v.visit([] (const auto& value) { std::cout << value; });
```

* variantVisitImpl() 遍历 Variant 的类型，检查活动值是否具有给定类型，然后在找到适当类型时进行操作

```cpp
template<typename R, typename V, typename Visitor,
  typename Head, typename... Tail>
R variantVisitImpl(V&& variant, Visitor&& vis, Typelist<Head, Tail...>) {
  if (variant.template is<Head>())
  {
    return static_cast<R>(
      std::forward<Visitor>(vis)(
        std::forward<V>(variant).template get<Head>()));
  }
  else if constexpr (sizeof...(Tail) > 0)
  {
    return variantVisitImpl<R>(std::forward<V>(variant),
      std::forward<Visitor>(vis),
      Typelist<Tail...>());
  }
  else
  {
    throw EmptyVariant();
  }
}
```

* visit() 的实现直接委托给 variantVisitImpl() 即可，传递 Variant 本身，转发访问者，并提供完整的类型列表。三种实现分别在 this 对象作为 Variant&、const Variant& 或 Variant&& 传递时被调用，类成员函数后加引用限定符的用法可参考 `Effective Modern C++` 条款12

```cpp
// variant/variantvisit.hpp

template<typename... Types>
  template<typename R, typename Visitor>
VisitResult<R, Visitor, Types&...>
Variant<Types...>::visit(Visitor&& vis)& {
  using Result = VisitResult<R, Visitor, Types&...>;
  return variantVisitImpl<Result>(*this, std::forward<Visitor>(vis),
    Typelist<Types...>());
}

template<typename... Types>
  template<typename R, typename Visitor>
VisitResult<R, Visitor, const Types&...>
Variant<Types...>::visit(Visitor&& vis) const& {
  using Result = VisitResult<R, Visitor, const Types &...>;
  return variantVisitImpl<Result>(*this, std::forward<Visitor>(vis),
    Typelist<Types...>());
}

template<typename... Types>
  template<typename R, typename Visitor>
VisitResult<R, Visitor, Types&&...>
Variant<Types...>::visit(Visitor&& vis) && {
  using Result = VisitResult<R, Visitor, Types&&...>;
  return variantVisitImpl<Result>(std::move(*this),
    std::forward<Visitor>(vis),
    Typelist<Types...>());
}
```

* 这里还需要实现 VisitResult 元函数。visit() 的结果类型仍不能确定，比如接受如下 lambda，其返回类型取决于输入的参数类型

```cpp
[] (const auto& value) { return value + 1; }
```

* visit() 设置一个模板参数 R，其默认实参为 ComputedResultType

```cpp
template<typename R = ComputedResultType, typename Visitor>
VisitResult<R, Visitor, Types&...> visit(Visitor&& vis) &;
```

* 现在希望显式指定返回结果的类型时，将 R 设为返回类型

```cpp
// visit的返回类型是Variant<int, double>中的类型
v.visit<Variant<int, double>>([] (const auto& value) { return value + 1; });
```

* 不显式指定时，将使用 R 的默认实参 ComputedResultType（一个只需声明无需实现的类，只是用作标签）

```cpp
class ComputedResultType;
```

* 元函数 VisitResult 的实现如下

```cpp
// 显式指定返回类型的情形
template<typename R, typename Visitor, typename... ElementTypes>
class VisitResultT {
 public:
  using Type = R;
};

// 对使用默认实参 ComputedResultType 的情形特化一个版本
template<typename Visitor, typename… ElementTypes>
class VisitResultT<ComputedResultType, Visitor, ElementTypes…> {
  ... // 实现见后
}

template<typename R, typename Visitor, typename... ElementTypes>
using VisitResult = typename VisitResultT<R, Visitor, ElementTypes...>::Type;
```

* 对于通用类型（common type），C++ 已经有了一个合理的概念：在三元表达式 `b ? x : y` 中，结果类型是 x 和 y 之间的通用类型，例如 x 为 int，y 为 double，则通用类型为 double。由此实现 CommonType 如下

```cpp
using std::declval;

template<typename T, typename U>
class CommonTypeT {
 public:
  using Type = decltype(true? declval<T>() : declval<U>());
};

template<typename T, typename U>
using CommonType = typename CommonTypeT<T, U>::Type;
```

* 特化的 VisitResultT 得到对 Variant 的每个类型调用访问者时生成的结果类型的通用类型，实现如下

```cpp
// 对T类型值调用访问者时生成的结果类型
template<typename Visitor, typename T>
using VisitElementResult = decltype(declval<Visitor>()(declval<T>()));

// 对每个元素类型调用访问者时的通用结果类型
template<typename Visitor, typename... ElementTypes>
class VisitResultT<ComputedResultType, Visitor, ElementTypes...> {
  using ResultTypes = // 每个元素调用访问者时生成的结果类型
    Typelist<VisitElementResult<Visitor, ElementTypes>...>;
 public:
  using Type = // 所有结果类型的通用类型
    Accumulate<PopFront<ResultTypes>, CommonTypeT, Front<ResultTypes>>;
};
```

* 标准库提供了 [std::common_type](https://en.cppreference.com/w/cpp/types/common_type)，它组合了 CommonTypeT 和 Accumulate 以对任意数量类型生成通用类型，因此特化的 VisitResultT 可以更简单地实现如下

```cpp
template<typename Visitor, typename... ElementTypes>
class VisitResultT<ComputedResultType, Visitor, ElementTypes...> {
 public:
  using Type =
    std::common_type_t<VisitElementResult<Visitor, ElementTypes>...>;
};
```

* 下例说明了 visit() 接受一个访问者时的返回类型

```cpp
Variant<int, short, double, float> v(3.14);
auto result = v.visit([] (const auto& value) { return value + 1; });
std::cout << typeid(result).name(); // double
```

## 默认构造

* Variant 应该允许默认构造，否则每次使用都要指定初始值。对于默认构造的方式，可能会想到设置 discriminator 为 0 来表示没有存储值，但这样的空 Variant 不能被访问或找到任何要提取的值，并且将空 Variant 的异常状态提升到了通用状态
* 一个避免引入空 Variant 的方法是初始化类型列表中首个类型的一个值

```cpp
// variant/variantdefaultctor.hpp

template<typename... Types>
Variant<Types...>::Variant()
{
  *this = Front<Typelist<Types...>>();
}
```

* 这个方法是简单和可预测的

```cpp
Variant<int, double> v;
if (v.is<int>()) std::cout << v.get<int>() <<'\n'; // 0（int 类型）
Variant<double, int> v2;
if (v2.is<double>()) std::cout << v2.get<double>(); // 0（double 类型）
```

## 拷贝和移动构造

* 要拷贝一个 Variant 需要确定它当前存储的类型，将该值拷贝构造到字符数组中，并设置 discriminator，这两点在 VariantChoice 的拷贝赋值运算符中已提供，通过对要拷贝的 Variant 使用 visit() 即可简洁紧凑地实现目的

```cpp
// variant/variantcopyctor.hpp

template<typename... Types>
Variant<Types...>::Variant(const Variant& source)
{
  if (!source.empty())
  {
    source.visit([&] (const auto& value) {
      *this = value; // 从 VariantChoice 继承的拷贝赋值运算符
    });
  }
}
```

* 移动构造同理

```cpp
// variant/variantmovector.hpp

template<typename... Types>
Variant<Types...>::Variant(Variant&& source)
{
  if (!source.empty())
  {
    std::move(source).visit([&] (auto&& value) { *this = std::move(value); });
  }
}
```

* 基于 visit 的实现也适用于模板化形式的拷贝和移动构造

```cpp
// variant/variantcopyctortmpl.hpp

template<typename... Types>
  template<typename... SourceTypes>
Variant<Types...>::Variant(const Variant<SourceTypes...>& source)
{
  if (!source.empty())
  {
    source.visit([&] (const auto& value) { *this = value; });
  }
}


// variant/variantmovectortmpl.hpp

template<typename... Types>
  template<typename... SourceTypes>
Variant<Types...>::Variant(Variant<SourceTypes...>&& source)
{
  if (!source.empty())
  {
    std::move(source).visit([&](auto&& value) { *this = std::move(value); });
  }
}
```

* 下面是 Variant 的构造和赋值的示例

```cpp
Variant<short, float, const char*> v1((short)42);
Variant<int, std::string, double> v2(v1); // short 到 int 的整型提升
std::cout << v2.get<int>(); // 42（int 类型）

v1 = 3.14f;
Variant<double, int, std::string> v3(std::move(v1)); // float 到 double 的浮点提升
std::cout << v3.get<double>(); // 3.14（double 类型）

v1 = "hello";
Variant<double, int, std::string> v4(std::move(v1)); // const char* 到 std::string 的转换
std::cout << v4.get<std::string>(); // hello
```

## 赋值运算符

* 赋值运算符与拷贝和移动构造类似

```cpp
// variant/variantcopyassign.hpp

template<typename... Types>
Variant<Types...>& Variant<Types...>::operator=(const Variant& source)
{
  if (!source.empty())
  {
    source.visit([&] (const auto& value) { *this = value; });
  }
  else
  {
    destroy(); // 源 Variant 为空时析构目标，隐式设置 discriminator 为 0
  }
  return *this;
}


// variant/variantmoveassign.hpp

template<typename... Types>
Variant<Types...>& Variant<Types...>::operator= (Variant&& source)
{
  if (!source.empty())
  {
    std::move(source).visit([&] (auto&& value) { *this = std::move(value); });
  }
  else
  {
    destroy();
  }
  return *this;
}


// variant/variantcopyassigntmpl.hpp

template<typename... Types>
  template<typename... SourceTypes>
Variant<Types...>& Variant<Types...>::operator=(const Variant<SourceTypes...>& source)
{
  if (!source.empty())
  {
    source.visit([&] (const auto& value) { *this = value; });
  }
  else
  {
    destroy();
  }
  return *this;
}


// variant/variantmoveassigntmpl.hpp

template<typename... Types>
  template<typename... SourceTypes>
Variant<Types...>& Variant<Types...>::operator=(Variant<SourceTypes...>&& source)
{
  if (!source.empty())
  {
    std::move(source).visit([&] (auto&& value) { *this = std::move(value); });
  }
  else
  {
    destroy();
  }
  return *this;
}
```
