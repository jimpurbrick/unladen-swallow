
/* Execute compiled code */

/* XXX TO DO:
   XXX speed up searching for keywords by using a dictionary
   XXX document it!
   */

/* enable more aggressive intra-module optimizations, where available */
#define PY_LOCAL_AGGRESSIVE

#include "Python.h"

#include "code.h"
#include "frameobject.h"
#include "eval.h"
#include "global_llvm_data.h"
#include "opcode.h"
#include "structmember.h"
#include "_llvmfunctionobject.h"

#include "llvm/Function.h"

#include <ctype.h>

#ifndef WITH_TSC

#define READ_TIMESTAMP(var)

#else

typedef unsigned long long uint64;

#if defined(__ppc__) /* <- Don't know if this is the correct symbol; this
			   section should work for GCC on any PowerPC
			   platform, irrespective of OS.
			   POWER?  Who knows :-) */

#define READ_TIMESTAMP(var) ppc_getcounter(&var)

static void
ppc_getcounter(uint64 *v)
{
	register unsigned long tbu, tb, tbu2;

  loop:
	asm volatile ("mftbu %0" : "=r" (tbu) );
	asm volatile ("mftb  %0" : "=r" (tb)  );
	asm volatile ("mftbu %0" : "=r" (tbu2));
	if (__builtin_expect(tbu != tbu2, 0)) goto loop;

	/* The slightly peculiar way of writing the next lines is
	   compiled better by GCC than any other way I tried. */
	((long*)(v))[0] = tbu;
	((long*)(v))[1] = tb;
}

#else /* this is for linux/x86 (and probably any other GCC/x86 combo) */

#define READ_TIMESTAMP(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))

#endif

void dump_tsc(int opcode, int ticked, uint64 inst0, uint64 inst1,
	      uint64 loop0, uint64 loop1, uint64 intr0, uint64 intr1)
{
	uint64 intr, inst, loop;
	PyThreadState *tstate = PyThreadState_Get();
	if (!tstate->interp->tscdump)
		return;
	intr = intr1 - intr0;
	inst = inst1 - inst0 - intr;
	loop = loop1 - loop0 - intr;
	fprintf(stderr, "opcode=%03d t=%d inst=%06lld loop=%06lld\n",
		opcode, ticked, inst, loop);
}

#endif

/* Turn this on if your compiler chokes on the big switch: */
/* #define CASE_TOO_BIG 1 */

#ifdef Py_DEBUG
/* For debugging the interpreter: */
#define LLTRACE  1	/* Low-level trace feature */
#define CHECKEXC 1	/* Double-check exception checking */
#endif

typedef PyObject *(*callproc)(PyObject *, PyObject *, PyObject *);

/* Forward declarations */
static PyObject * fast_function(PyObject *, PyObject ***, int, int, int);
static PyObject * do_call(PyObject *, PyObject ***, int, int);
static PyObject * ext_do_call(PyObject *, PyObject ***, int, int, int);
static PyObject * update_keyword_args(PyObject *, int, PyObject ***,
				      PyObject *);
static PyObject * update_star_args(int, int, PyObject *, PyObject ***);
static PyObject * load_args(PyObject ***, int);

/* Keep this in sync with llvm_fbuilder.cc */
#define CALL_FLAG_VAR 1
#define CALL_FLAG_KW 2

#ifdef LLTRACE
static int lltrace;
static int prtrace(PyObject *, char *);
#endif
static int call_trace(Py_tracefunc, PyObject *, PyFrameObject *,
		      int, PyObject *);
static int call_trace_protected(Py_tracefunc, PyObject *,
				 PyFrameObject *, int, PyObject *);
static void call_exc_trace(Py_tracefunc, PyObject *, PyFrameObject *);
static int maybe_call_line_trace(Py_tracefunc, PyObject *,
				  PyFrameObject *, int *, int *, int *);

static PyObject * cmp_outcome(int, PyObject *, PyObject *);
static void format_exc_check_arg(PyObject *, char *, PyObject *);
static PyObject * string_concatenate(PyObject *, PyObject *,
				    PyFrameObject *, unsigned char *);

#define NAME_ERROR_MSG \
	"name '%.200s' is not defined"
#define GLOBAL_NAME_ERROR_MSG \
	"global name '%.200s' is not defined"
#define UNBOUNDLOCAL_ERROR_MSG \
	"local variable '%.200s' referenced before assignment"
#define UNBOUNDFREE_ERROR_MSG \
	"free variable '%.200s' referenced before assignment" \
        " in enclosing scope"

/* Dynamic execution profile */
#ifdef DYNAMIC_EXECUTION_PROFILE
#ifdef DXPAIRS
static long dxpairs[257][256];
#define dxp dxpairs[256]
#else
static long dxp[256];
#endif
#endif

/* Function call profile */
#ifdef CALL_PROFILE
#define PCALL_NUM 11
static int pcall[PCALL_NUM];

#define PCALL_ALL 0
#define PCALL_FUNCTION 1
#define PCALL_FAST_FUNCTION 2
#define PCALL_FASTER_FUNCTION 3
#define PCALL_METHOD 4
#define PCALL_BOUND_METHOD 5
#define PCALL_CFUNCTION 6
#define PCALL_TYPE 7
#define PCALL_GENERATOR 8
#define PCALL_OTHER 9
#define PCALL_POP 10

/* Notes about the statistics

   PCALL_FAST stats

   FAST_FUNCTION means no argument tuple needs to be created.
   FASTER_FUNCTION means that the fast-path frame setup code is used.

   If there is a method call where the call can be optimized by changing
   the argument tuple and calling the function directly, it gets recorded
   twice.

   As a result, the relationship among the statistics appears to be
   PCALL_ALL == PCALL_FUNCTION + PCALL_METHOD - PCALL_BOUND_METHOD +
                PCALL_CFUNCTION + PCALL_TYPE + PCALL_GENERATOR + PCALL_OTHER
   PCALL_FUNCTION > PCALL_FAST_FUNCTION > PCALL_FASTER_FUNCTION
   PCALL_METHOD > PCALL_BOUND_METHOD
*/

#define PCALL(POS) pcall[POS]++

PyObject *
PyEval_GetCallStats(PyObject *self)
{
	return Py_BuildValue("iiiiiiiiiii",
			     pcall[0], pcall[1], pcall[2], pcall[3],
			     pcall[4], pcall[5], pcall[6], pcall[7],
			     pcall[8], pcall[9], pcall[10]);
}
#else
#define PCALL(O)

PyObject *
PyEval_GetCallStats(PyObject *self)
{
	Py_INCREF(Py_None);
	return Py_None;
}
#endif


#ifdef WITH_THREAD

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "pythread.h"

static PyThread_type_lock interpreter_lock = 0; /* This is the GIL */
static long main_thread = 0;

int
PyEval_ThreadsInitialized(void)
{
	return interpreter_lock != 0;
}

void
PyEval_InitThreads(void)
{
	if (interpreter_lock)
		return;
	interpreter_lock = PyThread_allocate_lock();
	PyThread_acquire_lock(interpreter_lock, 1);
	main_thread = PyThread_get_thread_ident();
}

void
PyEval_AcquireLock(void)
{
	PyThread_acquire_lock(interpreter_lock, 1);
}

void
PyEval_ReleaseLock(void)
{
	PyThread_release_lock(interpreter_lock);
}

void
PyEval_AcquireThread(PyThreadState *tstate)
{
	if (tstate == NULL)
		Py_FatalError("PyEval_AcquireThread: NULL new thread state");
	/* Check someone has called PyEval_InitThreads() to create the lock */
	assert(interpreter_lock);
	PyThread_acquire_lock(interpreter_lock, 1);
	if (PyThreadState_Swap(tstate) != NULL)
		Py_FatalError(
			"PyEval_AcquireThread: non-NULL old thread state");
}

void
PyEval_ReleaseThread(PyThreadState *tstate)
{
	if (tstate == NULL)
		Py_FatalError("PyEval_ReleaseThread: NULL thread state");
	if (PyThreadState_Swap(NULL) != tstate)
		Py_FatalError("PyEval_ReleaseThread: wrong thread state");
	PyThread_release_lock(interpreter_lock);
}

/* This function is called from PyOS_AfterFork to ensure that newly
   created child processes don't hold locks referring to threads which
   are not running in the child process.  (This could also be done using
   pthread_atfork mechanism, at least for the pthreads implementation.) */

void
PyEval_ReInitThreads(void)
{
	PyObject *threading, *result;
	PyThreadState *tstate;

	if (!interpreter_lock)
		return;
	/*XXX Can't use PyThread_free_lock here because it does too
	  much error-checking.  Doing this cleanly would require
	  adding a new function to each thread_*.h.  Instead, just
	  create a new lock and waste a little bit of memory */
	interpreter_lock = PyThread_allocate_lock();
	PyThread_acquire_lock(interpreter_lock, 1);
	main_thread = PyThread_get_thread_ident();

	/* Update the threading module with the new state.
	 */
	tstate = PyThreadState_GET();
	threading = PyMapping_GetItemString(tstate->interp->modules,
					    "threading");
	if (threading == NULL) {
		/* threading not imported */
		PyErr_Clear();
		return;
	}
	result = PyObject_CallMethod(threading, "_after_fork", NULL);
	if (result == NULL)
		PyErr_WriteUnraisable(threading);
	else
		Py_DECREF(result);
	Py_DECREF(threading);
}
#endif

/* Functions save_thread and restore_thread are always defined so
   dynamically loaded modules needn't be compiled separately for use
   with and without threads: */

PyThreadState *
PyEval_SaveThread(void)
{
	PyThreadState *tstate = PyThreadState_Swap(NULL);
	if (tstate == NULL)
		Py_FatalError("PyEval_SaveThread: NULL tstate");
#ifdef WITH_THREAD
	if (interpreter_lock)
		PyThread_release_lock(interpreter_lock);
#endif
	return tstate;
}

void
PyEval_RestoreThread(PyThreadState *tstate)
{
	if (tstate == NULL)
		Py_FatalError("PyEval_RestoreThread: NULL tstate");
#ifdef WITH_THREAD
	if (interpreter_lock) {
		int err = errno;
		PyThread_acquire_lock(interpreter_lock, 1);
		errno = err;
	}
#endif
	PyThreadState_Swap(tstate);
}


/* Mechanism whereby asynchronously executing callbacks (e.g. UNIX
   signal handlers or Mac I/O completion routines) can schedule calls
   to a function to be called synchronously.
   The synchronous function is called with one void* argument.
   It should return 0 for success or -1 for failure -- failure should
   be accompanied by an exception.

   If registry succeeds, the registry function returns 0; if it fails
   (e.g. due to too many pending calls) it returns -1 (without setting
   an exception condition).

   Note that because registry may occur from within signal handlers,
   or other asynchronous events, calling malloc() is unsafe!

#ifdef WITH_THREAD
   Any thread can schedule pending calls, but only the main thread
   will execute them.
#endif

   XXX WARNING!  ASYNCHRONOUSLY EXECUTING CODE!
   There are two possible race conditions:
   (1) nested asynchronous registry calls;
   (2) registry calls made while pending calls are being processed.
   While (1) is very unlikely, (2) is a real possibility.
   The current code is safe against (2), but not against (1).
   The safety against (2) is derived from the fact that only one
   thread (the main thread) ever takes things out of the queue.

   XXX Darn!  With the advent of thread state, we should have an array
   of pending calls per thread in the thread state!  Later...
*/

#define NPENDINGCALLS 32
static struct {
	int (*func)(void *);
	void *arg;
} pendingcalls[NPENDINGCALLS];
static volatile int pendingfirst = 0;
static volatile int pendinglast = 0;
static volatile int things_to_do = 0;

int
Py_AddPendingCall(int (*func)(void *), void *arg)
{
	static volatile int busy = 0;
	int i, j;
	/* XXX Begin critical section */
	/* XXX If you want this to be safe against nested
	   XXX asynchronous calls, you'll have to work harder! */
	if (busy)
		return -1;
	busy = 1;
	i = pendinglast;
	j = (i + 1) % NPENDINGCALLS;
	if (j == pendingfirst) {
		busy = 0;
		return -1; /* Queue full */
	}
	pendingcalls[i].func = func;
	pendingcalls[i].arg = arg;
	pendinglast = j;

	_Py_Ticker = 0;
	things_to_do = 1; /* Signal main loop */
	busy = 0;
	/* XXX End critical section */
	return 0;
}

int
Py_MakePendingCalls(void)
{
	static int busy = 0;
#ifdef WITH_THREAD
	if (main_thread && PyThread_get_thread_ident() != main_thread)
		return 0;
#endif
	if (busy)
		return 0;
	busy = 1;
	things_to_do = 0;
	for (;;) {
		int i;
		int (*func)(void *);
		void *arg;
		i = pendingfirst;
		if (i == pendinglast)
			break; /* Queue empty */
		func = pendingcalls[i].func;
		arg = pendingcalls[i].arg;
		pendingfirst = (i + 1) % NPENDINGCALLS;
		if (func(arg) < 0) {
			busy = 0;
			things_to_do = 1; /* We're not done yet */
			return -1;
		}
	}
	busy = 0;
	return 0;
}


/* The interpreter's recursion limit */

#ifndef Py_DEFAULT_RECURSION_LIMIT
#define Py_DEFAULT_RECURSION_LIMIT 1000
#endif
static int recursion_limit = Py_DEFAULT_RECURSION_LIMIT;
int _Py_CheckRecursionLimit = Py_DEFAULT_RECURSION_LIMIT;

int
Py_GetRecursionLimit(void)
{
	return recursion_limit;
}

void
Py_SetRecursionLimit(int new_limit)
{
	recursion_limit = new_limit;
	_Py_CheckRecursionLimit = recursion_limit;
}

/* the macro Py_EnterRecursiveCall() only calls _Py_CheckRecursiveCall()
   if the recursion_depth reaches _Py_CheckRecursionLimit.
   If USE_STACKCHECK, the macro decrements _Py_CheckRecursionLimit
   to guarantee that _Py_CheckRecursiveCall() is regularly called.
   Without USE_STACKCHECK, there is no need for this. */
int
_Py_CheckRecursiveCall(char *where)
{
	PyThreadState *tstate = PyThreadState_GET();

#ifdef USE_STACKCHECK
	if (PyOS_CheckStack()) {
		--tstate->recursion_depth;
		PyErr_SetString(PyExc_MemoryError, "Stack overflow");
		return -1;
	}
#endif
	if (tstate->recursion_depth > recursion_limit) {
		--tstate->recursion_depth;
		PyErr_Format(PyExc_RuntimeError,
			     "maximum recursion depth exceeded%s",
			     where);
		return -1;
	}
	_Py_CheckRecursionLimit = recursion_limit;
	return 0;
}

extern "C" void
_PyEval_RaiseForUnboundLocal(PyFrameObject *frame, int var_index)
{
	format_exc_check_arg(
		PyExc_UnboundLocalError,
		UNBOUNDLOCAL_ERROR_MSG,
		PyTuple_GetItem(frame->f_code->co_varnames, var_index));
}

/* Status code for main loop (reason for stack unwind) */
enum why_code {
		WHY_NOT =	0x0001,	/* No error */
		WHY_EXCEPTION = 0x0002,	/* Exception occurred */
		WHY_RERAISE =	0x0004,	/* Exception re-raised by 'finally' */
		WHY_RETURN =	0x0008,	/* 'return' statement */
		WHY_BREAK =	0x0010,	/* 'break' statement */
		WHY_CONTINUE =	0x0020,	/* 'continue' statement */
		WHY_YIELD =	0x0040	/* 'yield' operator */
};

static enum why_code do_raise(PyObject *, PyObject *, PyObject *);

/* Records whether tracing is on for any thread.  Counts the number of
   threads for which tstate->c_tracefunc is non-NULL, so if the value
   is 0, we know we don't have to check this thread's c_tracefunc.
   This speeds up the if statement in PyEval_EvalFrame() after
   fast_next_opcode*/
static int _Py_TracingPossible = 0;

/* for manipulating the thread switch and periodic "stuff" - used to be
   per thread, now just a pair o' globals */
int _Py_CheckInterval = 100;
volatile int _Py_Ticker = 100;

PyObject *
PyEval_EvalCode(PyCodeObject *co, PyObject *globals, PyObject *locals)
{
	return PyEval_EvalCodeEx(co,
			  globals, locals,
			  (PyObject **)NULL, 0,
			  (PyObject **)NULL, 0,
			  (PyObject **)NULL, 0,
			  NULL);
}


