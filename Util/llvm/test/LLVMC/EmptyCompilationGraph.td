// Check that the compilation graph can be empty.
// RUN: tblgen -I %p/../../include --gen-llvmc %s

include "llvm/CompilerDriver/Common.td"

def Graph : CompilationGraph<[]>;
