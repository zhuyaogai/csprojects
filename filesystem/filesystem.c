#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define InodeNum 1024      //i节点数目
#define BlkNum (80 * 1024) //磁盘块的数目
#define BlkSize 1024       //磁盘块大小为1K
#define BlkPerNode 1024    //每个文件包含的最大的磁盘块数目
#define DISK "disk.txt"
#define BUFF "buff.txt"                                  //读写文件时的缓冲文件
#define SuperBeg 0                                       //超级块的起始地址
#define InodeBeg sizeof(SuperBlk)                        //i节点区起始地址
#define BlockBeg (InodeBeg + InodeNum * sizeof(Inode))   //数据区起始地址
#define MaxDirNum (BlkPerNode * (BlkSize / sizeof(Dir))) //每个目录最大的文件数
#define DirPerBlk (BlkSize / sizeof(Dir))                //每个磁盘块包含的最大目录项
#define Directory 0
#define File 1
#define CommanNum (sizeof(command) / sizeof(char *)) //指令数目

/*

磁盘的结构：

    SuperBlk | Inodes | Blocks

    superBlk 是指明整个fs结构的，INode可以理解为文件和目录的元数据，Block是真正存储的数据

*/


typedef struct
{
    int inode_map[InodeNum];    //i节点位图     (指明文件或者目录的信息)     1024个INode
    int blk_map[BlkNum];        //磁盘块位图        (真正存放数据的地方)        80 * 1024块，每块是1K
    int inode_used;             //已被使用的i节点数目
    int blk_used;               //已被使用的磁盘块数目
} SuperBlk;

typedef struct
{
    int blk_identifier[BlkPerNode];     //占用的磁盘块编号
    int blk_num;                        //占用的磁盘块数目
    int file_size;                      //文件的大小
    int type;                           //文件的类型
} Inode;

typedef struct
{
    char name[30];   //目录名
    short inode_num; //目录对应的inode
} Dir;

Dir dir_table[MaxDirNum];   //将当前目录文件的内容都载入内存
int dir_num;                //相应编号的目录项数


int inode_num;              //当前目录的inode编号
Inode curr_inode;           //当前目录的inode结构
SuperBlk super_blk;         //文件系统的超级块
FILE *Disk;

/*指令集合*/
char *command[] = {"fmt", "exit", "mkdir", "rmdir", "cd", "ls", "mk", "rm", "vim", "pwd", "cat", "touch", "clear"};
char path[40] = "fantasy: root";

void create_fs();     //创建文件系统
int init_fs(void);   //初始化文件系统
int close_fs(void);  //关闭文件系统
int format_fs(void); //格式化文件系统

int open_dir(int);               //打开相应inode对应的目录
int close_dir(int);              //保存相应inode的目录
int show_dir(int);               //显示目录
int make_file(int, char *, int); //创建新的目录或文件
// int del_file(int, char *, int);  //删除子目录
int del_file(int, char *);
int enter_dir(int, char *);      //进入子目录


int del_dir(int inode, char * dirName);    // 删除目录
int del_dir_helper(int inode);       


int file_write(char *);     //写文件
int file_read(char *);      //读文件

int adjust_dir(char *);     //删除子目录后，调整原目录，使中间无空隙

int check_name(int, char *);    //检查重命名,返回-1表示名字不存在，否则返回相应inode
int type_check(char *);         //确定文件的类型

int free_inode(int);            //释放相应的inode
int apply_inode();              //申请inode,返还相应的inode号，返还-1则INODE用完
int init_dir_inode(int, int);   //初始化新建目录的inode
int init_file_inode(int);       //初始化新建文件的inode

void free_blk(int);      //释放相应的磁盘块
int get_blk(void);      //获取磁盘块

void change_path(char *);

void pwd();




