#if you wanna make a file empty or create an empty file, you can 
cat /dev/null > file.txt  #or you can 
echo "" > file.txt


# 2>&1 means redirect stderr to stdout. usually you use the command like this:
cat linuxer.txt > /dev/null 2>&1 
#cammand  cat linuxer.txt > /dev/null redirect the stdout to /dev/null,but stderr still
#output to stderr,so we can use 2>&1 to let stderr redirect to stdout.
  

