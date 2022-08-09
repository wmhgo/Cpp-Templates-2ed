* 表达式模板是为了支持一种数值数组的类引入的技术。比如希望像内置类型一样对数组进行下列操作，要在支持这种紧凑写法的同时获得高效率，就需要通过表达式模板来完成。表达式模板与元编程互补，元编程主要用于小的、大小固定的数组，表达式模板则用于能在运行期确定大小、中等大小的数组


```cpp
Array<double> x(1000), y(1000);
x = 1.2 * x + x * y;
```

## 临时变量与分割循环的问题

* 先看一种简单的支持数值数组操作的模板实现

```cpp
template <typename T>
class A {
 public:
  explicit A(std::size_t i) : v(new T[i]), n(i) { init(); }
  A(const A<T>& rhs) : v(new T[rhs.size()]), n(rhs.size()) { copy(rhs); }
  ~A() { delete[] v; }
  A<T>& operator=(const A<T>& rhs) {
    if (&rhs != this) copy(rhs);
    return *this;
  }
  std::size_t size() const { return n; }
  T& operator[](std::size_t i) { return v[i]; }
  const T& operator[](std::size_t i) const { return v[i]; }

 protected:
  void init() {
    for (std::size_t i = 0; i < size(); ++i) {
      v[i] = T {}
    };
  }
  void copy(const A<T>& rhs) {
    assert(size() == rhs.size());
    for (std::size_t i = 0; i < size(); ++i) {
      v[i] = rhs.v[i];
    }
  }

 private:
  T* v;
  std::size_t n;
};

template <typename T>
A<T> operator+(const A<T>& a, const A<T>& b) {
  assert(a.size() == b.size());
  A<T> res(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    res[i] = a[i] + b[i];
  }
  return res;
}

template <typename T>
A<T> operator*(const A<T>& a, const A<T>& b) {
  assert(a.size() == b.size());
  A<T> res(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    res[i] = a[i] * b[i];
  }
  return res;
}

template <typename T>
A<T> operator*(const T& s, const A<T>& a) {
  A<T> res(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    res[i] = s * a[i];
  }
  return res;
}
```

* 使用上面这些运算符即可完成表达式计算

```cpp
A<double> x(1000), y(1000);
... x = 1.2 * x + x * y;

// 计算过程相当于
tmp1 = 1.2 * x;      // 循环 1000 次元素操作，并创建和删除 tmp1
tmp2 = x * y;        // 循环 1000 次元素操作，并创建和删除 tmp2
tmp3 = tmp1 + tmp2;  // 循环 1000 次读写操作，并创建和删除 tmp3
x = tmp3;            // 1000 次读操作和写操作
```

* 但这个实现非常低效，有两方面原因
  * 每个运算符操作（除了赋值运算符）至少要生成一个临时数组，至少要生成 3 个大小为 1000 的临时数组
  * 每次使用运算符都要求对实参和结果数组进行额外遍历，生成一个 A 对象，需要读取 6000 次 double 值，写入 4000 次 double 值
* 每个数值数组库的实现都会面临的一个问题是，对于元素很多的数组，没有足够的内存容纳这些临时对象，因此通常使用 computed assignments（如 `+=`、`*=`）来代替前面的赋值运算符，这样就不需要创建任何临时对象

```cpp
template <typename T>
class A {
 public:
  A<T>& operator+=(const A<T>& b);
  A<T>& operator*=(const A<T>& b);
  A<T>& operator*=(const T& s);
};

template <class T>
A<T>& A<T>::operator+=(const A<T>& b) {
  assert(size() == rhs.size());
  for (std::size_t i = 0; i < size(); ++i) {
    (*this)[i] += b[i];
  }
  return *this;
}

template <class T>
A<T>& A<T>::operator*=(const A<T>& b) {
  assert(size() == rhs.size());
  for (std::size_t i = 0; i < size(); ++i) {
    (*this)[i] *= b[i];
  }
  return *this;
}

template <class T>
A<T>& A<T>::operator*=(const T& s) {
  for (std::size_t i = 0; i < size(); ++i) {
    (*this)[i] *= s;
  }
  return *this;
}
```

* 之前的表达式现在可以改写为

```cpp
A<double> x(1000), y(1000);
A<double> tmp{x};
tmp *= y;
x *= 1.2;
x += tmp;
```

* 但这样的缺点显而易见，符号变得不雅观，并且仍要创建一个非必要的局部变量 tmp，此外循环被分割成多个操作，且仍要对 double 进行 6000 次读和 4000 次写操作。理想操作是针对数组每个下标，只对表达式循环一次

```cpp
A<double> x(1000), y(1000);

for (int i = 0; i < x.size(); ++i) {
  x[i] = 1.2 * x[i] + x[i] * y[i];
}
```

