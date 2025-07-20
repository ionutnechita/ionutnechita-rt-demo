# RT Demo - Real-Time Kernel Demonstration

A comprehensive application to demonstrate and benchmark PREEMPT_RT kernel capabilities, featuring automatic CPU isolation detection and real-time latency measurement.

## Features

- **Automatic RT CPU Detection**: Parses kernel cmdline to detect isolated CPUs, RCU no-callbacks, and NO_HZ configurations
- **Real-Time Latency Measurement**: High-precision latency measurement using CLOCK_MONOTONIC
- **Intelligent CPU Affinity**: Automatically pins RT threads to isolated CPUs and load threads to normal CPUs
- **Comprehensive Statistics**: Min/max/average/percentile latency analysis
- **Multiple Build Configurations**: Debug, release, profiling, and sanitizer builds

## System Requirements

- Linux kernel with PREEMPT_RT patch (recommended)
- GCC or Clang compiler
- pthread and rt libraries
- Root privileges for real-time scheduling

## Building with Bazel

### Prerequisites

Install Bazel on your system:
```bash
# On Ubuntu/Debian
sudo apt install bazel

# On openSUSE
sudo zypper install bazel

# Or download from https://bazel.build/install
```

### Quick Build

```bash
# Standard build
./build.sh

# Debug build
./build.sh --config debug

# Optimized build with RT configuration
./build.sh --config rt rt_demo

# Build all variants
./build.sh all
```

### Manual Bazel Commands

```bash
# Standard build
bazel build //:rt_demo

# Debug build
bazel build --config=debug //:rt_demo_debug

# Release build with optimizations
bazel build --config=release //:rt_demo_release

# Build with AddressSanitizer
bazel build --config=asan //:rt_demo_asan

# Build with ThreadSanitizer
bazel build --config=tsan //:rt_demo_tsan
```

## Available Targets

| Target | Description |
|--------|-------------|
| `rt_demo` | Standard optimized build |
| `rt_demo_debug` | Debug build with symbols |
| `rt_demo_release` | Highly optimized release build |
| `rt_demo_profile` | Build with gprof profiling support |
| `rt_demo_asan` | AddressSanitizer build for memory debugging |
| `rt_demo_tsan` | ThreadSanitizer build for race condition detection |

## Running the Application

```bash
# Run for 30 seconds (requires root for RT privileges)
sudo ./bazel-bin/rt_demo 30

# Run with default 10 seconds
sudo ./bazel-bin/rt_demo

# Stop early with Ctrl+C
sudo ./bazel-bin/rt_demo 60
```

## Testing

```bash
# Run test suite
./build.sh test

# Or manually
bazel test //:rt_demo_test
```

## Configuration Options

### Bazel Configurations

- `debug`: Debug symbols, no optimization
- `release`: Maximum optimization, stripped binaries
- `rt`: Real-time optimized build
- `asan`: AddressSanitizer for memory error detection
- `tsan`: ThreadSanitizer for race condition detection
- `coverage`: Code coverage analysis

### Build Script Options

```bash
./build.sh [OPTIONS] [TARGET]

OPTIONS:
  -c, --config CONFIG    Build configuration (debug, release, rt, asan, tsan)
  -v, --verbose         Verbose output
  --clean              Clean before build
  -h, --help           Show help

TARGETS:
  rt_demo              Standard build (default)
  all                  Build all variants
  test                 Run tests
  docs                 Generate documentation
  analysis             Run static analysis
```

## Example Output

```
=== PREEMPT_RT KERNEL DEMO ===
Test duration: 30 seconds

=== RT CPU CONFIGURATION ===
Kernel cmdline: BOOT_IMAGE=/boot/vmlinuz-6.16.0-rc6-lowlatency-sunlight1 ...
Isolated CPUs (isolcpus): 14,15 (total: 2)
RCU no-callbacks CPUs: 14,15 (total: 2)
NO_HZ full CPUs: 14,15 (total: 2)

Recommended for RT: CPU 14 (isolated + nocb + nohz)

=== SYSTEM INFORMATION ===
Kernel: Linux version 6.16.0-rc6-lowlatency-sunlight1 ... SMP PREEMPT_RT
RT Kernel: Probably RT (detected: lowlatency/rt in version)
CPU cores: 16

Starting 4 load threads...
Starting RT measurement thread...

RT thread using isolated CPU 14
RT thread pinned to CPU 14
RT thread started with priority 99 on CPU 14

=== LATENCY STATISTICS ===
Samples collected: 10000
Minimum latency: 1234 ns (1.23 μs)
Average latency: 3456 ns (3.46 μs)
Maximum latency: 12789 ns (12.79 μs)
95th percentile: 8765 ns (8.77 μs)
99th percentile: 11234 ns (11.23 μs)
Performance: EXCELLENT for real-time
```

