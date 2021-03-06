// NOTE: Use '-fobjc-gc' to test the analysis being run twice, and multiple reports are not issued.
// RUN: clang-cc -analyze -analyzer-experimental-internal-checks -checker-cfref --analyzer-store=basic -fobjc-gc -analyzer-constraints=basic --verify -fblocks %s
// RUN: clang-cc -analyze -analyzer-experimental-internal-checks -checker-cfref --analyzer-store=basic -analyzer-constraints=range --verify -fblocks %s
// RUN: clang-cc -analyze -analyzer-experimental-internal-checks -checker-cfref --analyzer-store=region -analyzer-constraints=basic --verify -fblocks %s
// RUN: clang-cc -analyze -analyzer-experimental-internal-checks -checker-cfref --analyzer-store=region -analyzer-constraints=range --verify -fblocks %s

typedef struct objc_ivar *Ivar;
typedef struct objc_selector *SEL;
typedef signed char BOOL;
typedef int NSInteger;
typedef unsigned int NSUInteger;
typedef struct _NSZone NSZone;
@class NSInvocation, NSArray, NSMethodSignature, NSCoder, NSString, NSEnumerator;
@protocol NSObject
- (BOOL)isEqual:(id)object;
- (id)autorelease;
@end
@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end
@protocol NSMutableCopying  - (id)mutableCopyWithZone:(NSZone *)zone; @end
@protocol NSCoding
- (void)encodeWithCoder:(NSCoder *)aCoder;
@end
@interface NSObject <NSObject> {}
- (id)init;
+ (id)allocWithZone:(NSZone *)zone;
@end
extern id NSAllocateObject(Class aClass, NSUInteger extraBytes, NSZone *zone);
@interface NSString : NSObject <NSCopying, NSMutableCopying, NSCoding>
- (NSUInteger)length;
+ (id)stringWithUTF8String:(const char *)nullTerminatedCString;
@end extern NSString * const NSBundleDidLoadNotification;
@interface NSValue : NSObject <NSCopying, NSCoding>
- (void)getValue:(void *)value;
@end
@interface NSNumber : NSValue
- (char)charValue;
- (id)initWithBool:(BOOL)value;
@end
@interface NSAssertionHandler : NSObject {}
+ (NSAssertionHandler *)currentHandler;
- (void)handleFailureInMethod:(SEL)selector object:(id)object file:(NSString *)fileName lineNumber:(NSInteger)line description:(NSString *)format,...;
@end
extern NSString * const NSConnectionReplyMode;
typedef float CGFloat;
typedef struct _NSPoint {
    CGFloat x;
    CGFloat y;
} NSPoint;
typedef struct _NSSize {
    CGFloat width;
    CGFloat height;
} NSSize;
typedef struct _NSRect {
    NSPoint origin;
    NSSize size;
} NSRect;

// Reduced test case from crash in <rdar://problem/6253157>
@interface A @end
@implementation A
- (void)foo:(void (^)(NSObject *x))block {
  if (!((block != ((void *)0)))) {}
}
@end

// Reduced test case from crash in PR 2796;
//  http://llvm.org/bugs/show_bug.cgi?id=2796

unsigned foo(unsigned x) { return __alignof__((x)) + sizeof(x); }

// Improvement to path-sensitivity involving compound assignments.
//  Addresses false positive in <rdar://problem/6268365>
//

unsigned r6268365Aux();

void r6268365() {
  unsigned x = 0;
  x &= r6268365Aux();
  unsigned j = 0;
    
  if (x == 0) ++j;
  if (x == 0) x = x / j; // no-warning
}

void divzeroassume(unsigned x, unsigned j) {  
  x /= j;  
  if (j == 0) x /= 0;     // no-warning
  if (j == 0) x /= j;     // no-warning
  if (j == 0) x = x / 0;  // no-warning
}

void divzeroassumeB(unsigned x, unsigned j) {  
  x = x / j;  
  if (j == 0) x /= 0;     // no-warning
  if (j == 0) x /= j;     // no-warning
  if (j == 0) x = x / 0;  // no-warning
}