/* Interpreter main loop */

PyObject *
PyEval_EvalFrameEx(PyFrameObject *f, int throwflag) {
	/* This is for backward compatibility with extension modules that
           used this API; core interpreter code should call
           PyEval_EvalFrame() */
	PyObject *result;
	f->f_throwflag = throwflag;
	result = PyEval_EvalFrame(f);
	f->f_throwflag = 0;
	return result;
}

PyObject *
PyEval_EvalFrame(PyFrameObject *f)
{
#ifdef DXPAIRS
	int lastopcode = 0;
#endif
	register PyObject **stack_pointer;  /* Next free slot in value stack */
	register unsigned char *next_instr;
	register int opcode;	/* Current opcode */
	register int oparg;	/* Current opcode argument, if any */
	register enum why_code why; /* Reason for block stack unwind */
	register int err;	/* Error status -- nonzero if error */
	register PyObject *x;	/* Temporary objects popped off stack */
	register PyObject *v;
	register PyObject *w;
	register PyObject *u;
	register PyObject *t;
	register PyObject **fastlocals, **freevars;
	PyObject *retval = NULL;	/* Return value */
	PyThreadState *tstate = PyThreadState_GET();
	PyCodeObject *co;

	/* when tracing we set things up so that

               not (instr_lb <= current_bytecode_offset < instr_ub)

	   is true when the line being executed has changed.  The
           initial values are such as to make this false the first
           time it is tested. */
	int instr_ub = -1, instr_lb = 0, instr_prev = -1;

	unsigned char *first_instr;
	PyObject *names;
	PyObject *consts;
#if defined(Py_DEBUG) || defined(LLTRACE)
	/* Make it easier to find out where we are with a debugger */
	char *filename;
#endif

/* Computed GOTOs, or
       the-optimization-commonly-but-improperly-known-as-"threaded code"
   using gcc's labels-as-values extension
   (http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html).

   The traditional bytecode evaluation loop uses a "switch" statement, which
   decent compilers will optimize as a single indirect branch instruction 
   combined with a lookup table of jump addresses. However, since the
   indirect jump instruction is shared by all opcodes, the CPU will have a
   hard time making the right prediction for where to jump next (actually,
   it will be always wrong except in the uncommon case of a sequence of
   several identical opcodes).

   "Threaded code" in contrast, uses an explicit jump table and an explicit
   indirect jump instruction at the end of each opcode. Since the jump
   instruction is at a different address for each opcode, the CPU will make a
   separate prediction for each of these instructions, which is equivalent to
   predicting the second opcode of each opcode pair. These predictions have
   a much better chance to turn out valid, especially in small bytecode loops.

   A mispredicted branch on a modern CPU flushes the whole pipeline and
   can cost several CPU cycles (depending on the pipeline depth), 
   and potentially many more instructions (depending on the pipeline width).
   A correctly predicted branch, however, is nearly free.

   At the time of this writing, the "threaded code" version is up to 15-20%
   faster than the normal "switch" version, depending on the compiler and the
   CPU architecture.

   We disable the optimization if DYNAMIC_EXECUTION_PROFILE is defined,
   because it would render the measurements invalid.


   NOTE: care must be taken that the compiler doesn't try to "optimize" the
   indirect jumps by sharing them between all opcodes. Such optimizations
   can be disabled on gcc by using the -fno-gcse flag (or possibly
   -fno-crossjumping).
*/

#if defined(USE_COMPUTED_GOTOS) && defined(DYNAMIC_EXECUTION_PROFILE)
#undef USE_COMPUTED_GOTOS
#endif

#ifdef USE_COMPUTED_GOTOS
/* Import the static jump table */
#include "opcode_targets.h"

/* This macro is used when several opcodes defer to the same implementation
   (e.g. SETUP_LOOP, SETUP_FINALLY) */
#define TARGET_WITH_IMPL(op, impl) \
	TARGET_##op: \
		opcode = op; \
		if (HAS_ARG(op)) \
			oparg = NEXTARG(); \
	case op: \
		goto impl; \

#define TARGET(op) \
	TARGET_##op: \
		opcode = op; \
		if (HAS_ARG(op)) \
			oparg = NEXTARG(); \
	case op:


#define DISPATCH() \
	{ \
		/* Avoid multiple loads from _Py_Ticker despite `volatile` */ \
		int _tick = _Py_Ticker - 1; \
		_Py_Ticker = _tick; \
		if (_tick >= 0) { \
			FAST_DISPATCH(); \
		} \
		continue; \
	}

#ifdef LLTRACE
#define FAST_DISPATCH() \
	{ \
		if (!lltrace && !_Py_TracingPossible) { \
			f->f_lasti = INSTR_OFFSET(); \
			goto *opcode_targets[*next_instr++]; \
		} \
		goto fast_next_opcode; \
	}
#else
#define FAST_DISPATCH() \
	{ \
		if (!_Py_TracingPossible) { \
			f->f_lasti = INSTR_OFFSET(); \
			goto *opcode_targets[*next_instr++]; \
		} \
		goto fast_next_opcode; \
	}
#endif

#else
#define TARGET(op) \
	case op:
#define TARGET_WITH_IMPL(op, impl) \
	/* silence compiler warnings about `impl` unused */ \
	if (0) goto impl; \
	case op:
#define DISPATCH() continue
#define FAST_DISPATCH() goto fast_next_opcode
#endif


/* Tuple access macros */

#ifndef Py_DEBUG
#define GETITEM(v, i) PyTuple_GET_ITEM((PyTupleObject *)(v), (i))
#else
#define GETITEM(v, i) PyTuple_GetItem((v), (i))
#endif

#ifdef WITH_TSC
/* Use Pentium timestamp counter to mark certain events:
   inst0 -- beginning of switch statement for opcode dispatch
   inst1 -- end of switch statement (may be skipped)
   loop0 -- the top of the mainloop
   loop1 -- place where control returns again to top of mainloop
            (may be skipped)
   intr1 -- beginning of long interruption
   intr2 -- end of long interruption

   Many opcodes call out to helper C functions.  In some cases, the
   time in those functions should be counted towards the time for the
   opcode, but not in all cases.  For example, a CALL_FUNCTION opcode
   calls another Python function; there's no point in charge all the
   bytecode executed by the called function to the caller.

   It's hard to make a useful judgement statically.  In the presence
   of operator overloading, it's impossible to tell if a call will
   execute new Python code or not.

   It's a case-by-case judgement.  I'll use intr1 for the following
   case:

   CALL_FUNCTION (and friends)

 */
	uint64 inst0, inst1, loop0, loop1, intr0 = 0, intr1 = 0;
	int ticked = 0;

	READ_TIMESTAMP(inst0);
	READ_TIMESTAMP(inst1);
	READ_TIMESTAMP(loop0);
	READ_TIMESTAMP(loop1);

	/* shut up the compiler */
	opcode = 0;
#endif

/* Code access macros */

#define INSTR_OFFSET()	((int)(next_instr - first_instr))
#define NEXTOP()	(*next_instr++)
#define NEXTARG()	(next_instr += 2, (next_instr[-1]<<8) + next_instr[-2])
#define PEEKARG()	((next_instr[2]<<8) + next_instr[1])
#define JUMPTO(x)	(next_instr = first_instr + (x))
#define JUMPBY(x)	(next_instr += (x))

/* OpCode prediction macros
	Some opcodes tend to come in pairs thus making it possible to
	predict the second code when the first is run.  For example,
	GET_ITER is often followed by FOR_ITER. And FOR_ITER is often
	followed by STORE_FAST or UNPACK_SEQUENCE.

	Verifying the prediction costs a single high-speed test of a register
	variable against a constant.  If the pairing was good, then the
	processor's own internal branch predication has a high likelihood of
	success, resulting in a nearly zero-overhead transition to the
	next opcode.  A successful prediction saves a trip through the eval-loop
	including its two unpredictable branches, the HAS_ARG test and the
	switch-case.  Combined with the processor's internal branch prediction,
	a successful PREDICT has the effect of making the two opcodes run as if
	they were a single new opcode with the bodies combined.

    If collecting opcode statistics, your choices are to either keep the
	predictions turned-on and interpret the results as if some opcodes
	had been combined or turn-off predictions so that the opcode frequency
	counter updates for both opcodes.

    Opcode prediction is disabled with threaded code, since the latter allows
	the CPU to record separate branch prediction information for each
	opcode.

*/

#if defined(DYNAMIC_EXECUTION_PROFILE) || defined(USE_COMPUTED_GOTOS)
#define PREDICT(op)		if (0) goto PRED_##op
#define PREDICTED(op)		PRED_##op:
#define PREDICTED_WITH_ARG(op)	PRED_##op:
#else
#define PREDICT(op)		if (*next_instr == op) goto PRED_##op
#define PREDICTED(op)		PRED_##op: next_instr++
#define PREDICTED_WITH_ARG(op)	PRED_##op: oparg = PEEKARG(); next_instr += 3
#endif


/* Stack manipulation macros */

/* The stack can grow at most MAXINT deep, as co_nlocals and
   co_stacksize are ints. */
#define STACK_LEVEL()	((int)(stack_pointer - f->f_valuestack))
#define EMPTY()		(STACK_LEVEL() == 0)
#define TOP()		(stack_pointer[-1])
#define SECOND()	(stack_pointer[-2])
#define THIRD() 	(stack_pointer[-3])
#define FOURTH()	(stack_pointer[-4])
#define SET_TOP(v)	(stack_pointer[-1] = (v))
#define SET_SECOND(v)	(stack_pointer[-2] = (v))
#define SET_THIRD(v)	(stack_pointer[-3] = (v))
#define SET_FOURTH(v)	(stack_pointer[-4] = (v))
#define BASIC_STACKADJ(n)	(stack_pointer += n)
#define BASIC_PUSH(v)	(*stack_pointer++ = (v))
#define BASIC_POP()	(*--stack_pointer)

#ifdef LLTRACE
#define PUSH(v)		{ (void)(BASIC_PUSH(v), \
                               lltrace && prtrace(TOP(), "push")); \
                               assert(STACK_LEVEL() <= co->co_stacksize); }
#define POP()		((void)(lltrace && prtrace(TOP(), "pop")), \
			 BASIC_POP())
#define STACKADJ(n)	{ (void)(BASIC_STACKADJ(n), \
                               lltrace && prtrace(TOP(), "stackadj")); \
                               assert(STACK_LEVEL() <= co->co_stacksize); }
#define EXT_POP(STACK_POINTER) ((void)(lltrace && \
				prtrace((STACK_POINTER)[-1], "ext_pop")), \
				*--(STACK_POINTER))
#define EXT_PUSH(v, STACK_POINTER) ((void)(*(STACK_POINTER)++ = (v), \
                   lltrace && prtrace((STACK_POINTER)[-1], "ext_push")))
#else
#define PUSH(v)		BASIC_PUSH(v)
#define POP()		BASIC_POP()
#define STACKADJ(n)	BASIC_STACKADJ(n)
#define EXT_POP(STACK_POINTER) (*--(STACK_POINTER))
#define EXT_PUSH(v, STACK_POINTER) (*(STACK_POINTER)++ = (v))
#endif

/* Local variable macros */

#define GETLOCAL(i)	(fastlocals[i])

/* The SETLOCAL() macro must not DECREF the local variable in-place and
   then store the new value; it must copy the old value to a temporary
   value, then store the new value, and then DECREF the temporary value.
   This is because it is possible that during the DECREF the frame is
   accessed by other code (e.g. a __del__ method or gc.collect()) and the
   variable would be pointing to already-freed memory. */
#define SETLOCAL(i, value)	do { PyObject *tmp = GETLOCAL(i); \
				     GETLOCAL(i) = value; \
                                     Py_XDECREF(tmp); } while (0)

/* Start of code */

	if (f == NULL)
		return NULL;

	/* push frame */
	if (Py_EnterRecursiveCall(""))
		return NULL;

	co = f->f_code;
	tstate->frame = f;

	if (f->f_use_llvm) {
		int target_optimization_level = 0;
		if (target_optimization_level < Py_LlvmEverythingFlag)
			target_optimization_level = Py_LlvmEverythingFlag;
		if (co->co_optimization < target_optimization_level) {
			// Always optimize code to level 0 before JITting
			// it, since that speeds up the JIT.  Also, if
			// the LLVM version of the function wasn't
			// created yet, setting the optimization level
			// will create it.  If Py_LlvmEverythingFlag is
			// set to a higher number, optimize to that level
			// instead.
			PyObject *opt_level =
			    PyInt_FromLong(target_optimization_level);
			if (opt_level == NULL) {
				retval = NULL;
				goto exit_eval_frame;
			}
			if (PyObject_SetAttrString((PyObject *)co,
						   "co_optimization",
						   opt_level) == -1) {
				Py_DECREF(opt_level);
				retval = NULL;
				goto exit_eval_frame;
			}
			Py_DECREF(opt_level);
		}
		retval = _PyLlvmFunction_Eval(
			(PyLlvmFunctionObject *)co->co_llvm_function, f);
		goto exit_eval_frame;
	}

	if (tstate->use_tracing) {
		if (tstate->c_tracefunc != NULL) {
			/* tstate->c_tracefunc, if defined, is a
			   function that will be called on *every* entry
			   to a code block.  Its return value, if not
			   None, is a function that will be called at
			   the start of each executed line of code.
			   (Actually, the function must return itself
			   in order to continue tracing.)  The trace
			   functions are called with three arguments:
			   a pointer to the current frame, a string
			   indicating why the function is called, and
			   an argument which depends on the situation.
			   The global trace function is also called
			   whenever an exception is detected. */
			if (call_trace_protected(tstate->c_tracefunc,
						 tstate->c_traceobj,
						 f, PyTrace_CALL, Py_None)) {
				/* Trace function raised an error */
				goto exit_eval_frame;
			}
		}
		if (tstate->c_profilefunc != NULL) {
			/* Similar for c_profilefunc, except it needn't
			   return itself and isn't called for "line" events */
			if (call_trace_protected(tstate->c_profilefunc,
						 tstate->c_profileobj,
						 f, PyTrace_CALL, Py_None)) {
				/* Profile function raised an error */
				goto exit_eval_frame;
			}
		}
	}

	names = co->co_names;
	consts = co->co_consts;
	fastlocals = f->f_localsplus;
	freevars = f->f_localsplus + co->co_nlocals;
	first_instr = (unsigned char*) PyString_AS_STRING(co->co_code);
	/* An explanation is in order for the next line.

	   f->f_lasti now refers to the index of the last instruction
	   executed.  You might think this was obvious from the name, but
	   this wasn't always true before 2.3!  PyFrame_New now sets
	   f->f_lasti to -1 (i.e. the index *before* the first instruction)
	   and YIELD_VALUE doesn't fiddle with f_lasti any more.  So this
	   does work.  Promise.

	   When the PREDICT() macros are enabled, some opcode pairs follow in
           direct succession without updating f->f_lasti.  A successful
           prediction effectively links the two codes together as if they
           were a single new opcode; accordingly,f->f_lasti will point to
           the first code in the pair (for instance, GET_ITER followed by
           FOR_ITER is effectively a single opcode and f->f_lasti will point
           at to the beginning of the combined pair.)
	*/
	next_instr = first_instr + f->f_lasti + 1;
	stack_pointer = f->f_stacktop;
	assert(stack_pointer != NULL);
	f->f_stacktop = NULL;	/* remains NULL unless yield suspends frame */

#ifdef LLTRACE
	lltrace = PyDict_GetItemString(f->f_globals, "__lltrace__") != NULL;
#endif
#if defined(Py_DEBUG) || defined(LLTRACE)
	filename = PyString_AsString(co->co_filename);
