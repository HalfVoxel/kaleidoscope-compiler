SOURCES = $(shell find src/lexer src/parser src/codegen -name '*.cpp')
HEADERS = $(shell find src -name '*.h')
OBJ = ${SOURCES:.cpp=.o}

CC = llvm-g++
CFLAGS = -g -O3 -I ./
LLVMFLAGS = `/usr/local/Cellar/llvm/5.0.1/bin/llvm-config --cxxflags`
LLVMFLAGS_LINK = `/usr/local/Cellar/llvm/5.0.1/bin/llvm-config --cxxflags --ldflags --system-libs --libs all`

.PHONY: main

main: src/toy.cpp ${OBJ}
	${CC} ${CFLAGS} ${LLVMFLAGS_LINK} ${OBJ} $< -o $@

clean:
	rm -r ${OBJ}

%.o: %.cpp ${HEADERS}
	${CC} ${CFLAGS} ${LLVMFLAGS} -c $< -o $@
