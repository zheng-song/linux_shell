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



#====================================================================================================================
#                                          1.7 job control
#  "ps" 	"kill"		"jobs(show the job directory at current shell)"		"bg(transfer a job to background)"		
#  "fg(transfer a job to foreground)"
#	you can use "xload" as an example:
xload & #make the xload program run in background ,and if you want to make it run in foreground, you can use:
fg xload # you can use CTRL+"z" to make it stop,and the program trun into IDLE status.but how can you turn the IDLE status
# into ALIVE status ???????? .you can use "kill -9 [program ID]" to force kill it immediately.
# the "ps"  command is powerful than "jobs". if a job has been terminated,you can not kill it by "kill".but when you make it 
#alive ,such as "fg xload",the job will receive the kill signal immediately and be killed.
# "kill -l" command can list the signal type .you can use it such as "kill -SIGKILL/-9 [program ID]" to force kill it




#====================================================================================================================
#										1.8 system administrator command summary
#	"users" equals to "who -q" used to show the users login currently.
#	"groups" used to show groups information login in current shell 
#	"useradd"/"adduser(adduser is a symbol link to usersdd,wo use useradd usually)" used to add a user, "userdel" used to delete a user 
#normally we use "useradd [username]" first and then use "passwd [username]" to special a password to it.
sudo useradd zhengsong
sudo passwd zhengsong
sudo userdel zhengsong
sudo userdel -r zhengsong #we recommend you use this, because if you didn't add -r ,the /home/zhengsong directory will not be clean off
#????? when I use "useradd zhengsong" and "passwd zhengsong" create a new user and then "su - zhengsong" there comes out a promot
#????? saying "there have no directory,we will login in use HOME=/ ", but when I use "cat /dev/passwd | grep ^zhengsong" I can see 
#????? my home directory is /home/zhengsong , how it happens !!.
#

#	"usermod" used to modify all kind of attrbution of users, 
#	"groupmod" used to modify group name or group ID.
#	"who" used to show home directory of current login user.
# 	"w" used to show all login user's information,it's a extension of command "who".
#	"mesg" used to prevent other users to access your terminal."mesg y" admit others send message to my terminal,"mesg n"
#refuse others send meaasges to my terminal.
#	"wall" used to send message to all login user,those message alway show when login or logout,
wall reboot after 5 min, please logout
# 	"write [username]" used to send message to a particual user  
write zhengsong 
#"you will be force logout in 5 min,please save your important work now."
#use CTRL+c to terminate the process.


# 	"strace" used to trace system call and signal of a command
strace ls -la
#	"ltrace" used to trace Library Functions of a command.
ltrace ls  



# 	"free"  show memory and cache information based as byte.
free

#	"logrotate" used to manage system log file,it provide function of rotary,compress,delete,send eamil...usually we use
#"cron"regularly preform "logrotate" to manage log file.
cat /etc/rogrotate.conf  # 	
 