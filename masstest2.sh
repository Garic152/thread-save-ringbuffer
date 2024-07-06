#!/bin/bash

make all

test_executable=./build/test_daemon/test

for i in $(seq 1 100); do
  if [ ! -f $test_executable ]; then
    echo -e "\033[0;31mFILE NOT FOUND\033[0m --- $test_executable" 
  elif [ -x "$test_executable" ]; then
    echo -e "\n===> \033[0;34mRUNNING\033[0m $test_executable, NUM: $i <===\n"
    timeout 6s "$test_executable"
    if [ $? -eq 0 ]; then
			echo -e "\n\033[0;32mPASSED\033[0m"
    else
			echo -e "\n\033[0;31mFAILED\033[0m"
      exit 1
    fi
  fi
done

