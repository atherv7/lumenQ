.PHONY: all
all: build

.PHONY: init
init:
	@cmake -B build -DCMAKE_BUILD_TYPE=Release

.PHONY: build
build:
	@if [ ! -d "build" ]; then cmake -B build -DCMAKE_BUILD_TYPE=Release; fi
	@cmake --build build -j$$(nproc)
	@if [ -f "build/compile_commands.json" ] && [ ! -f "compile_commands.json" ]; then \
		ln -s build/compile_commands.json compile_commands.json; \
	fi

.PHONY: test
test:
	@cd build && ctest --output-on-failure

.PHONY: clean
clean:
	@echo "Scrubbing build directory and linter artifacts..."
	@rm -rf build compile_commands.json