// InitListExpr processing

typedef float __m128 __attribute__((__vector_size__(16), __may_alias__));
__m128 return128() {
  // This compound literal has a Vector type.  We currently just
  // return UnknownVal.
  return __extension__(__m128) { 0.0f, 0.0f, 0.0f, 0.0f };
}

typedef long long __v2di __attribute__ ((__vector_size__ (16)));
typedef long long __m128i __attribute__ ((__vector_size__ (16), __may_alias__));
__m128i vec128i(long long __q1, long long __q0) {
  // This compound literal returns true for both isVectorType() and 
  // isIntegerType().
  return __extension__ (__m128i)(__v2di){ __q0, __q1 };
}

// Zero-sized VLAs.
void check_zero_sized_VLA(int x) {
  if (x)
    return;

  int vla[x]; // expected-warning{{Declared variable-length array (VLA) has zero size}}
}

void check_uninit_sized_VLA() {
  int x;
  int vla[x]; // expected-warning{{Declared variable-length array (VLA) uses a garbage value as its size}}
}

// sizeof(void)
// - Tests a regression reported in PR 3211: http://llvm.org/bugs/show_bug.cgi?id=3211
void handle_sizeof_void(unsigned flag) {
  int* p = 0;

  if (flag) {
    if (sizeof(void) == 1)
      return;
    // Infeasible.
    *p = 1; // no-warning
  }
  
  void* q;
  
  if (!flag) {
    if (sizeof(*q) == 1)
      return;
    // Infeasibe.
    *p = 1; // no-warning
  }
    
  // Infeasible.
  *p = 1; // no-warning
}

// check deference of undefined values
void check_deref_undef(void) {
  int *p;
  *p = 0xDEADBEEF; // expected-warning{{Dereference of undefined pointer value}}
}

// PR 3422
void pr3422_helper(char *p);
void pr3422() {
  char buf[100];
  char *q = &buf[10];
  pr3422_helper(&q[1]);
}

// PR 3543 (handle empty statement expressions)
void pr_3543(void) {
  ({});
}

// <rdar://problem/6611677>
// This test case test the use of a vector type within an array subscript
// expression.
typedef long long __a64vector __attribute__((__vector_size__(8)));
typedef long long __a128vector __attribute__((__vector_size__(16)));
static inline __a64vector __attribute__((__always_inline__, __nodebug__))  
my_test_mm_movepi64_pi64(__a128vector a) {
  return (__a64vector)a[0];
}

// Test basic tracking of ivars associated with 'self'.
@interface SelfIvarTest : NSObject {
  int flag;
}
- (void)test_self_tracking;
@end

@implementation SelfIvarTest
- (void)test_self_tracking {
  char *p = 0;
  char c;

  if (flag)
    p = "hello";

  if (flag)
    c = *p; // no-warning
}
@end

// PR 3770
char pr3770(int x) {
  int y = x & 0x2;
  char *p = 0;
  if (y == 1)
    p = "hello";

  if (y == 1)
    return p[0]; // no-warning
    
  return 'a';
}

// PR 3772
// - We just want to test that this doesn't crash the analyzer.
typedef struct st ST;
struct st { char *name; };
extern ST *Cur_Pu;

void pr3772(void)
{
  static ST *last_Cur_Pu;
  if (last_Cur_Pu == Cur_Pu) {
    return;
  } 
}

// PR 3780 - This tests that StmtIterator isn't broken for VLAs in DeclGroups.
void pr3780(int sz) { typedef double MAT[sz][sz]; }

// <rdar://problem/6695527> - Test that we don't symbolicate doubles before
// we are ready to do something with them.
int rdar6695527(double x) {
  if (!x) { return 0; }
  return 1;
}

// <rdar://problem/6708148> - Test that we properly invalidate structs
//  passed-by-reference to a function.
void pr6708148_invalidate(NSRect *x);
void pr6708148_use(NSRect x);
void pr6708148_test(void) {
  NSRect x;
  pr6708148_invalidate(&x);
  pr6708148_use(x); // no-warning
}

