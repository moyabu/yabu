# Verwendung von $(0) in den Quellen

all:: aaa

aaa:: test-$(0)

test-aaa::
  echo "Xx: test-aaa"

#STDOUT:Xx: test-aaa
