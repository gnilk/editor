#!/bin/sh

# clone my forks and/or other private repos first
git clone https://github.com/gnilk/dukglue ext/dukglue
git clone https://github.com/gnilk/logger ext/logger

# clone specifically branch 2.7.0 of duktape
# NOTE: You need to run the configure script first!!!
git clone -b v2.7.0 https://github.com/svaarala/duktape ext/duktape-v2.7.0
git clone https://github.com/nlohmann/json ext/json
#
# Note: Other dependencies you need to install include
#
#   - SDL3 or SDL2 (depending on which you prefer)
#   - NCurses (the backend is still a hard requirement)
#   - yaml-cpp, https://github.com/jbeder/yaml-cpp
#   - nlohmann/json, https://github.com/nlohmann/json

# This is added already directly to the repo
# - stb, https://github.com/nothings/stb, ttf and rect_pack (added to my repo)
