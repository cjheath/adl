#
# Vanilla makefile for adl
#

CXX	=	g++
CXXFLAGS =	-std=c++17 $(COPT)

STRPP	=	../../strpp
PX	=	../../px

COPT	=	-I. -I$(STRPP)/include -DHAVE_PTHREADS

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		adlparser.h \
		adlstore.h \
		adlmem.h

SRCS	=	

BINSRC	=	\
		adl_scan.cpp \
		adlmem.cpp

OBJS	=	$(patsubst %,%,$(SRCS:.cpp=.o))
BINS	=	$(patsubst %,%,$(BINSRC:.cpp=))

LIBS	=	-L.. -L$(STRPP) -lstrpp
#		test/memory_monitor.cpp 

# vpath	%.cpp	../src
vpath	%.h	../include
vpath	%.a	..

all:	strpp px lib bins doc

bins:	$(BINS)

lib:

%:	%.cpp $(LIB) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< $(LIB) $(LIBS)

adlmem:		adlparser.h adlstore.h adlmem.h
adl_scan:	adlparser.h

doc:	adl-rr.html
adl-rr.html: px adl.px
	$(PX)/px -r adl.px > adl-rr.html

px:	
	@cd $(PX); $(MAKE) px

strpp:	
	@cd $(STRPP); $(MAKE) lib

test:

tests:

clean:
	rm -f $(BINS) $(OBJS)
	rm -rf *.dSYM

clobber:	clean

.PHONY:	all bins lib doc test tests clean clobber strpp px
