#!/usr/bin/env bash
# Recrée une base de test, compile et lance le test de scénario.
# Usage : ./tests/run_tests.sh   (nécessite un accès root MySQL : sudo mysql)
set -euo pipefail
cd "$(dirname "$0")/.."
MYSQL="${MYSQL_ADMIN_CMD:-sudo mysql}"
PASS="test_only_pass"

$MYSQL -e "DROP DATABASE IF EXISTS bibliotheque_test; DROP USER IF EXISTS 'biblio_user'@'localhost';"
sed -e 's/bibliotheque/bibliotheque_test/g' -e "s/CHANGE_MOI/$PASS/" sql/schema.sql | $MYSQL

BUILD_DIR="${BUILD_DIR:-build}"
SRC="$(pwd)"
mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR" && cmake "$SRC" >/dev/null && make -j2 test_biblio
BIBLIO_DB_PASS="$PASS" BIBLIO_DB_NAME=bibliotheque_test ./test_biblio
