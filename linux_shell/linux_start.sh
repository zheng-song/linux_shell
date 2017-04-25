#linux start with init process,and the create other process,init used to initialization linux system,and set stdin、stdout、stderr
#linux init process responsible for execute the shell script under /etc/rc.d/init.d,those Script are set according run level
#we can use "chkconfig" to set whether auto execute Script above.

cat /etc/profile | less
#	"/etc/profile" used to initialize system global shell variable 用户登入shell以后首先读取/etc/profile文件，该文件声明PATH、USER、
# LOGNAME、MAIL、HOSTNAME、HISTSIZE、INPUTRC等shell变量，系统全局变量初始化以后读取能够设置bell-style的全局读取线（read line）初始化文件
# /etc/inputrc以及/etc/profile.d目录，该目录保存设置特殊程序的全局环境文件。
cat /etc/inpturc
ls /etc/profil.d/

# 	"/etc/bashrc" 定义用于shell函数和别名的系统全局变量，在/etc/profile文件中可以查到shell环境变量和程序初始设置。/etc/bashrc文件包括shell函数
# 和别名的系统全局定义。
cat /etc/bashrc
cat ~/.bash_profile # 用于设置用户个人的环境设置文件，用于非全局的、属于用户个人的PATH和开始程序。
cat ~/.bashrc #用户个人的别名以及变量设置文件。可以定义用户的个性化指令别名，/etc/bashrc文件读取系统全局变量，然后设置特殊程序的变量
cat ~/.bash_logout # 该文件包括每一个用户的个人系统退出程序。






#==================================================================================================================
#2.2 命令行解析
# 向shell提示输入命令时，shell读取输入行并解析（parsing）命令行，然后分解为令牌（token）。