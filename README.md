lcovmerge - A utility to validate and merge lcov reports
===

## Usage

```bash  
# To merge reports with default options
# lcovmerge [-o <FILE>] <INPUT_FILE1> [INPUT_FILE2...]
lcovmerge -o coverage.info test1.info test2.info
lcovmerge test1.info test2.info > coverage.info
# To merge reports and discard line checksums
lcovmerge -d -o coverage.info test1.info test2.info
# To merge reports and generate checksums
lcovmerge -g -o coverage.info test1.info test2.info
# To merge reports and force regenerate all checksums (existing line checksums will be ignored)
lcovmerge -dg -o coverage.info test1.info test2.info
```

## Build lcovmerge

```bash
make config=release
```

