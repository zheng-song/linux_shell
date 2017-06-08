# -I dir 用来在头文件的搜索路径当中添加dir目录
# -L dir 用来在库文件的搜索路径列表当中添加dir目录
# -E 只进行预编译过程
# -g 在可执行程序中包含gdb调试信息
# -S 只是编译不汇编,生成汇编代码.
# -c 只是编译不链接,生成目标代码
# -static 链接静态库
# -llibrary 链接名为library的库文件.

# 创建和维护自己的静态库很容易,只要使用ar程序和gcc命令对函数分别进行编译即可.应该尽可能的把函数分别保存在不同的源文件
#当中,但是如果需要访问公共数据,则把他们放在同一个源文件中并使用在该文件当中声明的静态变量.

# 可以使用如下的命令来查找所需要的函数在哪一个函数库当中,然后编译的时候包含该库文件
####  nm -o /lib/*.so | grep 函数名
#### 例如: nm -o /lib/*.so | grep sin

CC=gcc
CCFLAGS=-Wall -g 

serachTree:SerachTree.c
	$(CC) $(CCFLAGS) $^ -o $@

clean:
	-rm -f serachTree