#endif

	why = WHY_NOT;
	w = NULL;

	if (f->f_throwflag) { /* support for generator.throw() */
		why = WHY_EXCEPTION;
		goto on_error;
	}

	for (;;) {
#ifdef WITH_TSC
		if (inst1 == 0) {
			/* Almost surely, the opcode executed a break
			   or a continue, preventing inst1 from being set
			   on the way out of the loop.
			*/
			READ_TIMESTAMP(inst1);
			loop1 = inst1;
		}
		dump_tsc(opcode, ticked, inst0, inst1, loop0, loop1,
			 intr0, intr1);
		ticked = 0;
		inst1 = 0;
		intr0 = 0;
		intr1 = 0;
		READ_TIMESTAMP(loop0);
#endif
		assert(stack_pointer >= f->f_valuestack); /* else underflow */
		assert(STACK_LEVEL() <= co->co_stacksize);  /* else overflow */

		/* Do periodic things.  Doing this every time through
		   the loop would add too much overhead, so we do it
		   only every Nth instruction.  We also do it if
		   ``things_to_do'' is set, i.e. when an asynchronous
		   event needs attention (e.g. a signal handler or
		   async I/O handler); see Py_AddPendingCall() and
		   Py_MakePendingCalls() above. */

		if (--_Py_Ticker < 0) {
			if (*next_instr == SETUP_FINALLY) {
				/* Make the last opcode before
				   a try: finally: block uninterruptable. */
				goto fast_next_opcode;
			}
			_Py_Ticker = _Py_CheckInterval;
			tstate->tick_counter++;
#ifdef WITH_TSC
			ticked = 1;
#endif
			if (things_to_do) {
				if (Py_MakePendingCalls() < 0) {
					why = WHY_EXCEPTION;
					goto on_error;
				}
				if (things_to_do)
					/* MakePendingCalls() didn't succeed.
					   Force early re-execution of this
					   "periodic" code, possibly after
					   a thread switch */
					_Py_Ticker = 0;
			}
#ifdef WITH_THREAD
			if (interpreter_lock) {
				/* Give another thread a chance */

				if (PyThreadState_Swap(NULL) != tstate)
					Py_FatalError("ceval: tstate mix-up");
				PyThread_release_lock(interpreter_lock);

				/* Other threads may run now */

				PyThread_acquire_lock(interpreter_lock, 1);
				if (PyThreadState_Swap(tstate) != NULL)
					Py_FatalError("ceval: orphan tstate");

				/* Check for thread interrupts */

				if (tstate->async_exc != NULL) {
					x = tstate->async_exc;
					tstate->async_exc = NULL;
					PyErr_SetNone(x);
					Py_DECREF(x);
					why = WHY_EXCEPTION;
					goto on_error;
				}
			}
#endif
		}

	fast_next_opcode:
		f->f_lasti = INSTR_OFFSET();

		/* line-by-line tracing support */

		if (_Py_TracingPossible &&
		    tstate->c_tracefunc != NULL && !tstate->tracing) {
			/* see maybe_call_line_trace
			   for expository comments */
			f->f_stacktop = stack_pointer;

			err = maybe_call_line_trace(tstate->c_tracefunc,
						    tstate->c_traceobj,
						    f, &instr_lb, &instr_ub,
						    &instr_prev);
			/* Reload possibly changed frame fields */
			JUMPTO(f->f_lasti);
			if (f->f_stacktop != NULL) {
				stack_pointer = f->f_stacktop;
				f->f_stacktop = NULL;
			}
			if (err) {
				/* trace function raised an exception */
				why = WHY_EXCEPTION;
				goto on_error;
			}
		}

		/* Extract opcode and argument */

		opcode = NEXTOP();
		oparg = 0;   /* allows oparg to be stored in a register because
			it doesn't have to be remembered across a full loop */
		if (HAS_ARG(opcode))
			oparg = NEXTARG();
	  dispatch_opcode:
#ifdef DYNAMIC_EXECUTION_PROFILE
#ifdef DXPAIRS
		dxpairs[lastopcode][opcode]++;
		lastopcode = opcode;
#endif
		dxp[opcode]++;
#endif

#ifdef LLTRACE
		/* Instruction tracing */

		if (lltrace) {
			if (HAS_ARG(opcode)) {
				printf("%d: %d, %d\n",
				       f->f_lasti, opcode, oparg);
			}
			else {
				printf("%d: %d\n",
				       f->f_lasti, opcode);
			}
		}
#endif

		/* Main switch on opcode */
		READ_TIMESTAMP(inst0);

		assert(why == WHY_NOT);
		/* XXX(jyasskin): Add an assertion under CHECKEXC that
		   !PyErr_Occurred(). */
		switch (opcode) {

		/* BEWARE!
		   It is essential that any operation that fails sets
		   why to anything but WHY_NOT, and that no operation
		   that succeeds does this! */

		/* case STOP_CODE: this is an error! */

		TARGET(NOP)
			FAST_DISPATCH();

		TARGET(LOAD_FAST)
			x = GETLOCAL(oparg);
			if (x != NULL) {
				Py_INCREF(x);
				PUSH(x);
				FAST_DISPATCH();
			}
			_PyEval_RaiseForUnboundLocal(f, oparg);
			why = WHY_EXCEPTION;
			break;

		TARGET(LOAD_CONST)
			x = GETITEM(consts, oparg);
			Py_INCREF(x);
			PUSH(x);
			FAST_DISPATCH();

		PREDICTED_WITH_ARG(STORE_FAST);
		TARGET(STORE_FAST)
			v = POP();
			SETLOCAL(oparg, v);
			FAST_DISPATCH();

		TARGET(POP_TOP)
			v = POP();
			Py_DECREF(v);
			FAST_DISPATCH();

		TARGET(ROT_TWO)
			v = TOP();
			w = SECOND();
			SET_TOP(w);
			SET_SECOND(v);
			FAST_DISPATCH();

		TARGET(ROT_THREE)
			v = TOP();
			w = SECOND();
			x = THIRD();
			SET_TOP(w);
			SET_SECOND(x);
			SET_THIRD(v);
			FAST_DISPATCH();

		TARGET(ROT_FOUR)
			u = TOP();
			v = SECOND();
			w = THIRD();
			x = FOURTH();
			SET_TOP(v);
			SET_SECOND(w);
			SET_THIRD(x);
			SET_FOURTH(u);
			FAST_DISPATCH();

		TARGET(DUP_TOP)
			v = TOP();
			Py_INCREF(v);
			PUSH(v);
			FAST_DISPATCH();

		TARGET(DUP_TOP_TWO)
			x = TOP();
			Py_INCREF(x);
			w = SECOND();
			Py_INCREF(w);
			STACKADJ(2);
			SET_TOP(x);
			SET_SECOND(w);
			FAST_DISPATCH();

		TARGET(DUP_TOP_THREE)
			x = TOP();
			Py_INCREF(x);
			w = SECOND();
			Py_INCREF(w);
			v = THIRD();
			Py_INCREF(v);
			STACKADJ(3);
			SET_TOP(x);
			SET_SECOND(w);
			SET_THIRD(v);
			FAST_DISPATCH();

		TARGET(UNARY_POSITIVE)
			v = TOP();
			x = PyNumber_Positive(v);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(UNARY_NEGATIVE)
			v = TOP();
			x = PyNumber_Negative(v);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(UNARY_NOT)
			v = TOP();
			err = PyObject_IsTrue(v);
			Py_DECREF(v);
			if (err == 0) {
				Py_INCREF(Py_True);
				SET_TOP(Py_True);
				DISPATCH();
			}
			else if (err > 0) {
				Py_INCREF(Py_False);
				SET_TOP(Py_False);
				DISPATCH();
			}
			STACKADJ(-1);
			why = WHY_EXCEPTION;
			break;

		TARGET(UNARY_CONVERT)
			v = TOP();
			x = PyObject_Repr(v);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(UNARY_INVERT)
			v = TOP();
			x = PyNumber_Invert(v);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_POWER)
			w = POP();
			v = TOP();
			x = PyNumber_Power(v, w, Py_None);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_MULTIPLY)
			w = POP();
			v = TOP();
			x = PyNumber_Multiply(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_DIVIDE)
			if (!_Py_QnewFlag) {
				w = POP();
				v = TOP();
				x = PyNumber_Divide(v, w);
				Py_DECREF(v);
				Py_DECREF(w);
				SET_TOP(x);
				if (x == NULL) {
					why = WHY_EXCEPTION;
					break;
				}
				DISPATCH();
			}
			/* -Qnew is in effect: jump to BINARY_TRUE_DIVIDE */
			goto _binary_true_divide;

		TARGET(BINARY_TRUE_DIVIDE)
		_binary_true_divide:
			w = POP();
			v = TOP();
			x = PyNumber_TrueDivide(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_FLOOR_DIVIDE)
			w = POP();
			v = TOP();
			x = PyNumber_FloorDivide(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_MODULO)
			w = POP();
			v = TOP();
			if (PyString_CheckExact(v))
				x = PyString_Format(v, w);
			else
				x = PyNumber_Remainder(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_ADD)
			w = POP();
			v = TOP();
			if (PyInt_CheckExact(v) && PyInt_CheckExact(w)) {
				/* INLINE: int + int */
				register long a, b, i;
				a = PyInt_AS_LONG(v);
				b = PyInt_AS_LONG(w);
				i = a + b;
				if ((i^a) < 0 && (i^b) < 0)
					goto slow_add;
				x = PyInt_FromLong(i);
			}
			else if (PyString_CheckExact(v) &&
				 PyString_CheckExact(w)) {
				x = string_concatenate(v, w, f, next_instr);
				/* string_concatenate consumed the ref to v */
				goto skip_decref_vx;
			}
			else {
			  slow_add:
				x = PyNumber_Add(v, w);
			}
			Py_DECREF(v);
		  skip_decref_vx:
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_SUBTRACT)
			w = POP();
			v = TOP();
			if (PyInt_CheckExact(v) && PyInt_CheckExact(w)) {
				/* INLINE: int - int */
				register long a, b, i;
				a = PyInt_AS_LONG(v);
				b = PyInt_AS_LONG(w);
				i = a - b;
				if ((i^a) < 0 && (i^~b) < 0)
					goto slow_sub;
				x = PyInt_FromLong(i);
			}
			else {
			  slow_sub:
				x = PyNumber_Subtract(v, w);
			}
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_SUBSCR)
			w = POP();
			v = TOP();
			if (PyList_CheckExact(v) && PyInt_CheckExact(w)) {
				/* INLINE: list[int] */
				Py_ssize_t i = PyInt_AsSsize_t(w);
				if (i < 0)
					i += PyList_GET_SIZE(v);
				if (i >= 0 && i < PyList_GET_SIZE(v)) {
					x = PyList_GET_ITEM(v, i);
					Py_INCREF(x);
				}
				else
					goto slow_get;
			}
			else
			  slow_get:
				x = PyObject_GetItem(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_LSHIFT)
			w = POP();
			v = TOP();
			x = PyNumber_Lshift(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_RSHIFT)
			w = POP();
			v = TOP();
			x = PyNumber_Rshift(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_AND)
			w = POP();
			v = TOP();
			x = PyNumber_And(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_XOR)
			w = POP();
			v = TOP();
			x = PyNumber_Xor(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BINARY_OR)
			w = POP();
			v = TOP();
			x = PyNumber_Or(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(LIST_APPEND)
			w = POP();
			v = POP();
			err = PyList_Append(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			PREDICT(JUMP_ABSOLUTE);
			DISPATCH();

		TARGET(INPLACE_POWER)
			w = POP();
			v = TOP();
			x = PyNumber_InPlacePower(v, w, Py_None);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_MULTIPLY)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceMultiply(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_DIVIDE)
			if (!_Py_QnewFlag) {
				w = POP();
				v = TOP();
				x = PyNumber_InPlaceDivide(v, w);
				Py_DECREF(v);
				Py_DECREF(w);
				SET_TOP(x);
				if (x == NULL) {
					why = WHY_EXCEPTION;
					break;
				}
				DISPATCH();
			}
			/* -Qnew is in effect: jump to INPLACE_TRUE_DIVIDE */
			goto _inplace_true_divide;

		TARGET(INPLACE_TRUE_DIVIDE)
		_inplace_true_divide:
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceTrueDivide(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_FLOOR_DIVIDE)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceFloorDivide(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_MODULO)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceRemainder(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_ADD)
			w = POP();
			v = TOP();
			if (PyInt_CheckExact(v) && PyInt_CheckExact(w)) {
				/* INLINE: int + int */
				register long a, b, i;
				a = PyInt_AS_LONG(v);
				b = PyInt_AS_LONG(w);
				i = a + b;
				if ((i^a) < 0 && (i^b) < 0)
					goto slow_iadd;
				x = PyInt_FromLong(i);
			}
			else if (PyString_CheckExact(v) &&
				 PyString_CheckExact(w)) {
				x = string_concatenate(v, w, f, next_instr);
				/* string_concatenate consumed the ref to v */
				goto skip_decref_v;
			}
			else {
			  slow_iadd:
				x = PyNumber_InPlaceAdd(v, w);
			}
			Py_DECREF(v);
		  skip_decref_v:
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_SUBTRACT)
			w = POP();
			v = TOP();
			if (PyInt_CheckExact(v) && PyInt_CheckExact(w)) {
				/* INLINE: int - int */
				register long a, b, i;
				a = PyInt_AS_LONG(v);
				b = PyInt_AS_LONG(w);
				i = a - b;
				if ((i^a) < 0 && (i^~b) < 0)
					goto slow_isub;
				x = PyInt_FromLong(i);
			}
			else {
			  slow_isub:
				x = PyNumber_InPlaceSubtract(v, w);
			}
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_LSHIFT)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceLshift(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_RSHIFT)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceRshift(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_AND)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceAnd(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_XOR)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceXor(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(INPLACE_OR)
			w = POP();
			v = TOP();
			x = PyNumber_InPlaceOr(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(SLICE_NONE)
			w = NULL;
			v = NULL;
			goto _slice_common;
		TARGET(SLICE_LEFT)
			w = NULL;
			v = POP();
			goto _slice_common;
		TARGET(SLICE_RIGHT)
			w = POP();
			v = NULL;
			goto _slice_common;
		TARGET(SLICE_BOTH)
			w = POP();
			v = POP();
		_slice_common:
			u = TOP();
			x = _PyEval_ApplySlice(u, v, w);
			Py_DECREF(u);
			Py_XDECREF(v);
			Py_XDECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(STORE_SLICE_NONE)
			w = NULL;
			v = NULL;
			goto _store_slice_common;
		TARGET(STORE_SLICE_LEFT)
			w = NULL;
			v = POP();
			goto _store_slice_common;
		TARGET(STORE_SLICE_RIGHT)
			w = POP();
			v = NULL;
			goto _store_slice_common;
		TARGET(STORE_SLICE_BOTH)
			w = POP();
			v = POP();
		_store_slice_common:
			u = POP();
			t = POP();
			err = _PyEval_AssignSlice(u, v, w, t); /* u[v:w] = t */
			Py_DECREF(t);
			Py_DECREF(u);
			Py_XDECREF(v);
			Py_XDECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(DELETE_SLICE_NONE)
			w = NULL;
			v = NULL;
			goto _delete_slice_common;
		TARGET(DELETE_SLICE_LEFT)
			w = NULL;
			v = POP();
			goto _delete_slice_common;
		TARGET(DELETE_SLICE_RIGHT)
			w = POP();
			v = NULL;
			goto _delete_slice_common;
		TARGET(DELETE_SLICE_BOTH)
			w = POP();
			v = POP();
			goto _delete_slice_common;
		_delete_slice_common:
			u = POP();
			err = _PyEval_AssignSlice(u, v, w, (PyObject *)NULL);
							/* del u[v:w] */
			Py_DECREF(u);
			Py_XDECREF(v);
			Py_XDECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(STORE_SUBSCR)
			w = TOP();
			v = SECOND();
			u = THIRD();
			STACKADJ(-3);
			/* v[w] = u */
			err = PyObject_SetItem(v, w, u);
			Py_DECREF(u);
			Py_DECREF(v);
			Py_DECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(DELETE_SUBSCR)
			w = TOP();
			v = SECOND();
			STACKADJ(-2);
			/* del v[w] */
			err = PyObject_DelItem(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

#ifdef CASE_TOO_BIG
		default: switch (opcode) {
#endif
		TARGET(RAISE_VARARGS_ZERO)
			u = NULL;
			v = NULL;
			w = NULL;
			goto _raise_varargs_common;
		TARGET(RAISE_VARARGS_ONE)
			u = NULL;
			v = NULL;
			w = POP();
			goto _raise_varargs_common;
		TARGET(RAISE_VARARGS_TWO)
			u = NULL;
			v = POP();
			w = POP();
			goto _raise_varargs_common;
		TARGET(RAISE_VARARGS_THREE)
			u = POP();
			v = POP();
			w = POP();
		_raise_varargs_common:
			why = do_raise(w, v, u);
			break;

		TARGET(RETURN_VALUE)
			retval = POP();
			why = WHY_RETURN;
			goto fast_block_end;

		TARGET(YIELD_VALUE)
			retval = POP();
			f->f_stacktop = stack_pointer;
			why = WHY_YIELD;
			goto fast_yield;

		TARGET(POP_BLOCK)
			{
				PyTryBlock *b = PyFrame_BlockPop(f);
				while (STACK_LEVEL() > b->b_level) {
					v = POP();
					Py_DECREF(v);
				}
			}
			DISPATCH();

		PREDICTED(END_FINALLY);
		TARGET(END_FINALLY)
			v = POP();
			if (PyInt_Check(v)) {
				why = (enum why_code) PyInt_AS_LONG(v);
				assert(why != WHY_YIELD);
				if (why == WHY_RETURN ||
				    why == WHY_CONTINUE)
					retval = POP();
			}
			else if (PyExceptionClass_Check(v) ||
			         PyString_Check(v)) {
				w = POP();
				u = POP();
				PyErr_Restore(v, w, u);
				why = WHY_RERAISE;
				break;
			}
			else if (v != Py_None) {
				PyErr_SetString(PyExc_SystemError,
					"'finally' pops bad exception");
				why = WHY_EXCEPTION;
			}
			Py_DECREF(v);
			break;

		TARGET(STORE_NAME)
			x = POP();
			err = _PyEval_StoreName(f, oparg, x);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(DELETE_NAME)
			err = _PyEval_DeleteName(f, oparg);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		PREDICTED_WITH_ARG(UNPACK_SEQUENCE);
		TARGET(UNPACK_SEQUENCE)
			v = POP();
			if (PyTuple_CheckExact(v) &&
			    PyTuple_GET_SIZE(v) == oparg) {
				PyObject **items = \
					((PyTupleObject *)v)->ob_item;
				while (oparg--) {
					w = items[oparg];
					Py_INCREF(w);
					PUSH(w);
				}
			} else if (PyList_CheckExact(v) &&
				   PyList_GET_SIZE(v) == oparg) {
				PyObject **items = \
					((PyListObject *)v)->ob_item;
				while (oparg--) {
					w = items[oparg];
					Py_INCREF(w);
					PUSH(w);
				}
			} else if (_PyEval_UnpackIterable(v, oparg,
					stack_pointer + oparg) < 0) {
				/* _PyEval_UnpackIterable() raised
				   an exception */
				Py_DECREF(v);
				why = WHY_EXCEPTION;
				break;
			} else {
				stack_pointer += oparg;
			}
			Py_DECREF(v);
			DISPATCH();

		TARGET(STORE_ATTR)
			w = GETITEM(names, oparg);
			v = TOP();
			u = SECOND();
			STACKADJ(-2);
			err = PyObject_SetAttr(v, w, u); /* v.w = u */
			Py_DECREF(v);
			Py_DECREF(u);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(DELETE_ATTR)
			w = GETITEM(names, oparg);
			v = POP();
			/* del v.w */
			err = PyObject_SetAttr(v, w, (PyObject *)NULL);
			Py_DECREF(v);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(STORE_GLOBAL)
			w = GETITEM(names, oparg);
			v = POP();
			err = PyDict_SetItem(f->f_globals, w, v);
			Py_DECREF(v);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(DELETE_GLOBAL)
			w = GETITEM(names, oparg);
			err = PyDict_DelItem(f->f_globals, w);
			if (err != 0) {
				_PyEval_RaiseForGlobalNameError(w);
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(LOAD_NAME)
			x = _PyEval_LoadName(f, oparg);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			PUSH(x);
			DISPATCH();

		TARGET(LOAD_GLOBAL)
			w = GETITEM(names, oparg);
			if (PyString_CheckExact(w)) {
				/* Inline the PyDict_GetItem() calls.
				   WARNING: this is an extreme speed hack.
				   Do not try this at home. */
				long hash = ((PyStringObject *)w)->ob_shash;
				if (hash != -1) {
					PyDictObject *d;
					PyDictEntry *e;
					d = (PyDictObject *)(f->f_globals);
					e = d->ma_lookup(d, w, hash);
					if (e == NULL) {
						why = WHY_EXCEPTION;
						break;
					}
					x = e->me_value;
					if (x != NULL) {
						Py_INCREF(x);
						PUSH(x);
						DISPATCH();
					}
					d = (PyDictObject *)(f->f_builtins);
					e = d->ma_lookup(d, w, hash);
					if (e == NULL) {
						why = WHY_EXCEPTION;
						break;
					}
					x = e->me_value;
					if (x != NULL) {
						Py_INCREF(x);
						PUSH(x);
						DISPATCH();
					}
					goto load_global_error;
				}
			}
			/* This is the un-inlined version of the code above */
			x = PyDict_GetItem(f->f_globals, w);
			if (x == NULL) {
				x = PyDict_GetItem(f->f_builtins, w);
				if (x == NULL) {
				  load_global_error:
				  	_PyEval_RaiseForGlobalNameError(w);
					why = WHY_EXCEPTION;
					break;
				}
			}
			Py_INCREF(x);
			PUSH(x);
			DISPATCH();

		TARGET(DELETE_FAST)
			x = GETLOCAL(oparg);
			if (x != NULL) {
				SETLOCAL(oparg, NULL);
				DISPATCH();
			}
			_PyEval_RaiseForUnboundLocal(f, oparg);
			why = WHY_EXCEPTION;
			break;

		TARGET(LOAD_CLOSURE)
			x = freevars[oparg];
			Py_INCREF(x);
			PUSH(x);
			DISPATCH();

		TARGET(LOAD_DEREF)
			x = freevars[oparg];
			w = PyCell_Get(x);
			if (w != NULL) {
				PUSH(w);
				DISPATCH();
			}
			why = WHY_EXCEPTION;
			/* Don't stomp existing exception */
			if (PyErr_Occurred())
				break;
			_PyEval_RaiseForUnboundFreeVar(f, oparg);
			break;

		TARGET(STORE_DEREF)
			w = POP();
			x = freevars[oparg];
			PyCell_Set(x, w);
			Py_DECREF(w);
			DISPATCH();

		TARGET(BUILD_TUPLE)
			x = PyTuple_New(oparg);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			for (; --oparg >= 0;) {
				w = POP();
				PyTuple_SET_ITEM(x, oparg, w);
			}
			PUSH(x);
			DISPATCH();

		TARGET(BUILD_LIST)
			x = PyList_New(oparg);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			for (; --oparg >= 0;) {
				w = POP();
				PyList_SET_ITEM(x, oparg, w);
			}
			PUSH(x);
			DISPATCH();

		TARGET(BUILD_MAP)
			x = _PyDict_NewPresized((Py_ssize_t)oparg);
			PUSH(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(STORE_MAP)
			w = TOP();     /* key */
			u = SECOND();  /* value */
			v = THIRD();   /* dict */
			STACKADJ(-2);
			assert (PyDict_CheckExact(v));
			err = PyDict_SetItem(v, w, u);  /* v[w] = u */
			Py_DECREF(u);
			Py_DECREF(w);
			if (err != 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(LOAD_ATTR)
			w = GETITEM(names, oparg);
			v = TOP();
			x = PyObject_GetAttr(v, w);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(COMPARE_OP)
			w = POP();
			v = TOP();
			if (PyInt_CheckExact(w) && PyInt_CheckExact(v)) {
				/* INLINE: cmp(int, int) */
				register long a, b;
				register int res;
				a = PyInt_AS_LONG(v);
				b = PyInt_AS_LONG(w);
				switch (oparg) {
				case PyCmp_LT: res = a <  b; break;
				case PyCmp_LE: res = a <= b; break;
				case PyCmp_EQ: res = a == b; break;
				case PyCmp_NE: res = a != b; break;
				case PyCmp_GT: res = a >  b; break;
				case PyCmp_GE: res = a >= b; break;
				case PyCmp_IS: res = v == w; break;
				case PyCmp_IS_NOT: res = v != w; break;
				default: goto slow_compare;
				}
				x = res ? Py_True : Py_False;
				Py_INCREF(x);
			}
			else {
			  slow_compare:
				x = cmp_outcome(oparg, v, w);
			}
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			PREDICT(POP_JUMP_IF_FALSE);
			PREDICT(POP_JUMP_IF_TRUE);
			DISPATCH();

		TARGET(JUMP_FORWARD)
			JUMPBY(oparg);
			FAST_DISPATCH();

		PREDICTED_WITH_ARG(POP_JUMP_IF_FALSE);
		TARGET(POP_JUMP_IF_FALSE)
			w = POP();
			if (w == Py_True) {
				Py_DECREF(w);;
				FAST_DISPATCH();
			}
			if (w == Py_False) {
				Py_DECREF(w);
				JUMPTO(oparg);
				FAST_DISPATCH();
			}
			err = PyObject_IsTrue(w);
			Py_DECREF(w);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			else if (err == 0)
				JUMPTO(oparg);
			DISPATCH();

		PREDICTED_WITH_ARG(POP_JUMP_IF_TRUE);
		TARGET(POP_JUMP_IF_TRUE)
			w = POP();
			if (w == Py_False) {
				Py_DECREF(w);
				FAST_DISPATCH();
			}
			if (w == Py_True) {
				Py_DECREF(w);
				JUMPTO(oparg);
				FAST_DISPATCH();
			}
			err = PyObject_IsTrue(w);
			Py_DECREF(w);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			else if (err > 0) {
				JUMPTO(oparg);
			}
			DISPATCH();

		TARGET(JUMP_IF_FALSE_OR_POP)
			w = TOP();
			if (w == Py_True) {
				STACKADJ(-1);
				Py_DECREF(w);
				FAST_DISPATCH();
			}
			if (w == Py_False) {
				JUMPTO(oparg);
				FAST_DISPATCH();
			}
			err = PyObject_IsTrue(w);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			else if (err > 0) {
				STACKADJ(-1);
				Py_DECREF(w);
			}
			else
				JUMPTO(oparg);
			DISPATCH();

		TARGET(JUMP_IF_TRUE_OR_POP)
			w = TOP();
			if (w == Py_False) {
				STACKADJ(-1);
				Py_DECREF(w);
				FAST_DISPATCH();
			}
			if (w == Py_True) {
				JUMPTO(oparg);
				FAST_DISPATCH();
			}
			err = PyObject_IsTrue(w);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			else if (err > 0) {
				JUMPTO(oparg);
			}
			else {
				STACKADJ(-1);
				Py_DECREF(w);
			}
			DISPATCH();

		PREDICTED_WITH_ARG(JUMP_ABSOLUTE);
		TARGET(JUMP_ABSOLUTE)
			JUMPTO(oparg);
#if FAST_LOOPS
			/* Enabling this path speeds-up all while and for-loops by bypassing
                           the per-loop checks for signals.  By default, this should be turned-off
                           because it prevents detection of a control-break in tight loops like
                           "while 1: pass".  Compile with this option turned-on when you need
                           the speed-up and do not need break checking inside tight loops (ones
                           that contain only instructions ending with goto fast_next_opcode).
                        */
			FAST_DISPATCH();
#else
			DISPATCH();
#endif

		TARGET(GET_ITER)
			/* before: [obj]; after [getiter(obj)] */
			v = TOP();
			x = PyObject_GetIter(v);
			Py_DECREF(v);
			if (x == NULL) {
				STACKADJ(-1);
				why = WHY_EXCEPTION;
				break;
			}
			SET_TOP(x);
			PREDICT(FOR_ITER);
			DISPATCH();

		PREDICTED_WITH_ARG(FOR_ITER);
		TARGET(FOR_ITER)
			/* before: [iter]; after: [iter, iter()] *or* [] */
			v = TOP();
			x = (*v->ob_type->tp_iternext)(v);
			if (x != NULL) {
				PUSH(x);
				PREDICT(STORE_FAST);
				PREDICT(UNPACK_SEQUENCE);
				DISPATCH();
			}
			if (PyErr_Occurred()) {
				if (!PyErr_ExceptionMatches(
						PyExc_StopIteration)) {
					why = WHY_EXCEPTION;
					break;
				}
				PyErr_Clear();
			}
			/* iterator ended normally */
 			v = POP();
			Py_DECREF(v);
			JUMPBY(oparg);
			DISPATCH();

		TARGET(BREAK_LOOP)
			why = WHY_BREAK;
			goto fast_block_end;

		TARGET(CONTINUE_LOOP)
			retval = PyInt_FromLong(oparg);
			if (!retval) {
				why = WHY_EXCEPTION;
				break;
			}
			why = WHY_CONTINUE;
			goto fast_block_end;

		TARGET_WITH_IMPL(SETUP_LOOP, _setup_finally)
		TARGET_WITH_IMPL(SETUP_EXCEPT, _setup_finally)
		TARGET(SETUP_FINALLY)
		_setup_finally:
			/* NOTE: If you add any new block-setup opcodes that
		           are not try/except/finally handlers, you may need
		           to update the PyGen_NeedsFinalizing() function.
		           */

			PyFrame_BlockSetup(f, opcode, INSTR_OFFSET() + oparg,
					   STACK_LEVEL());
			DISPATCH();

		TARGET(WITH_CLEANUP)
		{
			/* At the top of the stack are 1-3 values indicating
			   how/why we entered the finally clause:
			   - TOP = None
			   - (TOP, SECOND) = (WHY_{RETURN,CONTINUE}), retval
			   - TOP = WHY_*; no retval below it
			   - (TOP, SECOND, THIRD) = exc_info()
			   Below them is EXIT, the context.__exit__ bound method.
			   In the last case, we must call
			     EXIT(TOP, SECOND, THIRD)
			   otherwise we must call
			     EXIT(None, None, None)

			   In all cases, we remove EXIT from the stack, leaving
			   the rest in the same order.

			   In addition, if the stack represents an exception,
			   *and* the function call returns a 'true' value, we
			   "zap" this information, to prevent END_FINALLY from
			   re-raising the exception.  (But non-local gotos
			   should still be resumed.)
			*/

			PyObject *exit_func;

			u = POP();
			if (u == Py_None) {
			       	exit_func = TOP();
				SET_TOP(u);
				v = w = Py_None;
			}
			else if (PyInt_Check(u)) {
				switch(PyInt_AS_LONG(u)) {
				case WHY_RETURN:
				case WHY_CONTINUE:
					/* Retval in TOP. */
					exit_func = SECOND();
					SET_SECOND(TOP());
					SET_TOP(u);
					break;
				default:
					exit_func = TOP();
					SET_TOP(u);
					break;
				}
				u = v = w = Py_None;
			}
			else {
				v = TOP();
				w = SECOND();
				exit_func = THIRD();
				SET_TOP(u);
				SET_SECOND(v);
				SET_THIRD(w);
			}
			/* XXX Not the fastest way to call it... */
			x = PyObject_CallFunctionObjArgs(exit_func, u, v, w,
							 NULL);
			Py_DECREF(exit_func);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break; /* Go to error exit */
			}

			if (u != Py_None)
				err = PyObject_IsTrue(x);
			else
				err = 0;
			Py_DECREF(x);

			if (err < 0) {
				why = WHY_EXCEPTION;
				break; /* Go to error exit */
			}
			else if (err > 0) {
				/* There was an exception and a true return */
				STACKADJ(-2);
				Py_INCREF(Py_None);
				SET_TOP(Py_None);
				Py_DECREF(u);
				Py_DECREF(v);
				Py_DECREF(w);
			} else {
				/* The stack was rearranged to remove EXIT
				   above. Let END_FINALLY do its thing */
			}
			PREDICT(END_FINALLY);
			DISPATCH();
		}

		TARGET(CALL_FUNCTION)
		{
			PyObject **sp;
			PCALL(PCALL_ALL);
			sp = stack_pointer;
#ifdef WITH_TSC
			x = _PyEval_CallFunction(&sp, oparg, &intr0, &intr1);
#else
			x = _PyEval_CallFunction(&sp, oparg);
#endif
			stack_pointer = sp;
			PUSH(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();
		}

		TARGET(CALL_FUNCTION_VAR)
			err = _PyEval_CallFunctionVarKw(&stack_pointer, oparg,
			                                CALL_FLAG_VAR);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(CALL_FUNCTION_KW)
			err = _PyEval_CallFunctionVarKw(&stack_pointer, oparg,
			                                CALL_FLAG_KW);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(CALL_FUNCTION_VAR_KW)
			err = _PyEval_CallFunctionVarKw(&stack_pointer, oparg,
					        CALL_FLAG_VAR | CALL_FLAG_KW);
			if (err < 0) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(MAKE_CLOSURE)
		{
			v = POP(); /* code object */
			x = PyFunction_New(v, f->f_globals);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			Py_DECREF(v);
			if (x != NULL) {
				v = POP();
				if (PyFunction_SetClosure(x, v) != 0) {
					/* Can't happen unless bytecode is corrupt. */
					why = WHY_EXCEPTION;
					Py_DECREF(x);
					x = NULL;
				}
				Py_DECREF(v);
			}
			if (x != NULL && oparg > 0) {
				v = PyTuple_New(oparg);
				if (v == NULL) {
					Py_DECREF(x);
					why = WHY_EXCEPTION;
					break;
				}
				while (--oparg >= 0) {
					w = POP();
					PyTuple_SET_ITEM(v, oparg, w);
				}
				if (PyFunction_SetDefaults(x, v) != 0) {
					/* Can't happen unless
                                           PyFunction_SetDefaults changes. */
					why = WHY_EXCEPTION;
					Py_DECREF(v);
					break;
				}
				Py_DECREF(v);
			}
			PUSH(x);
			DISPATCH();
		}

		TARGET(BUILD_SLICE_TWO)
			v = POP();
			u = TOP();
			x = PySlice_New(u, v, NULL);
			Py_DECREF(u);
			Py_DECREF(v);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(BUILD_SLICE_THREE)
			w = POP();
			v = POP();
			u = TOP();
			x = PySlice_New(u, v, w);
			Py_DECREF(u);
			Py_DECREF(v);
			Py_DECREF(w);
			SET_TOP(x);
			if (x == NULL) {
				why = WHY_EXCEPTION;
				break;
			}
			DISPATCH();

		TARGET(EXTENDED_ARG)
			opcode = NEXTOP();
			oparg = oparg<<16 | NEXTARG();
			goto dispatch_opcode;

#ifdef USE_COMPUTED_GOTOS
		_unknown_opcode:
#endif
		default:
			fprintf(stderr,
				"XXX lineno: %d, opcode: %d\n",
				PyFrame_GetLineNumber(f),
				opcode);
			PyErr_SetString(PyExc_SystemError, "unknown opcode");
			why = WHY_EXCEPTION;
			break;

#ifdef CASE_TOO_BIG
		}
#endif

		} /* switch */

	    on_error:

		READ_TIMESTAMP(inst1);

		/* Quickly continue if no error occurred */

		if (why == WHY_NOT) {
#ifdef CHECKEXC
			/* This check is expensive! */
			if (PyErr_Occurred()) {
				fprintf(stderr,
					"XXX undetected error\n");
				why = WHY_EXCEPTION;
			}
			else {
#endif
				READ_TIMESTAMP(loop1);
				continue; /* Normal, fast path */
#ifdef CHECKEXC
			}
#endif
		}

		/* Double-check exception status */

		if (why == WHY_EXCEPTION || why == WHY_RERAISE) {
			if (!PyErr_Occurred()) {
				PyErr_SetString(PyExc_SystemError,
					"error return without exception set");
				why = WHY_EXCEPTION;
			}
		}
#ifdef CHECKEXC
		else {
			/* This check is expensive! */
			if (PyErr_Occurred()) {
				char buf[128];
				sprintf(buf, "Stack unwind with exception "
					"set and why=%d", why);
				Py_FatalError(buf);
			}
		}
#endif

		/* Log traceback info if this is a real exception */

		if (why == WHY_EXCEPTION) {
			PyTraceBack_Here(f);

			if (tstate->c_tracefunc != NULL)
				call_exc_trace(tstate->c_tracefunc,
					       tstate->c_traceobj, f);
		}

		/* For the rest, treat WHY_RERAISE as WHY_EXCEPTION */

		if (why == WHY_RERAISE)
			why = WHY_EXCEPTION;

		/* Unwind stacks if a (pseudo) exception occurred */

fast_block_end:
		while (why != WHY_NOT && f->f_iblock > 0) {
			PyTryBlock *b = PyFrame_BlockPop(f);

			assert(why != WHY_YIELD);
			if (b->b_type == SETUP_LOOP && why == WHY_CONTINUE) {
				/* For a continue inside a try block,
				   don't pop the block for the loop. */
				PyFrame_BlockSetup(f, b->b_type, b->b_handler,
						   b->b_level);
				why = WHY_NOT;
				JUMPTO(PyInt_AS_LONG(retval));
				Py_DECREF(retval);
				break;
			}

			while (STACK_LEVEL() > b->b_level) {
				v = POP();
				Py_XDECREF(v);
			}
			if (b->b_type == SETUP_LOOP && why == WHY_BREAK) {
				why = WHY_NOT;
				JUMPTO(b->b_handler);
				break;
			}
			if (b->b_type == SETUP_FINALLY ||
			    (b->b_type == SETUP_EXCEPT &&
			     why == WHY_EXCEPTION)) {
				if (why == WHY_EXCEPTION) {
					/* Keep this in sync with
					   _PyLlvm_WrapEnterExceptOrFinally
					   in llvm_fbuilder.cc. */
					PyObject *exc, *val, *tb;
					PyErr_Fetch(&exc, &val, &tb);
					if (val == NULL) {
						val = Py_None;
						Py_INCREF(val);
					}
					/* Make the raw exception data
					   available to the handler,
					   so a program can emulate the
					   Python main loop.  Don't do
					   this for 'finally'. */
					if (b->b_type == SETUP_EXCEPT) {
						PyErr_NormalizeException(
							&exc, &val, &tb);
						_PyEval_SetExcInfo(tstate,
							     exc, val, tb);
					}
					if (tb == NULL) {
						Py_INCREF(Py_None);
						PUSH(Py_None);
					} else
						PUSH(tb);
					PUSH(val);
					PUSH(exc);
					/* Within the except or finally block,
					   PyErr_Occurred() should be false.
					   END_FINALLY will restore the
					   exception if necessary. */
					PyErr_Clear();
				}
				else {
					if (why & (WHY_RETURN | WHY_CONTINUE))
						PUSH(retval);
					v = PyInt_FromLong((long)why);
					PUSH(v);
				}
				why = WHY_NOT;
				JUMPTO(b->b_handler);
				break;
			}
		} /* unwind stack */

		/* End the loop if we still have an error (or return) */

		if (why != WHY_NOT)
			break;
		READ_TIMESTAMP(loop1);

	} /* main loop */

	assert(why != WHY_YIELD);
	/* Pop remaining stack entries. */
	while (!EMPTY()) {
		v = POP();
		Py_XDECREF(v);
	}

	if (why != WHY_RETURN)
		retval = NULL;

fast_yield:
	if (tstate->use_tracing) {
		if (tstate->c_tracefunc) {
			if (why == WHY_RETURN || why == WHY_YIELD) {
				if (call_trace(tstate->c_tracefunc,
					       tstate->c_traceobj, f,
					       PyTrace_RETURN, retval)) {
					Py_XDECREF(retval);
					retval = NULL;
					why = WHY_EXCEPTION;
				}
			}
			else if (why == WHY_EXCEPTION) {
				call_trace_protected(tstate->c_tracefunc,
						     tstate->c_traceobj, f,
						     PyTrace_RETURN, NULL);
			}
		}
		if (tstate->c_profilefunc) {
			if (why == WHY_EXCEPTION)
				call_trace_protected(tstate->c_profilefunc,
						     tstate->c_profileobj, f,
						     PyTrace_RETURN, NULL);
			else if (call_trace(tstate->c_profilefunc,
					    tstate->c_profileobj, f,
					    PyTrace_RETURN, retval)) {
				Py_XDECREF(retval);
				retval = NULL;
				why = WHY_EXCEPTION;
			}
		}
	}

	if (tstate->frame->f_exc_type != NULL)
		_PyEval_ResetExcInfo(tstate);
	else {
		assert(tstate->frame->f_exc_value == NULL);
		assert(tstate->frame->f_exc_traceback == NULL);
	}

	/* pop frame */
exit_eval_frame:
	Py_LeaveRecursiveCall();
	tstate->frame = f->f_back;

	return retval;
}

/* This is gonna seem *real weird*, but if you put some other code between
   PyEval_EvalFrame() and PyEval_EvalCodeEx() you will need to adjust
   the test in the if statements in Misc/gdbinit (pystack and pystackv). */

PyObject *
PyEval_EvalCodeEx(PyCodeObject *co, PyObject *globals, PyObject *locals,
	   PyObject **args, int argcount, PyObject **kws, int kwcount,
	   PyObject **defs, int defcount, PyObject *closure)
{
	register PyFrameObject *f;
	register PyObject *retval = NULL;
	register PyObject **fastlocals, **freevars;
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *x, *u;

	if (globals == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"PyEval_EvalCodeEx: NULL globals");
		return NULL;
	}

	assert(tstate != NULL);
	assert(globals != NULL);
	f = PyFrame_New(tstate, co, globals, locals);
	if (f == NULL)
		return NULL;

	fastlocals = f->f_localsplus;
	freevars = f->f_localsplus + co->co_nlocals;

	if (co->co_argcount > 0 ||
	    co->co_flags & (CO_VARARGS | CO_VARKEYWORDS)) {
		int i;
		int n = argcount;
		PyObject *kwdict = NULL;
		if (co->co_flags & CO_VARKEYWORDS) {
			kwdict = PyDict_New();
			if (kwdict == NULL)
				goto fail;
			i = co->co_argcount;
			if (co->co_flags & CO_VARARGS)
				i++;
			SETLOCAL(i, kwdict);
		}
		if (argcount > co->co_argcount) {
			if (!(co->co_flags & CO_VARARGS)) {
				PyErr_Format(PyExc_TypeError,
				    "%.200s() takes %s %d "
				    "%sargument%s (%d given)",
				    PyString_AsString(co->co_name),
				    defcount ? "at most" : "exactly",
				    co->co_argcount,
				    kwcount ? "non-keyword " : "",
				    co->co_argcount == 1 ? "" : "s",
				    argcount);
				goto fail;
			}
			n = co->co_argcount;
		}
		for (i = 0; i < n; i++) {
			x = args[i];
			Py_INCREF(x);
			SETLOCAL(i, x);
		}
		if (co->co_flags & CO_VARARGS) {
			u = PyTuple_New(argcount - n);
			if (u == NULL)
				goto fail;
			SETLOCAL(co->co_argcount, u);
			for (i = n; i < argcount; i++) {
				x = args[i];
				Py_INCREF(x);
				PyTuple_SET_ITEM(u, i-n, x);
			}
		}
		for (i = 0; i < kwcount; i++) {
			PyObject **co_varnames;
			PyObject *keyword = kws[2*i];
			PyObject *value = kws[2*i + 1];
			int j;
			if (keyword == NULL || !PyString_Check(keyword)) {
				PyErr_Format(PyExc_TypeError,
				    "%.200s() keywords must be strings",
				    PyString_AsString(co->co_name));
				goto fail;
			}
			/* Speed hack: do raw pointer compares. As names are
			   normally interned this should almost always hit. */
			co_varnames = PySequence_Fast_ITEMS(co->co_varnames);
			for (j = 0; j < co->co_argcount; j++) {
				PyObject *nm = co_varnames[j];
				if (nm == keyword)
					goto kw_found;
			}
			/* Slow fallback, just in case */
			for (j = 0; j < co->co_argcount; j++) {
				PyObject *nm = co_varnames[j];
				int cmp = PyObject_RichCompareBool(
					keyword, nm, Py_EQ);
				if (cmp > 0)
					goto kw_found;
				else if (cmp < 0)
					goto fail;
			}
			/* Check errors from Compare */
			if (PyErr_Occurred())
				goto fail;
			if (j >= co->co_argcount) {
				if (kwdict == NULL) {
					PyErr_Format(PyExc_TypeError,
					    "%.200s() got an unexpected "
					    "keyword argument '%.400s'",
					    PyString_AsString(co->co_name),
					    PyString_AsString(keyword));
					goto fail;
				}
				PyDict_SetItem(kwdict, keyword, value);
				continue;
			}
kw_found:
			if (GETLOCAL(j) != NULL) {
				PyErr_Format(PyExc_TypeError,
						"%.200s() got multiple "
						"values for keyword "
						"argument '%.400s'",
						PyString_AsString(co->co_name),
						PyString_AsString(keyword));
				goto fail;
			}
			Py_INCREF(value);
			SETLOCAL(j, value);
		}
		if (argcount < co->co_argcount) {
			int m = co->co_argcount - defcount;
			for (i = argcount; i < m; i++) {
				if (GETLOCAL(i) == NULL) {
					PyErr_Format(PyExc_TypeError,
					    "%.200s() takes %s %d "
					    "%sargument%s (%d given)",
					    PyString_AsString(co->co_name),
					    ((co->co_flags & CO_VARARGS) ||
					     defcount) ? "at least"
						       : "exactly",
					    m, kwcount ? "non-keyword " : "",
					    m == 1 ? "" : "s", i);
					goto fail;
				}
			}
			if (n > m)
				i = n - m;
			else
				i = 0;
			for (; i < defcount; i++) {
				if (GETLOCAL(m+i) == NULL) {
					PyObject *def = defs[i];
					Py_INCREF(def);
					SETLOCAL(m+i, def);
				}
			}
		}
	}
	else {
		if (argcount > 0 || kwcount > 0) {
			PyErr_Format(PyExc_TypeError,
				     "%.200s() takes no arguments (%d given)",
				     PyString_AsString(co->co_name),
				     argcount + kwcount);
			goto fail;
		}
	}
	/* Allocate and initialize storage for cell vars, and copy free
	   vars into frame.  This isn't too efficient right now. */
	if (PyTuple_GET_SIZE(co->co_cellvars)) {
		int i, j, nargs, found;
		char *cellname, *argname;
		PyObject *c;

		nargs = co->co_argcount;
		if (co->co_flags & CO_VARARGS)
			nargs++;
		if (co->co_flags & CO_VARKEYWORDS)
			nargs++;

		/* Initialize each cell var, taking into account
		   cell vars that are initialized from arguments.

		   Should arrange for the compiler to put cellvars
		   that are arguments at the beginning of the cellvars
		   list so that we can march over it more efficiently?
		*/
		for (i = 0; i < PyTuple_GET_SIZE(co->co_cellvars); ++i) {
			cellname = PyString_AS_STRING(
				PyTuple_GET_ITEM(co->co_cellvars, i));
			found = 0;
			for (j = 0; j < nargs; j++) {
				argname = PyString_AS_STRING(
					PyTuple_GET_ITEM(co->co_varnames, j));
				if (strcmp(cellname, argname) == 0) {
					c = PyCell_New(GETLOCAL(j));
					if (c == NULL)
						goto fail;
					GETLOCAL(co->co_nlocals + i) = c;
					found = 1;
					break;
				}
			}
			if (found == 0) {
				c = PyCell_New(NULL);
				if (c == NULL)
					goto fail;
				SETLOCAL(co->co_nlocals + i, c);
			}
		}
	}
	if (PyTuple_GET_SIZE(co->co_freevars)) {
		int i;
		for (i = 0; i < PyTuple_GET_SIZE(co->co_freevars); ++i) {
			PyObject *o = PyTuple_GET_ITEM(closure, i);
			Py_INCREF(o);
			freevars[PyTuple_GET_SIZE(co->co_cellvars) + i] = o;
		}
	}

	if (co->co_flags & CO_GENERATOR) {
		/* Don't need to keep the reference to f_back, it will be set
		 * when the generator is resumed. */
		Py_XDECREF(f->f_back);
		f->f_back = NULL;

		PCALL(PCALL_GENERATOR);

		/* Create a new generator that owns the ready to run frame
		 * and return that as the value. */
		return PyGen_New(f);
	}

	retval = PyEval_EvalFrame(f);

fail: /* Jump here from prelude on failure */

	/* decref'ing the frame can cause __del__ methods to get invoked,
	   which can call back into Python.  While we're done with the
	   current Python frame (f), the associated C stack is still in use,
	   so recursion_depth must be boosted for the duration.
	*/
	assert(tstate != NULL);
	++tstate->recursion_depth;
	Py_DECREF(f);
	--tstate->recursion_depth;
	return retval;
}


/* Implementation notes for _PyEval_SetExcInfo() and _PyEval_ResetExcInfo():

- Below, 'exc_ZZZ' stands for 'exc_type', 'exc_value' and
  'exc_traceback'.  These always travel together.

- tstate->curexc_ZZZ is the "hot" exception that is set by
  PyErr_SetString(), cleared by PyErr_Clear(), and so on.

- Once an exception is caught by an except clause, it is retrieved
  from tstate->curexc_ZZZ by PyErr_Fetch() and set into
  tstate->exc_ZZZ by _PyEval_SetExcInfo(), from which sys.exc_info()
  can pick it up.  This is the primary task of _PyEval_SetExcInfo().

- Now let me explain the complicated dance with frame->f_exc_ZZZ.

  Long ago, when none of this existed, there were just a few globals:
  one set corresponding to the "hot" exception, and one set
  corresponding to sys.exc_ZZZ.  (Actually, the latter weren't C
  globals; they were simply stored as sys.exc_ZZZ.  For backwards
  compatibility, they still are!)  The problem was that in code like
  this:

     try:
	"something that may fail"
     except "some exception":
	"do something else first"
	"print the exception from sys.exc_ZZZ."

  if "do something else first" invoked something that raised and caught
  an exception, sys.exc_ZZZ were overwritten.  That was a frequent
  cause of subtle bugs.  I fixed this by changing the semantics as
  follows:

    - Within one frame, sys.exc_ZZZ will hold the last exception caught
      *in that frame*.

    - But initially, and as long as no exception is caught in a given
      frame, sys.exc_ZZZ will hold the last exception caught in the
      previous frame (or the frame before that, etc.).

  The first bullet fixed the bug in the above example.  The second
  bullet was for backwards compatibility: it was (and is) common to
  have a function that is called when an exception is caught, and to
  have that function access the caught exception via sys.exc_ZZZ.
  (Example: traceback.print_exc()).

  At the same time I fixed the problem that sys.exc_ZZZ weren't
  thread-safe, by introducing sys.exc_info() which gets it from tstate;
  but that's really a separate improvement.

  The _PyEval_ResetExcInfo() function in eval.cc restores the
  tstate->exc_ZZZ variables to what they were before the current frame
  was called.  The _PyEval_SetExcInfo() function saves them on the
  frame so that _PyEval_ResetExcInfo() can restore them.  The
  invariant is that frame->f_exc_ZZZ is NULL iff the current frame
  never caught an exception (where "catching" an exception applies
  only to successful except clauses); and if the current frame ever
  caught an exception, frame->f_exc_ZZZ is the exception that was
  stored in tstate->exc_ZZZ at the start of the current frame.

*/

void
_PyEval_SetExcInfo(PyThreadState *tstate,
		   PyObject *type, PyObject *value, PyObject *tb)
{
	PyFrameObject *frame = tstate->frame;
	PyObject *tmp_type, *tmp_value, *tmp_tb;

	assert(type != NULL);
	assert(frame != NULL);
	if (frame->f_exc_type == NULL) {
		assert(frame->f_exc_value == NULL);
		assert(frame->f_exc_traceback == NULL);
		/* This frame didn't catch an exception before. */
		/* Save previous exception of this thread in this frame. */
		if (tstate->exc_type == NULL) {
			/* XXX Why is this set to Py_None? */
			Py_INCREF(Py_None);
			tstate->exc_type = Py_None;
		}
		Py_INCREF(tstate->exc_type);
		Py_XINCREF(tstate->exc_value);
		Py_XINCREF(tstate->exc_traceback);
		frame->f_exc_type = tstate->exc_type;
		frame->f_exc_value = tstate->exc_value;
		frame->f_exc_traceback = tstate->exc_traceback;
	}
	/* Set new exception for this thread. */
	tmp_type = tstate->exc_type;
	tmp_value = tstate->exc_value;
	tmp_tb = tstate->exc_traceback;
	Py_INCREF(type);
	Py_XINCREF(value);
	Py_XINCREF(tb);
	tstate->exc_type = type;
	tstate->exc_value = value;
	tstate->exc_traceback = tb;
	Py_XDECREF(tmp_type);
	Py_XDECREF(tmp_value);
	Py_XDECREF(tmp_tb);
	/* For b/w compatibility */
	PySys_SetObject("exc_type", type);
	PySys_SetObject("exc_value", value);
	PySys_SetObject("exc_traceback", tb);
}

void
_PyEval_ResetExcInfo(PyThreadState *tstate)
{
	PyFrameObject *frame;
	PyObject *tmp_type, *tmp_value, *tmp_tb;

	/* It's a precondition that the thread state's frame caught an
	 * exception -- verify in a debug build.
	 */
	assert(tstate != NULL);
	frame = tstate->frame;
	assert(frame != NULL);
	assert(frame->f_exc_type != NULL);

	/* Copy the frame's exception info back to the thread state. */
	tmp_type = tstate->exc_type;
	tmp_value = tstate->exc_value;
	tmp_tb = tstate->exc_traceback;
	Py_INCREF(frame->f_exc_type);
	Py_XINCREF(frame->f_exc_value);
	Py_XINCREF(frame->f_exc_traceback);
	tstate->exc_type = frame->f_exc_type;
	tstate->exc_value = frame->f_exc_value;
	tstate->exc_traceback = frame->f_exc_traceback;
	Py_XDECREF(tmp_type);
	Py_XDECREF(tmp_value);
	Py_XDECREF(tmp_tb);

	/* For b/w compatibility */
	PySys_SetObject("exc_type", frame->f_exc_type);
	PySys_SetObject("exc_value", frame->f_exc_value);
	PySys_SetObject("exc_traceback", frame->f_exc_traceback);

	/* Clear the frame's exception info. */
	tmp_type = frame->f_exc_type;
	tmp_value = frame->f_exc_value;
	tmp_tb = frame->f_exc_traceback;
	frame->f_exc_type = NULL;
	frame->f_exc_value = NULL;
	frame->f_exc_traceback = NULL;
	Py_DECREF(tmp_type);
	Py_XDECREF(tmp_value);
	Py_XDECREF(tmp_tb);
}

/* Logic for the raise statement (too complicated for inlining).
   This *consumes* a reference count to each of its arguments. */
static enum why_code
do_raise(PyObject *type, PyObject *value, PyObject *tb)
{
	if (type == NULL) {
		/* Reraise */
		PyThreadState *tstate = PyThreadState_GET();
		type = tstate->exc_type == NULL ? Py_None : tstate->exc_type;
		value = tstate->exc_value;
		tb = tstate->exc_traceback;
		Py_XINCREF(type);
		Py_XINCREF(value);
		Py_XINCREF(tb);
	}

	/* We support the following forms of raise:
	   raise <class>, <classinstance>
	   raise <class>, <argument tuple>
	   raise <class>, None
	   raise <class>, <argument>
	   raise <classinstance>, None
	   raise <string>, <object>
	   raise <string>, None

	   An omitted second argument is the same as None.

	   In addition, raise <tuple>, <anything> is the same as
	   raising the tuple's first item (and it better have one!);
	   this rule is applied recursively.

	   Finally, an optional third argument can be supplied, which
	   gives the traceback to be substituted (useful when
	   re-raising an exception after examining it).  */

	/* First, check the traceback argument, replacing None with
	   NULL. */
	if (tb == Py_None) {
		Py_DECREF(tb);
		tb = NULL;
	}
	else if (tb != NULL && !PyTraceBack_Check(tb)) {
		PyErr_SetString(PyExc_TypeError,
			   "raise: arg 3 must be a traceback or None");
		goto raise_error;
	}

	/* Next, replace a missing value with None */
	if (value == NULL) {
		value = Py_None;
		Py_INCREF(value);
	}

	/* Next, repeatedly, replace a tuple exception with its first item */
	while (PyTuple_Check(type) && PyTuple_Size(type) > 0) {
		PyObject *tmp = type;
		type = PyTuple_GET_ITEM(type, 0);
		Py_INCREF(type);
		Py_DECREF(tmp);
	}

	if (PyExceptionClass_Check(type))
		PyErr_NormalizeException(&type, &value, &tb);

	else if (PyExceptionInstance_Check(type)) {
		/* Raising an instance.  The value should be a dummy. */
		if (value != Py_None) {
			PyErr_SetString(PyExc_TypeError,
			  "instance exception may not have a separate value");
			goto raise_error;
		}
		else {
			/* Normalize to raise <class>, <instance> */
			Py_DECREF(value);
			value = type;
			type = PyExceptionInstance_Class(type);
			Py_INCREF(type);
		}
	}
	else {
		/* Not something you can raise.  You get an exception
		   anyway, just not what you specified :-) */
		PyErr_Format(PyExc_TypeError,
			"exceptions must be classes or instances, not %s",
			type->ob_type->tp_name);
		goto raise_error;
	}

	assert(PyExceptionClass_Check(type));
	if (Py_Py3kWarningFlag && PyClass_Check(type)) {
		if (PyErr_WarnEx(PyExc_DeprecationWarning,
				"exceptions must derive from BaseException "
				"in 3.x", 1) < 0)
			goto raise_error;
	}

	PyErr_Restore(type, value, tb);
	if (tb == NULL)
		return WHY_EXCEPTION;
	else
		return WHY_RERAISE;
 raise_error:
	Py_XDECREF(value);
	Py_XDECREF(type);
	Py_XDECREF(tb);
	return WHY_EXCEPTION;
}

/* Expose do_raise with a slightly different return value for
   llvm_fbuilder.cc; return 1 for WHY_RERAISE, 0 for WHY_EXCEPTION */
int
_PyEval_DoRaise(PyObject *type, PyObject *value, PyObject *tb)
{
	enum why_code reason = do_raise(type, value, tb);
	switch (reason) {
	case WHY_EXCEPTION:
		return 0;
	case WHY_RERAISE:
		return 1;
	default:
		PyErr_Format(PyExc_SystemError,
			     "Unhandled why_code %d in _PyEval_DoRaise()",
			     reason);
		return 0;
	}
}

/* Iterate iterable argcount times and store the results on the stack (by
   manipulating stack_pointer). The iterable must have exactly argcount
   items. Return 0 for success, -1 if error. */
int
_PyEval_UnpackIterable(PyObject *iterable, int argcount,
                       PyObject **stack_pointer)
{
	int i;
	PyObject *it;  /* iter(v) */
	PyObject *item;

	assert(iterable != NULL);

	it = PyObject_GetIter(iterable);
	if (it == NULL)
		return -1;

	for (i = 0; i < argcount; i++) {
		item = PyIter_Next(it);
		if (item == NULL) {
			/* Iterator done, via error or exhaustion. */
			if (!PyErr_Occurred()) {
				PyErr_Format(PyExc_ValueError,
					"need more than %d value%s to unpack",
					i, i == 1 ? "" : "s");
			}
			goto Error;
		}
		*--stack_pointer = item;
	}

	/* We better have exhausted the iterator now. */
	item = PyIter_Next(it);
	if (item == NULL) {
		if (PyErr_Occurred())
			goto Error;
		Py_DECREF(it);
		return 0;
	}
	Py_DECREF(item);
	PyErr_SetString(PyExc_ValueError, "too many values to unpack");
	/* fall through */
Error:
	for (; i > 0; i--, stack_pointer++)
		Py_DECREF(*stack_pointer);
	Py_XDECREF(it);
	return -1;
}


#ifdef LLTRACE
static int
prtrace(PyObject *v, char *str)
{
	printf("%s ", str);
	if (PyObject_Print(v, stdout, 0) != 0)
		PyErr_Clear(); /* Don't know what else to do */
	printf("\n");
	return 1;
}
#endif

static void
call_exc_trace(Py_tracefunc func, PyObject *self, PyFrameObject *f)
{
	PyObject *type, *value, *traceback, *arg;
	int err;
	PyErr_Fetch(&type, &value, &traceback);
	if (value == NULL) {
		value = Py_None;
		Py_INCREF(value);
	}
	arg = PyTuple_Pack(3, type, value, traceback);
	if (arg == NULL) {
		PyErr_Restore(type, value, traceback);
		return;
	}
	err = call_trace(func, self, f, PyTrace_EXCEPTION, arg);
	Py_DECREF(arg);
	if (err == 0)
		PyErr_Restore(type, value, traceback);
	else {
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
	}
}

static int
call_trace_protected(Py_tracefunc func, PyObject *obj, PyFrameObject *frame,
		     int what, PyObject *arg)
{
	PyObject *type, *value, *traceback;
	int err;
	PyErr_Fetch(&type, &value, &traceback);
	err = call_trace(func, obj, frame, what, arg);
	if (err == 0)
	{
		PyErr_Restore(type, value, traceback);
		return 0;
	}
	else {
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		return -1;
	}
}

static int
call_trace(Py_tracefunc func, PyObject *obj, PyFrameObject *frame,
	   int what, PyObject *arg)
{
	register PyThreadState *tstate = frame->f_tstate;
	int result;
	if (tstate->tracing)
		return 0;
	tstate->tracing++;
	tstate->use_tracing = 0;
	result = func(obj, frame, what, arg);
	tstate->use_tracing = ((tstate->c_tracefunc != NULL)
			       || (tstate->c_profilefunc != NULL));
	tstate->tracing--;
	return result;
}

PyObject *
_PyEval_CallTracing(PyObject *func, PyObject *args)
{
	PyFrameObject *frame = PyEval_GetFrame();
	PyThreadState *tstate = frame->f_tstate;
	int save_tracing = tstate->tracing;
	int save_use_tracing = tstate->use_tracing;
	PyObject *result;

	tstate->tracing = 0;
	tstate->use_tracing = ((tstate->c_tracefunc != NULL)
			       || (tstate->c_profilefunc != NULL));
	result = PyObject_Call(func, args, NULL);
	tstate->tracing = save_tracing;
	tstate->use_tracing = save_use_tracing;
	return result;
}

/* Returns nonzero on exception. */
static int
maybe_call_line_trace(Py_tracefunc func, PyObject *obj,
		      PyFrameObject *frame, int *instr_lb, int *instr_ub,
		      int *instr_prev)
{
	int result = 0;

        /* If the last instruction executed isn't in the current
           instruction window, reset the window.  If the last
           instruction happens to fall at the start of a line or if it
           represents a jump backwards, call the trace function.
        */
	if ((frame->f_lasti < *instr_lb || frame->f_lasti >= *instr_ub)) {
		int line;
		PyAddrPair bounds;

		line = PyCode_CheckLineNumber(frame->f_code, frame->f_lasti,
					      &bounds);
		if (line >= 0) {
			frame->f_lineno = line;
			result = call_trace(func, obj, frame,
					    PyTrace_LINE, Py_None);
		}
		*instr_lb = bounds.ap_lower;
		*instr_ub = bounds.ap_upper;
	}
	else if (frame->f_lasti <= *instr_prev) {
		result = call_trace(func, obj, frame, PyTrace_LINE, Py_None);
	}
	*instr_prev = frame->f_lasti;
	return result;
}

void
PyEval_SetProfile(Py_tracefunc func, PyObject *arg)
{
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *temp = tstate->c_profileobj;
	Py_XINCREF(arg);
	tstate->c_profilefunc = NULL;
	tstate->c_profileobj = NULL;
	/* Must make sure that tracing is not ignored if 'temp' is freed */
	tstate->use_tracing = tstate->c_tracefunc != NULL;
	Py_XDECREF(temp);
	tstate->c_profilefunc = func;
	tstate->c_profileobj = arg;
	/* Flag that tracing or profiling is turned on */
	tstate->use_tracing = (func != NULL) || (tstate->c_tracefunc != NULL);
}

void
PyEval_SetTrace(Py_tracefunc func, PyObject *arg)
{
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *temp = tstate->c_traceobj;
	_Py_TracingPossible += (func != NULL) - (tstate->c_tracefunc != NULL);
	Py_XINCREF(arg);
	tstate->c_tracefunc = NULL;
	tstate->c_traceobj = NULL;
	/* Must make sure that profiling is not ignored if 'temp' is freed */
	tstate->use_tracing = tstate->c_profilefunc != NULL;
	Py_XDECREF(temp);
	tstate->c_tracefunc = func;
	tstate->c_traceobj = arg;
	/* Flag that tracing or profiling is turned on */
	tstate->use_tracing = ((func != NULL)
			       || (tstate->c_profilefunc != NULL));
}

PyObject *
PyEval_GetBuiltins(void)
{
	PyFrameObject *current_frame = PyEval_GetFrame();
	if (current_frame == NULL)
		return PyThreadState_GET()->interp->builtins;
	else
		return current_frame->f_builtins;
}

PyObject *
PyEval_GetLocals(void)
{
	PyFrameObject *current_frame = PyEval_GetFrame();
	if (current_frame == NULL)
		return NULL;
	PyFrame_FastToLocals(current_frame);
	return current_frame->f_locals;
}

PyObject *
PyEval_GetGlobals(void)
{
	PyFrameObject *current_frame = PyEval_GetFrame();
	if (current_frame == NULL)
		return NULL;
	else
		return current_frame->f_globals;
}

PyFrameObject *
PyEval_GetFrame(void)
{
	PyThreadState *tstate = PyThreadState_GET();
	return _PyThreadState_GetFrame(tstate);
}

int
PyEval_GetRestricted(void)
{
	PyFrameObject *current_frame = PyEval_GetFrame();
	return current_frame == NULL ? 0 : PyFrame_IsRestricted(current_frame);
}

int
PyEval_MergeCompilerFlags(PyCompilerFlags *cf)
{
	PyFrameObject *current_frame = PyEval_GetFrame();
	int result = cf->cf_flags != 0;

	if (current_frame != NULL) {
		const int codeflags = current_frame->f_code->co_flags;
		const int compilerflags = codeflags & PyCF_MASK;
		if (compilerflags) {
			result = 1;
			cf->cf_flags |= compilerflags;
		}
#if 0 /* future keyword */
		if (codeflags & CO_GENERATOR_ALLOWED) {
			result = 1;
			cf->cf_flags |= CO_GENERATOR_ALLOWED;
		}
#endif
	}
	return result;
}

int
Py_FlushLine(void)
{
	PyObject *f = PySys_GetObject("stdout");
	if (f == NULL)
		return 0;
	if (!PyFile_SoftSpace(f, 0))
		return 0;
	return PyFile_WriteString("\n", f);
}


/* External interface to call any callable object.
   The arg must be a tuple or NULL. */

#undef PyEval_CallObject
/* for backward compatibility: export this interface */

PyObject *
PyEval_CallObject(PyObject *func, PyObject *arg)
{
	return PyEval_CallObjectWithKeywords(func, arg, (PyObject *)NULL);
}
#define PyEval_CallObject(func,arg) \
        PyEval_CallObjectWithKeywords(func, arg, (PyObject *)NULL)

PyObject *
PyEval_CallObjectWithKeywords(PyObject *func, PyObject *arg, PyObject *kw)
{
	PyObject *result;

	if (arg == NULL) {
		arg = PyTuple_New(0);
		if (arg == NULL)
			return NULL;
	}
	else if (!PyTuple_Check(arg)) {
		PyErr_SetString(PyExc_TypeError,
				"argument list must be a tuple");
		return NULL;
	}
	else
		Py_INCREF(arg);

	if (kw != NULL && !PyDict_Check(kw)) {
		PyErr_SetString(PyExc_TypeError,
				"keyword list must be a dictionary");
		Py_DECREF(arg);
		return NULL;
	}

	result = PyObject_Call(func, arg, kw);
	Py_DECREF(arg);
	return result;
}

const char *
PyEval_GetFuncName(PyObject *func)
{
	if (PyMethod_Check(func))
		return PyEval_GetFuncName(PyMethod_GET_FUNCTION(func));
	else if (PyFunction_Check(func))
		return PyString_AsString(((PyFunctionObject*)func)->func_name);
	else if (PyCFunction_Check(func))
		return ((PyCFunctionObject*)func)->m_ml->ml_name;
	else if (PyClass_Check(func))
		return PyString_AsString(((PyClassObject*)func)->cl_name);
	else if (PyInstance_Check(func)) {
		return PyString_AsString(
			((PyInstanceObject*)func)->in_class->cl_name);
	} else {
		return func->ob_type->tp_name;
	}
}

const char *
PyEval_GetFuncDesc(PyObject *func)
{
	if (PyMethod_Check(func))
		return "()";
	else if (PyFunction_Check(func))
		return "()";
	else if (PyCFunction_Check(func))
		return "()";
	else if (PyClass_Check(func))
		return " constructor";
	else if (PyInstance_Check(func)) {
		return " instance";
	} else {
		return " object";
	}
}

static void
err_args(PyObject *func, int flags, int nargs)
{
	if (flags & METH_NOARGS)
		PyErr_Format(PyExc_TypeError,
			     "%.200s() takes no arguments (%d given)",
			     ((PyCFunctionObject *)func)->m_ml->ml_name,
			     nargs);
	else
		PyErr_Format(PyExc_TypeError,
			     "%.200s() takes exactly one argument (%d given)",
			     ((PyCFunctionObject *)func)->m_ml->ml_name,
			     nargs);
}

#define C_TRACE(x, call) \
if (tstate->use_tracing && tstate->c_profilefunc) { \
	if (call_trace(tstate->c_profilefunc, \
		tstate->c_profileobj, \
		tstate->frame, PyTrace_C_CALL, \
		func)) { \
		x = NULL; \
	} \
	else { \
		x = call; \
		if (tstate->c_profilefunc != NULL) { \
			if (x == NULL) { \
				call_trace_protected(tstate->c_profilefunc, \
					tstate->c_profileobj, \
					tstate->frame, PyTrace_C_EXCEPTION, \
					func); \
				/* XXX should pass (type, value, tb) */ \
			} else { \
				if (call_trace(tstate->c_profilefunc, \
					tstate->c_profileobj, \
					tstate->frame, PyTrace_C_RETURN, \
					func)) { \
					Py_DECREF(x); \
					x = NULL; \
				} \
			} \
		} \
	} \
} else { \
	x = call; \
	}

PyObject *
_PyEval_CallFunction(PyObject ***pp_stack, int oparg
#ifdef WITH_TSC
		, uint64* pintr0, uint64* pintr1
#endif
		)
{
	int na = oparg & 0xff;
	int nk = (oparg>>8) & 0xff;
	int n = na + 2 * nk;
	PyObject **pfunc = (*pp_stack) - n - 1;
	PyObject *func = *pfunc;
	PyObject *x, *w;

	/* Always dispatch PyCFunction first, because these are
	   presumed to be the most frequent callable object.
	*/
	if (PyCFunction_Check(func) && nk == 0) {
		int flags = PyCFunction_GET_FLAGS(func);
		PyThreadState *tstate = PyThreadState_GET();

		PCALL(PCALL_CFUNCTION);
		if (flags & (METH_NOARGS | METH_O)) {
			PyCFunction meth = PyCFunction_GET_FUNCTION(func);
			PyObject *self = PyCFunction_GET_SELF(func);
			if (flags & METH_NOARGS && na == 0) {
				C_TRACE(x, (*meth)(self,NULL));
			}
			else if (flags & METH_O && na == 1) {
				PyObject *arg = EXT_POP(*pp_stack);
				C_TRACE(x, (*meth)(self,arg));
				Py_DECREF(arg);
			}
			else {
				err_args(func, flags, na);
				x = NULL;
			}
		}
		else {
			PyObject *callargs;
			callargs = load_args(pp_stack, na);
			READ_TIMESTAMP(*pintr0);
			C_TRACE(x, PyCFunction_Call(func,callargs,NULL));
			READ_TIMESTAMP(*pintr1);
			Py_XDECREF(callargs);
		}
	} else {
		if (PyMethod_Check(func) && PyMethod_GET_SELF(func) != NULL) {
			/* optimize access to bound methods */
			PyObject *self = PyMethod_GET_SELF(func);
			PCALL(PCALL_METHOD);
			PCALL(PCALL_BOUND_METHOD);
			Py_INCREF(self);
			func = PyMethod_GET_FUNCTION(func);
			Py_INCREF(func);
			Py_DECREF(*pfunc);
			*pfunc = self;
			na++;
			n++;
		} else
			Py_INCREF(func);
		READ_TIMESTAMP(*pintr0);
		if (PyFunction_Check(func))
			x = fast_function(func, pp_stack, n, na, nk);
		else
			x = do_call(func, pp_stack, na, nk);
		READ_TIMESTAMP(*pintr1);
		Py_DECREF(func);
	}

	/* Clear the stack of the function object.  Also removes
           the arguments in case they weren't consumed already
           (fast_function() and err_args() leave them on the stack).
	 */
	while ((*pp_stack) > pfunc) {
		w = EXT_POP(*pp_stack);
		Py_DECREF(w);
		PCALL(PCALL_POP);
	}
	return x;
}

/* In spite of the name, not much like _PyEval_CallFunction, because it
   pushes the result of the call onto the stack, and it's apparently okay
   for it to modify the stack pointer directly. Does return -1 on failure, 0
   on success. */
#ifdef WITH_TSC
int
_PyEval_CallFunctionVarKw(PyObject ***stack_pointer, int oparg,
                          uint64* pintr0, uint64* pintr1)
#else
int
_PyEval_CallFunctionVarKw(PyObject ***stack_pointer, int oparg, int flags)
#endif
{
	PyObject **pfunc, *func, **sp, *result;
	/* oparg is the number of positional arguments and the number of
	   keyword arguments bitpacked into one int. */
	int num_posargs = oparg & 0xff;
	int num_kwargs = (oparg >> 8) & 0xff;
	int num_stackitems = num_posargs + 2 * num_kwargs;
	PCALL(PCALL_ALL);
	if (flags & CALL_FLAG_VAR)
		num_stackitems++;
	if (flags & CALL_FLAG_KW)
		num_stackitems++;
	pfunc = *stack_pointer - num_stackitems - 1;
	func  = *pfunc;
	if (PyMethod_Check(func) && PyMethod_GET_SELF(func) != NULL) {
		/* If func is a bound method object, replace func on the
		   stack with its self, func itself with its function, and
		   pretend we were called with one extra positional
		   argument. */
		PyObject *self = PyMethod_GET_SELF(func);
		Py_INCREF(self);
		func = PyMethod_GET_FUNCTION(func);
		Py_INCREF(func);
		Py_DECREF(*pfunc);
		*pfunc = self;
		num_posargs++;
	}
	else
		Py_INCREF(func);
	sp = *stack_pointer;
	READ_TIMESTAMP(*pintr0);
	result = ext_do_call(func, &sp, flags, num_posargs, num_kwargs);
	READ_TIMESTAMP(*pintr1);
	*stack_pointer = sp;
	Py_DECREF(func);
	while (*stack_pointer > pfunc) {
		PyObject *item = EXT_POP(*stack_pointer);
		Py_DECREF(item);
	}
	EXT_PUSH(result, *stack_pointer);
	if (result == NULL)
		return -1;
	else
		return 0;
}

/* The fast_function() function optimize calls for which no argument
   tuple is necessary; the objects are passed directly from the stack.
   For the simplest case -- a function that takes only positional
   arguments and is called with only positional arguments -- it
   inlines the most primitive frame setup code from
   PyEval_EvalCodeEx(), which vastly reduces the checks that must be
   done before evaluating the frame.
*/

static PyObject *
fast_function(PyObject *func, PyObject ***pp_stack, int n, int na, int nk)
{
	PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE(func);
	PyObject *globals = PyFunction_GET_GLOBALS(func);
	PyObject *argdefs = PyFunction_GET_DEFAULTS(func);
	PyObject **d = NULL;
	int nd = 0;

	PCALL(PCALL_FUNCTION);
	PCALL(PCALL_FAST_FUNCTION);
	if (argdefs == NULL && co->co_argcount == n && nk==0 &&
	    co->co_flags == (CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE)) {
		PyFrameObject *f;
		PyObject *retval = NULL;
		PyThreadState *tstate = PyThreadState_GET();
		PyObject **fastlocals, **stack;
		int i;

		PCALL(PCALL_FASTER_FUNCTION);
		assert(globals != NULL);
		/* XXX Perhaps we should create a specialized
		   PyFrame_New() that doesn't take locals, but does
		   take builtins without sanity checking them.
		*/
		assert(tstate != NULL);
		f = PyFrame_New(tstate, co, globals, NULL);
		if (f == NULL)
			return NULL;

		fastlocals = f->f_localsplus;
		stack = (*pp_stack) - n;

		for (i = 0; i < n; i++) {
			Py_INCREF(*stack);
			fastlocals[i] = *stack++;
		}
		retval = PyEval_EvalFrame(f);
		++tstate->recursion_depth;
		Py_DECREF(f);
		--tstate->recursion_depth;
		return retval;
	}
	if (argdefs != NULL) {
		d = &PyTuple_GET_ITEM(argdefs, 0);
		nd = Py_SIZE(argdefs);
	}
	return PyEval_EvalCodeEx(co, globals,
				 (PyObject *)NULL, (*pp_stack)-n, na,
				 (*pp_stack)-2*nk, nk, d, nd,
				 PyFunction_GET_CLOSURE(func));
}

static PyObject *
update_keyword_args(PyObject *orig_kwdict, int nk, PyObject ***pp_stack,
                    PyObject *func)
{
	PyObject *kwdict = NULL;
	if (orig_kwdict == NULL)
		kwdict = PyDict_New();
	else {
		kwdict = PyDict_Copy(orig_kwdict);
		Py_DECREF(orig_kwdict);
	}
	if (kwdict == NULL)
		return NULL;
	while (--nk >= 0) {
		int err;
		PyObject *value = EXT_POP(*pp_stack);
		PyObject *key = EXT_POP(*pp_stack);
		if (PyDict_GetItem(kwdict, key) != NULL) {
			PyErr_Format(PyExc_TypeError,
				     "%.200s%s got multiple values "
				     "for keyword argument '%.200s'",
				     PyEval_GetFuncName(func),
				     PyEval_GetFuncDesc(func),
				     PyString_AsString(key));
			Py_DECREF(key);
			Py_DECREF(value);
			Py_DECREF(kwdict);
			return NULL;
		}
		err = PyDict_SetItem(kwdict, key, value);
		Py_DECREF(key);
		Py_DECREF(value);
		if (err) {
			Py_DECREF(kwdict);
			return NULL;
		}
	}
	return kwdict;
}

static PyObject *
update_star_args(int nstack, int nstar, PyObject *stararg,
		 PyObject ***pp_stack)
{
	PyObject *callargs, *w;

	callargs = PyTuple_New(nstack + nstar);
	if (callargs == NULL) {
		return NULL;
	}
	if (nstar) {
		int i;
		for (i = 0; i < nstar; i++) {
			PyObject *a = PyTuple_GET_ITEM(stararg, i);
			Py_INCREF(a);
			PyTuple_SET_ITEM(callargs, nstack + i, a);
		}
	}
	while (--nstack >= 0) {
		w = EXT_POP(*pp_stack);
		PyTuple_SET_ITEM(callargs, nstack, w);
	}
	return callargs;
}

static PyObject *
load_args(PyObject ***pp_stack, int na)
{
	PyObject *args = PyTuple_New(na);
	PyObject *w;

	if (args == NULL)
		return NULL;
	while (--na >= 0) {
		w = EXT_POP(*pp_stack);
		PyTuple_SET_ITEM(args, na, w);
	}
	return args;
}

static PyObject *
do_call(PyObject *func, PyObject ***pp_stack, int na, int nk)
{
	PyObject *callargs = NULL;
	PyObject *kwdict = NULL;
	PyObject *result = NULL;

	if (nk > 0) {
		kwdict = update_keyword_args(NULL, nk, pp_stack, func);
		if (kwdict == NULL)
			goto call_fail;
	}
	callargs = load_args(pp_stack, na);
	if (callargs == NULL)
		goto call_fail;
#ifdef CALL_PROFILE
	/* At this point, we have to look at the type of func to
	   update the call stats properly.  Do it here so as to avoid
	   exposing the call stats machinery outside eval.cc.
	*/
	if (PyFunction_Check(func))
		PCALL(PCALL_FUNCTION);
	else if (PyMethod_Check(func))
		PCALL(PCALL_METHOD);
	else if (PyType_Check(func))
		PCALL(PCALL_TYPE);
	else
		PCALL(PCALL_OTHER);
#endif
	result = PyObject_Call(func, callargs, kwdict);
 call_fail:
	Py_XDECREF(callargs);
	Py_XDECREF(kwdict);
	return result;
}

static PyObject *
ext_do_call(PyObject *func, PyObject ***pp_stack, int flags, int na, int nk)
{
	int nstar = 0;
	PyObject *callargs = NULL;
	PyObject *stararg = NULL;
	PyObject *kwdict = NULL;
	PyObject *result = NULL;

	if (flags & CALL_FLAG_KW) {
		kwdict = EXT_POP(*pp_stack);
		if (!PyDict_Check(kwdict)) {
			PyObject *d;
			d = PyDict_New();
			if (d == NULL)
				goto ext_call_fail;
			if (PyDict_Update(d, kwdict) != 0) {
				Py_DECREF(d);
				/* PyDict_Update raises attribute
				 * error (percolated from an attempt
				 * to get 'keys' attribute) instead of
				 * a type error if its second argument
				 * is not a mapping.
				 */
				if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
					PyErr_Format(PyExc_TypeError,
						     "%.200s%.200s argument after ** "
						     "must be a mapping, not %.200s",
						     PyEval_GetFuncName(func),
						     PyEval_GetFuncDesc(func),
						     kwdict->ob_type->tp_name);
				}
				goto ext_call_fail;
			}
			Py_DECREF(kwdict);
			kwdict = d;
		}
	}
	if (flags & CALL_FLAG_VAR) {
		stararg = EXT_POP(*pp_stack);
		if (!PyTuple_Check(stararg)) {
			PyObject *t = NULL;
			t = PySequence_Tuple(stararg);
			if (t == NULL) {
				if (PyErr_ExceptionMatches(PyExc_TypeError)) {
					PyErr_Format(PyExc_TypeError,
						     "%.200s%.200s argument after * "
						     "must be a sequence, not %200s",
						     PyEval_GetFuncName(func),
						     PyEval_GetFuncDesc(func),
						     stararg->ob_type->tp_name);
				}
				goto ext_call_fail;
			}
			Py_DECREF(stararg);
			stararg = t;
		}
		nstar = PyTuple_GET_SIZE(stararg);
	}
	if (nk > 0) {
		kwdict = update_keyword_args(kwdict, nk, pp_stack, func);
		if (kwdict == NULL)
			goto ext_call_fail;
	}
	callargs = update_star_args(na, nstar, stararg, pp_stack);
	if (callargs == NULL)
		goto ext_call_fail;
#ifdef CALL_PROFILE
	/* At this point, we have to look at the type of func to
	   update the call stats properly.  Do it here so as to avoid
	   exposing the call stats machinery outside eval.cc.
	*/
	if (PyFunction_Check(func))
		PCALL(PCALL_FUNCTION);
	else if (PyMethod_Check(func))
		PCALL(PCALL_METHOD);
	else if (PyType_Check(func))
		PCALL(PCALL_TYPE);
	else
		PCALL(PCALL_OTHER);
#endif
	result = PyObject_Call(func, callargs, kwdict);
ext_call_fail:
	Py_XDECREF(callargs);
	Py_XDECREF(kwdict);
	Py_XDECREF(stararg);
	return result;
}

/* Extract a slice index from a PyInt or PyLong or an object with the
   nb_index slot defined, and store in *pi.
   Silently reduce values larger than PY_SSIZE_T_MAX to PY_SSIZE_T_MAX,
   and silently boost values less than -PY_SSIZE_T_MAX-1 to -PY_SSIZE_T_MAX-1.
   Return 0 on error, 1 on success.
*/
/* Note:  If v is NULL, return success without storing into *pi.  This
   is because_PyEval_SliceIndex() is called by _PyEval_ApplySlice(), which
   can be called by the SLICE opcode with v and/or w equal to NULL.
*/
int
_PyEval_SliceIndex(PyObject *v, Py_ssize_t *pi)
{
	if (v != NULL) {
		Py_ssize_t x;
		if (PyInt_Check(v)) {
			/* XXX(nnorwitz): I think PyInt_AS_LONG is correct,
			   however, it looks like it should be AsSsize_t.
			   There should be a comment here explaining why.
			*/
			x = PyInt_AS_LONG(v);
		}
		else if (PyIndex_Check(v)) {
			x = PyNumber_AsSsize_t(v, NULL);
			if (x == -1 && PyErr_Occurred())
				return 0;
		}
		else {
			PyErr_SetString(PyExc_TypeError,
					"slice indices must be integers or "
					"None or have an __index__ method");
			return 0;
		}
		*pi = x;
	}
	return 1;
}

#undef ISINDEX
#define ISINDEX(x) ((x) == NULL || \
		    PyInt_Check(x) || PyLong_Check(x) || PyIndex_Check(x))

