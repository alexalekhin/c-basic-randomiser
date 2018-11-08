# Simple Daemon #

The simplest daemon which does nothing.

## How to make sure it works ##

```bash
make
./daemon
ps -axj | grep ./daemon
```


# Basic Random Daemon  #

## Description ##

The daemon which gathers bytes of information from presumably random sources.

Every 5 seconds the daemon checks size of the special file ``` ~/random/buffer ```. If size is less than 5 mb (or file doesn't exist), the daemon fills it with random data.

## Sources of randomness ##

* RAM info
* State of the CPU
* Time
* System idletime and upime
