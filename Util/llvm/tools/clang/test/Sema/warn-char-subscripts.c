// RUN: clang-cc -Wchar-subscripts -fsyntax-only -verify %s

void t1() {
  int array[1] = { 0 };
  char subscript = 0;
  int val = array[subscript]; // expected-warning{{array subscript is of type 'char'}}
}

void t2() {
  int array[1] = { 0 };
  char subscript = 0;
  int val = subscript[array]; // expected-warning{{array subscript is of type 'char'}}
}

void t3() {
  int *array = 0;
  char subscript = 0;
  int val = array[subscript]; // expected-warning{{array subscript is of type 'char'}}
}

void t4() {
  int *array = 0;
  char subscript = 0;
  int val = subscript[array]; // expected-warning{{array subscript is of type 'char'}}
}

char returnsChar();
void t5() {
  int *array = 0;
  int val = array[returnsChar()]; // expected-warning{{array subscript is of type 'char'}}
}

void t6() {
  int array[1] = { 0 };
  signed char subscript = 0;
  int val = array[subscript]; // no warning for explicit signed char
}

void t7() {
  int array[1] = { 0 };
  unsigned char subscript = 0;
  int val = array[subscript]; // no warning for unsigned char
}

typedef char CharTy;
void t8() {
  int array[1] = { 0 };
  CharTy subscript = 0;
  int val = array[subscript]; // expected-warning{{array subscript is of type 'char'}}
}

typedef signed char SignedCharTy;
void t9() {
  int array[1] = { 0 };
  SignedCharTy subscript = 0;
  int val = array[subscript]; // no warning for explicit signed char
}

typedef unsigned char UnsignedCharTy;
void t10() {
  int array[1] = { 0 };
  UnsignedCharTy subscript = 0;
  int val = array[subscript]; // no warning for unsigned char
}