PyObject *
_PyEval_ApplySlice(PyObject *u, PyObject *v, PyObject *w) /* return u[v:w] */
{
	PyTypeObject *tp = u->ob_type;
	PySequenceMethods *sq = tp->tp_as_sequence;

	if (sq && sq->sq_slice && ISINDEX(v) && ISINDEX(w)) {
		Py_ssize_t ilow = 0, ihigh = PY_SSIZE_T_MAX;
		if (!_PyEval_SliceIndex(v, &ilow))
			return NULL;
		if (!_PyEval_SliceIndex(w, &ihigh))
			return NULL;
		return PySequence_GetSlice(u, ilow, ihigh);
	}
	else {
		PyObject *slice = PySlice_New(v, w, NULL);
		if (slice != NULL) {
			PyObject *res = PyObject_GetItem(u, slice);
			Py_DECREF(slice);
			return res;
		}
		else
			return NULL;
	}
}

int
_PyEval_AssignSlice(PyObject *u, PyObject *v, PyObject *w, PyObject *x)
	/* u[v:w] = x */
{
	PyTypeObject *tp = u->ob_type;
	PySequenceMethods *sq = tp->tp_as_sequence;

	if (sq && sq->sq_ass_slice && ISINDEX(v) && ISINDEX(w)) {
		Py_ssize_t ilow = 0, ihigh = PY_SSIZE_T_MAX;
		if (!_PyEval_SliceIndex(v, &ilow))
			return -1;
		if (!_PyEval_SliceIndex(w, &ihigh))
			return -1;
		if (x == NULL)
			return PySequence_DelSlice(u, ilow, ihigh);
		else
			return PySequence_SetSlice(u, ilow, ihigh, x);
	}
	else {
		PyObject *slice = PySlice_New(v, w, NULL);
		if (slice != NULL) {
			int res;
			if (x != NULL)
				res = PyObject_SetItem(u, slice, x);
			else
				res = PyObject_DelItem(u, slice);
			Py_DECREF(slice);
			return res;
		}
		else
			return -1;
	}
}

