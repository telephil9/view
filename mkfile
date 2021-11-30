</$objtype/mkfile

BIN=/$objtype/bin
LIB=posix/libposix.a$O
TARG=view
CFLAGS=-FTVw -p -Iposix
OFILES=view.$O io.$O sepmenuhit.$O utils.$O
HFILES=a.h

</sys/src/cmd/mkone

$LIB:V:
	cd posix
	mk

clean nuke:V:
	@{cd posix; mk $target}
	rm -f *.[$OS] [$OS].out $TARG
