// RUN: clang-cc -fsyntax-only -pedantic -verify %s
int* f(int) { return 0; }
float* f(float) { return 0; }
void f();

void test_f(int iv, float fv) {
  float* fp = f(fv);
  int* ip = f(iv);
}

int* g(int, float, int); // expected-note {{ candidate function }}
float* g(int, int, int); // expected-note {{ candidate function }}
double* g(int, float, float); // expected-note {{ candidate function }}
char* g(int, float, ...); // expected-note {{ candidate function }}
void g();

void test_g(int iv, float fv) {
  int* ip1 = g(iv, fv, 0);
  float* fp1 = g(iv, iv, 0);
  double* dp1 = g(iv, fv, fv);
  char* cp1 = g(0, 0);
  char* cp2 = g(0, 0, 0, iv, fv);

  double* dp2 = g(0, fv, 1.5); // expected-error {{ call to 'g' is ambiguous; candidates are: }}
}

double* h(double f);
int* h(int);

void test_h(float fv, unsigned char cv) {
  double* dp = h(fv);
  int* ip = h(cv);
}

int* i(int);
double* i(long);

void test_i(short sv, int iv, long lv, unsigned char ucv) {
  int* ip1 = i(sv);
  int* ip2 = i(iv);
  int* ip3 = i(ucv);
  double* dp1 = i(lv);
}

int* j(void*);
double* j(bool);

void test_j(int* ip) {
  int* ip1 = j(ip);
}

int* k(char*);
double* k(bool);

void test_k() {
  int* ip1 = k("foo");
  double* dp1 = k(L"foo");
}

int* l(wchar_t*);
double* l(bool);

void test_l() {
  int* ip1 = l(L"foo");
  double* dp1 = l("foo");
}

int* m(const char*);
double* m(char*);

void test_m() {
  int* ip = m("foo");
}

int* n(char*);
double* n(void*);
class E;

void test_n(E* e) {
  char ca[7];
  int* ip1 = n(ca);
  int* ip2 = n("foo");

  float fa[7];
  double* dp1 = n(fa);

  double* dp2 = n(e);
}

enum PromotesToInt {
  PromotesToIntValue = -1
};

enum PromotesToUnsignedInt {
  PromotesToUnsignedIntValue = 1u
};

int* o(int);
double* o(unsigned int);
float* o(long);

void test_o() {
  int* ip1 = o(PromotesToIntValue);
  double* dp1 = o(PromotesToUnsignedIntValue);
}

int* p(int);
double* p(double);

void test_p() {
  int* ip = p((short)1);
  double* dp = p(1.0f);
}

struct Bits {
  signed short int_bitfield : 5;
  unsigned int uint_bitfield : 8;
};

int* bitfields(int, int);
float* bitfields(unsigned int, int);

void test_bitfield(Bits bits, int x) {
  int* ip = bitfields(bits.int_bitfield, 0);
  float* fp = bitfields(bits.uint_bitfield, 0u);
}

int* multiparm(long, int, long); // expected-note {{ candidate function }}
float* multiparm(int, int, int); // expected-note {{ candidate function }}
double* multiparm(int, int, short); // expected-note {{ candidate function }}

void test_multiparm(long lv, short sv, int iv) {
  int* ip1 = multiparm(lv, iv, lv);
  int* ip2 = multiparm(lv, sv, lv);
  float* fp1 = multiparm(iv, iv, iv);
  float* fp2 = multiparm(sv, iv, iv);
  double* dp1 = multiparm(sv, sv, sv);
  double* dp2 = multiparm(iv, sv, sv);
  multiparm(sv, sv, lv); // expected-error {{ call to 'multiparm' is ambiguous; candidates are: }}
}

// Test overloading based on qualification vs. no qualification
// conversion.
int* quals1(int const * p);
char* quals1(int * p);

int* quals2(int const * const * pp);
char* quals2(int * * pp);

int* quals3(int const * * const * ppp);
char* quals3(int *** ppp);

