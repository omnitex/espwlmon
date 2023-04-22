# `wl-sim`

PC side command line `ESP-IDF` project for obtaining statistics about simulated memory sector erases over its lifetime.
Wear leveling logic and algorithms are based on `WL_Advanced` implementation. For any details about algorithms, mapping or Feistel network address randomization **see primarily `WL_Advanced` and not this sim**!

## Usage

Albeit this simulator is intended for algorithm testing and internal use in the process of writing my bachelor's thesis, you can run it via the `run.sh` bash script (*tip*: try the `help` argument).

But if you really want an example, well, here is one for comparison between base wear leveling mapping and `WL_Advanced` using the so called Feistel network address randomization
```
./run.sh b c c 10 0
```
vs
```
./run.sh f c c 10 0
```
*PS: sometimes the sim gets stuck for whatever reason so just Ctrl+C and run again* 