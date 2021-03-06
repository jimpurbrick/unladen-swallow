//===--- PathDiagnostic.h - Path-Specific Diagnostic Handling ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the PathDiagnostic-related interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PATH_DIAGNOSTIC_H
#define LLVM_CLANG_PATH_DIAGNOSTIC_H

#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/FoldingSet.h"

#include <vector>
#include <deque>
#include <string>
#include <algorithm>

namespace clang {
    
class Stmt;
class Decl;
class Preprocessor;
  
//===----------------------------------------------------------------------===//
// High-level interface for handlers of path-sensitive diagnostics.
//===----------------------------------------------------------------------===//

class PathDiagnostic;
    
class Stmt;
class Decl;
class Preprocessor;

class PathDiagnosticClient : public DiagnosticClient  {
public:
  PathDiagnosticClient() {}

  virtual ~PathDiagnosticClient() {};
  
  virtual void
  FlushDiagnostics(llvm::SmallVectorImpl<std::string> *FilesMade = 0) = 0;
  
  void FlushDiagnostics(llvm::SmallVectorImpl<std::string> &FilesMade) {
    FlushDiagnostics(&FilesMade);
  }
  
  virtual llvm::StringRef getName() const = 0;
  
  virtual void HandleDiagnostic(Diagnostic::Level DiagLevel,
                                const DiagnosticInfo &Info);
  virtual void HandlePathDiagnostic(const PathDiagnostic* D) = 0;

  enum PathGenerationScheme { Minimal, Extensive };
  virtual PathGenerationScheme getGenerationScheme() const { return Minimal; }
  virtual bool supportsLogicalOpControlFlow() const { return false; }
  virtual bool supportsAllBlockEdges() const { return false; }
  virtual bool useVerboseDescription() const { return true; }
};

//===----------------------------------------------------------------------===//
// Path-sensitive diagnostics.
//===----------------------------------------------------------------------===//

class PathDiagnosticRange : public SourceRange {
public:
  const bool isPoint;

  PathDiagnosticRange(const SourceRange &R, bool isP = false)
    : SourceRange(R), isPoint(isP) {}
};

class PathDiagnosticLocation {
private:
  enum Kind { RangeK, SingleLocK, StmtK, DeclK } K;
  SourceRange R;
  const Stmt *S;
  const Decl *D;
  const SourceManager *SM;
public:
  PathDiagnosticLocation()
    : K(SingleLocK), S(0), D(0), SM(0) {}

  PathDiagnosticLocation(FullSourceLoc L)
    : K(SingleLocK), R(L, L), S(0), D(0), SM(&L.getManager()) {}

  PathDiagnosticLocation(const Stmt *s, const SourceManager &sm)
    : K(StmtK), S(s), D(0), SM(&sm) {}

  PathDiagnosticLocation(SourceRange r, const SourceManager &sm)
    : K(RangeK), R(r), S(0), D(0), SM(&sm) {}

  PathDiagnosticLocation(const Decl *d, const SourceManager &sm)
    : K(DeclK), S(0), D(d), SM(&sm) {}

  bool operator==(const PathDiagnosticLocation &X) const {
    return K == X.K && R == X.R && S == X.S && D == X.D;
  }

  bool operator!=(const PathDiagnosticLocation &X) const {
    return K != X.K || R != X.R || S != X.S || D != X.D;;
  }

  PathDiagnosticLocation& operator=(const PathDiagnosticLocation &X) {
    K = X.K;
    R = X.R;
    S = X.S;
    D = X.D;
    SM = X.SM;
    return *this;
  }

  bool isValid() const {
    return SM != 0;
  }

  const SourceManager& getSourceManager() const { assert(isValid());return *SM;}

  FullSourceLoc asLocation() const;
  PathDiagnosticRange asRange() const;
  const Stmt *asStmt() const { assert(isValid()); return S; }
  const Decl *asDecl() const { assert(isValid()); return D; }

  bool hasRange() const { return K == StmtK || K == RangeK || K == DeclK; }

  void invalidate() {
    *this = PathDiagnosticLocation();
  }

  void flatten();

  const SourceManager& getManager() const { assert(isValid()); return *SM; }
  
