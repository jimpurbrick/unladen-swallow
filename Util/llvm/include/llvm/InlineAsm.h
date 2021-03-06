//===-- llvm/InlineAsm.h - Class to represent inline asm strings-*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class represents the inline asm strings, which are Value*'s that are
// used as the callee operand of call instructions.  InlineAsm's are uniqued
// like constants, and created via InlineAsm::get(...).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_INLINEASM_H
#define LLVM_INLINEASM_H

#include "llvm/Value.h"
#include <vector>

namespace llvm {

class PointerType;
class FunctionType;
class Module;

class InlineAsm : public Value {
  InlineAsm(const InlineAsm &);             // do not implement
  void operator=(const InlineAsm&);         // do not implement

  std::string AsmString, Constraints;
  bool HasSideEffects;
  bool IsAlignStack;
  
  InlineAsm(const FunctionType *Ty, StringRef AsmString,
            StringRef Constraints, bool hasSideEffects,
            bool isAlignStack = false);
  virtual ~InlineAsm();
public:

  /// InlineAsm::get - Return the the specified uniqued inline asm string.
  ///
  static InlineAsm *get(const FunctionType *Ty, StringRef AsmString,
                        StringRef Constraints, bool hasSideEffects,
                        bool isAlignStack = false);
  
  bool hasSideEffects() const { return HasSideEffects; }
  bool isAlignStack() const { return IsAlignStack; }
  
  /// getType - InlineAsm's are always pointers.
  ///
  const PointerType *getType() const {
    return reinterpret_cast<const PointerType*>(Value::getType());
  }
  
  /// getFunctionType - InlineAsm's are always pointers to functions.
  ///
  const FunctionType *getFunctionType() const;
  
  const std::string &getAsmString() const { return AsmString; }
  const std::string &getConstraintString() const { return Constraints; }

  /// Verify - This static method can be used by the parser to check to see if
  /// the specified constraint string is legal for the type.  This returns true
  /// if legal, false if not.
  ///
  static bool Verify(const FunctionType *Ty, StringRef Constraints);

  // Constraint String Parsing 
  enum ConstraintPrefix {
    isInput,            // 'x'
    isOutput,           // '=x'
    isClobber           // '~x'
  };
  
  struct ConstraintInfo {
    /// Type - The basic type of the constraint: input/output/clobber
    ///
    ConstraintPrefix Type;
    
    /// isEarlyClobber - "&": output operand writes result before inputs are all
    /// read.  This is only ever set for an output operand.
    bool isEarlyClobber; 
    
    /// MatchingInput - If this is not -1, this is an output constraint where an
    /// input constraint is required to match it (e.g. "0").  The value is the
    /// constraint number that matches this one (for example, if this is
    /// constraint #0 and constraint #4 has the value "0", this will be 4).
    signed char MatchingInput;
    
    /// hasMatchingInput - Return true if this is an output constraint that has
    /// a matching input constraint.
    bool hasMatchingInput() const { return MatchingInput != -1; }
    
    /// isCommutative - This is set to true for a constraint that is commutative
    /// with the next operand.
    bool isCommutative;
    
    /// isIndirect - True if this operand is an indirect operand.  This means
    /// that the address of the source or destination is present in the call
    /// instruction, instead of it being returned or passed in explicitly.  This
    /// is represented with a '*' in the asm string.
    bool isIndirect;
    
    /// Code - The constraint code, either the register name (in braces) or the
    /// constraint letter/number.
    std::vector<std::string> Codes;
    
    /// Parse - Analyze the specified string (e.g. "=*&{eax}") and fill in the
    /// fields in this structure.  If the constraint string is not understood,
    /// return true, otherwise return false.
    bool Parse(StringRef Str, 
               std::vector<InlineAsm::ConstraintInfo> &ConstraintsSoFar);
  };
  
  /// ParseConstraints - Split up the constraint string into the specific
  /// constraints and their prefixes.  If this returns an empty vector, and if
  /// the constraint string itself isn't empty, there was an error parsing.
  static std::vector<ConstraintInfo> 
    ParseConstraints(StringRef ConstraintString);
  
  /// ParseConstraints - Parse the constraints of this inlineasm object, 
  /// returning them the same way that ParseConstraints(str) does.
  std::vector<ConstraintInfo> 
  ParseConstraints() const {
    return ParseConstraints(Constraints);
  }
  
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const InlineAsm *) { return true; }
  static inline bool classof(const Value *V) {
    return V->getValueID() == Value::InlineAsmVal;
  }

  /// getNumOperandRegisters - Extract the number of registers field from the
  /// inline asm operand flag.
  static unsigned getNumOperandRegisters(unsigned Flag) {
    return (Flag & 0xffff) >> 3;
  }

  /// isUseOperandTiedToDef - Return true if the flag of the inline asm
  /// operand indicates it is an use operand that's matched to a def operand.
  static bool isUseOperandTiedToDef(unsigned Flag, unsigned &Idx) {
    if ((Flag & 0x80000000) == 0)
      return false;
    Idx = (Flag & ~0x80000000) >> 16;
    return true;
  }


};

} // End llvm namespace

#endif
