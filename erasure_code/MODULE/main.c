
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "md5_file.c"
#include "split.c"
#include "combine.c"
#include "check.c"
#include "lrc.h"
#define UNRECOVERABLE (-1)


int upload(char *filename, char *path, char *localblocks_path){
    int blocknum;
    int64_t *blockLen;
    int64_t *last_blocklen;
    last_blocklen=(int64_t *)malloc(sizeof(int64_t));
    blockLen=(int64_t *)malloc(sizeof(int64_t));
    int global_codeblocks_num;
    char **blockname;
    char *buff;
    int i;
    blockname=(char **)malloc(64*sizeof(char*));
    for(i=0;i<64;i++){
        blockname[i]=(char *)malloc(256*sizeof(char));
    }
    int64_t blocksize;
    blocknum=split(filename,path,blockLen,blockname,last_blocklen);
    //printf("%s",blockname[0]);
    if(blocknum <=0){
        printf("split error\n");
        exit(-1);
    }
    uint8_t array[4];
    //printf("1.\n");
    global_codeblocks_num=blocknum/4+(blocknum%4>0?1:0);
    for(i=0;i<4;i++){
        array[i]=blocknum/4+(blocknum%4>0?1:0);
    }
    array[3]=blocknum-3*(blocknum/4+(blocknum%4>0?1:0));
    lrc_t     *lrc  = &(lrc_t) {0};
    lrc_buf_t *buf  = &(lrc_buf_t) {0};
    if (lrc_init_n(lrc, 4, array, 4+global_codeblocks_num) != 0) {
        exit(-1);
    }
    if (lrc_buf_init(buf, lrc, *blockLen) != 0) {
        exit(-1);
    }
    //printf("2.\n");
    FILE *tmp;
    for(i=0;i<blocknum;i++){
        //printf("%s\n",blockname[i]);
        tmp=fopen(blockname[i],"rb");
        if(tmp== NULL){
            printf("Block reading error!\n");
            exit(-1);
        }
        fseek(tmp,0,SEEK_END);
        blocksize=ftell(tmp);
        fseek(tmp,0,SEEK_SET);
        buff=(char *)malloc(blocksize*sizeof(char));
        //printf("kkk\n");
        if(fread(buff,blocksize,1,tmp) == 0){
            printf("Block reading error!\n");
            exit(-1);
        }
        memcpy(buf->data[i],buff,blocksize);
        free(buff);
        fclose(tmp);
    }
    //printf("3.\n");
    if (lrc_encode(lrc, buf) != 0) {
        exit(-1);
    }
    //printf("4.\n");
    char tName[50];     //块文件名
    char *tdir;     //块存放目录
    char *md5;
    char *temppath;
    FILE *temp;
    for (i = 0; i < 4+global_codeblocks_num; i++)
    {
        if(i<4){
            memset(tName,0,50);
            tdir=NULL;
            md5=NULL;
            temppath=NULL;
            temp=NULL;
            sprintf(tName, "-%d-%d-%d-%d-%d-", *last_blocklen,blocknum,4+global_codeblocks_num,i+1,0);     //生成文件名
            tdir=(char *)malloc((strlen(localblocks_path)+1+strlen(tName)+strlen(filename))*sizeof(char));
            temppath=(char *)malloc((strlen(localblocks_path)+10+strlen(filename))*sizeof(char));
            if(strcpy(temppath,localblocks_path) == NULL){
                //printf("Path copying error\n");
                exit(-1);
            }
            if(strcat(temppath,filename) == NULL){
                //printf("MD5 value copying error!\n");
                exit(-1);
            }
            if(strcat(temppath,"_temp.tmp") == NULL){
                //printf("MD5 value copying error!\n");
                exit(-1);
            }
            //printf("5.\n");
            temp=fopen(temppath,"wb");
            if(temp ==  NULL){
                perror("Block generating error");
                exit(-1);
            }
            if(fwrite(buf->code[i],*blockLen,1,temp) == 0){          //writing the content of orignal file to a block
                printf("File writing error\n");
                exit(-1);
            }
            fclose(temp);
            md5= MD5_file(temppath,16);
            if(strcat(tName,md5) == NULL){
                printf("MD5 value copying error!\n");
                exit(-1);
            }
            if(strcat(tName,".tmp") == NULL){
                printf("Blockname copying error!\n");
                exit(-1);
            }
            if(strcpy(tdir,localblocks_path) == NULL){
                printf("Path copying error\n");
                exit(-1);
            }
            if(strcpy(tdir,filename) == NULL){
                printf("Filename copying error\n");
                exit(-1);
            }
            if(strcat(tdir,tName) == NULL){
                printf("Blockname copying error\n");
                exit(-1);
            }
            if(rename(temppath,tdir) !=0){
                exit(-1);
            }
            free(tdir);
            free(temppath);
        }
        else{
            memset(tName,0,50);
            tdir=NULL;
            md5=NULL;
            temppath=NULL;
            temp=NULL;
            sprintf(tName, "-%d-%d-%d-%d-%d-", *last_blocklen,blocknum,4+global_codeblocks_num,i+1,0);     //生成文件名
            tdir=(char *)malloc((strlen(path)+1+strlen(tName))*sizeof(char));
            temppath=(char *)malloc((strlen(path)+9)*sizeof(char));
            if(strcpy(temppath,path) == NULL){
                //printf("Path copying error\n");
                exit(-1);
            }
            if(strcat(temppath,"temp.tmp") == NULL){
                //printf("MD5 value copying error!\n");
                exit(-1);
            }
            temp=fopen(temppath,"wb");
            if(temp ==  NULL){
                perror("Block generating error");
                exit(-1);
            }
            if(fwrite(buf->code[i],*blockLen,1,temp) == 0){          //writing the content of orignal file to a block
                printf("File writing error\n");
                exit(-1);
            }
            fclose(temp);
            md5= MD5_file(temppath,16);
            if(strcat(tName,md5) == NULL){
                printf("MD5 value copying error!\n");
                exit(-1);
            }
            if(strcat(tName,".tmp") == NULL){
                printf("Blockname copying error!\n");
                exit(-1);
            }
            if(strcpy(tdir,path) == NULL){
                printf("Path copying error\n");
                exit(-1);
            }
            if(strcpy(tdir,filename) == NULL){
                printf("Filename copying error\n");
                exit(-1);
            }
            if(strcat(tdir,tName) == NULL){
                printf("Blockname copying error\n");
                exit(-1);
            }
            if(rename(temppath,tdir) !=0){
                exit(-1);
            }
            free(tdir);
            free(temppath);
        }
    }
    //printf("5.\n");
    free(blockname);
    lrc_destroy(lrc);
    lrc_buf_destroy(buf);
    return 0;
}


