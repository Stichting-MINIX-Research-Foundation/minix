// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -ferror-limit 100 -o - %s

void foo() {
}

bool foobool(int argc) {
  return argc;
}

struct S1; // expected-note {{declared here}} expected-note 4 {{forward declaration of 'S1'}}
extern S1 a;
class S2 {
  mutable int a;
  S2 &operator+=(const S2 &arg) { return (*this); }

public:
  S2() : a(0) {}
  S2(S2 &s2) : a(s2.a) {}
  static float S2s; // expected-note 2 {{static data member is predetermined as shared}}
  static const float S2sc;
};
const float S2::S2sc = 0; // expected-note 2 {{'S2sc' defined here}}
S2 b;                     // expected-note 2 {{'b' defined here}}
const S2 ba[5];           // expected-note 2 {{'ba' defined here}}
class S3 {
  int a;

public:
  S3() : a(0) {}
  S3(const S3 &s3) : a(s3.a) {}
  S3 operator+=(const S3 &arg1) { return arg1; }
};
int operator+=(const S3 &arg1, const S3 &arg2) { return 5; }
S3 c;               // expected-note 2 {{'c' defined here}}
const S3 ca[5];     // expected-note 2 {{'ca' defined here}}
extern const int f; // expected-note 4 {{'f' declared here}}
class S4 {          // expected-note {{'S4' declared here}}
  int a;
  S4();
  S4(const S4 &s4);
  S4 &operator+=(const S4 &arg) { return (*this); }

public:
  S4(int v) : a(v) {}
};
S4 &operator&=(S4 &arg1, S4 &arg2) { return arg1; }
class S5 {
  int a;
  S5() : a(0) {}
  S5(const S5 &s5) : a(s5.a) {}
  S5 &operator+=(const S5 &arg);

public:
  S5(int v) : a(v) {}
};
class S6 {
  int a;

public:
  S6() : a(6) {}
  operator int() { return 6; }
} o; // expected-note 2 {{'o' defined here}}

S3 h, k;
#pragma omp threadprivate(h) // expected-note 2 {{defined as threadprivate or thread local}}

template <class T>       // expected-note {{declared here}}
T tmain(T argc) {        // expected-note 2 {{'argc' defined here}}
  const T d = T();       // expected-note 4 {{'d' defined here}}
  const T da[5] = {T()}; // expected-note 2 {{'da' defined here}}
  T qa[5] = {T()};
  T i;
  T &j = i;                // expected-note 4 {{'j' defined here}}
  S3 &p = k;               // expected-note 2 {{'p' defined here}}
  const T &r = da[(int)i]; // expected-note 2 {{'r' defined here}}
  T &q = qa[(int)i];       // expected-note 2 {{'q' defined here}}
  T fl;                    // expected-note {{'fl' defined here}}
#pragma omp parallel
#pragma omp for reduction // expected-error {{expected '(' after 'reduction'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction + // expected-error {{expected '(' after 'reduction'}} expected-warning {{extra tokens at the end of '#pragma omp for' are ignored}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction( // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(- // expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction() // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(*) // expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected expression}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(\) // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(& : argc // expected-error {{expected ')'}} expected-note {{to match this '('}} expected-error {{variable of type 'float' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(| : argc, // expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}} expected-error {{variable of type 'float' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(|| : argc ? i : argc) // expected-error 2 {{expected variable name}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(foo : argc) //expected-error {{incorrect reduction identifier, expected one of '+', '-', '*', '&', '|', '^', '&&', '||', 'min' or 'max'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : argc)
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(^ : T) // expected-error {{'T' does not refer to a value}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : a, b, c, d, f) // expected-error {{reduction variable with incomplete type 'S1'}} expected-error 3 {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(min : a, b, c, d, f) // expected-error {{reduction variable with incomplete type 'S1'}} expected-error 2 {{arguments of OpenMP clause 'reduction' for 'min' or 'max' must be of arithmetic type}} expected-error 3 {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(max : qa[1]) // expected-error 2 {{expected variable name}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : ba) // expected-error {{a reduction variable with array type 'const S2 [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(* : ca) // expected-error {{a reduction variable with array type 'const S3 [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(- : da) // expected-error {{a reduction variable with array type 'const int [5]'}} expected-error {{a reduction variable with array type 'const float [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(^ : fl) // expected-error {{variable of type 'float' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : S2::S2s) // expected-error {{shared variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : S2::S2sc) // expected-error {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : h, k) // expected-error {{threadprivate or thread local variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : o) // expected-error {{variable of type 'class S6' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for private(i), reduction(+ : j), reduction(+ : q) // expected-error 4 {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel private(k)
#pragma omp for reduction(+ : p), reduction(+ : p) // expected-error 2 {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : p), reduction(+ : p) // expected-error 3 {{variable can appear only once in OpenMP 'reduction' clause}} expected-note 3 {{previously referenced here}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : r) // expected-error 2 {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel shared(i)
#pragma omp parallel reduction(min : i)
#pragma omp for reduction(max : j) // expected-error 2 {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel private(fl)  // expected-note 2 {{defined as private}}
#pragma omp for reduction(+ : fl) // expected-error 2 {{reduction variable must be shared}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel reduction(* : fl) // expected-note 2 {{defined as reduction}}
#pragma omp for reduction(+ : fl)      // expected-error 2 {{reduction variable must be shared}}
  for (int i = 0; i < 10; ++i)
    foo();

  return T();
}

