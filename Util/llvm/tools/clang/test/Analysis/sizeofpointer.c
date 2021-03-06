// RUN: clang-cc -analyze -warn-sizeof-pointer -verify %s

struct s {
};

int f(struct s *p) {
  return sizeof(p); // expected-warning{{The code calls sizeof() on a pointer type. This can produce an unexpected result.}}
}
