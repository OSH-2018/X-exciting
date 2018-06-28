#include "split.c"

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


/*int main(){
    char *filename;
    filename=(char *)malloc(100*sizeof(char));
    strcpy(filename,"test.pdf");
    char *path;
    path=(char *)malloc(100*sizeof(char));
    strcpy(path,"/home/py/桌面/lrc-erasure-code-master/test/1/");
    char *localblocks_path;
    localblocks_path=(char *)malloc(100*sizeof(char));
    strcpy(localblocks_path,"/home/py/桌面/lrc-erasure-code-master/test/1/");
    upload(filename,path,localblocks_path);
}
*/


