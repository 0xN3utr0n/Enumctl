# Enumctl
 Simple Linux process enumeration POC which uses D-bus's systemd interface. It's inner 
 workings are quite similar to systemd-cgls, but with the advantage of bypassing procfs hardening.
 The final goal is just to bring forward interest over D-bus's API and its potential for next-gen Linux
 malware.
 
 ### Advantages over classic enumeration techniques
 * Stealthier
 * Less and more efficient code
 * "hidepid=2 /proc" bypass
 
 ### Disadvantages over classic enumeration techniques
 * Must be dynamically linked
 * Some processes might not show up
 * Systemd 221 or higher required (ej. Centos7 217)
 
 ## Installing
 ``` make ```
 
 ## Usage
 ```
 ./Enumctl <opt>            
	 -u Show all users processes            
	 -d Show only active daemons
 ```
## Demo
## Tested on
```
Linux OpenSuse 4.12.14-lp150.12.48-default #1 SMP x86_64 GNU/Linux    
Linux Ubuntu 4.15.0-47-generic #50-Ubuntu SMP x86_64 GNU/Linux         
Linux debian 4.9.0-8-amd64 #1 SMP Debian 4.9.144-3.1 x86_64 GNU/Linux   
```
