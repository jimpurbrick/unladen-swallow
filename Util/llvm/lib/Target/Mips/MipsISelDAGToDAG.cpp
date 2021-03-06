//===-- MipsISelDAGToDAG.cpp - A dag to dag inst selector for Mips --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the MIPS target.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mips-isel"
#include "Mips.h"
#include "MipsISelLowering.h"
#include "MipsMachineFunction.h"
#include "MipsRegisterInfo.h"
#include "MipsSubtarget.h"
#include "MipsTargetMachine.h"
#include "llvm/GlobalValue.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Support/CFG.h"
#include "llvm/Type.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// MipsDAGToDAGISel - MIPS specific code to select MIPS machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace {

class MipsDAGToDAGISel : public SelectionDAGISel {

  /// TM - Keep a reference to MipsTargetMachine.
  MipsTargetMachine &TM;

  /// Subtarget - Keep a pointer to the MipsSubtarget around so that we can
  /// make the right decision when generating code for different targets.
  const MipsSubtarget &Subtarget;
 
public:
  explicit MipsDAGToDAGISel(MipsTargetMachine &tm) :
  SelectionDAGISel(tm),
  TM(tm), Subtarget(tm.getSubtarget<MipsSubtarget>()) {}
  
  virtual void InstructionSelect();

  // Pass Name
  virtual const char *getPassName() const {
    return "MIPS DAG->DAG Pattern Instruction Selection";
  } 
  

private:  
  // Include the pieces autogenerated from the target description.
  #include "MipsGenDAGISel.inc"

  /// getTargetMachine - Return a reference to the TargetMachine, casted
  /// to the target-specific type.
  const MipsTargetMachine &getTargetMachine() {
    return static_cast<const MipsTargetMachine &>(TM);
  }

  /// getInstrInfo - Return a reference to the TargetInstrInfo, casted
  /// to the target-specific type.
  const MipsInstrInfo *getInstrInfo() {
    return getTargetMachine().getInstrInfo();
  }

  SDNode *getGlobalBaseReg();
  SDNode *Select(SDValue N);

  // Complex Pattern.
  bool SelectAddr(SDValue Op, SDValue N, 
                  SDValue &Base, SDValue &Offset);

  SDNode *SelectLoadFp64(SDValue N);
  SDNode *SelectStoreFp64(SDValue N);

  // getI32Imm - Return a target constant with the specified
  // value, of type i32.
  inline SDValue getI32Imm(unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, MVT::i32);
  }


  #ifndef NDEBUG
  unsigned Indent;
  #endif
};

}

/// InstructionSelect - This callback is invoked by
/// SelectionDAGISel when it has created a SelectionDAG for us to codegen.
void MipsDAGToDAGISel::InstructionSelect() {
  // Codegen the basic block.
  DEBUG(errs() << "===== Instruction selection begins:\n");
  DEBUG(Indent = 0);

  // Select target instructions for the DAG.
  SelectRoot(*CurDAG);

  DEBUG(errs() << "===== Instruction selection ends:\n");

  CurDAG->RemoveDeadNodes();
}

/// getGlobalBaseReg - Output the instructions required to put the
/// GOT address into a register.
SDNode *MipsDAGToDAGISel::getGlobalBaseReg() {
  unsigned GlobalBaseReg = getInstrInfo()->getGlobalBaseReg(MF);
  return CurDAG->getRegister(GlobalBaseReg, TLI.getPointerTy()).getNode();
}