void test_quals(int * p, int * * pp, int * * * ppp) {
  char* q1 = quals1(p);
  char* q2 = quals2(pp);
  char* q3 = quals3(ppp);
}

// Test overloading based on qualification ranking (C++ 13.3.2)p3.
int* quals_rank1(int const * p);
float* quals_rank1(int const volatile *p);
char* quals_rank1(char*);
double* quals_rank1(const char*);

int* quals_rank2(int const * const * pp);
float* quals_rank2(int * const * pp);

void quals_rank3(int const * const * const volatile * p); // expected-note{{candidate function}}
void quals_rank3(int const * const volatile * const * p); // expected-note{{candidate function}}

void quals_rank3(int const *); // expected-note{{candidate function}}
void quals_rank3(int volatile *); // expected-note{{candidate function}}

void test_quals_ranking(int * p, int volatile *pq, int * * pp, int * * * ppp) {
  int* q1 = quals_rank1(p);
  float* q2 = quals_rank1(pq); 
  double* q3 = quals_rank1("string literal");
  char a[17];
  const char* ap = a;
  char* q4 = quals_rank1(a);
  double* q5 = quals_rank1(ap);

  float* q6 = quals_rank2(pp);

  quals_rank3(ppp); // expected-error {{call to 'quals_rank3' is ambiguous; candidates are:}}

  quals_rank3(p); // expected-error {{call to 'quals_rank3' is ambiguous; candidates are:}}
  quals_rank3(pq);
}

// Test overloading based on derived-to-base conversions
class A { };
class B : public A { };
class C : public B { };
class D : public C { };

int* derived1(A*);
char* derived1(const A*);
float* derived1(void*);

int* derived2(A*);
float* derived2(B*);

int* derived3(A*);
float* derived3(const B*);
char* derived3(C*);

void test_derived(B* b, B const* bc, C* c, const C* cc, void* v, D* d) {
  int* d1 = derived1(b);
  char* d2 = derived1(bc);
  int* d3 = derived1(c);
  char* d4 = derived1(cc);
  float* d5 = derived1(v);

  float* d6 = derived2(b);
  float* d7 = derived2(c);

  char* d8 = derived3(d);
}

// Test overloading of references. 
// (FIXME: tests binding to determine candidate sets, not overload 
//  resolution per se).
int* intref(int&);
float* intref(const int&);

void intref_test() {
  float* ir1 = intref(5);
  float* ir2 = intref(5.5);
}

// Test reference binding vs. standard conversions.
int& bind_vs_conv(const double&);
float& bind_vs_conv(int);

void bind_vs_conv_test()
{
  int& i1 = bind_vs_conv(1.0f);
  float& f1 = bind_vs_conv((short)1);
}

// Test that cv-qualifiers get subsumed in the reference binding.
struct X { };
struct Y { };
struct Z : X, Y { };

int& cvqual_subsume(X&); // expected-note{{candidate function}}
float& cvqual_subsume(const Y&); // expected-note{{candidate function}}

int& cvqual_subsume2(const X&);
float& cvqual_subsume2(const volatile Y&);

Z get_Z();

void cvqual_subsume_test(Z z) {
  cvqual_subsume(z); // expected-error{{call to 'cvqual_subsume' is ambiguous; candidates are:}}
  int& x = cvqual_subsume2(get_Z()); // okay: only binds to the first one
}

// Test overloading with cv-qualification differences in reference
// binding.
int& cvqual_diff(X&);
float& cvqual_diff(const X&);

void cvqual_diff_test(X x, Z z) {
  int& i1 = cvqual_diff(x);
  int& i2 = cvqual_diff(z);
}

// Test overloading with derived-to-base differences in reference
// binding.
struct Z2 : Z { };

int& db_rebind(X&);
long& db_rebind(Y&);
float& db_rebind(Z&);

void db_rebind_test(Z2 z2) {
  float& f1 = db_rebind(z2);
}

class string { };
class opt : public string { };

struct SR {
  SR(const string&);
};

void f(SR) { }

void g(opt o) {
  f(o);
}