* 这样不需要临时数组，每次迭代只需要两次读（x[i] 和 y[i]）和一次写（x[i]），在循环中共需 2000 次读和 1000 次写，但需要手动循环。为了兼顾高性能和避免循环，让代码更优雅且减少错误产生，就需要用到下面的技术

## 在模板实参中编码表达式

* 一个很好的解决思路是，直到看到整个表达式才对表达式各部分求值，在求值前只记录每个对象和该对象上的每个操作。这些操作在编译期已经确定，因此可以编码为模板实参

```cpp
1.2 * x + x * y;
```

* 上面的表达式中，`1.2 * x` 不是一个新的数组，而是一个用于表示 x 的每个值都乘 1.2 的对象，并没有真正进行计算，`x * y` 同理。这样就能把上面的表达式转为一个对象，其类型如下

```cpp
A_Add<A_Mult<A_Scalar<double>, Array<double>>,
      A_Mult<Array<double>, Array<double>>>
```

* 表达式 `1.2 * x + x * y` 的前序语法树的表示如下
```cpp
     +
   /   \
  *     *
 / \   / \
1.2 x x   y
```

## Operand

* 为了完整表示整个表达式，在每个 A_Add 和 A_Mult 对象中必须存储实参的引用，另外在 A_Scalar 对象中需要记录这个表示放大倍数的值或引用，下面是对这些操作数的可行定义

```cpp
template <typename T>
class A_Scalar {
 public:
  constexpr A_Scalar(const T& v) : val(v) {}
  // 对于任意下标都只返回 scalar 值
  constexpr const T& operator[](std::size_t) const { return val; }
  // scalar 视为 size 为 0 的 Array 操作数
  constexpr std::size_t size() const { return 0; };

 private:
  const T& val;  // value of the scalar
};

template <typename T>
class A_Traits {
 public:
  using ExprRef = const T&;
};

template <typename T>
class A_Traits<A_Scalar<T>> {
 public:
  using ExprRef = A_Scalar<T>;
};

template <typename T, typename OP1, typename OP2>
class A_Add {
 public:
  A_Add(const OP1& a, const OP2& b) : op1(a), op2(b) {}

  T operator[](std::size_t i) const { return op1[i] + op2[i]; }

  // size 为两者中较大者
  std::size_t size() const {
    assert(op1.size() == 0 || op2.size() == 0 || op1.size() == op2.size());
    return op1.size() != 0 ? op1.size() : op2.size();
  }

 private:
  typename A_Traits<OP1>::ExprRef op1;
  typename A_Traits<OP2>::ExprRef op2;
};

template <typename T, typename OP1, typename OP2>
class A_Mult {
 public:
  A_Mult(const OP1& a, const OP2& b) : op1(a), op2(b) {}

  T operator[](std::size_t i) const { return op1[i] * op2[i]; }

  std::size_t size() const {
    assert(op1.size() == 0 || op2.size() == 0 || op1.size() == op2.size());
    return op1.size() != 0 ? op1.size() : op2.size();
  }

 private:
  typename A_Traits<OP1>::ExprRef op1;
  typename A_Traits<OP2>::ExprRef op2;
};
```

* 使用 A_Traits 来定义操作数是必要的，A_Scalar 在运算符函数内部绑定，不能一直存在到完整表达式的求值，因此需要传值拷贝而非传引用

```cpp
// 对操作数一般是 const&
const OP1& op1;  // refer to first operand by reference
const OP2& op2;  // refer to second operand by reference
// 但对 scalar 则是原始值
OP1 op1;  // refer to first operand by value
OP2 op2;  // refer to second operand by value
```

* 如果 A_Scalar 对象引用的是顶层定义的 scalar，对这些 scalar 也可以用引用类型

## Array

* 下面创建一个 Array 类型，它能同时适用占用实际内存的数组和表达式模板，接口设计上应该与占用存储空间的真实数组相似，同时要与基于数组的表达式具有相同表示

```cpp
template <typename T, typename Rep = A<T>>
class Array {
 private:
  Rep expr_rep;  // Rep 是 A（占内存的数组）或 A_Add、A_Mult 等表达式模板
 public:
  explicit Array(std::size_t i) : expr_rep(i) {}

  // 从可能的表达式创建
  Array(const Rep& rb) : expr_rep(rb) {}

  // 相同类型的赋值运算符
  Array& operator=(const Array& b) {
    assert(size() == b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
      expr_rep[i] = b[i];
    }
    return *this;
  }

  // 不同类型的赋值运算符
  template <typename T2, typename Rep2>
  Array& operator=(const Array<T2, Rep2>& b) {
    assert(size() == b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
      expr_rep[i] = b[i];
    }
    return *this;
  }

  std::size_t size() const { return expr_rep.size(); }

  T& operator[](std::size_t i) {
    assert(i < size());
    return expr_rep[i];
  }

  decltype(auto) operator[](std::size_t i) const {
    assert(i < size());
    return expr_rep[i];
  }

  Rep& rep() { return expr_rep; }
  const Rep& rep() const { return expr_rep; }
};
```

