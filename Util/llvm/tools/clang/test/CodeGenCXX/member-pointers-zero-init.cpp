// RUN: clang-cc -emit-llvm %s -o %t -triple=x86_64-apple-darwin9

struct A {
  int i;
};

// RUN: grep "@a = global i64 -1" %t
int A::* a;

// RUN: grep "@aa = global \[2 x i64\] \[i64 -1, i64 -1\]" %t
int A::* aa[2];

// RUN: grep "@aaa = global \[2 x \[2 x i64\]\] \[\[2 x i64\] \[i64 -1, i64 -1\], \[2 x i64\] \[i64 -1, i64 -1\]\]" %t
int A::* aaa[2][2];

// RUN: grep "@b = global i64 -1" %t
int A::* b = 0;

void f() {
  // RUN: grep "%.* = icmp ne i64 %.*, -1" %t | count 2
  if (a) { }
  if (a != 0) { }

  // RUN: grep "%.* = icmp ne i64 -1, %.*" %t | count 1
  if (0 != a) { }
  
  // RUN: grep "%.* = icmp eq i64 %.*, -1" %t | count 1
  if (a == 0) { }

  // RUN: grep "%.* = icmp eq i64 -1, %.*" %t | count 1
  if (0 == a) { }
    
}