/* Returns NULL on error. */
PyObject *
_PyEval_LoadName(PyFrameObject *f, int name_index)
{
	PyObject *locals, *name, *x;
	name = GETITEM(f->f_code->co_names, name_index);
	if ((locals = f->f_locals) == NULL) {
		PyErr_Format(PyExc_SystemError,
			     "no locals when loading %s",
			     PyObject_REPR(name));
		return NULL;
	}
	if (PyDict_CheckExact(locals)) {
		x = PyDict_GetItem(locals, name);
		Py_XINCREF(x);
	} else {
		x = PyObject_GetItem(locals, name);
		if (x == NULL && PyErr_Occurred()) {
			if (!PyErr_ExceptionMatches(PyExc_KeyError)) {
				return NULL;
			}
			PyErr_Clear();
		}
	}
	if (x == NULL) {
		x = PyDict_GetItem(f->f_globals, name);
		if (x == NULL) {
			x = PyDict_GetItem(f->f_builtins, name);
			if (x == NULL) {
				format_exc_check_arg(PyExc_NameError,
						     NAME_ERROR_MSG, name);
				return NULL;
			}
		}
		Py_INCREF(x);
	}
	return x;
}

/* Returns non-zero on error. */
int
_PyEval_StoreName(PyFrameObject *f, int name_index, PyObject *to_store)
{
	int err;
	PyObject *a2, *x;
	a2 = GETITEM(f->f_code->co_names, name_index);
	if ((x = f->f_locals) != NULL) {
		if (PyDict_CheckExact(x))
			err = PyDict_SetItem(x, a2, to_store);
		else
			err = PyObject_SetItem(x, a2, to_store);
		Py_DECREF(to_store);
		return err;
	}
	PyErr_Format(PyExc_SystemError,
		     "no locals found when storing %s",
		     PyObject_REPR(a2));
	return -1;
}