## Operator

* 目前只是实现了用于代表运算符的、针对数值 Array 模板的运算符操作（如 A_Add），但没实现运算符本身（如 `operator+`）。正如前面所说，这些运算符只是用于代表表达式模板对象，实际上并不对结果数组求值。显然对每个普通的二元运算符，必须实现三个版本，即 array-array、array-scalar、scalar-array，如为了计算前面的表达式初始值需要用到下面的运算符

```cpp
// addition of two Arrays
template <typename T, typename R1, typename R2>
Array<T, A_Add<T, R1, R2>> operator+(const Array<T, R1>& a,
                                     const Array<T, R2>& b) {
  return Array<T, A_Add<T, R1, R2>>(A_Add<T, R1, R2>(a.rep(), b.rep()));
}

// multiplication of two Arrays
template <typename T, typename R1, typename R2>
Array<T, A_Mult<T, R1, R2>> operator*(const Array<T, R1>& a,
                                      const Array<T, R2>& b) {
  return Array<T, A_Mult<T, R1, R2>>(A_Mult<T, R1, R2>(a.rep(), b.rep()));
}

// multiplication of scalar and Array
template <typename T, typename R2>
Array<T, A_Mult<T, A_Scalar<T>, R2>> operator*(const T& s,
                                               const Array<T, R2>& b) {
  return Array<T, A_Mult<T, A_Scalar<T>, R2>>(
      A_Mult<T, A_Scalar<T>, R2>(A_Scalar<T>(s), b.rep()));
}
```

* 这些运算符声明看起来复杂，实际上函数做的工作不多。如对两个 Array 的加法运算符，首先生成一个用于 A_Add 对象，用于表示运算符和操作数

```cpp
A_Add<T, R1, R2>(a.rep(), b.rep())
```

* 接着把这个对象封装到一个 Array 中，从而可以借助 Array 来操作这个运算结果

```cpp
return Array<T, A_Add<T, R1, R2>>(...);
```

* scalar 和 Array 的乘法运算符先使用了 A_Scalar 模板创建 A_Mult 对象

```cpp
A_Mult<T, A_Scalar<T>, R2>(A_Scalar<T>(s), b.rep())
```

* 接着同样封装到 Array 中

```cpp
return Array<T, A_Mult<T, A_Scalar<T>, R2>>(...);
```

* 其他二元运算符实现类似，也可以用宏来声明这些运算符，从而只需要使用较少的代码

## Review

* 现在进行一个自顶向下的回顾。下面是要分析的代码

```cpp
Array<double> x(1000), y(1000);
x = 1.2 * x + x * y;
```

* 由于 x 和 y 的定义中省略了 Rep 实参，所以该参数使用默认值 `A<double>`，因此 x 和 y 是占用真实内存的数组，也就是说它们不只是用于记录操作。解析表达式时，编译器首先作用于左边的乘号，它是一个 scalar-array 运算符，于是重载解析规则选择 scalar-array 形式的乘法运算符

```cpp
template <typename T, typename R2>
Array<T, A_Mult<T, A_Scalar<T>, R2>> operator*(const T& s,
                                               const Array<T, R2>& b) {
  return Array<T, A_Mult<T, A_Scalar<T>, R2>>(
      A_Mult<T, A_Scalar<T>, R2>(A_Scalar<T>(s), b.rep()));
}
```

* 操作数类型是 `double` 和 `Array<double, A<double>>`，因此实际的结果类型

```cpp
Array<double, A_Mult<double, A_Scalar<double>, A<double>>>
```

* 结果的值是一个构造自 double 值 1.2 的 `A_Scalar<double>` 对象和一个表示 x 的 `A<double>` 对象的乘积
* 接着对第二个乘法求值，它是一个 array-array 操作，使用相应的运算符

```cpp
template <typename T, typename R1, typename R2>
Array<T, A_Mult<T, R1, R2>> operator*(const Array<T, R1>& a,
                                      const Array<T, R2>& b) {
  return Array<T, A_Mult<T, R1, R2>>(A_Mult<T, R1, R2>(a.rep(), b.rep()));
}
```

* 两个操作数类型都是 `Array<double, A<double>>`，因此结果类型为

```cpp
Array<double, A_Mult<double, A<double>, A<double>>>
```

