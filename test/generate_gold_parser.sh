#!/bin/bash

# Generate golden files for test suite
# Usage: ./generate_gold_parser.sh [build_dir]

BUILD_DIR="${1:-../build}"
SUITE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/suite"
DPARSER="$BUILD_DIR/dparser"

if [ ! -f "$DPARSER" ]; then
  echo "Error: dparser not found at $DPARSER"
  echo "Usage: $0 [build_dir]"
  exit 1
fi

echo "Generating golden files from test suite..."
success=0
skipped=0
failed=0

for test_file in "$SUITE_DIR"/test*.d; do
  test_num=$(basename "$test_file" .d | sed 's/test//')
  gold_file="$SUITE_DIR/test${test_num}.pgold"
  
  # Skip if gold file already exists
  if [ -f "$gold_file" ]; then
    echo "⊘ test$test_num: gold file exists (skipping)"
    ((skipped++))
    continue
  fi
  
  # Run parser and capture output
  output=$("$DPARSER" "$test_file" 2>&1)
  exit_code=$?
  
  if [ $exit_code -eq 0 ]; then
    # Parse succeeded: save AST output
    echo "$output" > "$gold_file"
    echo "✓ test$test_num: generated (success)"
    ((success++))
  else
    # Parse failed: save error marker
    echo "Parse error" > "$gold_file"
    echo "⚠ test$test_num: generated (parse error)"
    ((failed++))
  fi
done

echo ""
echo "Summary: $success generated, $skipped skipped, $failed errors"