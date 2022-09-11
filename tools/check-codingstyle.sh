#!/bin/sh

clang-format --dry-run lib/*.c
clang-format --dry-run lib/*.h
clang-format --dry-run include/*.h
