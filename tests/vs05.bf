# Verschachtelung von $(VAR) und $(n)
# - Ein Variablenwert kann $(n) enthalten
# - $(0) kann bereits in der Quellenliste benutzt werden.

TARGET=$(0)
SRC1=$(1)

all:: $(TARGET)_src
    echo 'Xx:$(TARGET)+$(SRC1)'

all_src::

#STDOUT:Xx:all+all_src

