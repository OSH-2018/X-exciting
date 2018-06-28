#define FILENAME_OVERFLOW -1
#define PATH_OVERFLOW  -2
#define COPY_ERROR  -3
#define OPENFILE_ERROR -4
#define BLOCK_GENERATE_ERROR -5
#define FILE_READ_ERROR -6
#define FILE_WRITE_ERROR -7
#define FILE_RENAME_ERROR -9
int split(char *filename,char *path,int64_t *blockLen, char **blockname,int64_t *last_blocklen)
{
    //Attention:Character "/" must be the end of path!
    unsigned int filename_length,path_length;
    filename_length=strlen(filename)+1;
    path_length=strlen(path)+1;
    if(filename_length >= 256){
        //printf("Filename is too long\n");
        return FILENAME_OVERFLOW;
    }
    else if(path_length >= 4096){
        //printf("Path is too long!\n");
        return PATH_OVERFLOW;
    }
    char *finalpath;
    finalpath=(char *)malloc((filename_length+path_length-1)*sizeof(char));
    if(strcpy(finalpath,path) == NULL){
        //printf("Path copy error\n");
        return COPY_ERROR;
    }
    if(strcat(finalpath,filename) == NULL){
        //printf("Filename copy error\n");
        return COPY_ERROR;
    }
    FILE *fsrc = fopen(finalpath, "rb");  // 源文件
    char *buf;
    int64_t blocklen;
    if (fsrc == NULL)
    {
        perror("打开错误：");
        return OPENFILE_ERROR;
    }
    fseek(fsrc, 0, SEEK_END);
    int64_t fLen = ftell(fsrc);  // 文件长度
    //printf("%lld\n",fLen);
    int blocknum;  //块数量
    if(fLen <= 10 ){
        blocklen=1;
        blocknum=fLen;
    }
    else if(fLen<= 131072 ){     //less than 128K
        blocklen=2048;      //2K
        blocknum=fLen/2048+(fLen%2048>0?1:0);
    }
    else if(fLen>131072 && fLen <= 10486576)  //larger than 128K but less than 1M
    {
        blocklen=32768;    //32K
        blocknum=fLen/32768+(fLen%2612144 > 0? 1:0);
    }
    else if(fLen > 1048576 && fLen <= 16777216) //larger than 1M but less than 16M
    {
        blocklen=262144;   //256K
        blocknum=fLen/262144+(fLen%2612144 > 0? 1:0);
    }
    else if(fLen > 16777216 && fLen <= 268435456) //larger than 16M but less than 256M
    {
        blocklen=4194304;      //4M
        blocknum=fLen/4194304+(fLen%4194304 > 0? 1:0);
    }
    else{       //larger than 256M
        blocklen=67108864;        //64M
        blocknum=fLen/67108864+(fLen%67108864 > 0? 1:0);
    }
    //printf("%lld\n",blocklen);
    //printf("%d\n",blocknum);
    *blockLen=blocklen;
    *last_blocklen=fLen%blocklen;
    //blockname=(char **)malloc(blocknum*sizeof(char *));
        for (int i = 0; i < blocknum; i++)  // 按块分割
        {
            char tName[100];     //块文件名
            char *tdir;     //块存放目录
            sprintf(tName, "-%d-%d-%d-%d-%d-", *last_blocklen,blocknum, 4 + blocknum / 4 + (blocknum % 4 > 0 ? 1 : 0), i + 1,1);     //生成文件名
            tdir = (char *) malloc((path_length + 100 + strlen(filename)) * sizeof(char));
            long offset = i * blocklen; //计算偏移量
            fseek(fsrc, offset, SEEK_SET);
            if (i == blocknum - 1) {
            blocklen = fLen - blocklen * (blocknum - 1);  //最后一块的长度
        }
            buf = (char *) malloc(blocklen * sizeof(char));
            if (fread(buf, blocklen, 1, fsrc) == 0) {
                //printf("File reading error!\n");
                return FILE_READ_ERROR;
            }
            char *temppath;
            temppath = (char *) malloc((path_length + 10 + strlen(filename)) * sizeof(char));
            if (strcpy(temppath, path) == NULL) {
                return COPY_ERROR;
            }
            if (strcat(temppath, filename) == NULL) {
                return COPY_ERROR;
            }
            if (strcat(temppath, "_temp.tmp") == NULL) {
                return COPY_ERROR;
            }
            FILE *temp;
            temp = fopen(temppath, "wb");
            if (temp == NULL) {
                perror("Block generating error");
                return BLOCK_GENERATE_ERROR;
            }
            if (fwrite(buf, blocklen, 1, temp) == 0) {          //writing the content of orignal file to a block
                //printf("File writing error\n");
                return FILE_WRITE_ERROR;
            }
            char *md5;
            fclose(temp);
            md5 = MD5_file(temppath, 16);
            if (strcat(tName, md5) == NULL) {
                return COPY_ERROR;
            }
            if (strcat(tName, ".tmp") == NULL) {
                return COPY_ERROR;
            }
            if (strcpy(tdir, path) == NULL) {
                return COPY_ERROR;
            }
            if (strcat(tdir, filename) == NULL) {
                return COPY_ERROR;
            }
            if (strcat(tdir, tName) == NULL) {
                return COPY_ERROR;
            }
            if (rename(temppath, tdir) != 0) {
                perror("error:");
                return FILE_RENAME_ERROR;
            }
            //blockname[i] = (char *) malloc((strlen(tdir) + 1) * sizeof(char));
            if (strcpy(blockname[i], tdir) == NULL) {
                return COPY_ERROR;
            }
            free(tdir);
            free(buf);
            free(temppath);
        }
    fclose(fsrc);
    return blocknum;
}