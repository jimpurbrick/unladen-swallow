//===--- MacroArgs.h - Formal argument info for Macros ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the MacroArgs interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_MACROARGS_H
#define LLVM_CLANG_MACROARGS_H

#include <vector>

namespace clang {
  class MacroInfo;
  class Preprocessor;
  class Token;

/// MacroArgs - An instance of this class captures information about
/// the formal arguments specified to a function-like macro invocation.
class MacroArgs {
  /// NumUnexpArgTokens - The number of raw, unexpanded tokens for the
  /// arguments.  All of the actual argument tokens are allocated immediately
  /// after the MacroArgs object in memory.  This is all of the arguments
  /// concatenated together, with 'EOF' markers at the end of each argument.
  unsigned NumUnexpArgTokens;

  /// PreExpArgTokens - Pre-expanded tokens for arguments that need them.  Empty
  /// if not yet computed.  This includes the EOF marker at the end of the
  /// stream.
  std::vector<std::vector<Token> > PreExpArgTokens;

  /// StringifiedArgs - This contains arguments in 'stringified' form.  If the
  /// stringified form of an argument has not yet been computed, this is empty.
  std::vector<Token> StringifiedArgs;

  /// VarargsElided - True if this is a C99 style varargs macro invocation and
  /// there was no argument specified for the "..." argument.  If the argument
  /// was specified (even empty) or this isn't a C99 style varargs function, or
  /// if in strict mode and the C99 varargs macro had only a ... argument, this
  /// is false.
  bool VarargsElided;

  MacroArgs(unsigned NumToks, bool varargsElided)
    : NumUnexpArgTokens(NumToks), VarargsElided(varargsElided) {}
  ~MacroArgs() {}
public:
  /// MacroArgs ctor function - Create a new MacroArgs object with the specified
  /// macro and argument info.
  static MacroArgs *create(const MacroInfo *MI,
                           const Token *UnexpArgTokens,
                           unsigned NumArgTokens, bool VarargsElided);

  /// destroy - Destroy and deallocate the memory for this object.
  ///
  void destroy();

  /// ArgNeedsPreexpansion - If we can prove that the argument won't be affected
  /// by pre-expansion, return false.  Otherwise, conservatively return true.
  bool ArgNeedsPreexpansion(const Token *ArgTok, Preprocessor &PP) const;

  /// getUnexpArgument - Return a pointer to the first token of the unexpanded
  /// token list for the specified formal.
  ///
  const Token *getUnexpArgument(unsigned Arg) const;

  /// getArgLength - Given a pointer to an expanded or unexpanded argument,
  /// return the number of tokens, not counting the EOF, that make up the
  /// argument.
  static unsigned getArgLength(const Token *ArgPtr);

  /// getPreExpArgument - Return the pre-expanded form of the specified
  /// argument.
  const std::vector<Token> &
    getPreExpArgument(unsigned Arg, Preprocessor &PP);

  /// getStringifiedArgument - Compute, cache, and return the specified argument
  /// that has been 'stringified' as required by the # operator.
  const Token &getStringifiedArgument(unsigned ArgNo, Preprocessor &PP);

  /// getNumArguments - Return the number of arguments passed into this macro
  /// invocation.
  unsigned getNumArguments() const { return NumUnexpArgTokens; }


  /// isVarargsElidedUse - Return true if this is a C99 style varargs macro
  /// invocation and there was no argument specified for the "..." argument.  If
  /// the argument was specified (even empty) or this isn't a C99 style varargs
  /// function, or if in strict mode and the C99 varargs macro had only a ...
  /// argument, this returns false.
  bool isVarargsElidedUse() const { return VarargsElided; }

  /// StringifyArgument - Implement C99 6.10.3.2p2, converting a sequence of
  /// tokens into the literal string token that should be produced by the C #
  /// preprocessor operator.  If Charify is true, then it should be turned into
  /// a character literal for the Microsoft charize (#@) extension.
  ///
  static Token StringifyArgument(const Token *ArgToks,
                                 Preprocessor &PP, bool Charify = false);
};

}  // end namespace clang

#endif
