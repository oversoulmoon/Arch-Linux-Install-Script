# Arch-Linux-Install-Script
This is inspired from Microsoft Deployment Toolit which allows for the quick deployment of multiple systems asynchronously. 

## How to use
This script is meant to be ran in the Arch Linux Installation media. On how to make one, please refer to the [Arch Linux Installation Guide](https://wiki.archlinux.org/title/Installation_guide).

Once booted into the installation media (it should be copied to ram and the media can be removed). Execute the following commands

```
$ curl -OL https://github.com/oversoulmoon/Arch-Linux-Install-Script/releases/download/v0.1.0-alpha/archInstall
$ ./ArchInstall
```

Read the promp until the Swap Size is choosen, once that done, the script can be left to run asynchronously.

Note: NetworkManager and openssh is install and enabled on the newly installed system. 
