#define NO_BLOCKS -8
int check(char **path, int block_num,int *valid, int datablocknum_int,char **sequential_path,int64_t *blockLen){
    //此处path不需要按照数据排列
    int m,i,j,k,t;
    char md5valuetemp[17];
    int flag=1;
    char *md5;
    int executed=0;
    FILE *block;
    int serialnumber;
    char *num;
    k=0;
    j=0;
    for(m=0;m<block_num;m++){
        k=0;
        j=0;
        executed=0;
        for(i=strlen(path[m])-1;i>=0;i--){
            if(path[m][i] == '-')
                k++;
            if(k == 1 && executed ==0){
                for(j=1;j<=16;j++){
                    md5valuetemp[j-1]=path[m][i+j];
                }
                md5valuetemp[16]='\0';
                executed=1;
            }
            else if(k == 2 && executed==1){
                if(path[m][i+1] == '1'){
                    flag =1;            //datablocks
                }
                else if(path[m][i+1] == '0'){
                    flag=0;             //codeblocks
                }
                j=i;
                executed=0;
            }
            else if(k == 3 && executed == 0){
                num=(char *)malloc((j-i)*sizeof(char));
                for(t=i+1;t<j;t++){
                    num[t-i-1]=path[m][t];
                }
                num[j-i-1]='\0';
                serialnumber=atoi(num);
                md5=MD5_file(path[m],16);
                if(strcmp(md5,md5valuetemp) == 0){
                    if(flag == 1){
                        valid[serialnumber-1]=1;
                        sequential_path[serialnumber-1]=(char *)malloc((strlen(path[m])+1)*sizeof(char));
                        if(strcpy(sequential_path[serialnumber-1],path[m]) == NULL){
                            return COPY_ERROR;
                        }
                    }
                    else{
                        if(serialnumber>0 && serialnumber <5){
                            block=fopen(path[m],"rb");
                            if(block!= NULL){
                                fseek(block,0,SEEK_END);
                                *blockLen=ftell(block);
                                //printf("%d\n",blockLen);
                                fclose(block);
                            }
                        }
                        valid[serialnumber+datablocknum_int-1]=1;
                        sequential_path[serialnumber-1+datablocknum_int]=(char *)malloc((strlen(path[m])+1)*sizeof(char));
                        if(strcpy(sequential_path[serialnumber-1+datablocknum_int],path[m]) == NULL){
                            return COPY_ERROR;
                        }
                    }

                }
                executed=1;
                free(num);
                break;
            }
        }
    }

    return 0;



}