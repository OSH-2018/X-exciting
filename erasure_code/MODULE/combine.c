int combine_blocks(char **data, int datablocknum, char *filename, int blocklen,int last_blocklen_int)                  //此处path必须按照数据块序号排列
{
    FILE *fdest = fopen(filename, "wb"); //合并生成的文件
    char *buf;
    int i;
    if ( fdest == NULL)
    {
        perror("File opening error");
        return FILE_WRITE_ERROR;
    }
    char tempName[60];
    //printf("\n\n%d,%d",blocklen,last_blocklen_int);
    for(i=0;i<datablocknum-1;i++){
        if(fwrite(data[i],blocklen,1,fdest) == 0){
            return FILE_WRITE_ERROR;
        }
    }
    fwrite(data[datablocknum-1],last_blocklen_int,1,fdest);
    fclose(fdest);
    return 0;
}