  void Profile(llvm::FoldingSetNodeID &ID) const;
};

class PathDiagnosticLocationPair {
private:
  PathDiagnosticLocation Start, End;
public:
  PathDiagnosticLocationPair(const PathDiagnosticLocation &start,
                             const PathDiagnosticLocation &end)
    : Start(start), End(end) {}

  const PathDiagnosticLocation &getStart() const { return Start; }
  const PathDiagnosticLocation &getEnd() const { return End; }

  void flatten() {
    Start.flatten();
    End.flatten();
  }
  
  void Profile(llvm::FoldingSetNodeID &ID) const {
    Start.Profile(ID);
    End.Profile(ID);
  }
};

//===----------------------------------------------------------------------===//
// Path "pieces" for path-sensitive diagnostics.
//===----------------------------------------------------------------------===//

class PathDiagnosticPiece {
public:
  enum Kind { ControlFlow, Event, Macro };
  enum DisplayHint { Above, Below };

private:
  const std::string str;
  std::vector<CodeModificationHint> CodeModificationHints;
  const Kind kind;
  const DisplayHint Hint;
  std::vector<SourceRange> ranges;

  // Do not implement:
  PathDiagnosticPiece();
  PathDiagnosticPiece(const PathDiagnosticPiece &P);
  PathDiagnosticPiece& operator=(const PathDiagnosticPiece &P);

protected:
  PathDiagnosticPiece(const std::string& s, Kind k, DisplayHint hint = Below);

  PathDiagnosticPiece(const char* s, Kind k, DisplayHint hint = Below);

  PathDiagnosticPiece(Kind k, DisplayHint hint = Below);

public:
  virtual ~PathDiagnosticPiece();

  const std::string& getString() const { return str; }

  /// getDisplayHint - Return a hint indicating where the diagnostic should
  ///  be displayed by the PathDiagnosticClient.
  DisplayHint getDisplayHint() const { return Hint; }

  virtual PathDiagnosticLocation getLocation() const = 0;
  virtual void flattenLocations() = 0;

  Kind getKind() const { return kind; }

  void addRange(SourceRange R) { ranges.push_back(R); }

  void addRange(SourceLocation B, SourceLocation E) {
    ranges.push_back(SourceRange(B,E));
  }

  void addCodeModificationHint(const CodeModificationHint& Hint) {
    CodeModificationHints.push_back(Hint);
  }

  typedef const SourceRange* range_iterator;

  range_iterator ranges_begin() const {
    return ranges.empty() ? NULL : &ranges[0];
  }

  range_iterator ranges_end() const {
    return ranges_begin() + ranges.size();
  }

  typedef const CodeModificationHint *code_modifications_iterator;

  code_modifications_iterator code_modifications_begin() const {
    return CodeModificationHints.empty()? 0 : &CodeModificationHints[0];
  }

  code_modifications_iterator code_modifications_end() const {
    return CodeModificationHints.empty()? 0
                   : &CodeModificationHints[0] + CodeModificationHints.size();
  }

  static inline bool classof(const PathDiagnosticPiece* P) {
    return true;
  }
  
  virtual void Profile(llvm::FoldingSetNodeID &ID) const;
};

class PathDiagnosticSpotPiece : public PathDiagnosticPiece {
private:
  PathDiagnosticLocation Pos;
public:
  PathDiagnosticSpotPiece(const PathDiagnosticLocation &pos,
                          const std::string& s,
                          PathDiagnosticPiece::Kind k,
                          bool addPosRange = true)
  : PathDiagnosticPiece(s, k), Pos(pos) {
    assert(Pos.asLocation().isValid() &&
           "PathDiagnosticSpotPiece's must have a valid location.");
    if (addPosRange && Pos.hasRange()) addRange(Pos.asRange());
  }

  PathDiagnosticLocation getLocation() const { return Pos; }
  virtual void flattenLocations() { Pos.flatten(); }
  
  virtual void Profile(llvm::FoldingSetNodeID &ID) const;
};

class PathDiagnosticEventPiece : public PathDiagnosticSpotPiece {

public:
  PathDiagnosticEventPiece(const PathDiagnosticLocation &pos,
                           const std::string& s, bool addPosRange = true)
    : PathDiagnosticSpotPiece(pos, s, Event, addPosRange) {}

