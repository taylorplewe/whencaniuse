CXX=c++
CXXFLAGS=-std=c++23
LDFLAGS=-l curl -l simdjson

out: gen-combined-whencaniuse-data-file

gen-combined-whencaniuse-data-file: gen-combined-whencaniuse-data-file.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@