/* Returns non-zero on error. */
int
_PyEval_DeleteName(PyFrameObject *f, int name_index)
{
	int err;
	PyObject *a1, *x;
	a1 = GETITEM(f->f_code->co_names, name_index);
	if ((x = f->f_locals) != NULL) {
		if ((err = PyObject_DelItem(x, a1)) != 0) {
			format_exc_check_arg(PyExc_NameError,
					     NAME_ERROR_MSG, a1);
		}
		return err;
	}
	PyErr_Format(PyExc_SystemError,
		     "no locals when deleting %s",
		     PyObject_REPR(a1));
	return -1;
}

#define Py3kExceptionClass_Check(x)     \
    (PyType_Check((x)) &&               \
     PyType_FastSubclass((PyTypeObject*)(x), Py_TPFLAGS_BASE_EXC_SUBCLASS))

#define CANNOT_CATCH_MSG "catching classes that don't inherit from " \
			 "BaseException is not allowed in 3.x"

/* Call PyErr_GivenExceptionMatches(), but check the exception type(s)
   for deprecated types: strings and non-BaseException-subclasses.
   Return -1 with an appropriate exception set on failure,
   1 if the given exception matches one or more of the given type(s),
   0 otherwise. */
extern "C" int
_PyEval_CheckedExceptionMatches(PyObject *exc, PyObject *exc_type)
{
	int ret_val;
	Py_ssize_t i, length;
	if (PyTuple_Check(exc_type)) {
		length = PyTuple_Size(exc_type);
		for (i = 0; i < length; i += 1) {
			PyObject *e = PyTuple_GET_ITEM(exc_type, i);
			if (PyString_Check(e)) {
				ret_val = PyErr_WarnEx(
					PyExc_DeprecationWarning,
					"catching of string "
					"exceptions is deprecated", 1);
				if (ret_val < 0)
					return -1;
			}
			else if (Py_Py3kWarningFlag  &&
				 !PyTuple_Check(e) &&
				 !Py3kExceptionClass_Check(e))
			{
				int ret_val;
				ret_val = PyErr_WarnEx(
					PyExc_DeprecationWarning,
					CANNOT_CATCH_MSG, 1);
				if (ret_val < 0)
					return -1;
			}
		}
	}
	else if (PyString_Check(exc_type)) {
		ret_val = PyErr_WarnEx(PyExc_DeprecationWarning,
				       "catching of string exceptions "
				       "is deprecated", 1);
		if (ret_val < 0)
			return -1;
	}
	else if (Py_Py3kWarningFlag  &&
		 !PyTuple_Check(exc_type) &&
		 !Py3kExceptionClass_Check(exc_type))
	{
		ret_val = PyErr_WarnEx(PyExc_DeprecationWarning,
				       CANNOT_CATCH_MSG, 1);
		if (ret_val < 0)
			return -1;
	}
	return PyErr_GivenExceptionMatches(exc, exc_type);
}

