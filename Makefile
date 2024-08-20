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

TARGETS		= sequential \
		  ffa2a \
		  mpiminiz \
		  generateTxt

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

sequential	: sequential.cpp utility.hpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c

ffa2a	: ffa2a.cpp utility.hpp
	$(CXX) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

mpiminiz	: mpiminiz.cpp utility.hpp
	$(CXXMPI) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c -fopenmp $(LDFLAGS)

generateTxt : generateTxt.cpp
	$(CXX) $(OPTFLAGS) -o $@ $< 

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