int main(int argc, char const *argv[])
{
    char comm[30], name[30];
    char *arg[] = {"vim", BUFF, NULL};       // BUFF --- "buff.txt"  
    int i, quit = 0, choice, status;

    // 文件形式模拟 文件系统结构
    Disk = fopen(DISK, "r+");        // DISK ---- "disk.txt"    以读/写方式打开文件，该文件必须存在

    if (!Disk) {
        create_fs();
        Disk = fopen(DISK, "r+");
    }

    init_fs();

    while (1)
    {
        printf("%s# ", path);
        scanf("%s", comm);
        choice = -1;

        for (i = 0; i < CommanNum; ++i)
        {
            if (strcmp(comm, command[i]) == 0)
            {
                choice = i;
                break;
            }
        }

        switch (choice)
        {
        /*格式化文件系统*/
        case 0:
            format_fs();
            break;
        /*退出文件系统*/
        case 1:
            quit = 1;
            break;
        /*创建子目录*/
        case 2:
            scanf("%s", name);
            make_file(inode_num, name, Directory);
            break;
        /*删除子目录*/
        case 3:
            scanf("%s", name);
            if (type_check(name) != Directory)
            {
                printf("rmdir: failed to remove '%s': Not a directory\n", name);
                break;
            }
            // del_file(inode_num, name, 0);
            del_dir(inode_num, name);
            break;
        /*进入子目录*/
        case 4:
            scanf("%s", name);
            if (type_check(name) != Directory)
            {
                printf("cd: %s: Not a directory\n", name);
                break;
            }
            if (enter_dir(inode_num, name))
            {
                change_path(name); //改变路径前缀
            }
            break;
        /*显示目录内容*/
        case 5:
            show_dir(inode_num);
            break;
        /*创建文件*/
        case 6:
            scanf("%s", name);
            make_file(inode_num, name, File);
            break;
        /*删除文件*/
        case 7:
            scanf("%s", name);
            if (type_check(name) != File)
            {
                printf("rm: cannot remove '%s': Not a file\n", name);
                break;
            }
            // del_file(inode_num, name, 0);
            del_file(inode_num, name);
            break;
        /*对文件进行编辑*/
        case 8:
            scanf("%s", name);
            // 先判断存在
            if (type_check(name) != File)
            {
                printf("vim: cannot edit '%s': Not a file\n", name);
                break;
            }

            /* 
                这里采用的是先删除了原有的文件，再重新写
            */
            file_read(name); //将数据从文件写入BUFF
            if (!fork())
            {
                execvp("vim", arg);
            }
            wait(&status);
            file_write(name); //将数据从BUFF写入文件
            break;
        case 9:
            pwd();
            break;

        case 10:
            scanf("%s", name);
            if (type_check(name) != File) {
                printf("cat: can not cat '%s': Not a file\n", name);
                break;
            }

            // 先读出到一个文件在写回去
            file_read(name);
            if (!fork())
                execvp("cat", arg);
            
            wait(&status);
            file_write(name);
            break;
        case 11:
            scanf("%s", name);
            int type = type_check(name);
            if (type == 0) {
                printf("This is a existed directory!\n");
                break;
            }

            if (type == 1) {
                printf("This file exists, and can't be touched!\n");
                break;
            }

            int inode_num = apply_inode();
            if (inode_num == -1) {
                printf("Inode number is zero, so you can't create a file!\n");
                break;
            }

            init_file_inode(inode_num);
            Dir dir;
            dir.inode_num = inode_num;
            strcpy(dir.name, name);
            dir_table[dir_num++] = dir;

            break;

        case 12:
            if (!fork())
                execvp("clear", NULL);
            
            wait(&status);
            break;
            
        default:
            printf("%s command not found\n", comm);
        }

        if (quit)
            break;
    }
    close_fs();

    fclose(Disk);
    return 0;
}


// 创建fs
void create_fs() {
    char * buffer = (char *)malloc(BlockBeg + BlkNum * BlkSize);   // 文件系统总大小
    memset(buffer, 0, BlockBeg + BlkNum * BlkSize);
    SuperBlk * sp = (SuperBlk *)buffer;
    sp->inode_map[0] = 1;
    sp->blk_map[0] = 1;
    sp->inode_used = 1;
    sp->blk_used = 1;

    Inode * inode = (Inode *)(buffer+InodeBeg);
    inode->blk_identifier[0] = 0;   // 第0块block
    inode->blk_num = 1;
    inode->type = Directory;
    inode->file_size = 2 * sizeof(Dir);

    Dir self;
    self.inode_num = 0;
    strcpy(self.name, ".");

    Dir parent;
    parent.inode_num = 0;      // parent 目录也是指向自己
    strcpy(parent.name, "..");

    Dir * s = (Dir *)(buffer + BlockBeg);
    (*s) = self;

    Dir * p = (Dir *)(buffer + BlockBeg + sizeof(Dir));
    (*p) = parent;

    FILE * f = fopen(DISK, "w+");
    fwrite(buffer, 1, BlockBeg + BlkNum * BlkSize, f);     // 写回磁盘

    fclose(f);
}

