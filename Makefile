# 
# FF_ROOT     pointing to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT
FF_ROOT		= ${HOME}/fastflow
MPI_ROOT		= ${HOME}/mpi
endif

CXX		= g++ -std=c++20
CXXMPI  = mpicxx -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	= -DFF_BOUNDED_BUFFER -DDEFAULT_BUFFER_CAPACITY=512

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= SEQ_minizip \
		  FF_minizip \
		  MPI_minizip \
		  generateTxt

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

SEQ_minizip	: SEQ_minizip.cpp utility.hpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c

FF_minizip	: FF_minizip.cpp utility.hpp
	$(CXX) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

MPI_minizip : MPI_minizip.cpp utility.hpp
	$(CXXMPI) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c -fopenmp $(LDFLAGS)

generateTxt : generateTxt.cpp
	$(CXX) $(OPTFLAGS) -o $@ $< 

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



