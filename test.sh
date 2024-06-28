#!/bin/bash
make clean
make all

# Initialize counters
passed=0
failed=0
summary_file=$(mktemp)

test_executables=(
  "./build/test_unthreaded_no_wrap/test_simple"
  "./build/test_unthreaded_no_wrap/test_complex"
  "./build/test_unthreaded_wrap/test_simple"
  "./build/test_unthreaded_wrap/test_complex"
)

for test_executable in "${test_executables[@]}"; do
  if [ ! -f $test_executable ]; then
    echo "echo -e \"\033[0;31mFILE NOT FOUND\033[0m --- $test_executable\"" >> $summary_file
  elif [ -x "$test_executable" ]; then
    echo -e "\n===> \033[0;34mRUNNING\033[0m $test_executable <===\n"
    "$test_executable" > /dev/null
    if [ $? -eq 0 ]; then
			echo "echo -e \"\033[0;32mPASSED\033[0m --- $test_executable\"" >> $summary_file
      passed=$((passed+1))
    else
			echo "echo -e \"\033[0;31mFAILED\033[0m --- $test_executable\"" >> $summary_file
      failed=$((failed+1))
    fi
  fi
done


test_executables_by_file=(
  "./build/test_unthreaded_no_wrap/test_by_file"
  "./build/test_unthreaded_wrap/test_by_file"
)

for test_executable in "${test_executables_by_file[@]}"; do
  dest_file=$(mktemp)
  if [ ! -f $test_executable ]; then
    echo "echo -e \"\033[0;31mFILE NOT FOUND\033[0m --- $test_executable\"" >> $summary_file
  elif [ -x "$test_executable" ]; then
    echo -e "\n===> \033[0;34mRUNNING\033[0m $test_executable <===\n"
    timeout 2s "$test_executable" ./test/testfile.txt "$dest_file" > /dev/null
    if [ $? -eq 0 ]; then
			echo "echo -e \"\033[0;32mPASSED\033[0m --- $test_executable\"" >> $summary_file
      passed=$((passed+1))
    else
			echo "echo -e \"\033[0;31mFAILED\033[0m --- $test_executable\"" >> $summary_file
      failed=$((failed+1))
    fi
  fi
  rm -rf $dest_file
done

test_executables_threaded=(
  "./build/test_threaded/test"
  "./build/test_daemon/test"
)

for test_executable in "${test_executables_threaded[@]}"; do
  if [ ! -f $test_executable ]; then
    echo "echo -e \"\033[0;31mFILE NOT FOUND\033[0m --- $test_executable\"" >> $summary_file
  elif [ -x "$test_executable" ]; then
    echo -e "\n===> \033[0;34mRUNNING\033[0m $test_executable <===\n"
    timeout 5s "$test_executable" > /dev/null
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
