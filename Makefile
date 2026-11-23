CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BIN := build/paisa-ledger

.PHONY: all demo test clean
all: $(BIN)
$(BIN): src/main.cpp
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) -o $@
demo: $(BIN)
	./$(BIN) --month 2026-11 --budget 5000 examples/transactions.csv
test: $(BIN)
	LEDGER_BIN="$(CURDIR)/$(BIN)" python3 -m unittest discover -s tests -v
clean:
	rm -f $(BIN)