int init_fs(void)
{
    fseek(Disk, SuperBeg, SEEK_SET);      // 定位到super块----0字节处
    fread(&super_blk, sizeof(SuperBlk), 1, Disk);    //读取超级块

    inode_num = 0; //当前根目录的inode为0

    if (!open_dir(inode_num))
    {
        printf("CANT'T OPEN ROOT DIRECTORY\n");
        return 0;
    }

    return 1;
}

int close_fs(void)
{
    fseek(Disk, SuperBeg, SEEK_SET);     
    fwrite(&super_blk, sizeof(SuperBlk), 1, Disk);       // 超级块写会磁盘

    close_dir(inode_num);
    return 1;
}

int format_fs(void)
{
    // 主要把super block处理好就可以了，因为这里是管理整个disk的使用数据


    /*格式化inode_map,保留根目录*/
    memset(super_blk.inode_map, 0, sizeof(super_blk.inode_map));
    super_blk.inode_map[0] = 1;
    super_blk.inode_used = 1;
    /*格式化blk_map,保留第一个磁盘块给根目录*/
    memset(super_blk.blk_map, 0, sizeof(super_blk.blk_map));
    super_blk.blk_map[0] = 1;
    super_blk.blk_used = 1;

    inode_num = 0; //将当前目录改为根目录

    /*读取根目录的i节点*/
    fseek(Disk, InodeBeg, SEEK_SET);
    fread(&curr_inode, sizeof(Inode), 1, Disk);
    //	printf("%d\n",curr_inode.file_size/sizeof(Dir));

    curr_inode.file_size = 2 * sizeof(Dir);
    curr_inode.blk_num = 1;
    curr_inode.blk_identifier[0] = 0; //第零块磁盘一定是根目录的

    /*仅.和..目录项有效*/
    dir_num = 2;

    strcpy(dir_table[0].name, ".");
    dir_table[0].inode_num = 0;
    strcpy(dir_table[1].name, "..");
    dir_table[1].inode_num = 0;

    strcpy(path, "fantasy: root");

    return 1;
}

int open_dir(int inode)
{
    int i;
    int pos = 0;
    int left;

    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);

    /*读出相应的i节点*/
    fread(&curr_inode, sizeof(Inode), 1, Disk);
    //	printf("%d\n",curr_inode.file_size);

    for (i = 0; i < curr_inode.blk_num - 1; ++i)
    {
        fseek(Disk, BlockBeg + BlkSize * curr_inode.blk_identifier[i], SEEK_SET);
        fread(dir_table + pos, sizeof(Dir), DirPerBlk, Disk);
        pos += DirPerBlk;
    }

    /*left为最后一个磁盘块内的目录项数*/
    left = curr_inode.file_size / sizeof(Dir) - DirPerBlk * (curr_inode.blk_num - 1);
    fseek(Disk, BlockBeg + BlkSize * curr_inode.blk_identifier[i], SEEK_SET);
    fread(dir_table + pos, sizeof(Dir), left, Disk);
    pos += left;

    dir_num = pos;

    return 1;
}

int close_dir(int inode)
{
    int i, pos = 0, left;

    /*数据写回磁盘块*/
    for (i = 0; i < curr_inode.blk_num - 1; ++i)
    {
        fseek(Disk, BlockBeg + BlkSize * curr_inode.blk_identifier[i], SEEK_SET);
        fwrite(dir_table + pos, sizeof(Dir), DirPerBlk, Disk);
        pos += DirPerBlk;
    }

    left = dir_num - pos;
    //	printf("left:%d",left);
    fseek(Disk, BlockBeg + BlkSize * curr_inode.blk_identifier[i], SEEK_SET);
    fwrite(dir_table + pos, sizeof(Dir), left, Disk);

    /*inode写回*/
    curr_inode.file_size = dir_num * sizeof(Dir);
    fseek(Disk, InodeBeg + inode * sizeof(Inode), SEEK_SET);
    fwrite(&curr_inode, sizeof(curr_inode), 1, Disk);

    return 1;
}

