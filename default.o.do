redo-ifchange src/$1.c
# CCFLAGS=-Wall -Werror -Os -fomit-frame-pointer -Wformat=2 -DNDEBUG
CCFLAGS="-Wall -Werror -O0 -g -Wformat=2 -DDEBUG"
INC=`libnet-config --defines`
gcc $CCFLAGS $INC -MD -MF $1.deps -c -o $3 src/$1.c
read DEPS < $1.deps
redo-ifchange ${DEPS#*:}
