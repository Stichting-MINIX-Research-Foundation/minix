# Minix 3 installation on VMware


## 

[![VMware installation](https://img.youtube.com/vi/pxEM2s0NBHQ/0.jpg)](https://www.youtube.com/watch?v=pxEM2s0NBHQ)

## Set passward

```
    # passwd
```

## Setting the Timezone
```
    # echo export TZ=Asia/Kolkata > /etc/rc.timezone
```

## Getting OpenSSH
```
    # pkgin update

    # pkgin install openssh

    # mkdir /etc/rc.d/

    # cp /usr/pkg/etc/rc.d/sshd /etc/rc.d/

    # printf 'sshd=YES\n' >> /etc/rc.conf

    # /etc/rc.d/sshd start
```

## Installing Common Packages
```
    # pkgin update

    # pkgin_sets
```

## Managing User 

```

```
