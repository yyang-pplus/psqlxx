#!/bin/bash

set -exuo pipefail

#
# This script generate the keyword.hpp file
#

THIS_DIR=$(dirname "$0")
source "$THIS_DIR/utils.sh"

PROJECT_ROOT_DIR=$(GetProjectRootDir)
KEYWORD_FILE="$PROJECT_ROOT_DIR/psqlxx/keyword.hpp"
PG_KW_URL="https://raw.githubusercontent.com/postgres/postgres/master/src/include/parser/kwlist.h"

rm $KEYWORD_FILE || true

cat << EOF >> $KEYWORD_FILE
#pragma once

/**
 * NOTE: This file is auto-generated.
 */

#include <string_view>
#include <unordered_set>


namespace psqlxx {

static inline const std::unordered_set<std::string_view> KEYWORDS = {
EOF

wget -O - -o /dev/null $PG_KW_URL | grep 'PG_KEYWORD(' | cut -d '"' -f 2 | xargs -I "%" echo \"%\", >> $KEYWORD_FILE

cat << EOF >> $KEYWORD_FILE
};

}//namespace psqlxx
EOF
