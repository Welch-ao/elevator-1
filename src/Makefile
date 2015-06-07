#makefile for ElevatorLogic

CPP_FILES := $(wildcard *.cpp)
OBJ_FILES := $(addprefix ../obj/,$(CPP_FILES:.cpp=.o))

CC_FLAGS := -D__GXXEXPERIMENTAL_CXX0X__ -Wall -std=c++0x

cat: $(OBJ_FILES)
	g++ -o ../bin/$@ $^

../obj/%.o: %.cpp
	g++ $(CC_FLAGS) -c -o $@ $<

all: cat

clean:
	rm ../obj/* ../bin/*

test:
	# recompile everything and test all known passed tests
	# write event logs and, if needed, error logs
	make clean && make all
	rm ../log/*
	find ../passed -type f | while read line; do \
		tmp=`mktemp`; \
	    ../bin/cat "$$line" > ../log/$$(basename "$$line").log 2> $$tmp; \
	    [ ! -s "$$tmp" ] || cp "$$tmp" ../log/$$(basename "$$line").err; \
	done; \