  PathDiagnosticEventPiece(const PathDiagnosticLocation &pos, const char* s,
                           bool addPosRange = true)
    : PathDiagnosticSpotPiece(pos, s, Event, addPosRange) {}

  ~PathDiagnosticEventPiece();

  static inline bool classof(const PathDiagnosticPiece* P) {
    return P->getKind() == Event;
  }
};

class PathDiagnosticControlFlowPiece : public PathDiagnosticPiece {
  std::vector<PathDiagnosticLocationPair> LPairs;
public:
  PathDiagnosticControlFlowPiece(const PathDiagnosticLocation &startPos,
                                 const PathDiagnosticLocation &endPos,
                                 const std::string& s)
    : PathDiagnosticPiece(s, ControlFlow) {
      LPairs.push_back(PathDiagnosticLocationPair(startPos, endPos));
    }

  PathDiagnosticControlFlowPiece(const PathDiagnosticLocation &startPos,
                                 const PathDiagnosticLocation &endPos,
                                 const char* s)
    : PathDiagnosticPiece(s, ControlFlow) {
      LPairs.push_back(PathDiagnosticLocationPair(startPos, endPos));
    }

  PathDiagnosticControlFlowPiece(const PathDiagnosticLocation &startPos,
                                 const PathDiagnosticLocation &endPos)
    : PathDiagnosticPiece(ControlFlow) {
      LPairs.push_back(PathDiagnosticLocationPair(startPos, endPos));
    }

  ~PathDiagnosticControlFlowPiece();

  PathDiagnosticLocation getStartLocation() const {
    assert(!LPairs.empty() &&
           "PathDiagnosticControlFlowPiece needs at least one location.");
    return LPairs[0].getStart();
  }

  PathDiagnosticLocation getEndLocation() const {
    assert(!LPairs.empty() &&
           "PathDiagnosticControlFlowPiece needs at least one location.");
    return LPairs[0].getEnd();
  }

  void push_back(const PathDiagnosticLocationPair &X) { LPairs.push_back(X); }

  virtual PathDiagnosticLocation getLocation() const {
    return getStartLocation();
  }

  typedef std::vector<PathDiagnosticLocationPair>::iterator iterator;
  iterator begin() { return LPairs.begin(); }
  iterator end()   { return LPairs.end(); }

  virtual void flattenLocations() {
    for (iterator I=begin(), E=end(); I!=E; ++I) I->flatten();
  }

  typedef std::vector<PathDiagnosticLocationPair>::const_iterator
          const_iterator;
  const_iterator begin() const { return LPairs.begin(); }
  const_iterator end() const   { return LPairs.end(); }

  static inline bool classof(const PathDiagnosticPiece* P) {
    return P->getKind() == ControlFlow;
  }
  
  virtual void Profile(llvm::FoldingSetNodeID &ID) const;
};

class PathDiagnosticMacroPiece : public PathDiagnosticSpotPiece {
  std::vector<PathDiagnosticPiece*> SubPieces;
public:
  PathDiagnosticMacroPiece(const PathDiagnosticLocation &pos)
    : PathDiagnosticSpotPiece(pos, "", Macro) {}

  ~PathDiagnosticMacroPiece();

  bool containsEvent() const;

  void push_back(PathDiagnosticPiece* P) { SubPieces.push_back(P); }

  typedef std::vector<PathDiagnosticPiece*>::iterator iterator;
  iterator begin() { return SubPieces.begin(); }
  iterator end() { return SubPieces.end(); }

  virtual void flattenLocations() {
    PathDiagnosticSpotPiece::flattenLocations();
    for (iterator I=begin(), E=end(); I!=E; ++I) (*I)->flattenLocations();
  }

  typedef std::vector<PathDiagnosticPiece*>::const_iterator const_iterator;
  const_iterator begin() const { return SubPieces.begin(); }
  const_iterator end() const { return SubPieces.end(); }

  static inline bool classof(const PathDiagnosticPiece* P) {
    return P->getKind() == Macro;
  }
  
