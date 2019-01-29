### 简单的文件系统模拟实现
##### 实现的类ext4文件系统
实现的命令有：
- fmt (格式化文件系统)
- exit (退出文件系统)
- mkdir (新建文件夹)
- rmdir (去除文件夹---递归删除)
- cd (打开文件夹)
- ls (列举文件/文件夹)
- mk (新建文件)
- rm (删除文件)
- vim (编辑文件)
- pwd (显示当前路径)
- cat (显示文件内容)
- touch (新建文件)
- clear (清屏)


磁盘的结构：

    SuperBlk | Inodes | Blocks

    superBlk 是指明整个fs结构的，INode可以理解为文件和目录的元数据，Block是真正存储的数据
