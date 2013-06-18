include $(OPENMRNPATH)/etc/path.mk

TARGET := $(shell basename `cd ../; pwd`)
BASENAME = $(shell basename `pwd`)
SRCDIR = $(OPENMRNPATH)/src/$(BASENAME)
VPATH = $(SRCDIR)

INCLUDES += -I./ -I$(OPENMRNPATH)/src/ -I $(OPENMRNPATH)/include
include $(OPENMRNPATH)/etc/$(TARGET).mk

exist := $(wildcard $(SRCDIR)/sources)
ifneq ($(strip $(exist)),)
include $(VPATH)/sources
else
exist := $(wildcard sources)
ifneq ($(strip $(exist)),)
include sources
else
FULLPATHCSRCS = $(wildcard $(VPATH)/*.c)
FULLPATHCXXSRCS = $(wildcard $(VPATH)/*.cxx)
FULLPATHCPPSRCS = $(wildcard $(VPATH)/*.cpp)
CSRCS = $(notdir $(FULLPATHCSRCS))
CXXSRCS = $(notdir $(FULLPATHCXXSRCS))
CPPSRCS = $(notdir $(FULLPATHCPPSRCS))
endif
endif

OBJS = $(CXXSRCS:.cxx=.o) $(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o) $(ARM_CSRCS:.c=.o)
LIBNAME = lib$(BASENAME).a

ARM_CSRCS ?=
ARM_OBJS = $(ARM_CSRCS:.c=.o)

ARM_CFLAGS += $(INCLUDES)
CFLAGS += $(INCLUDES)
CXXFLAGS += $(INCLUDES)

DEPS += TOOLPATH
MISSING_DEPS:=$(call find_missing_deps,$(DEPS))

ifneq ($(MISSING_DEPS),)

all docs clean veryclean tests:
	@echo "******************************************************************"
	@echo "*"
	@echo "*   Unable to build for $(TARGET), missing dependencies: $(MISSING_DEPS)"
	@echo "*"
	@echo "******************************************************************"

else
.PHONY: all
all: $(LIBNAME)

-include $(OBJS:.o=.d)

.SUFFIXES:
.SUFFIXES: .o .c .cxx .cpp

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM $(CXXFLAGS) $< > $*.d

.cxx.o:
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM $(CXXFLAGS) $< > $*.d

$(ARM_OBJS): %.o : %.c
	$(CC) $(ARM_CFLAGS) $< -o $@
	$(CC) -MM $(ARM_CFLAGS) $< > $*.d

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $(CFLAGS) $< > $*.d

$(LIBNAME): $(OBJS)
	$(AR) cr $(LIBNAME) $(OBJS)
	cp $(LIBNAME) ../lib/$(LIBNAME)

.PHONY: clean
clean:
	rm -rf *.o *.d *.a *.so *.dll

.PHONY: veryclean
veryclean: clean


.PHONY: tests
tests : 


endif
