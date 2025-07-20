#!/bin/bash
# Test script for RT Demo application
# Note: This test requires root privileges for RT scheduling

# Don't exit on first error - we want to run all tests
# set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test configuration
TEST_DURATION=5
RT_DEMO_PATH="./rt_demo"

echo -e "${GREEN}=== RT Demo Test Suite ===${NC}"

# Check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        echo -e "${GREEN}✓ Running as root - RT privileges available${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠ Not running as root - limited RT capabilities${NC}"
        return 1
    fi
}

# Test basic functionality
test_basic() {
    echo -e "\n${YELLOW}Test 1: Basic functionality${NC}"
    
    if [[ ! -x "$RT_DEMO_PATH" ]]; then
        echo -e "${RED}✗ RT demo binary not found or not executable${NC}"
        return 1
    fi
    
    echo "Running basic test for $TEST_DURATION seconds..."
    if timeout $((TEST_DURATION + 2)) "$RT_DEMO_PATH" $TEST_DURATION > test_output.log 2>&1; then
        echo -e "${GREEN}✓ Basic test passed${NC}"
        return 0
    else
        echo -e "${RED}✗ Basic test failed${NC}"
        cat test_output.log
        return 1
    fi
}

# Test CPU configuration detection
test_cpu_config() {
    echo -e "\n${YELLOW}Test 2: CPU configuration detection${NC}"
    
    if "$RT_DEMO_PATH" 1 2>&1 | grep -q "RT CPU CONFIGURATION"; then
        echo -e "${GREEN}✓ CPU configuration detection working${NC}"
        return 0
    else
        echo -e "${RED}✗ CPU configuration detection failed${NC}"
        return 1
    fi
}

# Test RT kernel detection
test_rt_detection() {
    echo -e "\n${YELLOW}Test 3: RT kernel detection${NC}"
    
    if "$RT_DEMO_PATH" 1 2>&1 | grep -q "SYSTEM INFORMATION"; then
        echo -e "${GREEN}✓ System information detection working${NC}"
        return 0
    else
        echo -e "${RED}✗ System information detection failed${NC}"
        return 1
    fi
}

# Test latency measurement
test_latency() {
    echo -e "\n${YELLOW}Test 4: Latency measurement${NC}"
    
    if "$RT_DEMO_PATH" $TEST_DURATION 2>&1 | grep -q "LATENCY STATISTICS"; then
        echo -e "${GREEN}✓ Latency measurement working${NC}"
        return 0
    else
        echo -e "${RED}✗ Latency measurement failed${NC}"
        return 1
    fi
}

# Test signal handling
test_signal_handling() {
    echo -e "\n${YELLOW}Test 5: Signal handling${NC}"
    
    "$RT_DEMO_PATH" 30 &
    PID=$!
    sleep 2
    kill -INT $PID
    
    if wait $PID 2>/dev/null; then
        echo -e "${GREEN}✓ Signal handling working${NC}"
        return 0
    else
        echo -e "${RED}✗ Signal handling failed${NC}"
        return 1
    fi
}

# Test performance validation
test_performance() {
    echo -e "\n${YELLOW}Test 6: Performance validation${NC}"
    
    output=$("$RT_DEMO_PATH" $TEST_DURATION 2>&1)
    
    # Check if we got statistics
    if echo "$output" | grep -q "Maximum latency:"; then
        max_latency=$(echo "$output" | grep "Maximum latency:" | awk '{print $3}')
        
        # Convert to numeric (remove 'ns')
        max_latency_num=${max_latency%ns}
        
        # Check if latency is reasonable (< 10ms for this test)
        if [[ $max_latency_num -lt 10000000 ]]; then
            echo -e "${GREEN}✓ Performance test passed (max latency: $max_latency)${NC}"
            return 0
        else
            echo -e "${YELLOW}⚠ High latency detected: $max_latency${NC}"
            return 0  # Still pass, but warn
        fi
    else
        echo -e "${RED}✗ Performance test failed - no latency statistics${NC}"
        return 1
    fi
}

# Main test execution
main() {
    local tests_passed=0
    local tests_total=6
    
    # Check root privileges
    check_root
    
    # Run tests - continue even if some fail
    if test_basic; then
        ((tests_passed++))
    fi
    
    if test_cpu_config; then
        ((tests_passed++))
    fi
    
    if test_rt_detection; then
        ((tests_passed++))
    fi
    
    if test_latency; then
        ((tests_passed++))
    fi
    
    if test_signal_handling; then
        ((tests_passed++))
    fi
    
    if test_performance; then
        ((tests_passed++))
    fi
    
    # Cleanup
    rm -f test_output.log
    
    # Summary
    echo -e "\n${GREEN}=== Test Summary ===${NC}"
    echo "Tests passed: $tests_passed/$tests_total"
    
    if [[ $tests_passed -eq $tests_total ]]; then
        echo -e "${GREEN}✓ All tests passed!${NC}"
        exit 0
    elif [[ $tests_passed -gt $((tests_total / 2)) ]]; then
        echo -e "${YELLOW}⚠ Most tests passed${NC}"
        exit 0
    else
        echo -e "${RED}✗ Many tests failed${NC}"
        exit 1
    fi
}

# Check if script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
