struct X {
  int v;
  typedef int t;
};

struct YB {
  typedef YB Y;
  int value;
  typedef int type;
};

struct YBRev {
  typedef int value;
  int type;
};

template<typename T> struct C : X, T {
  using T::value;
  using typename T::type;
  using X::v;
  using typename X::t;
};

template<typename T> struct D : X, T {
  // Mismatch in type/non-type-ness.
  using typename T::value;
  using T::type;
  using X::v;
  using typename X::t;
};

template<typename T> struct E : X, T {
  // Mismatch in using/access-declaration-ness.
  T::value;
  X::v;
};

template<typename T> struct F : X, T {
  // Mismatch in nested-name-specifier.
  using T::Y::value;
  using typename T::Y::type;
  using ::X::v;
  using typename ::X::t;
};

// Force instantiation.
typedef C<YB>::type I;
typedef D<YBRev>::t I;
typedef E<YB>::type I;
typedef F<YB>::type I;
