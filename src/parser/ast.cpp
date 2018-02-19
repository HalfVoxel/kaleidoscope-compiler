#include "ast.h"

raw_ostream &indent(raw_ostream &O, int size) {
  return O << std::string(size, ' ');
}