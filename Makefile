# set C++ compiler
CC := g++
CFLAGS := -std=c++11 -O2 -static
OUTPUT := IMR

# source
ifeq ($(OS), Windows_NT)
SRC_LIST := $(shell dir /b .\src\*.h)
else
SRC_LIST := $(shell ls ./src/*.h)
endif
SRC_PATH := ./src/
SRC := $(SRC_LIST)

# object
OBJ_PATH := ./obj/
ifeq ($(OS), Windows_NT)
OBJ := $(patsubst %.h, $(OBJ_PATH)%.o, $(SRC_LIST))
else
OBJ := $(patsubst $(SRC_PATH)%.h, $(OBJ_PATH)%.o, $(SRC_LIST))
endif

# part compile
fast: compile

# whole compile
all: clear compile

# compile only
compile: init $(OBJ)
ifeq ($(OS), Windows_NT)
	$(CC) $(CFLAGS) -o $(OBJ_PATH)main.o -c $(SRC_PATH)main.cpp
	$(CC) $(CFLAGS) -o $(OUTPUT).exe $(OBJ) $(OBJ_PATH)main.o
else
	$(CC) $(CFLAGS) -o $(OBJ_PATH)main.o -c $(SRC_PATH)main.cpp
	$(CC) $(CFLAGS) -o $(OUTPUT) $(OBJ) $(OBJ_PATH)main.o
endif

# compile pair of .h
$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp $(SRC_PATH)%.h
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clear init

init:
ifeq ($(OS), Windows_NT)
	IF NOT EXIST "obj" MKDIR obj
else
	mkdir -p obj
endif

# clear .exe/.out and .o
clear:
ifeq ($(OS), Windows_NT)
	del .\obj\*.o
	del Skyrmion.exe
else
	rm ./obj/*.o
	rm ./Skyrmion
endif