static PyObject *
cmp_outcome(int op, register PyObject *v, register PyObject *w)
{
	int res = 0;
	switch (op) {
	case PyCmp_IS:
		res = (v == w);
		break;
	case PyCmp_IS_NOT:
		res = (v != w);
		break;
	case PyCmp_IN:
		res = PySequence_Contains(w, v);
		if (res < 0)
			return NULL;
		break;
	case PyCmp_NOT_IN:
		res = PySequence_Contains(w, v);
		if (res < 0)
			return NULL;
		res = !res;
		break;
	case PyCmp_EXC_MATCH:
		res = _PyEval_CheckedExceptionMatches(v, w);
		if (res < 0)
			return NULL;
		break;
	default:
		return PyObject_RichCompare(v, w, op);
	}
	v = res ? Py_True : Py_False;
	Py_INCREF(v);
	return v;
}

void
_PyEval_RaiseForUnboundFreeVar(PyFrameObject *frame, int name_index)
{
	Py_ssize_t num_cells = PyTuple_GET_SIZE(frame->f_code->co_cellvars);
	if (name_index < num_cells) {
		_PyEval_RaiseForUnboundLocal(frame, name_index);
	} else {
		PyObject *varname = PyTuple_GET_ITEM(
			frame->f_code->co_freevars,
			name_index - num_cells);
		format_exc_check_arg(
			PyExc_NameError,
			UNBOUNDFREE_ERROR_MSG,
			varname);
	}
}

