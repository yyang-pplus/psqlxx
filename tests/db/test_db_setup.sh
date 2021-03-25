#!/bin/bash

#
# This script setup DB for tests
#

set -ex

source test_db_defines.sh

if psql --command "SELECT 1;" &> /dev/null; then
    CMD_PREFIX=""
else
    CMD_PREFIX="sudo -u postgres"
fi

$CMD_PREFIX psql --command "CREATE USER $ADMIN_USER_NAME WITH CREATEDB NOCREATEROLE LOGIN PASSWORD '$ADMIN_USER_PASSWORD';"

$CMD_PREFIX psql --command "CREATE USER $READ_USER_NAME WITH NOCREATEDB NOCREATEROLE LOGIN PASSWORD '$READ_USER_PASSWORD';"

$CMD_PREFIX createdb --owner=$ADMIN_USER_NAME $SHARED_DB_NAME

psql --host=$DB_HOST --command "SELECT 1;" "dbname=$SHARED_DB_NAME user=$ADMIN_USER_NAME password='$ADMIN_USER_PASSWORD'"