  virtual void Profile(llvm::FoldingSetNodeID &ID) const;
};

/// PathDiagnostic - PathDiagnostic objects represent a single path-sensitive
///  diagnostic.  It represents an ordered-collection of PathDiagnosticPieces,
///  each which represent the pieces of the path.
class PathDiagnostic : public llvm::FoldingSetNode {
  std::deque<PathDiagnosticPiece*> path;
  unsigned Size;
  std::string BugType;
  std::string Desc;
  std::string Category;
  std::deque<std::string> OtherDesc;

public:
  PathDiagnostic();

  PathDiagnostic(const char* bugtype, const char* desc, const char* category);

  PathDiagnostic(const std::string& bugtype, const std::string& desc,
                 const std::string& category);

  ~PathDiagnostic();

  const std::string& getDescription() const { return Desc; }
  const std::string& getBugType() const { return BugType; }
  const std::string& getCategory() const { return Category; }

  typedef std::deque<std::string>::const_iterator meta_iterator;
  meta_iterator meta_begin() const { return OtherDesc.begin(); }
  meta_iterator meta_end() const { return OtherDesc.end(); }
  void addMeta(const std::string& s) { OtherDesc.push_back(s); }
  void addMeta(const char* s) { OtherDesc.push_back(s); }

  PathDiagnosticLocation getLocation() const {
    assert(Size > 0 && "getLocation() requires a non-empty PathDiagnostic.");
    return rbegin()->getLocation();
  }

  void push_front(PathDiagnosticPiece* piece) {
    assert(piece);
    path.push_front(piece);
    ++Size;
  }

  void push_back(PathDiagnosticPiece* piece) {
    assert(piece);
    path.push_back(piece);
    ++Size;
  }

  PathDiagnosticPiece* back() {
    return path.back();
  }

  const PathDiagnosticPiece* back() const {
    return path.back();
  }

  unsigned size() const { return Size; }
  bool empty() const { return Size == 0; }

  void resetPath(bool deletePieces = true);

  class iterator {
  public:
    typedef std::deque<PathDiagnosticPiece*>::iterator ImplTy;

    typedef PathDiagnosticPiece              value_type;
    typedef value_type&                      reference;
    typedef value_type*                      pointer;
    typedef ptrdiff_t                        difference_type;
    typedef std::bidirectional_iterator_tag  iterator_category;

  private:
    ImplTy I;

  public:
    iterator(const ImplTy& i) : I(i) {}

    bool operator==(const iterator& X) const { return I == X.I; }
    bool operator!=(const iterator& X) const { return I != X.I; }

    PathDiagnosticPiece& operator*() const { return **I; }
    PathDiagnosticPiece* operator->() const { return *I; }

    iterator& operator++() { ++I; return *this; }
    iterator& operator--() { --I; return *this; }
  };

  class const_iterator {
  public:
    typedef std::deque<PathDiagnosticPiece*>::const_iterator ImplTy;

    typedef const PathDiagnosticPiece        value_type;
    typedef value_type&                      reference;
    typedef value_type*                      pointer;
    typedef ptrdiff_t                        difference_type;
    typedef std::bidirectional_iterator_tag  iterator_category;

  private:
    ImplTy I;

  public:
    const_iterator(const ImplTy& i) : I(i) {}

    bool operator==(const const_iterator& X) const { return I == X.I; }
    bool operator!=(const const_iterator& X) const { return I != X.I; }

    reference operator*() const { return **I; }
    pointer operator->() const { return *I; }

    const_iterator& operator++() { ++I; return *this; }
    const_iterator& operator--() { --I; return *this; }
  };

  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  // forward iterator creation methods.

  iterator begin() { return path.begin(); }
  iterator end() { return path.end(); }

  const_iterator begin() const { return path.begin(); }
  const_iterator end() const { return path.end(); }

  // reverse iterator creation methods.
  reverse_iterator rbegin()            { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const{ return const_reverse_iterator(end()); }
  reverse_iterator rend()              { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin());}

  void flattenLocations() {
    for (iterator I = begin(), E = end(); I != E; ++I) I->flattenLocations();
  }
  
  void Profile(llvm::FoldingSetNodeID &ID) const;
};  
} //end clang namespace
#endif
