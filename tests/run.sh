#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -pthread \
    -Itests/stubs -Imodule/jni/include tests/native_test.cpp module/jni/config.cpp -o "$out/native-test"
"$out/native-test"
${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -pthread \
    -Itests/stubs -Imodule/jni/include -Imodule/jni/aosp_fd_utils \
    tests/lifecycle_test.cpp module/jni/config.cpp -o "$out/lifecycle-test"
"$out/lifecycle-test"
${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -pthread tests/atexit_test.cpp -o "$out/atexit-test"
"$out/atexit-test"
${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -fsyntax-only \
    -Imodule/jni/include -Imodule/jni/aosp_fd_utils/include -Itests/stubs module/jni/fd_reopener.cpp
node tests/webui_test.js
python3 tests/test_packaging.py
printf 'Host regression checks passed. Android/runtime checks remain separate.\n'
