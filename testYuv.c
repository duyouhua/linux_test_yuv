#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include "utils.h"

#define WIDTH		320
#define	HIGHT		240
#define WIDTH_BIG       640
#define HIGHT_BIG       480
#define FILE_VIDEO  "/dev/video0"
#define JPG "./out/image_%s"
typedef struct{
    void *start;
	int length;
}BUFTYPE;
BUFTYPE *usr_buf;

static unsigned int n_buffer = 0;  
struct timeval time;
unsigned char* mjpeg_buff;
unsigned char* yuyv_buff;
unsigned char* yuv420_buff;
unsigned char* yuv420_buff_for_yv12;
unsigned char* yuv420_buff_for_nv21;
unsigned char* yuv420_buff_big;
unsigned char* nv21_test_buff;
unsigned char* nv21_test_buff_big;

int dump_yuv(void *addr, int length,char* name)
{
	FILE *fp;
	static int num = 0;
	//char image_name[20];
	
	//sprintf(image_name, JPG, name);
	printf("lilei getPropertyMultiScan() ##dump_yuv 111 \n");
	if((fp = fopen(name, "w")) == NULL)
	{
		perror("dump_yuv getPropertyMultiScan() Fail to fopen file");
		exit(EXIT_FAILURE);
	}
	//fwrite(addr, WIDTH*HIGHT*2 , 1, fp);
        fwrite(addr, length , 1, fp);
	usleep(500);
	fclose(fp);
	return 0;
}
//两个yuv图片合成
int merge_two_yuv_for_nv21(unsigned char *big_buff,int width_big,int height_big,unsigned char * small_buff,int width_small,int height_small)
{
    printf("merge_two_yuv_for_nv21 big_buff:%p,width_big:%d height_big:%d small_buff:%p width_small:%d height_small:%d \n",big_buff,width_big,height_big,small_buff,width_small,height_small);
    if(width_big<width_small || height_big < height_small)
    {
        printf("merge_two_yuv_for_nv21 error width_big<width_small || height_big < height_small \n");
        return -1;
    }
    //for test
    //nv21_test_buff = (unsigned char*)malloc(width_small * height_small * 2);
    //YUV420spRotateNegative90(nv21_test_buff,small_buff,width_small,height_small);
    //small_buff = nv21_test_buff;
//    dump_yuv(small_buff,width_small*height_small*3/2,"out/image_yv12_small_rotate_90.yuv");
//    return;

    int small_vu_distance = width_small*height_small;
    int large_vu_distance = width_big*height_big;

    int y_center_offset = ((height_big - height_small)/2)*width_big + (width_big-width_small)/2;
    int vu_center_offset = ((height_big - height_small)/4)*width_big + (width_big-width_small)/2;
    unsigned char * big_buff_y =big_buff + y_center_offset;
    unsigned char * big_buff_vu = big_buff+large_vu_distance+vu_center_offset;
    unsigned char * small_buff_y = small_buff; 
    unsigned char * small_buff_vu = small_buff + small_vu_distance;
    printf("##merge_two_yuv_for_nv21  large_vu_distance:%d,small_vu_distance:%d \n",large_vu_distance,small_vu_distance);
    for(int i=0;i<height_small;i++)
    {  
        memcpy(big_buff_y,small_buff_y,width_small);
        if(i%2 == 0){
		memcpy(big_buff_vu,small_buff_vu,width_small);
		big_buff_vu += width_big; 
        }
        big_buff_y += width_big;
        small_buff_y += width_small;
        small_buff_vu += width_small/2;  //或者在每两行y偏移一个width_small
     }
    //dump_yuv(nv21_test_buff,width_small*height_small*3/2,"nv21_small");
    //dump_yuv(nv21_test_buff_big,width_big*height_big*3/2,"nv21_big");
    dump_yuv(big_buff,width_big*height_big*3/2,"out/image_nv21_merge.yuv");
}
int merage_jpeg2yuv_for_nv21(char *jpegPath,int width_small,int height_small,unsigned char * camera_buff,int width_big,int height_big)
{
        int k = width_small;
        int j = height_small;
        //buff1为jpeg分配内存
        mjpeg_buff = (unsigned char*)malloc(width_small * height_small * 2);
        if(mjpeg_buff ==  NULL) {
                perror("merage_jpeg2yuv_for_nv21() step1 mjpeg_buff malloc err\n");
        }else{
                memset(mjpeg_buff, 0, width_small * height_small * 2);
        }
        //buff2为jpeg转码后的yuv422数据分配内存
        yuyv_buff = (unsigned char*)malloc(width_small * height_small * 2);
	if(yuyv_buff ==  NULL){
		perror("merage_jpeg2yuv_for_nv21() step2 yuyv_buff malloc err\n");
	}else{
		memset(yuyv_buff, 0, width_small * height_small * 2);
	}
        //为yuv422转换为nv21数据分配内存
        yuv420_buff = (unsigned char*)malloc(width_small * height_small * 1.5);
	if(yuv420_buff ==  NULL){
		perror("merage_jpeg2yuv_for_nv21() step3 yuv420_buff malloc yuv420_buff err\n");
	}else{
		memset(yuv420_buff, 0, width_small * height_small * 1.5);
	}

        FILE *pfile = fopen(jpegPath,"rb");
        printf("merage_jpeg2yuv_for_nv21() 111 pfile:%p \n",pfile);
        fseek(pfile,0,SEEK_END); /* 定位到文件末尾 */
        int len= ftell(pfile);
        fseek(pfile,0,SEEK_SET); /* 定位到文件开头 */
        size_t result = fread(mjpeg_buff,1,len,pfile); //读取mjpeg_buff
        if (result != len)
        {
        	printf("warning!! read mjpeg_buff:%zu \n",result);
        }
        jpeg_decode(&yuyv_buff, mjpeg_buff, &k, &j);   //将jpeg转码成yuyv422
        dump_yuv(yuyv_buff,width_small*height_small*2,"out/yuv422_small.yuv");
        return;
        YUV422ToNv21(yuyv_buff,yuv420_buff,width_small,height_small); //将yuyv422转码成nv21
        fclose(pfile); 
        printf("merage_jpeg2yuv_for_nv21() 222 mjpeg_buff:%p len:%d  \n",mjpeg_buff,len);
        merge_two_yuv_for_nv21(camera_buff,width_big,height_big,yuv420_buff,width_small,height_small);
}
//两个yv12图片合成
void merge_two_yuv_for_yv12(unsigned char *big_buff,int width_big,int height_big,unsigned char * small_buff,int width_small,int height_small)
{
    printf("lilei getPropertyMultiScan() merge_two_yuv_for_yv12 big_buff:%p,width_big:%d height_big:%d small_buff:%p width_small:%d height_small:%d \n",big_buff,width_big,height_big,small_buff,width_small,height_small);
    if(width_big<width_small || height_big < height_small)
    {
        printf("lilei getPropertyMultiScan() merge_two_yuv_for_yv12 error width_big<width_small || height_big < height_small \n");
        return;
    }
    int small_vu_distance = width_small*height_small;
    int large_vu_distance = width_big*height_big;

    int y_center_offset = ((height_big - height_small)/2)*width_big + (width_big-width_small)/2;
    int v_center_offset = ((height_big - height_small)/4)*width_big/2 + (width_big-width_small)/4;
    int u_center_offset = ((height_big - height_small)/4)*width_big/2 + (width_big-width_small)/4;
    unsigned char * big_buff_y =big_buff + y_center_offset;
    unsigned char * big_buff_v = big_buff+large_vu_distance + v_center_offset;
    unsigned char * big_buff_u = big_buff+large_vu_distance*5/4 + u_center_offset;
    unsigned char * small_buff_y = small_buff;
    unsigned char * small_buff_v = small_buff + small_vu_distance;
    unsigned char * small_buff_u = small_buff + small_vu_distance*5/4;
    printf("lilei getPropertyMultiScan() ##merge_two_yuv_for_yv12  large_vu_distance:%d,small_vu_distance:%d \n",large_vu_distance,small_vu_distance);
    for(int i=0;i<height_small;i++)
    {
        memcpy(big_buff_y,small_buff_y,width_small);
        if(i%2 == 0){
			memcpy(big_buff_v,small_buff_v,width_small/2);
			memcpy(big_buff_u,small_buff_u,width_small/2);
			big_buff_v += width_big/2;
			big_buff_u += width_big/2;
			small_buff_v += width_small/2;
			small_buff_u += width_small/2;
        }
        big_buff_y += width_big;
        small_buff_y += width_small;
     }

     //dump_yuv(nv21_test_buff,width_small*height_small*3/2,"/sdcard/DCIM/image_nv21_small.yuv");
     //dump_yuv(nv21_test_buff_big,width_big*height_big*3/2,"/sdcard/DCIM/image_nv21_big.yuv");
     dump_yuv(big_buff,width_big*height_big*3/2,"out/image_yv12_merge.yuv");
}
//将jpeg合并到yv12图像中
void merge_jpeg2yuv_for_yv12(const char *jpegPath,int width_small,int height_small,unsigned char * camera_buff,int width_big,int height_big)
{
        int k = width_small;
        int j = height_small;
        //buff1为jpeg分配内存
        mjpeg_buff = (unsigned char*)malloc(width_small * height_small * 2);
        if(mjpeg_buff ==  NULL) {
                printf("lilei getPropertyMultiScan() merge_jpeg2yuv_for_yv12() step1 mjpeg_buff malloc err\n");
        }else{
                memset(mjpeg_buff, 0, width_small * height_small * 2);
        }
        //buff2为jpeg转码后的yuv422数据分配内存
        yuyv_buff = (unsigned char*)malloc(width_small * height_small * 2);
	if(yuyv_buff ==  NULL){
		printf("lilei getPropertyMultiScan() merge_jpeg2yuv_for_yv12() step2 yuyv_buff malloc err\n");
	}else{
		memset(yuyv_buff, 0, width_small * height_small * 2);
	}
        //为yuv422转换为nv21数据分配内存
        yuv420_buff = (unsigned char*)malloc(width_small * height_small * 1.5);
	if(yuv420_buff ==  NULL){
		printf("lilei getPropertyMultiScan() merge_jpeg2yuv_for_yv12() step3 yuv420_buff malloc yuv420_buff err\n");
	}else{
		memset(yuv420_buff, 0, width_small * height_small * 1.5);
	}

        FILE *pfile = fopen(jpegPath,"rb");
        printf("lilei getPropertyMultiScan() merge_jpeg2yuv_for_yv12() 111 pfile:%p \n",pfile);
        fseek(pfile,0,SEEK_END); /* 定位到文件末尾 */
        int len= ftell(pfile);
        fseek(pfile,0,SEEK_SET); /* 定位到文件开头 */
        size_t result = fread(mjpeg_buff,1,len,pfile); //读取mjpeg_buff
        jpeg_decode(&yuyv_buff, mjpeg_buff, &k, &j);   //将jpeg转码成yuyv422
        dump_yuv(yuyv_buff,width_big*height_big*2,"out/dump_yuyv.yuv");
        //YUV422ToYv12(yuyv_buff,yuv420_buff,width_small,height_small); //将yuyv422转码成yv12
        fclose(pfile);
        printf("lilei getPropertyMultiScan() merge_jpeg2yuv_for_yv12() 222 mjpeg_buff:%p len:%d  \n",mjpeg_buff,len);
        //merge_two_yuv_for_yv12(camera_buff,width_big,height_big,yuv420_buff,width_small,height_small);
        free(mjpeg_buff);
        free(yuv420_buff);
        free(yuyv_buff);

}
//将nv21合并到yv12图像中
void merge_yuv2yuv_for_yv12(const char *jpegPath,int width_small,int height_small,unsigned char * camera_buff,int width_big,int height_big)
{
        int k = width_small;
        int j = height_small;

        //为yuv422转换为nv21数据分配内存
        yuv420_buff = (unsigned char*)malloc(width_small * height_small * 1.5);
		if(yuv420_buff ==  NULL){
			printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_yv12() step3 yuv420_buff malloc yuv420_buff err\n");
		}else{
			memset(yuv420_buff, 0, width_small * height_small * 1.5);
		}
		//为nv21转换为yv12数据分配内存
	    yuv420_buff_for_yv12 = (unsigned char*)malloc(width_small * height_small * 1.5);
		if(yuv420_buff_for_yv12 ==  NULL){
			printf("lilei getPropertyMultiScan() gome_merge_yuv2yuv_for_yv12() step3 malloc yuv420_buff_for_yv12 err\n");
		}else{
			memset(yuv420_buff_for_yv12, 0, width_small * height_small * 1.5);
		}

        FILE *pfile = fopen(jpegPath,"rb");
        printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_yv12() 111 pfile:%p \n",pfile);
        fseek(pfile,0,SEEK_END); /* 定位到文件末尾 */
        int len= ftell(pfile);
        fseek(pfile,0,SEEK_SET); /* 定位到文件开头 */
        size_t result = fread(yuv420_buff,1,len,pfile); //读取mjpeg_buff

        dump_yuv(yuv420_buff,width_small*height_small*3/2,"out/qrd_image_nv21.yuv");
        Nv21ToYv12(yuv420_buff,yuv420_buff_for_yv12,width_small,height_small); //将nv21转码成yv12
        dump_yuv(yuv420_buff_for_yv12,width_small*height_small*3/2,"out/qrd_image_nv21_to_yv12.yuv");
        fclose(pfile);
        printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_yv12() 222 mjpeg_buff:%p len:%d  \n",yuv420_buff,len);
        merge_two_yuv_for_yv12(camera_buff,width_big,height_big,yuv420_buff_for_yv12,width_small,height_small);
        free(yuv420_buff);
        free(yuv420_buff_for_yv12);

}
//将yuv420合并到nv21图像中
void merge_yuv2yuv_for_nv21(const char *jpegPath,int width_small,int height_small,unsigned char * camera_buff,int width_big,int height_big)
{
        int k = width_small;
        int j = height_small;

        //为yuv422转换为nv21数据分配内存
        yuv420_buff = (unsigned char*)malloc(width_small * height_small * 1.5);
	if(yuv420_buff ==  NULL){
		printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_nv21() step3 yuv420_buff malloc yuv420_buff err\n");
	}else{
		memset(yuv420_buff, 0, width_small * height_small * 1.5);
	}

        FILE *pfile = fopen(jpegPath,"rb");
        printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_nv21() 111 pfile:%p \n",pfile);
        fseek(pfile,0,SEEK_END); /* 定位到文件末尾 */
        int len= ftell(pfile);
        fseek(pfile,0,SEEK_SET); /* 定位到文件开头 */
        size_t result = fread(yuv420_buff,1,len,pfile); //读取mjpeg_buff

        dump_yuv(yuv420_buff,width_small*height_small*3/2,"out/qrd_image_yuv420.yuv");
        //YUV422ToYv12(yuyv_buff,yuv420_buff,width_small,height_small); //将yuyv422转码成yv12
        fclose(pfile);
        printf("lilei getPropertyMultiScan() merge_yuv2yuv_for_nv21() 222 mjpeg_buff:%p len:%d  \n",yuv420_buff,len);
        merge_two_yuv_for_nv21(camera_buff,width_big,height_big,yuv420_buff,width_small,height_small);
        free(yuv420_buff);

}
int get_camera_buff()
{
	yuv420_buff_big = (unsigned char*)malloc(WIDTH_BIG * HIGHT_BIG * 1.5);
        if(yuv420_buff_big ==  NULL){
                perror("get_camera_buff() malloc yuv420_buff_big err\n");
        }else{
		memset(yuv420_buff_big, 0, WIDTH_BIG * HIGHT_BIG * 1.5);
	}       
        //////step2
        char stryuvpath[80]={"camera_taobao_yv12_640x480.yuv"};
        FILE *pyuvfile = fopen(stryuvpath,"rb"); 
        fseek(pyuvfile,0,SEEK_END); /* 定位到文件末尾 */
        int lenyuv= ftell(pyuvfile);
        fseek(pyuvfile,0l,SEEK_SET); /* 定位到文件开头 */ 
        size_t result = fread(yuv420_buff_big,1,lenyuv,pyuvfile); 
        fclose(pyuvfile);
        printf("get_camera_buff() lenyuv:%d result:%d\n",lenyuv,result);
        
}

int main()
{
	printf("testyuv.c main() 000 \n");
        get_camera_buff();

        merge_yuv2yuv_for_yv12("image1_640x480.yuv",640,480,yuv420_buff_big,WIDTH_BIG,HIGHT_BIG);

       // merge_yuv2yuv_for_nv21("image1_320x280.yuv",320,280,yuv420_buff_big,WIDTH_BIG,HIGHT_BIG);

        //merge_jpeg2yuv_for_yv12("1.jpg",320,240,yuv420_buff_big,WIDTH_BIG,HIGHT_BIG);
        //merage_jpeg2yuv_for_nv21("1_300x200.jpg",640,480,yuv420_buff_big,WIDTH_BIG,HIGHT_BIG);
}