* 这次 A_Mult 所封装的两个参数对象都引用了一个 `A<double>`，一个用于表示 x 对象，另一个用于表示 y 对象
* 最后对加法运算符求值，依然是 array-array 操作 ，操作数类型是上面两个结果的类型，调用 array-array 的加法运算符

```cpp
template <typename T, typename R1, typename R2>
Array<T, A_Add<T, R1, R2>> operator+(const Array<T, R1>& a,
                                     const Array<T, R2>& b) {
  return Array<T, A_Add<T, R1, R2>>(A_Add<T, R1, R2>(a.rep(), b.rep()));
}
```

* R1 替换为

```cpp
A_Mult<double, A_Scalar<double>, A<double>>
```

* R2 替换为

```cpp
A_Mult<double, A<double>, A<double>>
```

* 最终赋值运算符右边的表达式类型为

```cpp
Array<double,
  A_Add<double,
      A_Mult<double, A_Scalar<double>, A<double>>,
      A_Mult<double, A<double>, A<double>>
    >
  >
```

* 这个类型与 Array 模板的赋值运算符模板进行匹配

```cpp
template <typename T, typename Rep = A<T>>
class Array {
 public:
  // assignment operator for arrays of different type
  template <typename T2, typename Rep2>
  Array& operator=(const Array<T2, Rep2>& b) {
    assert(size() == b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
      expr_rep[i] = b[i];
    }
    return *this;
  }
};
```

* 赋值运算符将使用参数 b 的下标运算符来计算目标数组 x 的每个元素，参数的实际类型为

```cpp
A_Add<double,
    A_Mult<double, A_Scalar<double>, A<double>>,
    A_Mult<double, A<double>, A<double>>
  >
```

* 对下标 i，得出的 x 为

```cpp
(1.2 * x[i]) + (x[i] * y[i])
```

* 这正是所期望计算的表达式

## Assignment

* 对于一个 Rep 实参基于 A_Mult 或 A_Add 表达式模板的数组，是不能为该数组实例化写操作的（即编写 `a + b = c` 的式子毫无意义），但可以编写其他的表达式模板，从而能对这些表达式模板的结果赋值，如以具有整数值数组为下标的索引操作通常会涉及到子集的选择，即 `x[y] = 2 * x[y]` 等价于

```cpp
for (std::size_t i = 0; i < y.size(); ++i) {
  x[y[i]] = 2 * x[y[i]];
}
```

* 为了使上面这种写法可行，必须令这种基于表达式模板的数组的行为能像一个左值，即可写。这个表达式模板与 A_Mult 等类似，唯一区别在于它提供了下标运算符的 const 版本和 non-const 版本，并返回一个左值引用

```cpp
template <typename T, typename A1, typename A2>
class A_Subscript {
 public:
  A_Subscript(const A1& a, const A2& b) : a1(a), a2(b) {}

  T& operator[](std::size_t i) { return a1[a2[i]]; }

  decltype(auto) operator[](std::size_t i) const { return a1[a2[i]]; }

  std::size_t size() const { return a2.size(); }

 private:
  const A1& a1;  // reference to first operand
  const A2& a2;  // reference to second operand
};
```

* 接着在 Array 中添加下标运算符模板即可

```cpp
template <typename T, typename Rep = A<T>>
class Array {
 public:
  template <class T2, class Rep2>
  Array<T, A_Subscript<T, Rep, Rep2>> operator[](const Array<T2, Rep2>& b) {
    return Array<T, A_Subscript<T, Rep, Rep2>>(
        A_Subscript<T, Rep, Rep2>(*this, b));
  }

  template <class T2, class Rep2>
  decltype(auto) operator[](const Array<T2, Rep2>& b) const {
    return Array<T, A_Subscript<T, Rep, Rep2>>(
        A_Subscript<T, Rep, Rep2>(*this, b));
  }
};
```

## 性能与约束

* 表达式模板可以提高数组操作性能，跟踪其行为可以发现许多很小的内联函数互相调用，在调用堆栈还分配了许多小的表达式模板对象，因此编译器必须执行完整的内联小对象和去除小对象操作，以产生性能上和手写循环媲美的代码
* 表达式模板并没有解决所有涉及数组数值操作的问题，如对 `x = A * x` 这种矩阵 - vector 乘法，x 是一个大小为 n 的 vector，A 是一个`n * n` 矩阵。问题在于临时变量的使用不可避免，因为最终结果的每个元素都依赖于最初 x 的每个元素，而表达式模板会在一次计算上马上更新x的首个元素，计算下一个元素时用到这个已更新的元素就改变了原来的数组
* 但针对 `x = A * y`，如果 x 和 y 不互为别名，就不需要一个临时对象，这意味着必须在运行期知道操作数是否为别名关系，反过来又说明必须生成一个用于表示表达式树的运行期结构，而不是在表达式模板的类型中编码这棵树
