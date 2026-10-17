CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread
RELEASE_FLAGS := -O3 -DNDEBUG

BUILD_DIR := build

KVSTORE_SRC := kvstore.cpp
MAIN_SRC := main.cpp
BENCHMARK_SRC := benchmark.cpp

.PHONY: all clean help \
        kvstore kvstore-shared \
        benchmark benchmark-shared \
        kvstore-metrics benchmark-metrics \
        kvstore-shared-metrics benchmark-shared-metrics

# Build everything
all: kvstore kvstore-shared benchmark benchmark-shared \
     kvstore-metrics benchmark-metrics \
     kvstore-shared-metrics benchmark-shared-metrics

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Interactive application using std::mutex
kvstore: $(BUILD_DIR)/kvstore

$(BUILD_DIR)/kvstore: $(MAIN_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 $(MAIN_SRC) $(KVSTORE_SRC) -o $@

# Interactive application using std::shared_mutex
kvstore-shared: $(BUILD_DIR)/kvstore-shared

$(BUILD_DIR)/kvstore-shared: $(MAIN_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -DUSE_SHARED_MUTEX \
		$(MAIN_SRC) $(KVSTORE_SRC) -o $@

# Benchmark using std::mutex
benchmark: $(BUILD_DIR)/benchmark

$(BUILD_DIR)/benchmark: $(BENCHMARK_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) \
		$(BENCHMARK_SRC) $(KVSTORE_SRC) -o $@

# Benchmark using std::shared_mutex
benchmark-shared: $(BUILD_DIR)/benchmark-shared

$(BUILD_DIR)/benchmark-shared: $(BENCHMARK_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -DUSE_SHARED_MUTEX \
		$(BENCHMARK_SRC) $(KVSTORE_SRC) -o $@

# Interactive application with metrics
kvstore-metrics: $(BUILD_DIR)/kvstore-metrics

$(BUILD_DIR)/kvstore-metrics: $(MAIN_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -DENABLE_METRICS \
		$(MAIN_SRC) $(KVSTORE_SRC) -o $@

# Benchmark with metrics
benchmark-metrics: $(BUILD_DIR)/benchmark-metrics

$(BUILD_DIR)/benchmark-metrics: $(BENCHMARK_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -DENABLE_METRICS \
		$(BENCHMARK_SRC) $(KVSTORE_SRC) -o $@

# Interactive application with shared_mutex and metrics
kvstore-shared-metrics: $(BUILD_DIR)/kvstore-shared-metrics

$(BUILD_DIR)/kvstore-shared-metrics: $(MAIN_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -DUSE_SHARED_MUTEX -DENABLE_METRICS \
		$(MAIN_SRC) $(KVSTORE_SRC) -o $@

# Benchmark with shared_mutex and metrics
benchmark-shared-metrics: $(BUILD_DIR)/benchmark-shared-metrics

$(BUILD_DIR)/benchmark-shared-metrics: $(BENCHMARK_SRC) $(KVSTORE_SRC) kvstore.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) \
		-DUSE_SHARED_MUTEX -DENABLE_METRICS \
		$(BENCHMARK_SRC) $(KVSTORE_SRC) -o $@

# Remove compiled binaries and benchmark output
clean:
	rm -rf $(BUILD_DIR) benchmark_results.csv

# Show available targets
help:
	@echo "Available targets:"
	@echo "  all                       Build all applications and variants"
	@echo "  kvstore                   Build interactive app (mutex)"
	@echo "  kvstore-shared            Build interactive app (shared_mutex)"
	@echo "  benchmark                 Build benchmark (mutex)"
	@echo "  benchmark-shared          Build benchmark (shared_mutex)"
	@echo "  kvstore-metrics           Build interactive app with metrics"
	@echo "  benchmark-metrics         Build benchmark with metrics"
	@echo "  kvstore-shared-metrics    Build shared_mutex app with metrics"
	@echo "  benchmark-shared-metrics  Build shared_mutex benchmark with metrics"
	@echo "  clean                     Remove build artifacts and CSV output"