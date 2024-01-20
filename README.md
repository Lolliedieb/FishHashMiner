# FishHash OpenCL Miner


# Usage
## One Liner Examples
```
Linux: ./fishHashMiner --server <hostName>:<portNumer> --user <walletAddr or userName> --devices <deviceList>
Windows: ./fishHashMiner.exe --server <hostName>:<portNumer> --user <walletAddr or userName>  --devices <deviceList>
```

## Parameters
### --server 
Passes the address and port of the pool the miner will mine on to the miner.
The pool address can be an IP or any other valid server address.

### --user
This parameter is used to pass the wallet to mine on or the pool user name to the miner.

### --pass (Optional)
If the pool requires a password to the username, this can be used to pass it to the miner.

### --devices (Optional)
Selects the devices to mine on. If not specified the miner will run on all devices found on the system. 
For example to mine on GPUs 0,1 and 3 but not number 2 use --devices 0,1,3
To list all devices that are present in the system and get their order start the miner with --devices -2 .
Then all devices will be listed, but none selected for mining. The miner closes when no devices were 
selected for mining or all selected miner fail in the compatibility check.

# How to build
## Linux 
1. Install the `gcc`, `cmake` and `boost` packages for your system.
2. Install the c++ headers for `OpenCL` according to the instruction on their github page 'https://github.com/KhronosGroup/OpenCL-CLHPP'
3. Build the miner with `cmake . && make`. You might require to add `-D CMAKE_PREFIX_PATH` to point cmake to the installation directiories of the OpenCL headers as described in the link in 2.
