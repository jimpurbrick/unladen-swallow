//===--- HeaderSearchOptions.h ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FRONTEND_HEADERSEARCHOPTIONS_H
#define LLVM_CLANG_FRONTEND_HEADERSEARCHOPTIONS_H

#include "llvm/ADT/StringRef.h"
#include <vector>

namespace clang {

namespace frontend {
  /// IncludeDirGroup - Identifiers the group a include entry belongs to, which
  /// represents its relative positive in the search list.
  enum IncludeDirGroup {
    Quoted = 0,     ///< `#include ""` paths. Thing `gcc -iquote`.
    Angled,         ///< Paths for both `#include ""` and `#include <>`. (`-I`)
    System,         ///< Like Angled, but marks system directories.
    After           ///< Like System, but searched after the system directories.
  };
}

/// HeaderSearchOptions - Helper class for storing options related to the
/// initialization of the HeaderSearch object.
class HeaderSearchOptions {
public:
  struct Entry {
    std::string Path;
    frontend::IncludeDirGroup Group;
    unsigned IsUserSupplied : 1;
    unsigned IsFramework : 1;

    Entry(llvm::StringRef _Path, frontend::IncludeDirGroup _Group,
          bool _IsUserSupplied, bool _IsFramework)
      : Path(_Path), Group(_Group), IsUserSupplied(_IsUserSupplied),
        IsFramework(_IsFramework) {}
  };

  /// If non-empty, the directory to use as a "virtual system root" for include
  /// paths.
  std::string Sysroot;

  /// User specified include entries.
  std::vector<Entry> UserEntries;

  /// A (system-path) delimited list of include paths to be added from the
  /// environment following the user specified includes (but prior to builtin
  /// and standard includes). This is parsed in the same manner as the CPATH
  /// environment variable for gcc.
  std::string EnvIncPath;

  /// Per-language environmental include paths, see \see EnvIncPath.
  std::string CEnvIncPath;
  std::string ObjCEnvIncPath;
  std::string CXXEnvIncPath;
  std::string ObjCXXEnvIncPath;

  /// If non-empty, the path to the compiler builtin include directory, which
  /// will be searched following the user and environment includes.
  std::string BuiltinIncludePath;

  /// Include the system standard include search directories.
  unsigned UseStandardIncludes : 1;

  /// Whether header search information should be output as for -v.
  unsigned Verbose : 1;

public:
  HeaderSearchOptions(llvm::StringRef _Sysroot = "/")
    : Sysroot(_Sysroot), UseStandardIncludes(true), Verbose(false) {}

  /// AddPath - Add the \arg Path path to the specified \arg Group list.
  void AddPath(llvm::StringRef Path, frontend::IncludeDirGroup Group,
               bool IsUserSupplied, bool IsFramework) {
    UserEntries.push_back(Entry(Path, Group, IsUserSupplied, IsFramework));
  }
};

} // end namespace clang

#endif
