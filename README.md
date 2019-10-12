# Enumctl
Linux process enumeration tool that uses systemd D-Bus API. Its inner 
workings are quite similar to systemd-cgls, but with the advantage of bypassing procfs hardening.

Enumctl can be used as a standalone executable or as a library.

 ### Advantages over classic enumeration techniques
 * Stealthier
 * More efficient and less code
 * "hidepid=2 /proc" bypass
 
 ### Disadvantages over classic enumeration techniques
 * Must be dynamically linked
 * Some processes might not show up
 * Systemd 221 or higher required (ej. Centos7 217)

## Prerequisites
```
libsystemd-dev
pkg-config
systemd V221 or higher
gcc 4.9 or higher
```

## Install
 ``` 
 git clone https://github.com/0xN3utr0n/Enumctl.git
 cd Enumctl
 make 
 ```
 
 ## Usage
 ```
 ./Enumctl <opt>            
	 -u Show all users' processes            
	 -d Show only active daemons
 ```
## Demo
https://asciinema.org/a/245060
## Tested on
```
Linux OpenSuse 4.12.14-lp150.12.48-default #1 SMP x86_64 GNU/Linux    
Linux Ubuntu 4.15.0-47-generic #50-Ubuntu SMP x86_64 GNU/Linux         
Linux debian 4.9.0-8-amd64 #1 SMP Debian x86_64 GNU/Linux  
