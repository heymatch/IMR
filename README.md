# How to Build

## Compile Environment

* Linux (Ubuntu, WSL2)
* g++ 11
* Makefile

## Makefile command

* 直接下`make`指令就可以編譯

```bash
make
```

* 清除obj，並重新編譯

```bash
make all
```

# Program Command

## Basic Command

* 相關程式見：`src/main.cpp`

```bash
# ./IMR MODE TRACE_TYPE SETTING INPUT_TRACE_FILE OUTPUT_TRACE_FILE EVALUATION_DIR
./IMR partition msr setting/sample.txt test/msr-cambridge1-sample.csv test/msr-cambridge1-sample/partition/trace test/msr-cambridge1-sample/partition/
```

## Test Script

```bash
# bash sample.sh TRACE_TYPE(msr/systor17)
bash sample.sh msr
bash sample.sh systor17
```

# Project Structure

* `/setting`: 參數設定。相關程式見：`src/IMR_Base.cpp`的`IMR_Base::initialize()`
* `/src`: 程式碼
* `/test`: 測試用資料

# Output

輸出資料夾為`trace名稱/方法/資料`

format: `(trace_name)/(crosstrack-inplace/crosstrack-outplace/partition/sequential-inplace/sequential-outplace)/(dist/eval/trace)`

e.g. `msr-cambridge1-sample/partition/trace`

* `dist`: 分佈型資料。例如：partition的資料，update size的分佈。
* `eval`: 統計資料。例如：update count、track size。
* `trace`: trace file，可直接使用到DiskSim