CXX      = g++
CPPFLAGS = -std=c++17 -O2 -Wall -Isrc
# CPPFLAGS = -std=c++17 -O0 -g -ggdb3 -Wall -Isrc   # debug

# OpenMP (bật cho revenue + ic)
OMPFLAGS = -fopenmp

LDLIBS   = -lpthread

.PHONY: all clean debug

# Project submodular: mỗi objective 1 binary
all: maxcut revenue ic preproc preproc_ic

# --- main với objective MAX-CUT (submodular) ---
maxcut: src/main.cpp
	$(CXX) src/main.cpp -o maxcut \
	    $(CPPFLAGS) -DSFUNC_MAXCUT $(LDLIBS)

# --- main với objective REVENUE (submodular) + OpenMP ---
revenue: src/main.cpp
	$(CXX) src/main.cpp -o revenue \
	    $(CPPFLAGS) $(OMPFLAGS) -DSFUNC_REVENUE $(LDLIBS)

# --- main với objective IC (submodular) + OpenMP ---
ic: src/main.cpp
	$(CXX) src/main.cpp -o ic \
	    $(CPPFLAGS) $(OMPFLAGS) -DSFUNC_IC $(LDLIBS)

# --- preprocess: edges.txt -> graph.bin (submodular scalar + node alpha) ---
preproc: src/data/preprocess.cpp
	$(CXX) src/data/preprocess.cpp -o preproc \
	    $(CPPFLAGS) $(LDLIBS)

# --- preprocess IC: edges.txt -> graph.bin (ALWAYS directed + ALWAYS normalize incoming) ---
preproc_ic: src/data/preprocess_ic.cpp
	$(CXX) src/data/preprocess_ic.cpp -o preproc_ic \
	    $(CPPFLAGS) $(LDLIBS)

debug: src/main.cpp
	$(CXX) src/main.cpp -o maxcut_debug \
	    -std=c++17 -O0 -g -ggdb3 -Wall -Isrc -DSFUNC_MAXCUT $(LDLIBS)

clean:
	rm -f maxcut revenue ic preproc preproc_ic maxcut_debug
