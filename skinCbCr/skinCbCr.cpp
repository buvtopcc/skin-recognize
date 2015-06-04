#include"stdafx.h"
#include<windows.h>
#include"cv.h"
#include"highgui.h"
#include"stdio.h"

void skipframe(CvCapture* capture,int n){
	for(int i=1;i<=n;i++)
	cvQueryFrame(capture);                                                                              //在视频中跳过一帧，获取下一帧
}

void cvSkinSegment(IplImage* img, IplImage* mask){
	CvSize imageSize = cvSize(img->width, img->height);
	IplImage *imgY = cvCreateImage(imageSize, IPL_DEPTH_8U, 1);
	IplImage *imgCr = cvCreateImage(imageSize, IPL_DEPTH_8U, 1);
	IplImage *imgCb = cvCreateImage(imageSize, IPL_DEPTH_8U, 1);
	IplImage *imgYCrCb = cvCreateImage(imageSize, img->depth, img->nChannels);
	cvCvtColor(img,imgYCrCb,CV_BGR2YCrCb);																//将BGR图片转化成YCrCb格式图片
	
	/*将Y、Cr、Cb通道分离，然后用三个指针分别对每一个通道的像素点就行处理*/
	cvSplit(imgYCrCb, imgY, imgCr, imgCb, 0);
	int y, cr, cb, l, x1, y1, value;
	unsigned char *pY, *pCr, *pCb, *pMask;
	pY = (unsigned char *)imgY->imageData;
	pCr = (unsigned char *)imgCr->imageData;
	pCb = (unsigned char *)imgCb->imageData;
	pMask = (unsigned char *)mask->imageData;
	
	cvSetZero(mask);																					 //将mask指向的图片像素点的值都清零
	l = img->height * img->width;																		 //计算总的像素点数，用来确定下面的循环计算
	 
	/*在Cb Cr空间上找到一个可以拟合常规肤色分布的椭圆形，然后把在椭圆形区域内的像素点标记为肤色*/
	for (int i = 0; i < l; i++){
		y  = *pY;
		cr = *pCr;
		cb = *pCb;
		cb -= 109;
		cr -= 152;
		x1 = (819*cr-614*cb)/32 + 51;
		y1 = (819*cr+614*cb)/32 + 77;
		x1 = x1*41/1024;
		y1 = y1*73/1024;
		value = x1*x1+y1*y1;
		if(y<100)	(*pMask)=(value<700) ? 255:0;                                                        //做阈值判断的命令
		else		(*pMask)=(value<850)? 255:0;
		pY++;
		pCr++;
		pCb++;
		pMask++;
	}
	cvReleaseImage(&imgY);
	cvReleaseImage(&imgCr);
	cvReleaseImage(&imgCb);
	cvReleaseImage(&imgYCrCb);
}
int main()
{
	/*每次运行程序都将日志文件清空，方便看此次得到的数据*/
	FILE *fp;
	fp=fopen("d:\\gesturebook.log","w");
	fclose(fp);    

	/*另外一种实现清空的方法
	remove("d:\\gesturebook.log");  
	*/

	//声明调用系统时间的变量
	time_t timer;
	struct tm *ptrtime;

	cvNamedWindow( "origin",CV_WINDOW_AUTOSIZE);     //创建名字为ORIGIN窗口
	CvCapture* capture;                  
	capture = cvCreateCameraCapture(0);              //从摄像头读取视频
	IplImage* frame;                      
	skipframe(capture,3);                            //这个比较重要，因为此处不加下面判断第一帧得到返回值总是为空导致程序在接下来的break结束。猜想可能是因为具体的摄像头获取有关，因为有些电脑直接用函数cvQueryFrame得到的第一帧就是非空的，那么此处就可以不加
	while(1) {                                       //用一个一直为真的条件使之一直能从摄像头读取帧，直到手动产生一个esc的动作使之跳出循环
		frame = cvQueryFrame( capture );
		if( !frame ) break;              
		//cvShowImage( "origin", frame );	                                       //仅用来测试用
		IplImage* dstcrcb=cvCreateImage(cvGetSize(frame),8,1);
		cvSkinSegment(frame,dstcrcb);                                              //调用肤色检测函数处理获得的帧图片
		cvDilate(dstcrcb,dstcrcb,NULL,1);                                          //进行膨胀处理，放大像素点的值 
		cvSmooth(dstcrcb,dstcrcb,CV_GAUSSIAN,3,3,0,0);//3x3                        //进行高斯平滑处理

		{   /*设置感兴趣的roi区域*/
			int width = 640;
			int height = 160;
			cvSetImageROI(frame,cvRect(0,320,width,height));

			/*画轮廓*/
			IplImage *dsw = cvCreateImage(cvGetSize(dstcrcb), 8, 1);  
			IplImage *dst = cvCreateImage(cvGetSize(dstcrcb), 8, 3);  
			CvMemStorage *storage = cvCreateMemStorage(0);  
			CvSeq *first_contour = NULL;  

			cvThreshold(dstcrcb, dsw, 130, 255, CV_THRESH_BINARY);                                                                   //对灰度图像进行阈值操作得到二值图像
			cvFindContours(dsw, storage, &first_contour, sizeof(CvContour),CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);                //寻找轮廓
			cvZero(dst);  
			int cnt = 0;  
			for(; first_contour != 0; first_contour = first_contour->h_next)  
			{  
				cnt++;  
				CvScalar color = CV_RGB(255, 255,255);  
				cvDrawContours(dst, first_contour, color, color, 0, 2, CV_FILLED, cvPoint(0, 0));  
				CvRect rect = cvBoundingRect(first_contour,0);
				if(rect.width>100&&rect.width<500)                                                                                  //对识别的肤色区域大致的限制，防止类似的皮肤的背景干扰得到不是预期的轮廓                                                                   
				{
					cvRectangle(frame, cvPoint(rect.x, rect.y), cvPoint(rect.x + 100, rect.y+130),CV_RGB(255, 255, 255), 1, 8, 0);  //画矩形框，起始点是识别的轮廓的左上角点，大小为100*130，这个数据是多次试验发现识别手势较好的大小
					if((rect.x-frame->origin)>=0&&(rect.x-frame->origin)<=2)                                                        //根据识别的手势距离图像的左边界的距离是否足够近，如果满足则产生认为是有一个向左滑动动作，即产生一个相应的page up动作
					{	 /*调用time()函数获取当前时间*/
						timer=time(NULL);
						/*调用localtime()函数将获得的系统时间转化为指向struct tm的指针指向的结构体*/
						ptrtime = localtime( &timer ) ;
						keybd_event(VK_LEFT,0,0,0);                                  //调用系统键鼠事件，产生一个按下左键（即page down）的动作
						if((fp=fopen("d:\\gesturebook.log","a"))==NULL)              //打开文件
						{
						     printf("can't open the file!\n");
							 exit(1);
						}
						else
						fprintf(fp,"%s	:Page Down\n\n",asctime( ptrtime));         //将时间以及操作写到文件中
    					fclose(fp);                                                 //关闭文件
						skipframe(capture,7);                                       //跳过几帧为了防止读取多次操作
					}

					if((frame->origin+frame->width-rect.x-rect.width)>=0&&(frame->origin+frame->width-rect.x-rect.width)<=2)           ////根据识别的手势距离图像的边界的距离是否足够近，如果满足则产生认为是有一个向右滑动动作，即产生一个相应的page down动作
					{  
						/*调用time()函数获取当前时间*/
						timer=time(NULL);
						/*调用localtime()函数将获得的系统时间转化为指向struct tm的指针指向的结构体*/
						ptrtime = localtime( &timer ) ;
						keybd_event(VK_RIGHT,0,0,0);                                 //调用系统键鼠事件，产生一个按下左键（即page down）的动作
						if((fp=fopen("d:\\gesturebook.log","a"))==NULL)              //打开文件
						{
						     printf("can't open the file!\n");
							 exit(1);
						}
						else
						fprintf(fp,"%s	:Page UP\n\n",asctime( ptrtime));
						fclose(fp);						
						skipframe(capture,5);
					}
				}
			} 
			cvResetImageROI(frame);//重置ROI
			cvShowImage( "origin", frame );	
			//cvShowImage("out", dstcrcb);
			cvReleaseImage(&dst);
			cvReleaseImage(&dsw);
			cvReleaseMemStorage(&storage);  
		}

		char c = cvWaitKey(5);
		if( c == 27 ) break;
		cvReleaseImage(&dstcrcb);

	}
	cvReleaseCapture( &capture );
	cvDestroyWindow( "origin" );
}