// Handle both kinds of noreturn attributes for pruning paths.
void rdar_6777003_noret() __attribute__((noreturn));
void rdar_6777003_analyzer_noret() __attribute__((analyzer_noreturn));

void rdar_6777003(int x) {
  int *p = 0;
  
  if (x == 1) {
    rdar_6777003_noret();
    *p = 1; // no-warning;    
  }
  
  if (x == 2) {
    rdar_6777003_analyzer_noret();
    *p = 1; // no-warning;
  }
  
  *p = 1; // expected-warning{{Dereference of null pointer}}  
}

// For pointer arithmetic, --/++ should be treated as preserving non-nullness,
// regardless of how well the underlying StoreManager reasons about pointer
// arithmetic.
// <rdar://problem/6777209>
void rdar_6777209(char *p) {
  if (p == 0)
    return;
  
  ++p;
  
  // This branch should always be infeasible.
  if (p == 0)
    *p = 'c'; // no-warning
}

// PR 4033.  A symbolic 'void *' pointer can be used as the address for a
// computed goto.
typedef void *Opcode;
Opcode pr_4033_getOpcode();
void pr_4033(void) {
next_opcode:
  {
    Opcode op = pr_4033_getOpcode();
    if (op) goto *op;
  }
}

// Test invalidating pointers-to-pointers with slightly different types.  This
// example came from a recent false positive due to a regression where the
// branch condition was falsely reported as being uninitialized.
void invalidate_by_ref(char **x);
int test_invalidate_by_ref() {
  unsigned short y;
  invalidate_by_ref((char**) &y);
  if (y) // no-warning
    return 1;
  return 0;  
}

// Test for <rdar://problem/7027684>.  This just tests that the CFG is
// constructed correctly.  Previously, the successor block of the entrance
// was the block containing the merge for '?', which would trigger an
// assertion failure.
int rdar_7027684_aux();
int rdar_7027684_aux_2() __attribute__((noreturn));
void rdar_7027684(int x, int y) {
  {}; // this empty compound statement is critical.
  (rdar_7027684_aux() ? rdar_7027684_aux_2() : (void) 0);
}

// Test that we handle casts of string literals to arbitrary types.
unsigned const char *string_literal_test1() {
  return (const unsigned char*) "hello";
}

const float *string_literal_test2() {
  return (const float*) "hello";
}

// Test that we handle casts *from* incomplete struct types.
extern const struct _FooAssertStruct _cmd;
void test_cast_from_incomplete_struct_aux(volatile const void *x);
void test_cast_from_incomplete_struct() {
  test_cast_from_incomplete_struct_aux(&_cmd);
}

// Test for <rdar://problem/7034511> 
//  "ValueManager::makeIntVal(uint64_t X, QualType T) should return a 'Loc' 
//   when 'T' is a pointer"
//
// Previously this case would crash.
void test_rdar_7034511(NSArray *y) {
  NSObject *x;
  for (x in y) {}
  if (x == ((void*) 0)) {}
}

// Handle casts of function pointers (CodeTextRegions) to arbitrary pointer
// types. This was previously causing a crash in CastRegion.
void handle_funcptr_voidptr_casts() {
  void **ptr;
  typedef void *PVOID;
  typedef void *PCHAR;  
  typedef long INT_PTR, *PINT_PTR;
  typedef INT_PTR (*FARPROC)();
  FARPROC handle_funcptr_voidptr_casts_aux();
  PVOID handle_funcptr_voidptr_casts_aux_2(PVOID volatile *x);
  PVOID handle_funcptr_voidptr_casts_aux_3(PCHAR volatile *x);  
  
  ptr = (void**) handle_funcptr_voidptr_casts_aux();
  handle_funcptr_voidptr_casts_aux_2(ptr);
  handle_funcptr_voidptr_casts_aux_3(ptr);
}

// RegionStore::Retrieve previously crashed on this example.  This example
// was previously in the test file 'xfail_regionstore_wine_crash.c'.
void testA() {
  long x = 0;
  char *y = (char *) &x;
  if (!*y)
    return;
}

