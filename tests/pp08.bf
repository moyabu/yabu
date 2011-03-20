# Präprozessor: .include mit Regenerierungsskript
# 

.filename=includetest-$(.TIMESTAMP)

.include $(.filename)
  echo 'all:' >$(._)
  echo '    echo "Xx:ok"' >>$(._)
  echo '    rm $(._)' >>$(._)

#STDOUT:Xx:ok