int download(char **path,int block_num){    //path需包含localblocks
    int *valid;
    char *filename=NULL;
    int blocknum;
    int64_t *blockLen;
    char **sequential_path;
    char *buff;
    /* ======================================================================================*/
    int i,j,k,t;
    int datablocknum_int=0;
    char *datablocknum_char;
    int codeblocknum_int=0;
    char *codeblocknum_char;
    char *last_blocklen_char;
    int last_blocklen_int;
    int executed=0;
    if(path[0] == NULL){
        return NO_BLOCKS;
    }
    else{
        k=0;
        j=0;
        for(i=strlen(path[0])-1;i>=0;i--){
            if(path[0][i] == '-'){
                k++;
            }
            if(k == 3 && executed == 0){
                j=i;
                executed=1;
            }
            else if(k == 4 && executed == 1){
                codeblocknum_char=(char *)malloc((j-i)*sizeof(char));
                for(t=i+1;t<j;t++){
                    codeblocknum_char[t-i-1]=path[0][t];
                }
                codeblocknum_char[j-i-1]='\0';
                codeblocknum_int=atoi(codeblocknum_char);
                j=i;
                executed=0;
            }
            else if(k == 5 && executed == 0){
                datablocknum_char=(char *)malloc((j-i)*sizeof(char));
                for(t=i+1;t<j;t++){
                    datablocknum_char[t-i-1]=path[0][t];
                }
                datablocknum_char[j-i-1]='\0';
                datablocknum_int=atoi(datablocknum_char);
                filename=(char *)malloc((i+1)*sizeof(char));
                j=i;
                executed=1;

            }
            else if(k== 6 && executed==1){
                last_blocklen_char=(char *)malloc((j-i)*sizeof(char));
                for(t=i+1;t<j;t++){
                    last_blocklen_char[t-i-1]=path[0][t];
                }
                last_blocklen_char[j-i-1]='\0';
                last_blocklen_int=atoi(last_blocklen_char);
                for(t=0;t<i;t++){
                    filename[t]=path[0][t];
                }
                filename[i]='\0';
                break;
            }
        }
    }
    // printf("1.\n");
    valid=(int *)malloc((datablocknum_int+codeblocknum_int)*sizeof(int));
    for(i=0;i<datablocknum_int+codeblocknum_int;i++){
        valid[i]=0;
    }
    sequential_path=(char **)malloc((datablocknum_int+codeblocknum_int)*sizeof(char *));
    for(i=0;i<datablocknum_int+codeblocknum_int;i++){
        sequential_path[i]=(char*)malloc(100*sizeof(char));
    }
    blockLen=(int64_t *)malloc(sizeof(int64_t));
    //printf("%d",valid[0]);
    // printf("2.\n");
    check(path,block_num,valid,datablocknum_int,sequential_path,blockLen);
    blocknum=datablocknum_int+codeblocknum_int;
    //printf("%s\n",sequential_path[10]);
    //printf("%d",blocknum);
    // printf("%d",*blockLen);
    //printf("3.\n");
    /*=====================================================================================================*/
    int8_t erased[blocknum];
    int8_t source[blocknum];
    int64_t blocksize[blocknum];
    //printf("%d\n",valid[0]);
    for(i=0;i<blocknum;i++){
        if(valid[i] == 0){
            erased[i]=1;
        }
        else{
            erased[i]=0;
        }
        source[i]=0;
        //printf("%d,",valid[i]);
    }
    //printf("\n");
    lrc_t     *lrc  = &(lrc_t) {0};
    lrc_buf_t *buf  = &(lrc_buf_t) {0};
    uint8_t array[4];
    for(i=0;i<4;i++){
        array[i]=datablocknum_int/4+(datablocknum_int%4>0?1:0);
    }
    array[3]=datablocknum_int-3*(datablocknum_int/4+(datablocknum_int%4>0?1:0));
    if (lrc_init_n(lrc, 4, array, codeblocknum_int) != 0) {
        exit(-1);
    }
    if (lrc_buf_init(buf, lrc, *blockLen) != 0) {
        exit(-1);
    }
    //printf("4.\n");
    /*for(i=0;i<blocknum;i++){
        printf("%d,",erased[i]);
    }
    printf("\n");*/
    if(lrc_get_source(lrc,erased,source) != 0){
        return UNRECOVERABLE;
    }
    for(i=0;i<blocknum;i++){
        printf("%d,",source[i]);
    }
    //printf("\n");
    //printf("5.\n");
    FILE *tmp;
    for(i=0;i<blocknum;i++){
        //printf("lll\n");
        if(source[i] == 1){
            //printf("%d,",i);
            tmp=fopen(sequential_path[i],"rb");
            if(tmp == NULL){
                printf("Block reading error!\n");
                exit(-1);
            }
            //printf("lllk\n");
            fseek(tmp,0,SEEK_END);
            //printf("lalal\n");
            blocksize[i]=ftell(tmp);
            //printf("wkkk\n");
            fseek(tmp,0,SEEK_SET);
            //printf("%d",blocksize[i]);
            //printf("\n%d\n",*blockLen);
            //  printf("kkk\n");
            buff=(char *)malloc(blocksize[i]*sizeof(char));
            if(fread(buff,blocksize[i],1,tmp) == 0){
                printf("Block reading error!\n");
                exit(-1);
            }
            //printf("2333\n");
            if(i<datablocknum_int){
                // printf("34444\n");
                memcpy(buf->data[i],buff,blocksize[i]);
                //printf("56666\n");
            }
            else{
                memcpy(buf->code[i-datablocknum_int],buff,blocksize[i]);
            }
            fclose(tmp);
            free(buff);
        }
    }
//printf("\n");
//printf("6.\n");
    if(lrc_decode(lrc,buf,erased) !=0){
        exit(-1);
    }
    //printf("7.\n");
    //printf("%d,%d",*blockLen,last_blocklen_int);
    int block_len;
    block_len=*blockLen;
    combine_blocks(buf->data,datablocknum_int,filename,block_len,last_blocklen_int);
    //printf("8.\n");
    lrc_destroy(lrc);
    lrc_buf_destroy(buf);

    return 0;
}