/*创建新的目录项或者文件*/
int make_file(int inode, char *name, int type)
{
    int new_node;
    int blk_need = 1;       // 本目录需要增加磁盘块则blk_need=2
    int t;

    if (dir_num > MaxDirNum)
    {   //超过了目录文件能包含的最大目录项
        printf("mkdir: cannot create directory '%s' :Directory full\n", name);
        return 0;
    }

    if (check_name(inode, name) != -1)
    {   //防止重命名
        printf("mkdir: cannnot create file '%s' :File exist\n", name);
        return 0;
    }

    if (dir_num / DirPerBlk != (dir_num + 1) / DirPerBlk)
    {   //本目录也要增加磁盘块
        blk_need = 2;
    }

    //	printf("blk_used:%d\n",super_blk.blk_used);
    if (super_blk.blk_used + blk_need > BlkNum)
    {
        printf("mkdir: cannot create file '%s' :Block used up\n", name);
        return 0;
    }

    if (blk_need == 2)
    {   //本目录需要增加磁盘块
        t = curr_inode.blk_num++;
        curr_inode.blk_identifier[t] = get_blk();
    }

    /*申请inode*/
    new_node = apply_inode();

    if (new_node == -1)
    {
        printf("mkdir: cannot create file '%s' :Inode used up\n", name);
        return 0;
    }

    if (type == Directory)
    {
        /*初始化新建目录的inode*/
        init_dir_inode(new_node, inode);
    }
    else if (type == File)
    {
        /*初始化新建文件的inode*/
        init_file_inode(new_node);
    }

    strcpy(dir_table[dir_num].name, name);
    dir_table[dir_num++].inode_num = new_node;

    return 1;
}

/*显示目录内容*/
int show_dir(int inode)
{
    int i, color = 32;
    for (i = 0; i < dir_num; ++i)
    {
        if (type_check(dir_table[i].name) == Directory)
        {
            /*目录显示绿色*/
            printf("d   \033[1;%dm%s\t\033[0m\n", color, dir_table[i].name);
        }
        else
        {
            printf("f   %s\n", dir_table[i].name);
        }

        // if (!((i + 1) % 4))
        //     printf("\n"); //4个一行
        // printf("%s %d  type: %d\n", dir_table[i].name, dir_table[i].inode_num, type_check(dir_table[i].name));
    }
    // printf("\n");

    return 1;
}

/*申请inode*/
int apply_inode()
{
    int i;

    if (super_blk.inode_used >= InodeNum)
    {
        return -1; //inode节点用完
    }

    super_blk.inode_used++;

    for (i = 0; i < InodeNum; ++i)
    {
        if (!super_blk.inode_map[i])
        { //找到一个空的i节点
            super_blk.inode_map[i] = 1;
            return i;
        }
    }

    return -1;
}

int free_inode(int inode)
{
    Inode temp;
    int i;
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fread(&temp, sizeof(Inode), 1, Disk);    // 这里指针已经向前移动了，因为读取数据就会向前移动 offset 

    for (i = 0; i < temp.blk_num; ++i)
    {
        free_blk(temp.blk_identifier[i]);
    }

    super_blk.inode_map[inode] = 0;
    super_blk.inode_used--;

    //把INode也清0
    memset(&temp, 0, sizeof(Inode));
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);   // 重置指针
    fwrite(&temp, sizeof(Inode), 1, Disk);

    return 1;
}

/*进入子目录*/
int enter_dir(int inode, char *name)
{
    int child;
    child = check_name(inode, name);

    if (child == -1)
    { //该子目录不存在
        printf("cd: %s: No such file or directory\n", name);
        return 0;
    }

    /*关闭当前目录,进入下一级目录*/
    close_dir(inode);
    inode_num = child;
    open_dir(child);

    return 1;
}

