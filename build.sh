#!/bin/bash
# Build script for RT Demo using Bazel

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Default configuration
CONFIG="release"
VERBOSE=""
CLEAN=""

# Usage function
usage() {
    cat << EOF
Usage: $0 [OPTIONS] [TARGET]

Build RT Demo application using Bazel

OPTIONS:
    -c, --config CONFIG    Build configuration (debug, release, rt, asan, tsan)
    -v, --verbose         Verbose output
    --clean              Clean before build
    -h, --help           Show this help

TARGETS:
    rt_demo              Standard build (default)
    rt_demo_debug        Debug build with symbols
    rt_demo_release      Optimized release build
    rt_demo_profile      Build with profiling support
    rt_demo_asan         Build with AddressSanitizer
    rt_demo_tsan         Build with ThreadSanitizer
    all                  Build all variants
    test                 Run tests (interactive choice: basic/full/both)
    test-basic           Run basic tests only (no root required)
    test-full            Run full RT tests only (requires sudo)
    docs                 Generate documentation

EXAMPLES:
    $0                           # Build release version
    $0 --config debug rt_demo    # Build debug version
    $0 --clean all               # Clean and build all variants
    $0 test                      # Run tests
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--config)
            CONFIG="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="--verbose_failures --sandbox_debug"
            shift
            ;;
        --clean)
            CLEAN="yes"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option $1"
            usage
            exit 1
            ;;
        *)
            TARGET="$1"
            shift
            ;;
    esac
done

# Set default target
TARGET=${TARGET:-rt_demo}

echo -e "${GREEN}=== Building RT Demo with Bazel ===${NC}"
echo "Configuration: $CONFIG"
echo "Target: $TARGET"
echo ""

# Clean if requested
if [[ "$CLEAN" == "yes" ]]; then
    echo -e "${YELLOW}Cleaning previous builds...${NC}"
    bazel clean --expunge
    echo ""
fi

# Validate configuration
case $CONFIG in
    debug|release|rt|asan|tsan|msan|ubsan|coverage)
        ;;
    *)
        echo -e "${RED}Invalid configuration: $CONFIG${NC}"
        echo "Valid configurations: debug, release, rt, asan, tsan, msan, ubsan, coverage"
        exit 1
        ;;
esac

# Build function
build_target() {
    local target=$1
    local config=$2
    
    echo -e "${YELLOW}Building $target with $config configuration...${NC}"
    
    bazel build \
        --config=$config \
        $VERBOSE \
        //:$target
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}✓ Successfully built $target${NC}"
        
        # Show binary location
        binary_path=$(bazel info bazel-bin)/$target
        if [[ -f "$binary_path" ]]; then
            echo "Binary location: $binary_path"
            ls -la "$binary_path"
        fi
    else
        echo -e "${RED}✗ Failed to build $target${NC}"
        return 1
    fi
}

# Test function
run_tests() {
    echo -e "${YELLOW}Running tests...${NC}"
    
    # Make test scripts executable
    chmod +x test_rt_demo.sh test_basic.sh
    
    echo -e "${YELLOW}Note: RT tests require root privileges for optimal results${NC}"
    echo -e "${YELLOW}Choose test method:${NC}"
    echo "1) Run basic test (no root required, limited functionality)"
    echo "2) Run full RT test with sudo (complete RT capabilities)"
    echo "3) Run both"
    
    read -p "Enter choice (1-3) [default: 2]: " choice
    choice=${choice:-2}
    
    case $choice in
        1)
            echo -e "${YELLOW}Running basic test (no root required)...${NC}"
            ./test_basic.sh
            ;;
        2)
            echo -e "${YELLOW}Running full RT test with sudo...${NC}"
            sudo ./test_rt_demo.sh
            ;;
        3)
            echo -e "${YELLOW}Running basic test first...${NC}"
            ./test_basic.sh
            
            echo -e "\n${YELLOW}Now running full RT test with sudo...${NC}"
            sudo ./test_rt_demo.sh
            ;;
        *)
            echo -e "${RED}Invalid choice, running basic test...${NC}"
            ./test_basic.sh
            ;;
    esac
    
    local exit_code=$?
    if [[ $exit_code -eq 0 ]]; then
        echo -e "${GREEN}✓ Tests completed successfully${NC}"
    else
        echo -e "${RED}✗ Some tests failed${NC}"
        return 1
    fi
}

