#include "combine.c"
#include "check.c"
#define UNRECOVERABLE (-1)

int download(char **path,int block_num){    //path需包含localblocks
    int *valid;
    char *filename=NULL;
    int blocknum;
    int64_t *blockLen;
    char **sequential_path;
    char *buff;
    /* ========================================================================================*/
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
    /*for(i=0;i<blocknum;i++){
        printf("%d,",source[i]);
    }*/
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
