// Test for the OptionPreprocessor and any*.
// RUN: ignore tblgen -I %p/../../include --gen-llvmc %s -o %t
// RUN: grep W1 %t
// RUN: grep W2 %t
// RUN: grep W3 %t

include "llvm/CompilerDriver/Common.td"

def OptList : OptionList<[
(switch_option "foo", (help "dummy")),
(switch_option "bar", (help "dummy")),
(switch_option "baz", (help "dummy")),
(parameter_option "foo_p", (help "dummy")),
(parameter_option "bar_p", (help "dummy")),
(parameter_option "baz_p", (help "dummy"))
]>;

def Preprocess : OptionPreprocessor<
(case (and (switch_on "foo"), (any_switch_on ["bar", "baz"])),
           (warning "W1"),
      (and (switch_on ["foo", "bar"]), (any_empty ["foo_p", "bar_p"])),
           (warning "W2"),
      (and (empty ["foo_p", "bar_p"]), (any_not_empty ["baz_p"])),
           (warning "W3"))
>;

// Shut up warnings...
def dummy : Tool<
[(in_language "dummy"),
 (out_language "dummy"),
 (output_suffix "d"),
 (cmd_line "dummy $INFILE -o $OUTFILE"),
 (actions (case (switch_on "foo"), (error),
                (switch_on "bar"), (error),
                (switch_on "baz"), (error),
                (not_empty "foo_p"), (error),
                (not_empty "bar_p"), (error),
                (not_empty "baz_p"), (error)))
]>;

def Graph : CompilationGraph<[Edge<"root", "dummy">]>;