// RegionStoreManager previously crashed on this example.  The problem is that
// the value bound to the field of b->grue after the call to testB_aux is
// a symbolic region.  The second '*__gruep__' involves performing a load
// from a 'int*' that really is a 'void**'.  The loaded location must be
// implicitly converted to an integer that wraps a location.  Previosly we would
// get a crash here due to an assertion failure.
typedef struct _BStruct { void *grue; } BStruct;
void testB_aux(void *ptr);
void testB(BStruct *b) {
  {
    int *__gruep__ = ((int *)&((b)->grue));
    int __gruev__ = *__gruep__;
    testB_aux(__gruep__);
  }
  {
    int *__gruep__ = ((int *)&((b)->grue));
    int __gruev__ = *__gruep__;
    if (~0 != __gruev__) {}
  }
}

void test_trivial_symbolic_comparison(int *x) {
  int test_trivial_symbolic_comparison_aux();
  int a = test_trivial_symbolic_comparison_aux();
  int b = a;
  if (a != b) {
    int *p = 0;
    *p = 0xDEADBEEF;     // no-warning
  }
  
  a = a == 1;
  b = b == 1;
  if (a != b) {
    int *p = 0;
    *p = 0xDEADBEEF;     // no-warning
  }
}

// Test for:
//  <rdar://problem/7062158> false positive null dereference due to
//   BasicStoreManager not tracking *static* globals
//
// This just tests the proper tracking of symbolic values for globals (both 
// static and non-static).
//
static int* x_rdar_7062158;
void rdar_7062158() {
  int *current = x_rdar_7062158;
  if (current == x_rdar_7062158)
    return;
    
  int *p = 0;
  *p = 0xDEADBEEF; // no-warning  
}

int* x_rdar_7062158_2;
void rdar_7062158_2() {
  int *current = x_rdar_7062158_2;
  if (current == x_rdar_7062158_2)
    return;
    
  int *p = 0;
  *p = 0xDEADBEEF; // no-warning  
}

// This test reproduces a case for a crash when analyzing ClamAV using
// RegionStoreManager (the crash doesn't exhibit in BasicStoreManager because
// it isn't doing anything smart about arrays).  The problem is that on the
// second line, 'p = &p[i]', p is assigned an ElementRegion whose index
// is a 16-bit integer.  On the third line, a new ElementRegion is created
// based on the previous region, but there the region uses a 32-bit integer,
// resulting in a clash of values (an assertion failure at best).  We resolve
// this problem by implicitly converting index values to 'int' when the
// ElementRegion is created.
unsigned char test_array_index_bitwidth(const unsigned char *p) {
  unsigned short i = 0;
  for (i = 0; i < 2; i++) p = &p[i];  
  return p[i+1];
}

// This case tests that CastRegion handles casts involving BlockPointerTypes.
// It should not crash.
void test_block_cast() {
  id test_block_cast_aux();
  (void (^)(void *))test_block_cast_aux(); // expected-warning{{expression result unused}}
}

// Test comparison of 'id' instance variable to a null void* constant after
// performing an OSAtomicCompareAndSwap32Barrier.
// This previously was a crash in RegionStoreManager.
@interface TestIdNull {
  id x;
}
-(int)foo;
@end
@implementation TestIdNull
-(int)foo {
  OSAtomicCompareAndSwap32Barrier(0, (signed)2, (signed*)&x);  
  if (x == (void*) 0) { return 0; }
  return 1;
}
@end

// PR 4594 - This was a crash when handling casts in SimpleSValuator.
void PR4594() {
  char *buf[1];
  char **foo = buf;
  *foo = "test";
}

// Test invalidation logic where an integer is casted to an array with a
// different sign and then invalidated.
void test_invalidate_cast_int() {
  void test_invalidate_cast_int_aux(unsigned *i);
  signed i;  
  test_invalidate_cast_int_aux((unsigned*) &i);
  if (i < 0)
    return;
}

