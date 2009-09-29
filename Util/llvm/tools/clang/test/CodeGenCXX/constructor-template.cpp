// RUN: clang-cc -triple x86_64-apple-darwin -std=c++0x -S %s -o %t-64.s &&
// RUN: FileCheck -check-prefix LP64 --input-file=%t-64.s %s &&
// RUN: clang-cc -triple i386-apple-darwin -std=c++0x -S %s -o %t-32.s &&
// RUN: FileCheck -check-prefix LP32 --input-file=%t-32.s %s &&
// RUN: true

// PR4826
struct A {
  A() {
  }
};

template<typename T>
struct B {
  B(T) {}
  
  A nodes;
};


// PR4853
template <typename T> class List {
public:
  List(){ }     // List<BinomialNode<int>*>::List() remains undefined.
  ~List() {}
};

template <typename T> class Node {
 int i;
public:
 Node(){ }      // Node<BinomialNode<int>*>::Node() remains undefined.
 ~Node() {}
};


template<typename T> class BinomialNode : Node<BinomialNode<T>*> {
public:
  BinomialNode(T value) {}
  List<BinomialNode<T>*> nodes;
};

int main() {
  B<int> *n = new B<int>(4);
  BinomialNode<int> *node = new BinomialNode<int>(1);
  delete node;
}

// CHECK-LP64: __ZN4ListIP12BinomialNodeIiEED1Ev:
// CHECK-LP64: __ZN4ListIP12BinomialNodeIiEED2Ev:
// CHECK-LP64: __ZN4NodeIP12BinomialNodeIiEEC1Ev:
// CHECK-LP64: __ZN4ListIP12BinomialNodeIiEEC1Ev:

// CHECK-LP32: __ZN4ListIP12BinomialNodeIiEED1Ev:
// CHECK-LP32: __ZN4ListIP12BinomialNodeIiEED2Ev:
// CHECK-LP32: __ZN4NodeIP12BinomialNodeIiEEC1Ev:
// CHECK-LP32: __ZN4ListIP12BinomialNodeIiEEC1Ev: