## Simple ARP cache poisoner ##

programmed as exercise for my Computer Networking studies back in 2008.

## Dependencies ##

* [libnet][1] for packet injection
* [libdnet][2] for reading the local ARP cache
* [make][3] for compilation
* [doxygen][4] to create the documentation

## Install and run ##

    # make
    # sudo bin/arppoison

[1]: http://libnet-dev.sourceforge.net/
[2]: http://libdnet.sourceforge.net/
[3]: http://www.gnu.org/software/make/make.html
[4]: http://www.doxygen.org/