/// ComplexPattern used on MipsInstrInfo
/// Used on Mips Load/Store instructions
bool MipsDAGToDAGISel::
SelectAddr(SDValue Op, SDValue Addr, SDValue &Offset, SDValue &Base)
{
  // if Address is FI, get the TargetFrameIndex.
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, MVT::i32);
    return true;
  }
    
  // on PIC code Load GA
  if (TM.getRelocationModel() == Reloc::PIC_) {
    if ((Addr.getOpcode() == ISD::TargetGlobalAddress) || 
        (Addr.getOpcode() == ISD::TargetJumpTable)){
      Base   = CurDAG->getRegister(Mips::GP, MVT::i32);
      Offset = Addr;
      return true;
    }
  } else {
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
        Addr.getOpcode() == ISD::TargetGlobalAddress))
      return false;
  }    
  
  // Operand is a result from an ADD.
  if (Addr.getOpcode() == ISD::ADD) {
    if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      if (Predicate_immSExt16(CN)) {

        // If the first operand is a FI, get the TargetFI Node
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                    (Addr.getOperand(0))) {
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
        } else {
          Base = Addr.getOperand(0);
        }

        Offset = CurDAG->getTargetConstant(CN->getZExtValue(), MVT::i32);
        return true;
      }
    }

    // When loading from constant pools, load the lower address part in
    // the instruction itself. Instead of:
    //  lui $2, %hi($CPI1_0)
    //  addiu $2, $2, %lo($CPI1_0)
    //  lwc1 $f0, 0($2)
    // Generate:
    //  lui $2, %hi($CPI1_0)
    //  lwc1 $f0, %lo($CPI1_0)($2)
    if (Addr.getOperand(0).getOpcode() == MipsISD::Hi &&
        Addr.getOperand(1).getOpcode() == MipsISD::Lo) {
      SDValue LoVal = Addr.getOperand(1); 
      if (ConstantPoolSDNode *CP = dyn_cast<ConstantPoolSDNode>(
          LoVal.getOperand(0))) {
        if (!CP->getOffset()) {
          Base = Addr.getOperand(0);
          Offset = LoVal.getOperand(0);
          return true;
        }
      }
    }
  }

  Base   = Addr;
  Offset = CurDAG->getTargetConstant(0, MVT::i32);
  return true;
}

SDNode *MipsDAGToDAGISel::SelectLoadFp64(SDValue N) {
  MVT::SimpleValueType NVT = 
    N.getNode()->getValueType(0).getSimpleVT().SimpleTy;

  if (!Subtarget.isMips1() || NVT != MVT::f64)
    return NULL;

  if (!Predicate_unindexedload(N.getNode()) ||
      !Predicate_load(N.getNode()))
    return NULL;

  SDValue Chain = N.getOperand(0);
  SDValue N1 = N.getOperand(1);
  SDValue Offset0, Offset1, Base;

  if (!SelectAddr(N, N1, Offset0, Base) ||
      N1.getValueType() != MVT::i32)
    return NULL;

  MachineSDNode::mmo_iterator MemRefs0 = MF->allocateMemRefsArray(1);
  MemRefs0[0] = cast<MemSDNode>(N)->getMemOperand();
  DebugLoc dl = N.getDebugLoc();

  // The second load should start after for 4 bytes. 
  if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(Offset0))
    Offset1 = CurDAG->getTargetConstant(C->getSExtValue()+4, MVT::i32);
  else if (ConstantPoolSDNode *CP = dyn_cast<ConstantPoolSDNode>(Offset0))
    Offset1 = CurDAG->getTargetConstantPool(CP->getConstVal(), 
                                            MVT::i32, 
                                            CP->getAlignment(), 
                                            CP->getOffset()+4, 
                                            CP->getTargetFlags());
  else
    return NULL;

  // Instead of:
  //    ldc $f0, X($3)
  // Generate:
  //    lwc $f0, X($3)
  //    lwc $f1, X+4($3)
  SDNode *LD0 = CurDAG->getMachineNode(Mips::LWC1, dl, MVT::f32, 
                                    MVT::Other, Offset0, Base, Chain);
  SDValue Undef = SDValue(CurDAG->getMachineNode(TargetInstrInfo::IMPLICIT_DEF,
                                                 dl, NVT), 0);
  SDValue I0 = CurDAG->getTargetInsertSubreg(Mips::SUBREG_FPEVEN, dl, 
                            MVT::f64, Undef, SDValue(LD0, 0));

  SDNode *LD1 = CurDAG->getMachineNode(Mips::LWC1, dl, MVT::f32,
                          MVT::Other, Offset1, Base, SDValue(LD0, 1));
  SDValue I1 = CurDAG->getTargetInsertSubreg(Mips::SUBREG_FPODD, dl, 
                            MVT::f64, I0, SDValue(LD1, 0));

  ReplaceUses(N, I1);
  ReplaceUses(N.getValue(1), Chain);
  cast<MachineSDNode>(LD0)->setMemRefs(MemRefs0, MemRefs0 + 1);
  cast<MachineSDNode>(LD1)->setMemRefs(MemRefs0, MemRefs0 + 1);
  return I1.getNode();
}