// /*递归删除文件夹*/
// int del_file(int inode, char *name, int deepth)
// {
//     int child, i, t;
//     Inode temp;

//     if (!strcmp(name, ".") || !strcmp(name, ".."))
//     {
//         /*不允许删除.和..*/
//         printf("rmdir: failed to remove '%s': Invalid argument\n", name);
//         return 0;
//     }

//     child = check_name(inode, name);
//     if (child == -1)
//     { //子目录不存在
//         printf("rmdir: failed to remove '%s': No such file or directory\n", name);
//         return 0;
//     }

//     /*读取当前子目录的Inode结构*/
//     fseek(Disk, InodeBeg + sizeof(Inode) * child, SEEK_SET);
//     fread(&temp, sizeof(Inode), 1, Disk);

//     if (temp.type == File)
//     {

//         /*如果是文件则释放相应Inode即可*/
//         free_inode(child);
//         /*若是最上层文件，需调整目录*/
//         if (deepth == 0) {
//             adjust_dir(name);
//         }

//         /*删除自身在目录中的内容*/
//         if (dir_num / DirPerBlk != (dir_num - 1) / DirPerBlk) {
//             /*有磁盘块可以释放*/
//             curr_inode.blk_num--;
//             t = curr_inode.blk_identifier[curr_inode.blk_num];
//             free_blk(t); //释放相应的磁盘块
//         }

//         return 1;
//     }
//     else
//     {
//         /*否则进入子目录*/
//         enter_dir(inode, name);
//     }

//     // printf("hello world!\n");

//     for (i = 2; i < dir_num; ++i)    // 0 1 两个忽略
//     {
//         del_file(child, dir_table[i].name, deepth + 1);
//     }

//     enter_dir(child, ".."); //返回上层目录
//     free_inode(child);

//     if (deepth == 0)
//     {
//         /*删除自身在目录中的内容*/
//         if (dir_num / DirPerBlk != (dir_num - 1) / DirPerBlk)
//         {
//             /*有磁盘块可以释放*/
//             curr_inode.blk_num--;
//             t = curr_inode.blk_identifier[curr_inode.blk_num];
//             free_blk(t); //释放相应的磁盘块
//         }
//         adjust_dir(name); //因为可能在非末尾处删除，因此要移动dir_table的内容
//     }                     /*非初始目录直接释放Inode*/

//     return 1;
// }


int del_file(int inode, char *name) {
    int child, i, t;

    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        /*不允许删除.和..*/
        printf("rmdir: failed to remove '%s': Invalid argument\n", name);
        return 0;
    }

    child = check_name(inode, name);
    if (child == -1) {
        printf("The file doesn't exist！\n");
        return 0;
    }

    /*如果是文件则释放相应Inode即可*/
    free_inode(child);
    adjust_dir(name);

        /*删除自身在目录中的内容*/
    if (dir_num / DirPerBlk != (dir_num - 1) / DirPerBlk) {
        /*有磁盘块可以释放*/
        curr_inode.blk_num--;
        t = curr_inode.blk_identifier[curr_inode.blk_num];
        free_blk(t); //释放相应的磁盘块
    }

    return 1;
}

int del_dir(int inode, char * dirName) {
    if (!strcmp(dirName, ".") || !strcmp(dirName, "..")) {
        /*不允许删除.和..*/
        printf("rmdir: failed to remove '%s': Invalid argument\n", dirName);
        return 0;
    }

    int child = check_name(inode, dirName);
    if (child == -1) {
        printf("rmdir: failed to remove '%s': No such directory\n", dirName);
        return 0;
    }

    del_dir_helper(child);

    int num = dir_num-1;
    if (num / DirPerBlk != (num-1) / DirPerBlk) {
        --curr_inode.blk_num;
        int t = curr_inode.blk_identifier[curr_inode.blk_num];
        free_blk(t);
    }

    adjust_dir(dirName);

    return 1;
}