## Performance Interpretation

- **< 100μs max latency**: EXCELLENT for real-time applications
- **< 1ms max latency**: GOOD for real-time applications
- **> 1ms max latency**: POOR for real-time applications

## Kernel Configuration

### Optimal RT Setup

For best results, configure your kernel cmdline with:

```bash
isolcpus=managed_irq,domain,14,15
rcu_nocbs=14,15
nohz_full=14,15
irqaffinity=0-13
skew_tick=1
rcu_nocb_poll
nohz=on
```

### CPU Isolation Benefits

- **isolcpus**: Prevents normal scheduler from using specified CPUs
- **rcu_nocbs**: Moves RCU callbacks away from isolated CPUs
- **nohz_full**: Eliminates periodic timer interrupts on isolated CPUs
- **irqaffinity**: Routes hardware interrupts to non-isolated CPUs

## Development Workflow

### 1. Build and Test

```bash
# Clean build with tests
./build.sh --clean test

# Development cycle
./build.sh --config debug rt_demo_debug
sudo ./bazel-bin/rt_demo_debug 10
```

### 2. Performance Analysis

```bash
# Build with profiling
./build.sh --config release rt_demo_profile
sudo ./bazel-bin/rt_demo_profile 30

# Analyze with gprof
gprof ./bazel-bin/rt_demo_profile gmon.out > profile_report.txt
```

### 3. Memory Debugging

```bash
# Address sanitizer
./build.sh --config asan rt_demo_asan
sudo ./bazel-bin/rt_demo_asan 10

# Thread sanitizer
./build.sh --config tsan rt_demo_tsan
sudo ./bazel-bin/rt_demo_tsan 10
```

### 4. Static Analysis

```bash
# Generate analysis report
./build.sh analysis
cat ./bazel-bin/analysis_report.txt
```

## Troubleshooting

### Common Issues

1. **Permission Denied**
   ```bash
   # Solution: Run with sudo
   sudo ./bazel-bin/rt_demo 30
   ```

2. **RT Scheduling Failed**
   ```bash
   # Check RT limits
   ulimit -r
   
   # Set in /etc/security/limits.conf
   * hard rtprio 99
   * soft rtprio 99
   ```

3. **High Latencies**
   - Verify PREEMPT_RT kernel
   - Check CPU isolation configuration
   - Disable unnecessary services
   - Verify IRQ affinity

4. **Build Failures**
   ```bash
   # Clean and rebuild
   bazel clean --expunge
   ./build.sh --clean rt_demo
   ```

### System Optimization

For optimal RT performance:

```bash
# Disable CPU frequency scaling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable CPU idle states on isolated CPUs
echo 1 | sudo tee /sys/devices/system/cpu/cpu14/cpuidle/state*/disable
echo 1 | sudo tee /sys/devices/system/cpu/cpu15/cpuidle/state*/disable

# Set RT throttling
echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us
```

## Advanced Usage

### Custom CPU Configuration

To test with specific CPUs:

```bash
# Edit rt_demo.c and modify cpu_config manually
# Or use taskset to force specific CPU
sudo taskset -c 14 ./bazel-bin/rt_demo 30
```

### Benchmarking Different Configurations

```bash
# Test on standard kernel
./bazel-bin/rt_demo 60 > standard_kernel.log

# Test on RT kernel
sudo ./bazel-bin/rt_demo 60 > rt_kernel.log

# Compare results
grep "Maximum latency" *.log
```

### Integration with Other Tools

```bash
# Use with cyclictest for comparison
sudo cyclictest -t1 -p99 -i1000 -l10000 -q &
sudo ./bazel-bin/rt_demo 30
```

## File Structure

```
rt_demo/
├── WORKSPACE              # Bazel workspace configuration
├── BUILD.bazel            # Build targets and rules
├── .bazelrc               # Bazel configuration options
├── build.sh               # Build automation script
├── test_rt_demo.sh        # Test suite
├── rt_demo.c              # Main application source
├── Makefile               # Traditional make support
└── README.md              # This file
```

## Contributing

### Code Style

- Follow Linux kernel coding style
- Use meaningful variable names
- Add comments for complex logic
- Ensure thread safety

### Testing

Before submitting changes:

