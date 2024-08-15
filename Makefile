# 
# FF_ROOT     pointing to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT
FF_ROOT		= ${HOME}/fastflow
endif

CXX		= g++ -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	= -DFF_BOUNDED_BUFFER -DDEFAULT_BUFFER_CAPACITY=512

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= compdecomp \
		  ffc_farm

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

compdecomp	: compdecomp.cpp utility.hpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c

ffc_farm       : ffc_farm.cpp utility.hpp cmdline.hpp datatask.hpp reader.hpp worker.hpp writer.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)


clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