SDNode *MipsDAGToDAGISel::SelectStoreFp64(SDValue N) {

  if (!Subtarget.isMips1() || 
      N.getOperand(1).getValueType() != MVT::f64)
    return NULL;

  SDValue Chain = N.getOperand(0);

  if (!Predicate_unindexedstore(N.getNode()) ||
      !Predicate_store(N.getNode()))
    return NULL;

  SDValue N1 = N.getOperand(1);
  SDValue N2 = N.getOperand(2);
  SDValue Offset0, Offset1, Base;

  if (!SelectAddr(N, N2, Offset0, Base) ||
      N1.getValueType() != MVT::f64 ||
      N2.getValueType() != MVT::i32)
    return NULL;

  MachineSDNode::mmo_iterator MemRefs0 = MF->allocateMemRefsArray(1);
  MemRefs0[0] = cast<MemSDNode>(N)->getMemOperand();
  DebugLoc dl = N.getDebugLoc();

  // Get the even and odd part from the f64 register
  SDValue FPOdd = CurDAG->getTargetExtractSubreg(Mips::SUBREG_FPODD, 
                                                 dl, MVT::f32, N1);
  SDValue FPEven = CurDAG->getTargetExtractSubreg(Mips::SUBREG_FPEVEN,
                                                 dl, MVT::f32, N1);

  // The second store should start after for 4 bytes. 
  if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(Offset0))
    Offset1 = CurDAG->getTargetConstant(C->getSExtValue()+4, MVT::i32);
  else
    return NULL;

  // Instead of:
  //    sdc $f0, X($3)
  // Generate:
  //    swc $f0, X($3)
  //    swc $f1, X+4($3)
  SDValue Ops0[] = { FPEven, Offset0, Base, Chain };
  Chain = SDValue(CurDAG->getMachineNode(Mips::SWC1, dl,
                                       MVT::Other, Ops0, 4), 0);
  cast<MachineSDNode>(Chain.getNode())->setMemRefs(MemRefs0, MemRefs0 + 1);

  SDValue Ops1[] = { FPOdd, Offset1, Base, Chain };
  Chain = SDValue(CurDAG->getMachineNode(Mips::SWC1, dl,
                                       MVT::Other, Ops1, 4), 0);
  cast<MachineSDNode>(Chain.getNode())->setMemRefs(MemRefs0, MemRefs0 + 1);

  ReplaceUses(N.getValue(0), Chain);
  return Chain.getNode();
}