```bash
# Run full test suite
./build.sh test

# Check with sanitizers
./build.sh --config asan rt_demo_asan
./build.sh --config tsan rt_demo_tsan

# Verify static analysis
./build.sh analysis
```

### Performance Validation

Ensure changes don't regress performance:

```bash
# Baseline measurement
sudo ./bazel-bin/rt_demo 60 > baseline.log

# After changes
sudo ./bazel-bin/rt_demo 60 > modified.log

# Compare results
diff baseline.log modified.log

sudo ./test_rt_demo.sh 
=== RT Demo Test Suite ===
✓ Running as root - RT privileges available

Test 1: Basic functionality
Running basic test for 5 seconds...
✓ Basic test passed

Test 2: CPU configuration detection
✓ CPU configuration detection working

Test 3: RT kernel detection
✓ System information detection working

Test 4: Latency measurement
✓ Latency measurement working

Test 5: Signal handling
=== PREEMPT_RT KERNEL DEMO ===
Test duration: 30 seconds
To stop early, press Ctrl+C

=== RT CPU CONFIGURATION ===
Kernel cmdline: BOOT_IMAGE=/boot/vmlinuz-6.16.0-rc6-lowlatency-sunlight1 root=UUID=d1eec41d-ce10-4373-8fe7-c6409bc7aa79 skew_tick=1 rcu_nocb_poll rcu_nocbs=14,15 nohz=on nohz_full=14,15 kthread_cpus=14,15 irqaffinity=0,1,2,3,4,5,6,7,8,9,10,11,12,13 isolcpus=managed_irq,domain,14,15 splash=silent nouveau.modeset=0 clocksource=tsc tsc=reliable resume=/dev/disk/by-uuid/e782df8e-d971-41ff-bf58-f482b5dbe683 quiet security=apparmor mitigations=auto

Isolated CPUs (isolcpus): 14,15 (total: 2)
RCU no-callbacks CPUs: 14,15 (total: 2)
NO_HZ full CPUs: 14,15 (total: 2)

Recommended for RT: CPU 14 (isolated + nocb + nohz)

=== SYSTEM INFORMATION ===
Kernel: Linux version 6.16.0-rc6-lowlatency-sunlight1 (ionutnechita@ionutnechita-arz2022) (gcc (SUSE Linux) 15.1.1 20250714, GNU ld (GNU Binutils; openSUSE Tumbleweed) 2.43.1.20241209-9) #1 SMP PREEMPT_RT Sun Jul 20 15:14:21 EEST 2025
RT Kernel: CONFIG_PREEMPT_RT=y detected
CPU cores: 16
RT priority limit: 0 (max: 0)
RT time limit: -1 μs (max: -1 μs)

Starting 4 load threads...
Load thread 0 started
Load thread 0 avoiding isolated CPUs
Load thread 1 started
Load thread 1 avoiding isolated CPUs
Load thread 2 started
Load thread 2 avoiding isolated CPUs
Starting RT measurement thread...

Load thread 3 started
Load thread 3 avoiding isolated CPUs
RT thread using isolated CPU 14
RT thread pinned to CPU 14
RT thread started with priority 99 on CPU 14
Samples collected: 1000, current latency: 20373 ns

Received signal 2, stopping...
Stopping threads...
RT thread finished
Load thread 2 finished
Load thread 3 finished

=== LATENCY STATISTICS ===
Samples collected: 1939
Minimum latency: 18539 ns (18.54 μs)
Average latency: 20185 ns (20.18 μs)
Maximum latency: 32717 ns (32.72 μs)
95th percentile: 20523 ns (20.52 μs)
99th percentile: 24040 ns (24.04 μs)
Performance: EXCELLENT for real-time

Test completed.
✓ Signal handling working

Test 6: Performance validation
✓ Performance test passed (max latency: 1208033)

=== Test Summary ===
Tests passed: 6/6
✓ All tests passed!


```

## References

- [PREEMPT_RT Wiki](https://wiki.linuxfoundation.org/realtime/start)
- [Real-Time Linux Documentation](https://www.kernel.org/doc/html/latest/scheduler/sched-rt-group.html)
- [Bazel Build System](https://bazel.build/)
- [CPU Isolation Guide](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/7/html/tuning_guide/isolating_cpus_using_tuned-profiles-realtime)

## License

This project is released under the MIT License. See LICENSE file for details.

## Authors

- Ionut Nechita - Initial implementation and RT optimization

---

**Note**: This application requires root privileges for real-time scheduling and may need specific kernel configurations for optimal performance.
