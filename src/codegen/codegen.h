#pragma once
#include "../parser/ast.h"
#include "../common.h"

extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

struct DebugInfo {
  DICompileUnit *TheCU;
  DIType *DblTy;
  std::vector<DIScope *> LexicalBlocks;

  void emitLocation(ExprAST *AST);
  DIType *getDoubleTy();
};

extern DebugInfo KSDbgInfo;