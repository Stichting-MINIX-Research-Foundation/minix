# VMware installation on Ubuntu


## prerequisite

```
    $ sudo apt update 

    $ sudo  apt-get upgrade

    $ sudo apt install gcc build-essential linux-headers-$(uname -r)
```

## download  VMware Workstation 15.5 Pro 

(https://www.vmware.com/in/products/workstation-pro/workstation-pro-evaluation.html)


## Run command in terminal commands in downloaded folder :
```
    $ sudo chmod +x VMware-Workstation-*.bundle

    $ sudo ./VMware-Workstation-*.bundle
```

## If you get error : unable to install all modules see log ...

First confirm your VMware version by command 
```     
    $ vmware -v 
```

Replace abcd with vmware version : wget https://github.com/mkubecek/vmware-host-modules/archive/workstation-ab.c.d.tar.gz E.g. for me (version 15.5.1)
```
    $ wget https://github.com/mkubecek/vmware-host-modules/archive/workstation-15.5.1.tar.gz
    
    $ tar  -xzf workstation-*.tar.gz                             (replace * with vmware version)  
    
    $ cd vmware-host-modules-workstation-*/
    
    $ Make
    
    $ sudo make install
```

## To uninstall VMware
```
    $ sudo vmware-installer -u vmware-workstation
```

## 

[![VMware installation](https://img.youtube.com/vi/Eq0rPj0dRGA/0.jpg)](https://www.youtube.com/watch?v=Eq0rPj0dRGA)

##

License Key : YF390-0HF8P-M81RQ-2DXQE-M2UT6 
