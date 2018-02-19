#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
#include <cctype>
#include <cstdio>
#include "common.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "codegen/codegen.h"

using namespace llvm;
using namespace llvm::orc;
using namespace std;

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);

std::unique_ptr<llvm::Module> TheModule;
std::map<std::string, llvm::AllocaInst *> NamedValues;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
std::unique_ptr<DIBuilder> DBuilder;
std::map<char, int> BinopPrecedence;

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModule() {
  // Open a new module.
  TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());
}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (!FnAST->codegen())
      fprintf(stderr, "Error reading function definition:");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (!ProtoAST->codegen())
      fprintf(stderr, "Error reading extern");
    else
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (!FnAST->codegen()) {
      fprintf(stderr, "Error generating code for top level expr");
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (1) {
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      cout << "Handle" << endl;
      HandleTopLevelExpression();
      cout << "Done" << endl;
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef LLVM_ON_WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

TargetMachine* initializeBuildTarget() {
  auto TargetTriple = sys::getDefaultTargetTriple();
  cout << TargetTriple << endl;
  std::string Error;
  auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

  auto CPU = "generic";
  auto Features = "";

  TargetOptions opt;
  auto RM = Optional<Reloc::Model>();
  auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
  TheModule->setTargetTriple(TargetTriple);
  return TheTargetMachine;
}

void initializeDebugInfo () {
  // Add the current debug info version into the module.
  TheModule->addModuleFlag(Module::Warning, "Debug Info Version",
                           DEBUG_METADATA_VERSION);

  // Darwin only supports dwarf2.
  if (Triple(sys::getProcessTriple()).isOSDarwin())
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);

  // Construct the DIBuilder, we do this here because we need the module.
  DBuilder = llvm::make_unique<DIBuilder>(*TheModule);

  cout << "!" << endl;
  // Create the compile unit for the module.
  // Currently down as "fib.ks" as a filename since we're redirecting stdin
  // but we'd like actual source locations.
  auto file = DBuilder->createFile("toy.ks", ".");
  cout << "!" << endl;


  KSDbgInfo.TheCU = DBuilder->createCompileUnit(
      dwarf::DW_LANG_C, file, "Kaleidoscope Compiler", 0, "", 0);
}

void writeWibrary(TargetMachine* targetMachine) {
  auto Filename = "output.o";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    exit(1);
  }
  legacy::PassManager pass;
  auto FileType = TargetMachine::CGFT_ObjectFile;

  if (targetMachine->addPassesToEmitFile(pass, dest, FileType)) {
    errs() << "TheTargetMachine can't emit a file of this type";
    exit(1);
  }

  pass.run(*TheModule);
  dest.flush();

  outs() << "Wrote " << Filename << "\n";
}
//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  cout << "!" << endl;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Test
  // InitializeAllTargetInfos();
  // InitializeAllTargets();
  // InitializeAllTargetMCs();
  // InitializeAllAsmParsers();
  // InitializeAllAsmPrinters();

  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  getNextToken();
  cout << "!" << endl;

  TheJIT = llvm::make_unique<KaleidoscopeJIT>();

  InitializeModule();

  auto targetMachine = initializeBuildTarget();

  initializeDebugInfo();

  // Run the main "interpreter loop" now.
  MainLoop();

  // Finalize the debug info.
  DBuilder->finalize();

  // Print out all of the generated code.
  TheModule->print(errs(), nullptr);

  writeWibrary(targetMachine);

  return 0;
}
