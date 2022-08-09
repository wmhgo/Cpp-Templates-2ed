## [函数对象](https://en.cppreference.com/w/cpp/named_req/FunctionObject)、[指针](https://en.cppreference.com/w/cpp/language/pointer#.E6.8C.87.E5.90.91.E5.87.BD.E6.95.B0.E6.8C.87.E9.92.88)与[std::function](https://en.cppreference.com/w/cpp/utility/functional/function)

* 函数对象用于给模板提供一些可定制行为

```cpp
#include <iostream>
#include <vector>

template<typename F>
void forUpTo(int n, F f)
{
  for (int i = 0; i < n; ++i) f(i);
}

void print(int i)
{
  std::cout << i << ' ';
}

int main()
{
  std::vector<int> v;
  forUpTo(5, [&v] (int i) { v.push_back(i); });
  forUpTo(5, print); // prints 0 1 2 3 4
}
```

* forUpTo() 可用于任何函数对象，每次使用 forUpTo() 都将产生一个不同的实例化，这个模板很小，但如果很大则实例化的代码也会很大。一个限制代码增长的方法是将其改为无需实例化的非模板

```cpp
void forUpTo(int n, void (*f)(int))
{
  for (int i = 0; i < n; ++i) f(i);
}
```

* 但这样就不允许接受 lambda，因为 lambda 不能转为函数指针

```cpp
forUpTo(5, [&v] (int i) { v.push_back(i); }); // 错误：lambda 不能转为 void(*)(int)
```