int del_dir_helper(int inode) {

    int i;
    int pos = 0;
    int left;

    Inode cur_Inode;

    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);

    /*读出相应的i节点*/
    fread(&cur_Inode, sizeof(Inode), 1, Disk);

    int dirNum = cur_Inode.file_size / sizeof(Dir);
    Dir dirTable[dirNum];

    for (i = 0; i < cur_Inode.blk_num - 1; ++i) {
        fseek(Disk, BlockBeg + BlkSize * cur_Inode.blk_identifier[i], SEEK_SET);
        fread(dirTable + pos, sizeof(Dir), DirPerBlk, Disk);
        pos += DirPerBlk;
    }

    /*left为最后一个磁盘块内的目录项数*/
    left = cur_Inode.file_size / sizeof(Dir) - DirPerBlk * (cur_Inode.blk_num - 1);
    fseek(Disk, BlockBeg + BlkSize * cur_Inode.blk_identifier[i], SEEK_SET);
    fread(dirTable + pos, sizeof(Dir), left, Disk);

    i = 2;
    Inode childInode;
    for (;i < dirNum;++i) {
        int child = dirTable[i].inode_num;
        fseek(Disk, InodeBeg + sizeof(Inode) * child, SEEK_SET);
        fread(&childInode, sizeof(Inode), 1, Disk);

        if (childInode.type == File) {
            free_inode(child);
        }
        else 
            del_dir_helper(child);
    }   

    free_inode(inode);

    return 1;
}

int adjust_dir(char *name)
{
    int pos;
    for (pos = 0; pos < dir_num; ++pos)
    {
        /*先找到被删除的目录的位置*/
        if (strcmp(dir_table[pos].name, name) == 0)
            break;
    }

    for (pos++; pos < dir_num; ++pos)
    {
        
        /*pos之后的元素都往前移动一位*/
        dir_table[pos - 1] = dir_table[pos];
    }

    dir_num--;
    return 1;
}

/*初始化新建目录的inode*/
int init_dir_inode(int child, int father)
{
    Inode temp;
    Dir dot[2];
    int blk_pos;

    fseek(Disk, InodeBeg + sizeof(Inode) * child, SEEK_SET);
    fread(&temp, sizeof(Inode), 1, Disk);

    blk_pos = get_blk(); //获取新磁盘块的编号

    temp.blk_num = 1;
    temp.blk_identifier[0] = blk_pos;
    temp.type = Directory;
    temp.file_size = 2 * sizeof(Dir);

    /*将初始化完毕的Inode结构写回*/
    fseek(Disk, InodeBeg + sizeof(Inode) * child, SEEK_SET);
    fwrite(&temp, sizeof(Inode), 1, Disk);

    strcpy(dot[0].name, "."); //指向目录本身
    dot[0].inode_num = child;

    strcpy(dot[1].name, "..");
    dot[1].inode_num = father;

    /*将新目录的数据写进数据块*/
    fseek(Disk, BlockBeg + BlkSize * blk_pos, SEEK_SET);
    fwrite(dot, sizeof(Dir), 2, Disk);

    return 1;
}

/*初始化新建文件的indoe*/
int init_file_inode(int inode)
{
    Inode temp;
    /*读取相应的Inode*/
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fread(&temp, sizeof(Inode), 1, Disk);

    temp.blk_num = 0;
    temp.type = File;
    temp.file_size = 0;
    /*将已经初始化的Inode写回*/
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fwrite(&temp, sizeof(Inode), 1, Disk);

    return 1;
}

/*申请未被使用的磁盘块*/
int get_blk()
{
    int i;
    
    for (i = 0; i < BlkNum; ++i)
    {   //找到未被使用的块
        if (!super_blk.blk_map[i])
        {
            super_blk.blk_used++;
            super_blk.blk_map[i] = 1;
            return i;
        }
    }

    return -1; //没有多余的磁盘块
}

/*释放磁盘块*/
void free_blk(int blk_pos)
{
    super_blk.blk_used--;
    super_blk.blk_map[blk_pos] = 0;
}

/*检查重命名*/
int check_name(int inode, char *name)
{
    int i;
    for (i = 0; i < dir_num; ++i)
    {
        /*存在重命名*/
        if (strcmp(name, dir_table[i].name) == 0)
        {
            return dir_table[i].inode_num;
        }
    }

    return -1;
}

