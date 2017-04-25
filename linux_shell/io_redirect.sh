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


#	"cron" used to set super/common user's scheduling work
ls -l /etc | grep cron*

#	"ip" used to show and manipulate route/device/route and tunnel strategy 
ip link show
man ip

#	"route" used to show/manipulate the IP routing table.
route  # equles to "ip route list"
route add defult gw 172.16.55.254


#	"tcpdump" used to dump traffic on a network,if there have no options,then show all packages captured.
tcpdump tcp port 21 # show the tcp packages communicate through 21 port 
tcpdump tcp


#	"sync" force synchronize cached writes to persisten storage
sync

#	"mkswap" used to create swap file or set up a swap area
dd if=/dev/zero of=swapfile bs=1024 count=8192  # create a file with context filled of zero and the size is 8M
#	"dd" used to convert and copy a file,if(read from file instead of stdin) of(write to FILE instead of stdout)
ls -l swapfile
mkswap swapfile 8192
sync
free # checkout the swap memory,you will find the swapfile didn't effect yet
swapon swapfile # "swapon"/"swapoff" used to enable/disable devices and files for paging and swapping
free # checkout the swap memory,you will find the swapfile effected.and the swap memory increased by 8192

# mkfs.ext3/mkfs.ext4/mkfs.vfat

fdisk -l
fdisk /dev/sda2

#	"fsck" used to check/repair/debugging file system,but before you check a file system,you should umount it first
#fdisk -l 
df -h  # "df" can used to report file system disk space usage
fsck /dev/sda8
badblocks /dev/sda8

#	"lsusb" used to list USB devices 	"lspci" used to list PCI device 
lsusb
lspci

#	"chroot" used alter home dir of root user. for more details see man chroot
chroot

# 	"tmpwatch" used to auto delete the file that never visited at specified period of time. usually used in "cron"
#to manage temporary file
cat /etc/cron.daily/tmpwatch

#	"dump" used to backup ext2/3/4 filesystem.dump can read lower disk zone,and create backup file in binary format
#and then you can use "restore" to recover the backup file.
man dump
dump -0 -u -f/home/zhengsong/backup /dev/sda8 #backup /dev/sda8 to /home/zhengsong/backup
restore ft /dev/sda8 #check file list of the file
restore rf /dev/sda8 /home/zhengsong/backup #recover the filesystem

#	"ulimit" used to set system resource upper limit "-f" used to limit file size,such as:
ulimit -f 1000 # set file size up to 1M.
ulimit -a #show all limit in current system.

umask # used to set maskrdev

lsmod
insmod
rmmod
modprobe
modinfo usbcore

# 	"ldd" used to show a executable file's shared library dependency
ldd /bin/ls

#	"watch" used to re-execute command at specified time intervals,and show the result full-screen.
watch -n 5 tail /var/log/messages
watch -n 60 from
watch -d ls -l
 