// Reduced from a crash involving the cast of an Objective-C symbolic region to
// 'char *'
static NSNumber *test_ivar_offset(id self, SEL _cmd, Ivar inIvar) {
  return [[[NSNumber allocWithZone:((void*)0)] initWithBool:*(_Bool *)((char *)self + ivar_getOffset(inIvar))] autorelease];
}

// Reduced from a crash in StoreManager::CastRegion involving a divide-by-zero.
// This resulted from not properly handling region casts to 'const void*'.
void test_cast_const_voidptr() {
  char x[10];
  char *p = &x[1];
  const void* q = p;
}

// Reduced from a crash when analyzing Wine.  This test handles loads from
// function addresses.
typedef long (*FARPROC)();
FARPROC test_load_func(FARPROC origfun) {
  if (!*(unsigned char*) origfun)
    return origfun;
  return 0;
}

// Test passing-by-value an initialized struct variable.
struct test_pass_val {
  int x;
  int y;
};
void test_pass_val_aux(struct test_pass_val s);
void test_pass_val() {
  struct test_pass_val s;
  s.x = 1;
  s.y = 2;
  test_pass_val_aux(s);
}

// This is a reduced test case of a false positive that previously appeared
// in RegionStoreManager.  Previously the array access resulted in dereferencing
// an undefined value.
int test_array_compound(int *q, int *r, int *z) {
  int *array[] = { q, r, z };
  int j = 0;
  for (unsigned i = 0; i < 3 ; ++i)
    if (*array[i]) ++j; // no-warning
  return j;
}

// This test case previously crashed with -analyzer-store=basic because the
// symbolic value stored in 'x' wouldn't be implicitly casted to a signed value
// during the comparison.
int rdar_7124210(unsigned int x) {
  enum { SOME_CONSTANT = 123 };
  int compare = ((signed) SOME_CONSTANT) == *((signed *) &x);
  return compare ? 0 : 1; // Forces the evaluation of the symbolic constraint.
}

void pr4781(unsigned long *raw1) {
  unsigned long *cook, *raw0;
  unsigned long dough[32];
  int i;
  cook = dough;
  for( i = 0; i < 16; i++, raw1++ ) {
    raw0 = raw1++;
    *cook = (*raw0 & 0x00fc0000L) << 6;
    *cook |= (*raw0 & 0x00000fc0L) << 10;
  }
}

// <rdar://problem/7185647> - 'self' should be treated as being non-null
// upon entry to an objective-c method.
@interface RDar7185647
- (id)foo;
@end
@implementation RDar7185647
- (id) foo {
  if (self)
    return self;
  *((int *) 0x0) = 0xDEADBEEF; // no-warning
  return self;
}
@end

// Test reasoning of __builtin_offsetof;
struct test_offsetof_A {
  int x;
  int y;
};
struct test_offsetof_B {
  int w;
  int z;
};
void test_offsetof_1() {
  if (__builtin_offsetof(struct test_offsetof_A, x) ==
      __builtin_offsetof(struct test_offsetof_B, w))
    return;
  int *p = 0;
  *p = 0xDEADBEEF; // no-warning
}
void test_offsetof_2() {
  if (__builtin_offsetof(struct test_offsetof_A, y) ==
      __builtin_offsetof(struct test_offsetof_B, z))
    return;
  int *p = 0;
  *p = 0xDEADBEEF; // no-warning
}
void test_offsetof_3() {
  if (__builtin_offsetof(struct test_offsetof_A, y) -
      __builtin_offsetof(struct test_offsetof_A, x)
      ==
      __builtin_offsetof(struct test_offsetof_B, z) -
      __builtin_offsetof(struct test_offsetof_B, w))
    return;
  int *p = 0;
  *p = 0xDEADBEEF; // no-warning
}
void test_offsetof_4() {
  if (__builtin_offsetof(struct test_offsetof_A, y) ==
      __builtin_offsetof(struct test_offsetof_B, w))
    return;
  int *p = 0;
  *p = 0xDEADBEEF; // expected-warning{{Dereference of null pointer}}
}