# Documentation function
generate_docs() {
    echo -e "${YELLOW}Generating documentation...${NC}"
    
    bazel build \
        $VERBOSE \
        //:docs
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}✓ Documentation generated${NC}"
        docs_path=$(bazel info bazel-bin)/rt_demo_docs.html
        echo "Documentation: $docs_path"
    else
        echo -e "${RED}✗ Failed to generate documentation${NC}"
        return 1
    fi
}

# Static analysis function
static_analysis() {
    echo -e "${YELLOW}Running static analysis...${NC}"
    
    bazel build \
        $VERBOSE \
        //:static_analysis
    
    if [[ $? -eq 0 ]]; then
        echo -e "${GREEN}✓ Static analysis completed${NC}"
        report_path=$(bazel info bazel-bin)/analysis_report.txt
        if [[ -f "$report_path" ]]; then
            echo "Analysis report: $report_path"
            echo "--- Report Preview ---"
            head -20 "$report_path"
        fi
    else
        echo -e "${RED}✗ Static analysis failed${NC}"
        return 1
    fi
}

# Main build logic
case $TARGET in
    rt_demo|rt_demo_debug|rt_demo_release|rt_demo_profile|rt_demo_asan|rt_demo_tsan)
        build_target "$TARGET" "$CONFIG"
        ;;
    all)
        echo -e "${YELLOW}Building all variants...${NC}"
        build_target "rt_demo" "$CONFIG"
        build_target "rt_demo_debug" "debug"
        build_target "rt_demo_release" "release"
        build_target "rt_demo_profile" "$CONFIG"
        if command -v clang &> /dev/null; then
            build_target "rt_demo_asan" "asan"
            build_target "rt_demo_tsan" "tsan"
        else
            echo -e "${YELLOW}Skipping sanitizer builds (clang not available)${NC}"
        fi
        ;;
    test)
        build_target "rt_demo" "$CONFIG"
        sudo cp ./bazel-bin/rt_demo ./rt_demo
        run_tests
        ;;
    test-basic)
        build_target "rt_demo" "$CONFIG"
        echo -e "${YELLOW}Running basic tests (no root required)...${NC}"
        build_target "rt_demo" "$CONFIG"
        sudo cp ./bazel-bin/rt_demo ./rt_demo
        chmod +x test_basic.sh
        ./test_basic.sh
        ;;
    test-full)
        build_target "rt_demo" "$CONFIG"
        echo -e "${YELLOW}Running full RT tests (requires sudo)...${NC}"
        build_target "rt_demo" "$CONFIG"
        sudo cp ./bazel-bin/rt_demo ./rt_demo
        chmod +x test_rt_demo.sh
        sudo ./test_rt_demo.sh
        ;;
    docs)
        generate_docs
        ;;
    analysis)
        static_analysis
        ;;
    *)
        echo -e "${RED}Unknown target: $TARGET${NC}"
        usage
        exit 1
        ;;
esac

echo -e "\n${GREEN}Build completed!${NC}"

# Show usage instructions
if [[ "$TARGET" == "rt_demo" || "$TARGET" == "all" ]]; then
    echo -e "\n${YELLOW}Usage instructions:${NC}"
    echo "To run the application:"
    echo "  sudo ./bazel-bin/rt_demo 30"
    echo ""
    echo "To install:"
    echo "  sudo cp ./bazel-bin/rt_demo /usr/local/bin/"
fi
