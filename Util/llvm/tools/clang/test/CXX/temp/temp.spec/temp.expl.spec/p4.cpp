// RUN: clang-cc -fsyntax-only -verify %s

struct IntHolder { // expected-note{{here}}
  IntHolder(int);
};

template<typename T, typename U>
struct X { // expected-note{{here}}
  void f() { 
    T t; // expected-error{{no matching}}
  }

  void g() { }
  
  struct Inner { 
    T value; 	// expected-note {{member is declared here}}
  };
  
  static T value;
};

template<typename T, typename U>
T X<T, U>::value; // expected-error{{no matching constructor}}

IntHolder &test_X_IntHolderInt(X<IntHolder, int> xih) {
  xih.g(); // okay
  xih.f(); // expected-note{{instantiation}}
  
  // FIXME: diagnostic here has incorrect reason (PR5154)
  X<IntHolder, int>::Inner inner; // expected-error{{implicit default}}
  
  return X<IntHolder, int>::value; // expected-note{{instantiation}}
}

// Explicitly specialize the members of X<IntHolder, long> to not cause
// problems with instantiation.
template<>
void X<IntHolder, long>::f() { }

template<>
struct X<IntHolder, long>::Inner {
  Inner() : value(17) { }
  IntHolder value;
};

template<>
IntHolder X<IntHolder, long>::value = 17;

IntHolder &test_X_IntHolderInt(X<IntHolder, long> xih) {
  xih.g(); // okay
  xih.f(); // okay, uses specialization
  
  X<IntHolder, long>::Inner inner; // okay, uses specialization
  
  return X<IntHolder, long>::value; // okay, uses specialization
}

template<>
X<IntHolder, long>::X() { } // expected-error{{instantiated member}}