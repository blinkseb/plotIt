ObjSuf = o
SrcSuf = cc
DllSuf = so

ROOTCFLAGS = $(shell root-config --cflags)
ROOTLIBS   = $(shell root-config --noldflags --libs)

CXXFLAGS = -g -Wall -pedantic -fPIC --std=c++11 -O3
LD       = $(CXX)
LDDIR    = -L$(shell root-config --libdir) -Lexternal/lib -L$(BOOST_ROOT)/lib/
LDFLAGS  = -fPIC $(shell root-config --ldflags) $(LDDIR)
SOFLAGS  =
AR       = ar
ARFLAGS  = -cq

CXXFLAGS    += $(ROOTCFLAGS) $(INCLUDES) -Iinclude/ -Iexternal/include/ -I$(shell echo $(BOOST_ROOT))/include
LIBS        = $(ROOTLIBS) -lboost_filesystem -lboost_regex -lboost_system -luuid
STATIC_LIBS = -lyaml-cpp
GLIBS       = $(ROOTGLIBS)
#------------------------------------------------------------------------------
SOURCES     = $(wildcard src/*.$(SrcSuf))
OBJECTS     = $(SOURCES:.$(SrcSuf)=.$(ObjSuf))
DEPENDS     = $(SOURCES:.$(SrcSuf)=.d)
SOBJECTS    = $(SOURCES:.$(SrcSuf)=.$(DllSuf))

.SUFFIXES: .$(SrcSuf) .$(ObjSuf)

###

all: plotIt

clean:
	@rm -f $(OBJECTS);
	@rm -f $(DEPENDS);

plotIt: $(OBJECTS)
	@echo "Linking $@..."
	@$(LD) $(SOFLAGS) $(LDFLAGS) $+ -o $@ -Wl,-Bstatic $(STATIC_LIBS) -Wl,-Bdynamic $(LIBS)

%.o: %.cc
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

# Make the dependencies
%.d: %.cc
	@ $(CXX) $(CXXFLAGS) -MM $< | sed -e 's@^\(.*\)\.o:@src/\1.d src/\1.o:@' > $@

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPENDS)
endif