/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
SDNode* MipsDAGToDAGISel::Select(SDValue N) {
  SDNode *Node = N.getNode();
  unsigned Opcode = Node->getOpcode();
  DebugLoc dl = Node->getDebugLoc();

  // Dump information about the Node being selected
  DEBUG(errs().indent(Indent) << "Selecting: ";
        Node->dump(CurDAG);
        errs() << "\n");
  DEBUG(Indent += 2);

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(errs().indent(Indent-2) << "== ";
          Node->dump(CurDAG);
          errs() << "\n");
    DEBUG(Indent -= 2);
    return NULL;
  }

  ///
  // Instruction Selection not handled by the auto-generated 
  // tablegen selection should be handled here.
  /// 
  switch(Opcode) {

    default: break;

    case ISD::SUBE: 
    case ISD::ADDE: {
      SDValue InFlag = Node->getOperand(2), CmpLHS;
      unsigned Opc = InFlag.getOpcode(); Opc=Opc;
      assert(((Opc == ISD::ADDC || Opc == ISD::ADDE) || 
              (Opc == ISD::SUBC || Opc == ISD::SUBE)) &&  
             "(ADD|SUB)E flag operand must come from (ADD|SUB)C/E insn");

      unsigned MOp;
      if (Opcode == ISD::ADDE) {
        CmpLHS = InFlag.getValue(0);
        MOp = Mips::ADDu;
      } else { 
        CmpLHS = InFlag.getOperand(0);
        MOp = Mips::SUBu;
      }

      SDValue Ops[] = { CmpLHS, InFlag.getOperand(1) };

      SDValue LHS = Node->getOperand(0);
      SDValue RHS = Node->getOperand(1);

      EVT VT = LHS.getValueType();
      SDNode *Carry = CurDAG->getMachineNode(Mips::SLTu, dl, VT, Ops, 2);
      SDNode *AddCarry = CurDAG->getMachineNode(Mips::ADDu, dl, VT, 
                                                SDValue(Carry,0), RHS);

      return CurDAG->SelectNodeTo(N.getNode(), MOp, VT, MVT::Flag,
                                  LHS, SDValue(AddCarry,0));
    }

    /// Mul/Div with two results
    case ISD::SDIVREM:
    case ISD::UDIVREM:
    case ISD::SMUL_LOHI:
    case ISD::UMUL_LOHI: {
      SDValue Op1 = Node->getOperand(0);
      SDValue Op2 = Node->getOperand(1);

      unsigned Op;
      if (Opcode == ISD::UMUL_LOHI || Opcode == ISD::SMUL_LOHI)
        Op = (Opcode == ISD::UMUL_LOHI ? Mips::MULTu : Mips::MULT);
      else
        Op = (Opcode == ISD::UDIVREM ? Mips::DIVu : Mips::DIV);

      SDNode *Node = CurDAG->getMachineNode(Op, dl, MVT::Flag, Op1, Op2);

      SDValue InFlag = SDValue(Node, 0);
      SDNode *Lo = CurDAG->getMachineNode(Mips::MFLO, dl, MVT::i32, 
                                          MVT::Flag, InFlag);
      InFlag = SDValue(Lo,1);
      SDNode *Hi = CurDAG->getMachineNode(Mips::MFHI, dl, MVT::i32, InFlag);

      if (!N.getValue(0).use_empty()) 
        ReplaceUses(N.getValue(0), SDValue(Lo,0));

      if (!N.getValue(1).use_empty()) 
        ReplaceUses(N.getValue(1), SDValue(Hi,0));

      return NULL;
    }

    /// Special Muls
    case ISD::MUL: 
    case ISD::MULHS:
    case ISD::MULHU: {
      SDValue MulOp1 = Node->getOperand(0);
      SDValue MulOp2 = Node->getOperand(1);

      unsigned MulOp  = (Opcode == ISD::MULHU ? Mips::MULTu : Mips::MULT);
      SDNode *MulNode = CurDAG->getMachineNode(MulOp, dl, 
                                               MVT::Flag, MulOp1, MulOp2);

      SDValue InFlag = SDValue(MulNode, 0);

      if (MulOp == ISD::MUL)
        return CurDAG->getMachineNode(Mips::MFLO, dl, MVT::i32, InFlag);
      else
        return CurDAG->getMachineNode(Mips::MFHI, dl, MVT::i32, InFlag);
    }

    /// Div/Rem operations
    case ISD::SREM:
    case ISD::UREM:
    case ISD::SDIV: 
    case ISD::UDIV: {
      SDValue Op1 = Node->getOperand(0);
      SDValue Op2 = Node->getOperand(1);

      unsigned Op, MOp;
      if (Opcode == ISD::SDIV || Opcode == ISD::UDIV) {
        Op  = (Opcode == ISD::SDIV ? Mips::DIV : Mips::DIVu);
        MOp = Mips::MFLO;
      } else {
        Op  = (Opcode == ISD::SREM ? Mips::DIV : Mips::DIVu);
        MOp = Mips::MFHI;
      }
      SDNode *Node = CurDAG->getMachineNode(Op, dl, MVT::Flag, Op1, Op2);

      SDValue InFlag = SDValue(Node, 0);
      return CurDAG->getMachineNode(MOp, dl, MVT::i32, InFlag);
    }

    // Get target GOT address.
    case ISD::GLOBAL_OFFSET_TABLE:
      return getGlobalBaseReg();

    case ISD::ConstantFP: {
      ConstantFPSDNode *CN = dyn_cast<ConstantFPSDNode>(N);
      if (N.getValueType() == MVT::f64 && CN->isExactlyValue(+0.0)) { 
        SDValue Zero = CurDAG->getRegister(Mips::ZERO, MVT::i32);
        ReplaceUses(N, Zero);
        return Zero.getNode();
      }
      break;
    }

    case ISD::LOAD:
      if (SDNode *ResNode = SelectLoadFp64(N))
        return ResNode;
      // Other cases are autogenerated.
      break;

    case ISD::STORE:
      if (SDNode *ResNode = SelectStoreFp64(N))
        return ResNode;
      // Other cases are autogenerated.
      break;

    /// Handle direct and indirect calls when using PIC. On PIC, when 
    /// GOT is smaller than about 64k (small code) the GA target is 
    /// loaded with only one instruction. Otherwise GA's target must 
    /// be loaded with 3 instructions. 
    case MipsISD::JmpLink: {
      if (TM.getRelocationModel() == Reloc::PIC_) {
        SDValue Chain  = Node->getOperand(0);
        SDValue Callee = Node->getOperand(1);
        SDValue T9Reg = CurDAG->getRegister(Mips::T9, MVT::i32);
        SDValue InFlag(0, 0);

        if ( (isa<GlobalAddressSDNode>(Callee)) ||
             (isa<ExternalSymbolSDNode>(Callee)) )
        {
          /// Direct call for global addresses and external symbols
          SDValue GPReg = CurDAG->getRegister(Mips::GP, MVT::i32);

          // Use load to get GOT target
          SDValue Ops[] = { Callee, GPReg, Chain };
          SDValue Load = SDValue(CurDAG->getMachineNode(Mips::LW, dl, MVT::i32, 
                                     MVT::Other, Ops, 3), 0);
          Chain = Load.getValue(1);

          // Call target must be on T9
          Chain = CurDAG->getCopyToReg(Chain, dl, T9Reg, Load, InFlag);
        } else 
          /// Indirect call
          Chain = CurDAG->getCopyToReg(Chain, dl, T9Reg, Callee, InFlag);

        // Emit Jump and Link Register
        SDNode *ResNode = CurDAG->getMachineNode(Mips::JALR, dl, MVT::Other,
                                  MVT::Flag, T9Reg, Chain);
        Chain  = SDValue(ResNode, 0);
        InFlag = SDValue(ResNode, 1);
        ReplaceUses(SDValue(Node, 0), Chain);
        ReplaceUses(SDValue(Node, 1), InFlag);
        return ResNode;
      } 
    }
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(N);

  DEBUG(errs().indent(Indent-2) << "=> ");
  if (ResNode == NULL || ResNode == N.getNode())
    DEBUG(N.getNode()->dump(CurDAG));
  else
    DEBUG(ResNode->dump(CurDAG));
  DEBUG(errs() << "\n");
  DEBUG(Indent -= 2);

  return ResNode;
}

/// createMipsISelDag - This pass converts a legalized DAG into a 
/// MIPS-specific DAG, ready for instruction scheduling.
FunctionPass *llvm::createMipsISelDag(MipsTargetMachine &TM) {
  return new MipsDAGToDAGISel(TM);
}
