test_command=$1
expected_output=$2
test_log=$3
$test_command > $test_log 2>&1
diff $test_log $expected_output