void
_PyEval_RaiseForGlobalNameError(PyObject *name)
{
	format_exc_check_arg(PyExc_NameError, GLOBAL_NAME_ERROR_MSG, name);
}

static void
format_exc_check_arg(PyObject *exc, char *format_str, PyObject *obj)
{
	char *obj_str;

	if (!obj)
		return;

	obj_str = PyString_AsString(obj);
	if (!obj_str)
		return;

	PyErr_Format(exc, format_str, obj_str);
}

static PyObject *
string_concatenate(PyObject *v, PyObject *w,
		   PyFrameObject *f, unsigned char *next_instr)
{
	/* This function implements 'variable += expr' when both arguments
	   are strings. */
	Py_ssize_t v_len = PyString_GET_SIZE(v);
	Py_ssize_t w_len = PyString_GET_SIZE(w);
	Py_ssize_t new_len = v_len + w_len;
	if (new_len < 0) {
		PyErr_SetString(PyExc_OverflowError,
				"strings are too large to concat");
		return NULL;
	}

	if (v->ob_refcnt == 2) {
		/* In the common case, there are 2 references to the value
		 * stored in 'variable' when the += is performed: one on the
		 * value stack (in 'v') and one still stored in the
		 * 'variable'.  We try to delete the variable now to reduce
		 * the refcnt to 1.
		 */
		switch (*next_instr) {
		case STORE_FAST:
		{
			int oparg = PEEKARG();
			PyObject **fastlocals = f->f_localsplus;
			if (GETLOCAL(oparg) == v)
				SETLOCAL(oparg, NULL);
			break;
		}
		case STORE_DEREF:
		{
			PyObject **freevars = (f->f_localsplus +
					       f->f_code->co_nlocals);
			PyObject *c = freevars[PEEKARG()];
			if (PyCell_GET(c) == v)
				PyCell_Set(c, NULL);
			break;
		}
		case STORE_NAME:
		{
			PyObject *names = f->f_code->co_names;
			PyObject *name = GETITEM(names, PEEKARG());
			PyObject *locals = f->f_locals;
			if (PyDict_CheckExact(locals) &&
			    PyDict_GetItem(locals, name) == v) {
				if (PyDict_DelItem(locals, name) != 0) {
					PyErr_Clear();
				}
			}
			break;
		}
		}
	}

	if (v->ob_refcnt == 1 && !PyString_CHECK_INTERNED(v)) {
		/* Now we own the last reference to 'v', so we can resize it
		 * in-place.
		 */
		if (_PyString_Resize(&v, new_len) != 0) {
			/* XXX if _PyString_Resize() fails, 'v' has been
			 * deallocated so it cannot be put back into
			 * 'variable'.  The MemoryError is raised when there
			 * is no value in 'variable', which might (very
			 * remotely) be a cause of incompatibilities.
			 */
			return NULL;
		}
		/* copy 'w' into the newly allocated area of 'v' */
		memcpy(PyString_AS_STRING(v) + v_len,
		       PyString_AS_STRING(w), w_len);
		return v;
	}
	else {
		/* When in-place resizing is not an option. */
		PyString_Concat(&v, w);
		return v;
	}
}

#ifdef DYNAMIC_EXECUTION_PROFILE

static PyObject *
getarray(long a[256])
{
	int i;
	PyObject *l = PyList_New(256);
	if (l == NULL) return NULL;
	for (i = 0; i < 256; i++) {
		PyObject *x = PyInt_FromLong(a[i]);
		if (x == NULL) {
			Py_DECREF(l);
			return NULL;
		}
		PyList_SetItem(l, i, x);
	}
	for (i = 0; i < 256; i++)
		a[i] = 0;
	return l;
}

PyObject *
_Py_GetDXProfile(PyObject *self, PyObject *args)
{
#ifndef DXPAIRS
	return getarray(dxp);
#else
	int i;
	PyObject *l = PyList_New(257);
	if (l == NULL) return NULL;
	for (i = 0; i < 257; i++) {
		PyObject *x = getarray(dxpairs[i]);
		if (x == NULL) {
			Py_DECREF(l);
			return NULL;
		}
		PyList_SetItem(l, i, x);
	}
	return l;
#endif
}

#endif
