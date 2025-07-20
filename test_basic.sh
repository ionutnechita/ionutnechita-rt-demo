#!/bin/bash
# Basic test for RT Demo (no root required)

set -e

echo "=== RT Demo Basic Test (No Root Required) ==="

RT_DEMO="./rt_demo"

# Test 1: Binary exists and is executable
echo "Test 1: Binary check..."
if [[ -x "$RT_DEMO" ]]; then
    echo "✓ Binary exists and is executable"
else
    echo "✗ Binary not found or not executable"
    exit 1
fi

# Test 2: Help/version output (run for 1 second and kill)
echo "Test 2: Basic execution..."
timeout 3 "$RT_DEMO" 1 > /tmp/rt_demo_output.log 2>&1 || true

if grep -q "PREEMPT_RT KERNEL DEMO" /tmp/rt_demo_output.log; then
    echo "✓ Application starts correctly"
else
    echo "✗ Application failed to start properly"
    cat /tmp/rt_demo_output.log
    exit 1
fi

# Test 3: CPU configuration detection
echo "Test 3: CPU configuration detection..."
if grep -q "RT CPU CONFIGURATION" /tmp/rt_demo_output.log; then
    echo "✓ CPU configuration detection works"
else
    echo "✗ CPU configuration detection failed"
    exit 1
fi

# Test 4: System information
echo "Test 4: System information..."
if grep -q "SYSTEM INFORMATION" /tmp/rt_demo_output.log; then
    echo "✓ System information detection works"
else
    echo "✗ System information detection failed"
    exit 1
fi

# Test 5: Check for expected warnings (normal without root)
echo "Test 5: Expected warnings check..."
if grep -q "Warning.*Could not.*memory.*limit\|Operation not permitted" /tmp/rt_demo_output.log; then
    echo "✓ Expected warnings present (normal without root privileges)"
else
    echo "⚠ No permission warnings found (might be running with elevated privileges)"
fi

# Cleanup
rm -f /tmp/rt_demo_output.log

echo "=== Basic Test Summary ==="
echo "✓ All basic tests passed!"
echo "Note: For full RT testing, run with root privileges using:"
echo "  sudo ./test_rt_demo.sh"

exit 0