void change_path(char *name)
{
    int pos;
    if (strcmp(name, ".") == 0)
    { //进入本目录则路径不变
        return;
    }
    else if (strcmp(name, "..") == 0)
    { //进入上层目录，将最后一个'/'后的内容去掉
        pos = strlen(path) - 1;
        for (; pos >= 0; --pos)
        {
            if (path[pos] == '/')
            {
                path[pos] = '\0';
                break;
            }
        }
    }
    else
    { //否则在路径末尾添加子目录
        strcat(path, "/");
        strcat(path, name);
    }

    return;
}

int type_check(char *name)
{
    int i, inode;
    Inode temp;
    for (i = 0; i < dir_num; ++i)
    {
        if (strcmp(name, dir_table[i].name) == 0)
        {
            inode = dir_table[i].inode_num;
            fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
            fread(&temp, sizeof(Inode), 1, Disk);
            return temp.type;
        }
    }
    return -1; //该文件或目录不存在
}

/*读文件函数*/
int file_read(char *name)
{
    int inode, i, blk_num;
    Inode temp;
    FILE *fp = fopen(BUFF, "w+"); // 打开可读/写文件，若文件存在则文件长度清为零，即该文件内容会消失；若文件不存在则创建该文件。
    char buff[BlkSize];
    //printf("read\n");

    inode = check_name(inode_num, name);

    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fread(&temp, sizeof(temp), 1, Disk);

    if (temp.blk_num == 0)
    { //如果源文件没有内容,则直接退出
        fclose(fp);
        return 1;
    }
    // printf("read\n");
    for (i = 0; i < temp.blk_num - 1; ++i)
    {
        blk_num = temp.blk_identifier[i];
        /*读出文件包含的磁盘块*/
        fseek(Disk, BlockBeg + BlkSize * blk_num, SEEK_SET);
        fread(buff, sizeof(char), BlkSize, Disk);
        /*写入BUFF*/
        fwrite(buff, sizeof(char), BlkSize, fp);
        free_blk(blk_num); //直接将该磁盘块释放
        temp.file_size -= BlkSize;
    }

    /*最后一块磁盘块可能未满*/
    blk_num = temp.blk_identifier[i];
    fseek(Disk, BlockBeg + BlkSize * blk_num, SEEK_SET);
    fread(buff, sizeof(char), temp.file_size, Disk);
    fwrite(buff, sizeof(char), temp.file_size, fp);
    free_blk(blk_num);
    /*修改inode信息*/
    temp.file_size = 0;
    temp.blk_num = 0;

    /*将修改后的Inode写回*/
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fwrite(&temp, sizeof(Inode), 1, Disk);

    fclose(fp);
    return 1;
}

/*写文件函数*/
int file_write(char *name)
{
    int inode, i;
    int num, blk_num;
    FILE *fp = fopen(BUFF, "r");
    Inode temp;
    char buff[BlkSize];

    inode = check_name(inode_num, name);

    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fread(&temp, sizeof(Inode), 1, Disk);

    while ((num = fread(buff, sizeof(char), BlkSize, fp)))
    {
        // printf("num:%d\n", num);
        if ((blk_num = get_blk()) == -1)
        {
            printf("error:	block has been used up\n");
            break;
        }
        /*改变Inode结构的相应状态*/
        temp.blk_identifier[temp.blk_num++] = blk_num;
        temp.file_size += num;

        /*将数据写回磁盘块*/
        fseek(Disk, BlockBeg + BlkSize * blk_num, SEEK_SET);
        fwrite(buff, sizeof(char), num, Disk);
    }

    /*将修改后的Inode写回*/
    fseek(Disk, InodeBeg + sizeof(Inode) * inode, SEEK_SET);
    fwrite(&temp, sizeof(Inode), 1, Disk);

    fclose(fp);
    return 1;
}


void pwd() {
    int i;
    for (i = 0;i < strlen(path);++i) {
        if (path[i] == '/')
            break;
    }

    if (i == strlen(path))
        printf("/\n");
    else 
        printf("%s\n", &path[i]);

}