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


#pipe can receive stdin and then through all kinds of filters make the output become the resoult you want
#the filters include :
#1.sort :
#2.uniq :uniq always used to receive the sorted result and get rid of the repeated one.
#3.grep :receive data based as line,search speacial character pattern and output it.
#4.fmt : receive files from input and output as specialed format.
#5.pr : 		#6.head  		#7.tail			
#8.tr : can used to modify(upper/lower characters),repeated,delete the input then output, for example:it can transfor
#the file used in DOS to UNIX
#9.sed 			#10. awk





# lsattr: used to list the attribution of a file or directory, and the command "chattr" used to change attribution of a 
# file or directory 
lsattr /home/zjie/tmp/test.c     #checkout the attribution of test.c
chattr +i /home/zjie/tmp/test.c 	# add i(immutable) attribution to test.c,now even if root can not delete the file,
chattr +a /home/zjie/tmp/test.c 	# add a(append only) attribution to test.c,one can only write characters to the end of file 
#and cannot modify it.but only root user can use chattr command,before you can remove a file you should remove immuable/append attribution
 


