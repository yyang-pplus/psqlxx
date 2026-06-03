#!/bin/bash

set -euo pipefail

#
# This script setup DB for current user
#

sudo -u postgres createuser --superuser $USER
sudo -u postgres createdb $USER
