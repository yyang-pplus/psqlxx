#!/bin/bash

set -euo pipefail

#
# This script installs required dependencies.
#

sudo apt update

sudo apt --yes install pkg-config
sudo apt --yes install libpq-dev postgresql postgresql-client postgresql-contrib
sudo apt --yes install libedit-dev

sudo apt --yes install shunit2
