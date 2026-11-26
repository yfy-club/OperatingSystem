CC = gcc
CFLAGS = -std=gnu17 -Wall -Wextra -Wpedantic -Wconversion -Werror -O2 -g
RENDER_PYTHON ?= $(if $(wildcard reports/text/.venv/bin/python),reports/text/.venv/bin/python,python3)
SOURCES := $(wildcard src/lab*/*.c)
PROGRAMS := $(patsubst src/%.c,build/%,$(SOURCES))

.PHONY: all test record reports images
all: $(PROGRAMS)

build/%: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

test:
	python3 tests/verify.py
	python3 reports/generate_reports.py --check

record:
	python3 tests/verify.py --record

reports:
	python3 reports/generate_reports.py

images:
	$(RENDER_PYTHON) reports/text/render_all.py
