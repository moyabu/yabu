# Verschachtelung von $(VAR) und %n
# Die Ersetzung von $(n) erfolgt nach %-Substitution

all: stop-server

start-% stop-% check-%:: $(0).c
    echo 'Xx:$(0):$(1)'
      
%.c::      

#STDOUT:Xx:stop-server:stop-server.c
