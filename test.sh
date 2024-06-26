#!/bin/bash
make clean
make all

# Initialize counters
passed=0
failed=0
summary_file=$(mktemp)

# Iterate over the test executables
for test_executable in $(find build/ -type f \( -name "test" -o -name "test_*" \)); do
  if [ -x "$test_executable" ]; then
    echo -e "\n===> \033[0;34mRUNNING\033[0m $test_executable <===\n"
    "$test_executable"
    if [ $? -eq 0 ]; then
			echo "echo -e \"\033[0;32mPASSED\033[0m --- $test_executable\"" >> $summary_file
      passed=$((passed+1))
    else
			echo "echo -e \"\033[0;31mFAILED\033[0m --- $test_executable\"" >> $summary_file
      failed=$((failed+1))
    fi
  fi
done

total=$((passed + failed))

echo -e "===SUMMARY===\n"

source $summary_file
rm -rf $summary_file

echo "---"

if [ $failed -ne 0 ]; then
  echo -e "\033[0;31mFAILED:\033[0m $failed / $total"
fi
if [ $passed -ne 0 ]; then
  echo -e "\033[0;32mPASSED:\033[0m $passed / $total"
fi

exit $failed
