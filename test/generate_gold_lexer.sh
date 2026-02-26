#!/bin/bash

# Generate lexer golden files (.lgold) for the test suite.
# Usage: ./generate_gold_lexer.sh [build_dir]

BUILD_DIR="${1:-../build}"
SUITE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/suite"
DLEXER="$BUILD_DIR/dlexer"

if [ ! -f "$DLEXER" ]; then
  echo "Error: dlexer not found at $DLEXER"
  echo "Usage: $0 [build_dir]"
  exit 1
fi

echo "Generating .lgold files from test suite..."
success=0
skipped=0
failed=0

for test_file in "$SUITE_DIR"/test*.d; do
  test_num=$(basename "$test_file" .d | sed 's/test//')
  gold_file="$SUITE_DIR/test${test_num}.lgold"

  # Skip if gold file already exists
  if [ -f "$gold_file" ]; then
    echo "⊘ test$test_num: .lgold exists (skipping)"
    ((skipped++))
    continue
  fi

  # Run dlexer and capture output
  output=$("$DLEXER" "$test_file" 2>/dev/null)
  exit_code=$?

  if [ $exit_code -eq 0 ]; then
    echo "$output" > "$gold_file"
    echo "✓ test$test_num: generated"
    ((success++))
  else
    echo "✗ test$test_num: dlexer failed (exit $exit_code)"
    ((failed++))
  fi
done

echo ""
echo "Done: $success generated, $skipped skipped, $failed failed"