int main(int argc, char **argv) {
  const int d = 5;       // expected-note 2 {{'d' defined here}}
  const int da[5] = {0}; // expected-note {{'da' defined here}}
  int qa[5] = {0};
  S4 e(4); // expected-note {{'e' defined here}}
  S5 g(5); // expected-note {{'g' defined here}}
  int i;
  int &j = i;           // expected-note 2 {{'j' defined here}}
  S3 &p = k;            // expected-note 2 {{'p' defined here}}
  const int &r = da[i]; // expected-note {{'r' defined here}}
  int &q = qa[i];       // expected-note {{'q' defined here}}
  float fl;             // expected-note {{'fl' defined here}}
#pragma omp parallel
#pragma omp for reduction // expected-error {{expected '(' after 'reduction'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction + // expected-error {{expected '(' after 'reduction'}} expected-warning {{extra tokens at the end of '#pragma omp for' are ignored}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction( // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(- // expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction() // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(*) // expected-warning {{missing ':' after reduction identifier - ignoring}} expected-error {{expected expression}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(\) // expected-error {{expected unqualified-id}} expected-warning {{missing ':' after reduction identifier - ignoring}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(foo : argc // expected-error {{expected ')'}} expected-note {{to match this '('}} expected-error {{incorrect reduction identifier, expected one of '+', '-', '*', '&', '|', '^', '&&', '||', 'min' or 'max'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(| : argc, // expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(|| : argc > 0 ? argv[1] : argv[2]) // expected-error {{expected variable name}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(~ : argc) // expected-error {{expected unqualified-id}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : argc)
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(^ : S1) // expected-error {{'S1' does not refer to a value}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : a, b, c, d, f) // expected-error {{reduction variable with incomplete type 'S1'}} expected-error 2 {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(min : a, b, c, d, f) // expected-error {{reduction variable with incomplete type 'S1'}} expected-error 2 {{arguments of OpenMP clause 'reduction' for 'min' or 'max' must be of arithmetic type}} expected-error 2 {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(max : argv[1]) // expected-error {{expected variable name}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : ba) // expected-error {{a reduction variable with array type 'const S2 [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(* : ca) // expected-error {{a reduction variable with array type 'const S3 [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(- : da) // expected-error {{a reduction variable with array type 'const int [5]'}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(^ : fl) // expected-error {{variable of type 'float' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : S2::S2s) // expected-error {{shared variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(&& : S2::S2sc) // expected-error {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(& : e, g) // expected-error {{reduction variable must have an accessible, unambiguous default constructor}} expected-error {{variable of type 'S5' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : h, k) // expected-error {{threadprivate or thread local variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : o) // expected-error {{variable of type 'class S6' is not valid for specified reduction operation}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for private(i), reduction(+ : j), reduction(+ : q) // expected-error 2 {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel private(k)
#pragma omp for reduction(+ : p), reduction(+ : p) // expected-error 2 {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : p), reduction(+ : p) // expected-error {{variable can appear only once in OpenMP 'reduction' clause}} expected-note {{previously referenced here}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel
#pragma omp for reduction(+ : r) // expected-error {{const-qualified variable cannot be reduction}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel shared(i)
#pragma omp parallel reduction(min : i)
#pragma omp for reduction(max : j) // expected-error {{argument of OpenMP clause 'reduction' must reference the same object in all threads}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel private(fl)  // expected-note {{defined as private}}
#pragma omp for reduction(+ : fl) // expected-error {{reduction variable must be shared}}
  for (int i = 0; i < 10; ++i)
    foo();
#pragma omp parallel reduction(* : fl) // expected-note {{defined as reduction}}
#pragma omp for reduction(+ : fl)      // expected-error {{reduction variable must be shared}}
  for (int i = 0; i < 10; ++i)
    foo();

  return tmain(argc) + tmain(fl); // expected-note {{in instantiation of function template specialization 'tmain<int>' requested here}} expected-note {{in instantiation of function template specialization 'tmain<float>' requested here}}
}