* 为此需要使用 [std::function](https://en.cppreference.com/w/cpp/utility/functional/function)

```cpp
#include <functional>

void forUpTo(int n, std::function<void(int)> f)
{
  for (int i = 0; i < n; ++i) f(i);
}
```

* 这个实现提供了一些静态多态方面的特点，它能处理函数对象，但本身只是单个实现的非模板函数。它使用类型擦除（type erasure）的技术做到这点，即在编译期去掉不需要关心的原有类型（与实参推断相反），类型擦除桥接了静态多态与动态多态的沟壑

## 实现[std::function](https://en.cppreference.com/w/cpp/utility/functional/function)

* [std::function](https://en.cppreference.com/w/cpp/utility/functional/function) 是一个广义的函数指针形式，不同于函数指针的是，它能存储一个 lambda 或其他任何带有合适的 operator() 的函数对象。下面实现一个能替代 [std::function](https://en.cppreference.com/w/cpp/utility/functional/function) 的类模板

```cpp
#include <iostream>
#include <vector>
#include <type_traits>

template<typename R, typename... Args>
class B { // 桥接口：负责函数对象的所有权和操作
 public: // 实现为抽象基类，作为类模板A动态多态的基础
  virtual ~B() {}  
  virtual B* clone() const = 0;
  virtual R invoke(Args... args) const = 0;
};

template<typename F, typename R, typename... Args>
class X : public B<R, Args...> { // 抽象基类的实现
 private:
  F f; // 参数化存储的函数对象类型，以实现类型擦除
 public:
  template<typename T>
  X(T&& f) : f(std::forward<T>(f)) {}

  virtual X* clone() const override
  {
    return new X(f);
  }

  virtual R invoke(Args... args) const override
  {
    return f(std::forward<Args>(args)...);
  }
};

// 原始模板
template<typename Signature>
class A;

// 偏特化
template<typename R, typename... Args>
class A<R(Args...)> {
 private:
  B<R, Args...>* bridge; // 该指针负责管理函数对象
 public:
  A() : bridge(nullptr) {}

  A(const A& other) : bridge(nullptr)
  {
    if (other.bridge)
    {
      bridge = other.bridge->clone();
    }
  }

  A(A& other) : A(static_cast<const A&>(other)) {}

  A(A&& other) noexcept : bridge(other.bridge)
  {
    other.bridge = nullptr;
  }

  template<typename F>
  A(F&& f) : bridge(nullptr) // 从任意函数对象构造
  {
    using Functor = std::decay_t<F>;
    using Bridge = X<Functor, R, Args...>; // X的实例化存储一个函数对象副本
    bridge = new Bridge(std::forward<F>(f)); // 派生类到基类的转换，F 丢失，类型擦除
  }

  A& operator=(const A& other)
  {
    A tmp(other);
    swap(*this, tmp);
    return *this;
  }

  A& operator=(A&& other) noexcept
  {
    delete bridge;
    bridge = other.bridge;
    other.bridge = nullptr;
    return *this;
  }

  template<typename F>
  A& operator=(F&& f)
  {
    A tmp(std::forward<F>(f));
    swap(*this, tmp);
    return *this;
  }

  ~A() { delete bridge; }

  friend void swap(A& fp1, A& fp2) noexcept
  {
    std::swap(fp1.bridge, fp2.bridge);
  }

  explicit operator bool() const
  {
    return bridge == nullptr;
  }

  R operator()(Args... args) const
  {
    return bridge->invoke(std::forward<Args>(args)...);
  }
};

void forUpTo(int n, A<void(int)> f)
{
  for (int i = 0; i < n; ++i) f(i);
}

void print(int i)
{
  std::cout << i << ' ';
}

int main()
{
  std::vector<int> v;
  forUpTo(5, [&v] (int i) { v.push_back(i); });
  forUpTo(5, print);
}
```

* 上述 A 模板还不支持函数指针提供的一个操作：测试是否两个 A 对象将调用相同的函数。这需要桥接口 B 提供一个 equals 操作

```cpp
virtual bool equals(const B* fb) const = 0;
```

* 接着在 X 中实现

```cpp
virtual bool equals(const B<R, Args...>* fb) const override
{
  if (auto specFb = dynamic_cast<const X*> (fb))
  {
    return f == specFb->f; // 要求 f 有 operator==
  }
  return false;
}
```

* 最终为 A 实现 `operator==`

```cpp
friend bool operator==(const A& f1, const A& f2) {
  if (!f1 || !f2)
  {
    return !f1 && !f2;
  }
  return f1.bridge->equals(f2.bridge); // equals 要求 operator==
}

friend bool operator!=(const A& f1, const A& f2)
{
  return !(f1 == f2);
}
```

* 这个实现还有一个问题：如果 A 用没有 `operator==` 的函数对象赋值或初始化，编译将报错，即使 A 的 `operator==` 还没被使用。这个问题来源于类型擦除：一旦 A 被赋值或初始化，就丢失了函数对象的类型（派生类到基类的转换），这就要求在实例化前得知类型信息，这个信息包括对一个函数对象的 `operator==` 的调用。为此可以用 SFINAE-based traits 检查 `operator==` 是否可用

```cpp
template<typename T>
class IsEqualityComparable {
 private:
  static void* conv(bool);
  template<typename U>
  static std::true_type test(
    decltype(conv(std::declval<const U&>() == std::declval<const U&>())),
    decltype(conv(!(std::declval<const U&>() == std::declval<const U&>())))
  );

  template<typename U>
  static std::false_type test(...);
 public:
  static constexpr bool value = decltype(test<T>(nullptr, nullptr))::value;
};

// 构造一个 TryEquals 在调用类型没有合适的 == 时抛出异常
template<typename T, bool EqComparable = IsEqualityComparable<T>::value>
struct TryEquals {
  static bool equals(const T& x1, const T& x2)
  {
    return x1 == x2;
  }
};

class NotEqualityComparable : public std::exception {};

template<typename T>
struct TryEquals<T, false> {
  static bool equals(const T& x1, const T& x2)
  {
    throw NotEqualityComparable();
  }
};
```

* 在 X 中实现 equals 时，使用 TryEquals 替代 `operator==` 即可达到同样的目的

```cpp
virtual bool equals(const B<R, Args...>* fb) const override
{
  if (auto specFb = dynamic_cast<const X*> (fb))
  {
    return TryEquals<F>::equals(f, specFb->f);
  }
  return false;
}
```


## 性能考虑

* 类型擦除同时提供了部分静态多态和动态多态的优点。使用类型擦除的泛型代码的性能更接近于动态多态，因为两者都通过虚函数使用动态分派，因此可能丢失一些静态多态的传统优点，如编译器内联调用的能力。虽然这种性能损失能否感知依赖于应用程序，但通常很容易判断。考虑相对虚函数调用开销需要执行多少工作：如果两者接近，类型擦除可能比静态多态慢得多，反之如果函数调用执行大量工作，如查询数据库、排序容器或更新用户接口，类型擦除的开销就算不上多大