int main() {
    char **path;
    path=(char**)malloc(34*sizeof(char*));
    int i;
    for(i=0;i<28;i++){
        path[i]=(char *)malloc(100*sizeof(char));
    }
    int error;
    strcpy(path[0],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-1-0-ff61ced9e11f26f5.tmp");
    strcpy(path[9],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-10-1-d4024501bc2658f0.tmp");
    strcpy(path[5],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-6-1-03ecf43d5e0ed0c5.tmp");
    strcpy(path[10],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-11-1-5a37e2866d592fcb.tmp");
    strcpy(path[25],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-1-1-7854cf77ad1d35b4.tmp");
    strcpy(path[11],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-12-1-950c79fd6c250b43.tmp");
    //strcpy(path[12],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-13-1-3bedf9d13d9a9e86.tmp");
    strcpy(path[12],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-6-0-93644fb786f95e8c.tmp");
    strcpy(path[13],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-14-1-3c44124b27e8ee11.tmp");
    strcpy(path[26],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-15-1-46bd2c50620d2a89.tmp");
    strcpy(path[15],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-16-1-79a835d1a3805a3f.tmp");
    strcpy(path[16],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-17-1-e2cc65ea62e298e9.tmp");
    strcpy(path[17],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-18-1-af29011af65fc406.tmp");
    strcpy(path[18],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-19-1-149447718a71bc6d.tmp");
    strcpy(path[19],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-20-1-de7435a05864c058.tmp");
    strcpy(path[14],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-2-0-3efe85b7b7e694a2.tmp");
    strcpy(path[20],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-21-1-9aaaa8b272aa7754.tmp");
    strcpy(path[1],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-2-1-8835c7293fb90857.tmp");
    strcpy(path[21],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-22-1-0a4c1b0bdf9b4824.tmp");
    //strcpy(path[22],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-23-1-fd4f1db658e61ca8.tmp");
    strcpy(path[23],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-24-1-a1f1553c0fc41d39.tmp");
    strcpy(path[27],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-3-0-c9c9dfd0b55d9c18.tmp");
    strcpy(path[2],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-3-1-48a9f4d1a0ac37c1.tmp");
    strcpy(path[22],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-4-0-011a6cfb347987e9.tmp");
    strcpy(path[3],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-4-1-e46232da20183820.tmp");
    strcpy(path[24],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-5-0-547bbf1fc0244f68.tmp");
    strcpy(path[6],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-7-1-37162839e8b32803.tmp");
    strcpy(path[7],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-8-1-e7f8ac5b379ff939.tmp");
    strcpy(path[8],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-9-1-820c69d0308e9d81.tmp");
    strcpy(path[4],"/home/py/桌面/lrc-erasure-code-master/test/test.pdf-39618-24-10-5-1-54ffef53740b8522.tmp");

    /*FILE *tmp;
    FILE *dest;
    char *buf;
    buf=(char *)malloc(4*1024*1024*sizeof(char));
    dest=fopen("/home/py/桌面/lrc-erasure-code-master/test/test.pdf","wb");
    for(i=0;i<24;i++){
        tmp=fopen(path[i],"rb");
        if(i!=23){
            fread(buf,4*1024*1024,1,tmp);
            fwrite(buf,4*1024*1024,1,dest);
            fclose(tmp);
        }
        else{
            fread(buf,39618,1,tmp);
            fwrite(buf,39618,1,dest);
            fclose(tmp);
        }


    }
    fclose(dest);*/
    error=download(path,28);
    printf("\n\n%d",error);
    /*FILE *wrong;
    FILE *right;
    int i=0;
    char *wbuf,*rbuf;
    wbuf=(char *)malloc(sizeof(char));
    rbuf=(char *)malloc(sizeof(char));
    wrong=fopen("/Users/py/Desktop/test.pdf","rb");
    right=fopen("/Users/py/Desktop/test_.pdf","rb");
    while(1){
        i++;
        fread(wbuf,1,1,wrong);
        fread(rbuf,1,1,right);
        if(*wbuf != *rbuf){
            printf("%d\n",i);

        }
        if(feof(wrong)){
            break;
        }
    }
*/
    return 0;


}

