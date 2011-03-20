# Variablenersetzung: Verschachtelte Transformationsregeln

PROGS=aaa bbb
MODES=debug release

all:
   echo Xx:$(PROGS:*=$(MODES@:@=*.@))

#STDOUT:Xx:aaa.debug aaa.release bbb.debug bbb.release
