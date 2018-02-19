#pragma once
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "KaleidoscopeJIT.h"

using namespace llvm;

extern LLVMContext TheContext;
extern IRBuilder<> Builder;

extern std::unique_ptr<llvm::Module> TheModule;
extern std::map<std::string, llvm::AllocaInst *> NamedValues;
extern std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;

extern std::unique_ptr<DIBuilder> DBuilder;

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
extern std::map<char, int> BinopPrecedence;