// <rdar://problem/6829164> "nil receiver" false positive: make tracking 
// of the MemRegion for 'self' path-sensitive
@interface RDar6829164 : NSObject {
  double x; int y;
}
- (id) init;
@end

id rdar_6829164_1();
double rdar_6829164_2();

@implementation RDar6829164
- (id) init {
  if((self = [super init]) != 0) {
    id z = rdar_6829164_1();
    y = (z != 0);
    if (y)
      x = rdar_6829164_2();
  }
  return self;
}
@end

// <rdar://problem/7242015> - Invalidate values passed-by-reference
// to functions when the pointer to the value is passed as an integer.
void test_7242015_aux(unsigned long);
int rdar_7242015() {
  int x;
  test_7242015_aux((unsigned long) &x); // no-warning
  return x; // Previously we return and uninitialized value when
            // using RegionStore.
}

// <rdar://problem/7242006> [RegionStore] compound literal assignment with
//  floats not honored
CGFloat rdar7242006(CGFloat x) {
  NSSize y = (NSSize){x, 10};
  return y.width; // no-warning
}

// PR 4988 - This test exhibits a case where a function can be referenced
//  when not explicitly used in an "lvalue" context (as far as the analyzer is
//  concerned). This previously triggered a crash due to an invalid assertion.
void pr_4988(void) {
  pr_4988; // expected-warning{{expression result unused}}
}

// <rdar://problem/7152418> - A 'signed char' is used as a flag, which is
//  implicitly converted to an int.
void *rdar7152418_bar();
@interface RDar7152418 {
  signed char x;
}
-(char)foo;
@end;
@implementation RDar7152418
-(char)foo {
  char *p = 0;
  void *result = 0;
  if (x) {
    result = rdar7152418_bar();
    p = "hello";
  }
  if (!result) {
    result = rdar7152418_bar();
    if (result && x)
      return *p; // no-warning
  }
  return 1;
}

//===----------------------------------------------------------------------===//
// Test constant-folding of symbolic values, automatically handling type
// conversions of the symbol as necessary.
//===----------------------------------------------------------------------===//

// Previously this would crash once we started eagerly evaluating symbols whose 
// values were constrained to a single value.
void test_symbol_fold_1(signed char x) {
  while (1) {
    if (x == ((signed char) 0)) {}
  }
}

// This previously caused a crash because it triggered an assertion in APSInt.
void test_symbol_fold_2(unsigned int * p, unsigned int n,
                        const unsigned int * grumpkin, unsigned int dn) {
  unsigned int i;
  unsigned int tempsub[8];
  unsigned int *solgrumpkin = tempsub + n;
  for (i = 0; i < n; i++)
    solgrumpkin[i] = (i < dn) ? ~grumpkin[i] : 0xFFFFFFFF;
  for (i <<= 5; i < (n << 5); i++) {}
}

// This previously caused a crash because it triggered an assertion in APSInt.
// 'x' would evaluate to a 8-bit constant (because of the return value of
// test_symbol_fold_3_aux()) which would not get properly promoted to an
// integer.
char test_symbol_fold_3_aux(void);
unsigned test_symbol_fold_3(void) {
  unsigned x = test_symbol_fold_3_aux();
  if (x == 54)
    return (x << 8) | 0x5;
  return 0;
} 

//===----------------------------------------------------------------------===//
// Tests for the warning of casting a non-struct type to a struct type
//===----------------------------------------------------------------------===//

typedef struct {unsigned int v;} NSSwappedFloat;

NSSwappedFloat test_cast_nonstruct_to_struct(float x) {
  struct hodor {
    float number;
    NSSwappedFloat sf;
  };
  return ((struct hodor *)&x)->sf; // expected-warning{{Casting a non-structure type to a structure type and accessing a field can lead to memory access errors or data corruption}}
}

NSSwappedFloat test_cast_nonstruct_to_union(float x) {
  union bran {
    float number;
    NSSwappedFloat sf;
  };
  return ((union bran *)&x)->sf; // no-warning
}

void test_undefined_array_subscript() {
  int i, a[10];
  int *p = &a[i]; // expected-warning{{Array subscript is undefined}}
}
@end
