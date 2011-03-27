DEPS="arppoison.o"
redo-ifchange $DEPS
LIBS="`libnet-config --libs` -ldnet"
gcc $LIBS -o $3 $DEPS
