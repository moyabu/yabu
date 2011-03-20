# Fehler G31, wenn ein Ziel nicht erzeugt wird

all: nofile

nofile:
    rm -f nofile
    true

#SHOULD_FAIL:G31
