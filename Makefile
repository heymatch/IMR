# set C++ compiler
CC := g++
CFLAGS := -std=c++11 -O2 -static
OUTPUT := IMR

$(shell mkdir -p obj)

# part compile
IMR: obj/main.o obj/IMR_Base.o obj/IMR_Sequential.o obj/IMR_Crosstrack.o obj/IMR_Partition.o 
ifeq ($(OS), Windows_NT)
	$(CC) $(CFLAGS) -o $(OUTPUT).exe $^
else
	$(CC) $(CFLAGS) -o $(OUTPUT) $^
endif

obj/main.o: src/main.cpp 
	$(CC) $(CFLAGS) -o $@ -c $<

obj/IMR_Base.o: src/IMR_Base.cpp src/IMR_Base.h
	$(CC) $(CFLAGS) -o $@ -c $<

obj/IMR_Sequential.o: src/IMR_Sequential.cpp src/IMR_Base.cpp obj/IMR_Base.o
	$(CC) $(CFLAGS) -o $@ -c $<

obj/IMR_Crosstrack.o: src/IMR_Crosstrack.cpp src/IMR_Base.cpp obj/IMR_Base.o
	$(CC) $(CFLAGS) -o $@ -c $<

obj/IMR_Partition.o: src/IMR_Partition.cpp src/IMR_Base.cpp obj/IMR_Base.o
	$(CC) $(CFLAGS) -o $@ -c $<

# whole compile
all: clean IMR

# compile pair of .h
$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp $(SRC_PATH)%.h
	

.PHONY: clean init 

init:
ifeq ($(OS), Windows_NT)
	IF NOT EXIST "obj" MKDIR obj
else
	mkdir -p obj
endif

# clean .exe/.out and .o
clean:
ifeq ($(OS), Windows_NT)
	del .\obj\*.o
	del IMR.exe
else
	rm ./obj/*.o
	rm ./IMR
endif
