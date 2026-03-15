#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>

using namespace clang;

struct VariableStats {
  int global = 0;
  int static_global = 0;
  int static_local = 0;
  int local = 0;
  int parameter = 0;

  int total_files = 0;
  int total_functions = 0;
  std::string current_file;

  void print() {
    llvm::outs() << "\n=== Variable Statistics for Translation Unit ===\n";
    if (!current_file.empty()) {
      llvm::outs() << "File: " << current_file << "\n";
    }
    llvm::outs() << "Global variables:        " << global << "\n";
    llvm::outs() << "Static global variables: " << static_global << "\n";
    llvm::outs() << "Static local variables:  " << static_local << "\n";
    llvm::outs() << "Local variables:         " << local << "\n";
    llvm::outs() << "Function parameters:     " << parameter << "\n";
    llvm::outs() << "==============================================\n";
    llvm::outs() << "TOTAL:                   "
                 << (global + static_global + static_local + local + parameter)
                 << "\n";
    if (total_functions > 0) {
      llvm::outs() << "Functions analyzed:      " << total_functions << "\n";
    }
    llvm::outs() << "\n";
  }
};

class EgorovaVariableStatsVisitor final
    : public RecursiveASTVisitor<EgorovaVariableStatsVisitor> {
public:
  explicit EgorovaVariableStatsVisitor(ASTContext *context,
                                       VariableStats &stats)
      : m_context(context), m_stats(stats) {

    SourceManager &sm = context->getSourceManager();
    if (sm.isInMainFile(sm.getLocForStartOfFile(sm.getMainFileID()))) {
      StringRef filename =
          sm.getFilename(sm.getLocForStartOfFile(sm.getMainFileID()));
      if (!filename.empty()) {
        m_stats.current_file = filename.str();
      }
    }
  }

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (FD->isThisDeclarationADefinition()) {
      m_stats.total_functions++;
    }
    return true;
  }

  bool VisitVarDecl(VarDecl *VD) {
    if (VD->isImplicit() || VD->getLocation().isInvalid()) {
      return true;
    }

    if (!isInMainFile(VD->getLocation())) {
      return true;
    }

    if (isa<ParmVarDecl>(VD)) {
      return true;
    }

    if (!VD->isFileVarDecl()) {
      if (VD->isStaticLocal()) {
        m_stats.static_local++;
        llvm::outs() << "Static local: " << VD->getNameAsString() << " ("
                     << VD->getType().getAsString() << ")\n";
      } else {
        m_stats.local++;
        llvm::outs() << "Local: " << VD->getNameAsString() << " ("
                     << VD->getType().getAsString() << ")\n";
      }
    } else {
      if (VD->getStorageClass() == SC_Static) {
        m_stats.static_global++;
        llvm::outs() << "Static global: " << VD->getNameAsString() << " ("
                     << VD->getType().getAsString() << ")\n";
      } else {
        m_stats.global++;
        llvm::outs() << "Global: " << VD->getNameAsString() << " ("
                     << VD->getType().getAsString() << ")\n";
      }
    }
    return true;
  }

  bool VisitParmVarDecl(ParmVarDecl *PD) {
    if (!isInMainFile(PD->getLocation())) {
      return true;
    }

    m_stats.parameter++;
    llvm::outs() << "Parameter: " << PD->getNameAsString() << " ("
                 << PD->getType().getAsString() << ")\n";
    return true;
  }

private:
  bool isInMainFile(SourceLocation Loc) {
    if (Loc.isInvalid())
      return false;
    SourceManager &SM = m_context->getSourceManager();
    return SM.isInMainFile(Loc);
  }

  ASTContext *m_context;
  VariableStats &m_stats;
};

class EgorovaVariableStatsConsumer final : public ASTConsumer {
public:
  explicit EgorovaVariableStatsConsumer(ASTContext *context)
      : m_stats(), m_visitor(context, m_stats) {}

  void HandleTranslationUnit(ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_stats.print();
  }

private:
  VariableStats m_stats;
  EgorovaVariableStatsVisitor m_visitor;
};

class EgorovaVariableStatsAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<EgorovaVariableStatsConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

static FrontendPluginRegistry::Add<EgorovaVariableStatsAction>
    X("egorova-variable-stats",
      "Plugin for collecting statistics about variables in translation unit");
