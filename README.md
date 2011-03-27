## Simple ARP cache poisoner ##

programmed as exercise for my Computer Networking studies back in 2008.

## Dependencies ##

* [libnet][1] for packet injection
* [libdnet][2] for reading the local ARP cache
* [redo][3] for compilation
* [doxygen][4] to create the documentation

## Compile ##

    # redo

If redo is not installed, you can use
    # ./do
instead.

## Run ##

    sudo ./arppoison

[1]: http://libnet-dev.sourceforge.net/
[2]: http://libdnet.sourceforge.net/
[3]: http://github.com/apenwarr/redo
[4]: http://www.doxygen.org/
