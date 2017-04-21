#if you wanna make a file empty or create an empty file, you can 
cat /dev/null > file1.txt  #or you can 
echo "" > file1.txt


# 2>&1 means redirect stderr to stdout. usually you use the command like this:
cat linuxer.txt > /dev/null 2>&1 
#cammand  cat linuxer.txt > /dev/null redirect the stdout to /dev/null,but stderr still
#output to stderr,so we can use 2>&1 to let stderr redirect to stdout.


#stdin means input data through keyboard,but sometimes you use file to input data.
#we can use < in this situation
sort < file1.txt
sort < file1.txt > sorted_file.txt


#pipe is used to connect stdout and stdin,make the stdout of previous command become stdin of next command
du -h --max-depth 1 | sort -nr 
du -h --max-depth 0 
find . -type f -print | wc -l
# --max-depth 0 means show the total size of the directory,--max-depth 1 means only unfold 1 direcyory
#"find" used to search, .means to search in current directory '-type f' means find all file types in file
#and print then count the number of line. the outcomes show how much file we have in current directory 




