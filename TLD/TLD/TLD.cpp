// TLD.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

//#include "pch.h"

#include <iostream>
using std::cin;
using std::cout;
using std::endl;
using std::cerr;
#include <fstream>
using std::fstream;
using std::ifstream;
using std::ofstream;
using std::ios;
#include <string>
using std::string;

//opencv
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

//STRUCK
#include "./src/Tracker.h"
#include "./src/Config.h"
#include "./src/vot.hpp"

using namespace cv;

//STRUCK main函数中实现使用的
static const int kLiveBoxWidth = 80;
static const int kLiveBoxHeight = 80;

#define PRECISION_THRESHOLD_NUMBER 10  //精确率的10个阈值
#define SUCCESS_THRESHOLD_NUMBER 9  //成功率的9个阈值
#define DIAGRAM_SIZE 700 // 图的长/宽
#define DIAGRAM_INTERVAL 100 //图的留空
#define DIAGRAM_CANVAS (DIAGRAM_SIZE-2*DIAGRAM_INTERVAL)  //实际作图区域大小
int PrecisionThreshold[PRECISION_THRESHOLD_NUMBER] = { 100 * 100,90 * 90,80 * 80,70 * 70,60 * 60,50 * 50,40 * 40,30 * 30,20 * 20,10 * 10 }; //Precision的阈值
double SuccessThreshold[SUCCESS_THRESHOLD_NUMBER] = { 0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9 }; //Success的阈值

Scalar KCF_color(255, 0, 0);
Scalar TLD_color(0, 255, 0);
Scalar GOTURN_color(0, 0, 255);
Scalar STRUCK_color(150, 150, 200);

//STRUCK
void rectangle(Mat& rMat, const FloatRect& rRect, const Scalar& rColour)
{
	IntRect r(rRect);
	rectangle(rMat, Point(r.XMin(), r.YMin()), Point(r.XMax(), r.YMax()), rColour);
}

//给定对应帧的ground truth中数据，与实际追踪得到的box，计算对应的精确率并更新到存放精确率数据的数组中
void calcPrecisionRate(int* precisionData,int* precisionThreshold, double g_x,double g_y,double g_width,double g_height,Rect2d &box)
{
	double g_centerX = (g_x + g_width) / 2.0;
	double g_centerY = (g_y + g_height) / 2.0;

	double r_centerX = (box.x + box.width) / 2.0;
	double r_centerY = (box.y + box.height) / 2.0;

	double squareDistance = (g_centerX - r_centerX)*(g_centerX - r_centerX) + (g_centerY - r_centerY)*(g_centerY - r_centerY);

	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		if (squareDistance <= precisionThreshold[i])
		{
			precisionData[i]++;
		}
	}
}

//给定对应帧的ground truth中数据，与实际追踪得到的box，计算对应的成功率并更新到存放成功率数据的数组中
void calcSuccessRate(int *successData, double* successThreshold, double g_x, double g_y, double g_width, double g_height, Rect2d &box)
{
	double startX = min(g_x, box.x);
	double endX = max(g_x + g_width, box.x + box.width);
	double width = g_width + box.width - (endX - startX);

	double startY = min(g_y, box.y);
	double endY = max(g_y + g_height, box.y + box.height);
	double height = g_height + box.height - (endY - startY);

	double overlaps_area = width * height;

	double g_area = g_width * g_height;
	double r_area = box.width*box.height;
	double total_area = g_area + r_area - overlaps_area;

	double ratio = overlaps_area / total_area;
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		if (ratio >= successThreshold[i])
		{
			successData[i]++;
		}
	}
}

//给定对应帧的ground truth中数据，与实际追踪得到的box，计算对应的精确率并更新到存放精确率数据的数组中
void calcPrecisionRate(int* precisionData, int* precisionThreshold, double g_x, double g_y, double g_width, double g_height, FloatRect box)
{
	double g_centerX = (g_x + g_width) / 2.0;
	double g_centerY = (g_y + g_height) / 2.0;

	double r_centerX = (box.XMin() + box.Width()) / 2.0;
	double r_centerY = (box.YMin() + box.Height()) / 2.0;

	double squareDistance = (g_centerX - r_centerX)*(g_centerX - r_centerX) + (g_centerY - r_centerY)*(g_centerY - r_centerY);

	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		if (squareDistance <= precisionThreshold[i])
		{
			precisionData[i]++;
		}
	}
}

//给定对应帧的ground truth中数据，与实际追踪得到的box，计算对应的成功率并更新到存放成功率数据的数组中
void calcSuccessRate(int *successData, double* successThreshold, double g_x, double g_y, double g_width, double g_height, FloatRect box)
{
	float startX = min(float(g_x), box.XMin());
	float endX = max(float(g_x) + float(g_width), box.XMin() + box.Width());
	float width = float(g_width) + box.Width() - (endX - startX);

	float startY = min(float(g_y), box.YMin());
	float endY = max(float(g_y) + float(g_height), box.YMin() + box.Height());
	float height = float(g_height) + box.Height() - (endY - startY);

	float overlaps_area = width * height;

	float g_area = g_width * g_height;
	float r_area = box.Width()*box.Height();
	float total_area = g_area + r_area - overlaps_area;

	float ratio = overlaps_area / total_area;
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		if (ratio >= successThreshold[i])
		{
			successData[i]++;
		}
	}
}

//给定记录精确率数组以及总帧数，得到最后的精确率，并画在对应图上
void drawPrecisionRate(Mat& diagram, int* precisionData, int totalFrames,Scalar color)
{
	Point2d lastPoint;
	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		double y = DIAGRAM_INTERVAL + DIAGRAM_CANVAS - ((precisionData[i] * 1.0 / totalFrames)* DIAGRAM_CANVAS);
		double x = i * (DIAGRAM_CANVAS / 10) + (DIAGRAM_CANVAS / 10)+DIAGRAM_INTERVAL;
		if (i == 0)
		{
			lastPoint.x = x;
			lastPoint.y = y;
		}
		else
		{
			line(diagram, lastPoint, Point2d(x, y), color);
			lastPoint.x = x;
			lastPoint.y = y;
		}		
	}
}

//给定记录正确率数组以及总帧数，得到最后的正确率，并画在对应图上
void drawSuccessRate(Mat& diagram, int* successData, int totalFrames, Scalar color)
{
	Point2d lastPoint;
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		double y = DIAGRAM_INTERVAL + DIAGRAM_CANVAS - ((successData[i] * 1.0 / totalFrames)* DIAGRAM_CANVAS);
		double x = i * (DIAGRAM_CANVAS / 10) + (DIAGRAM_CANVAS / 10) + DIAGRAM_INTERVAL;
		if (i == 0)
		{
			lastPoint.x = x;
			lastPoint.y = y;
		}
		else
		{
			line(diagram, lastPoint, Point2d(x, y), color);
			lastPoint.x = x;
			lastPoint.y = y;
		}
	}
}

//SuccessRate图的初始化
void initializeSuccessRateDiagram(Mat& successRateDiagram)
{
	//1--画坐标轴
	Point2d y_axes_up(DIAGRAM_INTERVAL, DIAGRAM_INTERVAL);
	Point2d y_axes_down(DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);

	Point2d x_axes_left(DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);
	Point2d x_axes_right(DIAGRAM_CANVAS + DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);

	line(successRateDiagram, x_axes_left, x_axes_right, Scalar(255, 255, 255));
	line(successRateDiagram, y_axes_down, y_axes_up, Scalar(255, 255, 255));

	line(successRateDiagram, x_axes_left, x_axes_right, Scalar(255, 255, 255));
	line(successRateDiagram, y_axes_down, y_axes_up, Scalar(255, 255, 255));

	//2--画坐标轴上数字
	//字体
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, 8);
	const int fontWidth = 45; //CV_FONT_HERSHEY_PLAIN 对应字体的宽
	const int fontHeight = 10;
	IplImage* SuccessRateImage = &IplImage(successRateDiagram);

	//y轴
	cvPutText(SuccessRateImage, "100%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 0), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "90%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 1), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "80%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 2), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "70%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 3), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "60%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 4), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "50%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 5), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "40%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 6), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "30%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 7), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "20%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 8), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "10%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 9), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 10), &font, Scalar(255, 255, 255));
	//x轴
	cvPutText(SuccessRateImage, "0.1", Point2d((DIAGRAM_CANVAS / 10) * 1 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.2", Point2d((DIAGRAM_CANVAS / 10) * 2 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.3", Point2d((DIAGRAM_CANVAS / 10) * 3 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.4", Point2d((DIAGRAM_CANVAS / 10) * 4 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.5", Point2d((DIAGRAM_CANVAS / 10) * 5 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.6", Point2d((DIAGRAM_CANVAS / 10) * 6 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.7", Point2d((DIAGRAM_CANVAS / 10) * 7 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.8", Point2d((DIAGRAM_CANVAS / 10) * 8 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(SuccessRateImage, "0.9", Point2d((DIAGRAM_CANVAS / 10) * 9 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));

	//3--画上图例
	int lineLength = DIAGRAM_INTERVAL;
	int lineInterval = fontHeight + 5;

	int LeftLineStartX = DIAGRAM_INTERVAL;
	int LeftLineEndX = DIAGRAM_INTERVAL + lineLength;
	int firstLineY = DIAGRAM_INTERVAL / 2 - (fontHeight / 2);


	int RightLineStartX = DIAGRAM_INTERVAL + lineLength + fontWidth + 30;
	int RightLineEndX = DIAGRAM_INTERVAL + lineLength + fontWidth + 30 + lineLength;
	int secondLindeY = firstLineY + fontHeight + 10;


	int firstColAnnotationX = LeftLineEndX + 5;
	int firstLineAnnotationY = DIAGRAM_INTERVAL / 2;

	int secondColAnnotaionX = RightLineEndX + 5;
	int secondLineAnnotaionY = DIAGRAM_INTERVAL / 2 + fontHeight + 10;
	//TLD
	line(successRateDiagram, Point2d(LeftLineStartX, firstLineY), Point2d(LeftLineEndX, firstLineY), TLD_color);
	cvPutText(SuccessRateImage, "TLD", Point2d(firstColAnnotationX, firstLineAnnotationY), &font, TLD_color);

	//KCF
	line(successRateDiagram, Point2d(RightLineStartX, firstLineY), Point2d(RightLineEndX, firstLineY), KCF_color);
	cvPutText(SuccessRateImage, "KCF", Point2d(secondColAnnotaionX, firstLineAnnotationY), &font, KCF_color);

	//GOTURN
	line(successRateDiagram, Point2d(LeftLineStartX, secondLindeY), Point2d(LeftLineEndX, secondLindeY), GOTURN_color);
	cvPutText(SuccessRateImage, "GOTURN", Point2d(firstColAnnotationX, secondLineAnnotaionY), &font, GOTURN_color);

	//STRUCK
	line(successRateDiagram, Point2d(RightLineStartX, secondLindeY), Point2d(RightLineEndX, secondLindeY), STRUCK_color);
	cvPutText(SuccessRateImage, "STRUCK", Point2d(secondColAnnotaionX, secondLineAnnotaionY), &font, STRUCK_color);
}

//PrecisionRate图的初始化
void initializePrecisionRateDiagram(Mat& precisionRateDiagram)
{
	//1--画坐标轴
	Point2d y_axes_up(DIAGRAM_INTERVAL, DIAGRAM_INTERVAL);
	Point2d y_axes_down(DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);

	Point2d x_axes_left(DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);
	Point2d x_axes_right(DIAGRAM_CANVAS + DIAGRAM_INTERVAL, DIAGRAM_CANVAS + DIAGRAM_INTERVAL);

	line(precisionRateDiagram, x_axes_left, x_axes_right, Scalar(255, 255, 255));
	line(precisionRateDiagram, y_axes_down, y_axes_up, Scalar(255, 255, 255));

	line(precisionRateDiagram, x_axes_left, x_axes_right, Scalar(255, 255, 255));
	line(precisionRateDiagram, y_axes_down, y_axes_up, Scalar(255, 255, 255));

	//2--画坐标轴上数字
	//字体
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, 8);
	const int fontWidth = 45; //CV_FONT_HERSHEY_PLAIN 对应字体 4个字符的宽
	const int fontHeight = 10; //CV_FONT_HERSHEY_PLAIN 对应字体的高 
	IplImage* PrecisionRateImage = &IplImage(precisionRateDiagram);

	//y轴
	cvPutText(PrecisionRateImage, "100%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 0), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "90%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 1), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "80%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 2), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "70%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 3), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "60%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 4), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "50%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 5), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "40%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 6), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "30%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 7), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "20%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 8), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "10%", Point2d(DIAGRAM_INTERVAL - fontWidth, DIAGRAM_INTERVAL + (DIAGRAM_CANVAS / 10) * 9), &font, Scalar(255, 255, 255));

	//x轴
	cvPutText(PrecisionRateImage, "100", Point2d((DIAGRAM_CANVAS / 10) * 1 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "90", Point2d((DIAGRAM_CANVAS / 10) * 2 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "80", Point2d((DIAGRAM_CANVAS / 10) * 3 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "70", Point2d((DIAGRAM_CANVAS / 10) * 4 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "60", Point2d((DIAGRAM_CANVAS / 10) * 5 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "50", Point2d((DIAGRAM_CANVAS / 10) * 6 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "40", Point2d((DIAGRAM_CANVAS / 10) * 7 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "30", Point2d((DIAGRAM_CANVAS / 10) * 8 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "20", Point2d((DIAGRAM_CANVAS / 10) * 9 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));
	cvPutText(PrecisionRateImage, "10", Point2d((DIAGRAM_CANVAS / 10) * 10 + DIAGRAM_INTERVAL - fontWidth / 2, DIAGRAM_CANVAS + DIAGRAM_INTERVAL + fontHeight + 10), &font, Scalar(255, 255, 255));

	//3--画上图例
	int lineLength = DIAGRAM_INTERVAL;
	int lineInterval = fontHeight + 5;

	int LeftLineStartX = DIAGRAM_INTERVAL;
	int LeftLineEndX = DIAGRAM_INTERVAL + lineLength;
	int firstLineY = DIAGRAM_INTERVAL / 2 - (fontHeight / 2);
	

	int RightLineStartX = DIAGRAM_INTERVAL + lineLength + fontWidth + 30;
	int RightLineEndX = DIAGRAM_INTERVAL + lineLength + fontWidth + 30 + lineLength;
	int secondLindeY = firstLineY + fontHeight + 10;


	int firstColAnnotationX = LeftLineEndX + 5;
	int firstLineAnnotationY = DIAGRAM_INTERVAL / 2;

	int secondColAnnotaionX = RightLineEndX + 5;
	int secondLineAnnotaionY = DIAGRAM_INTERVAL / 2 + fontHeight + 10;
	//TLD
	line(precisionRateDiagram, Point2d(LeftLineStartX, firstLineY), Point2d(LeftLineEndX, firstLineY), TLD_color);
	cvPutText(PrecisionRateImage, "TLD", Point2d(firstColAnnotationX, firstLineAnnotationY), &font, TLD_color);

	//KCF
	line(precisionRateDiagram, Point2d(RightLineStartX, firstLineY), Point2d(RightLineEndX, firstLineY), KCF_color);
	cvPutText(PrecisionRateImage, "KCF", Point2d(secondColAnnotaionX, firstLineAnnotationY), &font, KCF_color);

	//GOTURN
	line(precisionRateDiagram, Point2d(LeftLineStartX, secondLindeY), Point2d(LeftLineEndX, secondLindeY), GOTURN_color);
	cvPutText(PrecisionRateImage, "GOTURN", Point2d(firstColAnnotationX, secondLineAnnotaionY), &font, GOTURN_color);

	//STRUCK
	line(precisionRateDiagram, Point2d(RightLineStartX, secondLindeY), Point2d(RightLineEndX, secondLindeY), STRUCK_color);
	cvPutText(PrecisionRateImage, "STRUCK", Point2d(secondColAnnotaionX, secondLineAnnotaionY), &font, STRUCK_color);
}

Rect2d changeDataStyle(double point1x, double point1y, double point2x, double point2y, double point3x, double point3y, double point4x, double point4y)
{	
	double leftx = min(point4x, min(point3x, min(point1x, point2x)));
	double upy = min(point4y, min(point3y, min(point1y, point2y)));
	double rightx = max(point4x, max(point3x, max(point1x, point2x)));
	double downy = max(point4y, max(point3y, max(point1y, point2y)));

	Point2d leftUp(leftx, upy);

	double width = rightx - leftx;
	double height =  downy- upy;

	Rect2d result(leftUp.x, leftUp.y, width, height);

	return result;
}


int main(int argc, char** argv) {
	//创建tracker
	Ptr<cv::Tracker> TLDTracker = TrackerTLD::create();
	Ptr<cv::Tracker> KCFTracker = TrackerKCF::create();
	Ptr<cv::Tracker> GOTURNTracker = TrackerGOTURN::create();	

	//配置类生成
	string configPath = "config.txt";
	Config conf(configPath);
	cout << conf << endl;

	//文件路径	
	string videoPath(conf.videoPath);
	string groundtruthPath(conf.groundtruthPath);
	string precisionRateDiagramSavingPath(conf.precisionRateDiagramSavingPath);
	precisionRateDiagramSavingPath += conf.sequenceName;
	precisionRateDiagramSavingPath += "_PrecisionRate.jpg";
	string successRateDiagramSavingPath(conf.successRateDiagramSavingPath);
	successRateDiagramSavingPath += conf.sequenceName;
	successRateDiagramSavingPath += "_SuccessRate.jpg";
	//储存每一帧的数据
	Mat frame; 
	int totalFrameNumber;
	
	/*画图相关*/
	//1--创建图片变量
	Mat PrecisionRate(DIAGRAM_SIZE, DIAGRAM_SIZE, CV_8UC3,Scalar(0,0,0)); //储存反应PrecisionRate的图（中心距离）
	Mat SuccessRate(DIAGRAM_SIZE, DIAGRAM_SIZE, CV_8UC3,Scalar(0,0,0));//储存反应SuccessRate的图（覆盖率）	
	


	//2--初始化图（坐标轴、数字）
	initializePrecisionRateDiagram(PrecisionRate);
	initializeSuccessRateDiagram(SuccessRate);

	//打开的ground truth文件
	fstream groudtruth;

	//临时变量
	double g_x;
	double g_y;
	double g_width;
	double g_height;

	/*1---------------------------------------------------------TLD*/
	cout << "开始TLD追踪" << endl;

	//打开图片序列
	VideoCapture video(videoPath.c_str());
	if (!video.isOpened()) {
		cerr << "cannot read video!" << endl;
		return -1;
	}
	else
	{
		totalFrameNumber = video.get(CV_CAP_PROP_FRAME_COUNT);
	}

	//画图用数据初始化
	int PrecisionData[PRECISION_THRESHOLD_NUMBER] = { 0 }; //Precision对应阈值的数据
	int SuccessData[SUCCESS_THRESHOLD_NUMBER] = { 0 }; //Success对应阈值的数据

	//读取对应视频的groundtruth数据
	groudtruth.open(groundtruthPath.c_str(), ios::in);

	//读取第一帧
	video.read(frame); 	

	//读取第一帧的groundtruth数据并初始化包围盒
	Rect2d box_TLD;
	if (conf.XYWidthHeight)
	{
		groudtruth >> g_x >> g_y >> g_width >> g_height;
		box_TLD = Rect2d(g_x, g_y, g_width, g_height);
	}
	else
	{
		double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
		groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

		box_TLD = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
	}
		
	//初始化tracker
	TLDTracker->init(frame, box_TLD);	
	

	//遍历视频的每一帧
	while (video.read(frame)) 
	{
		TLDTracker->update(frame, box_TLD);		

		if (conf.XYWidthHeight)
		{
			groudtruth >> g_x >> g_y >> g_width >> g_height;
		}
		else
		{
			double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
			groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

			Rect2d temp = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
			g_x = temp.x; g_y = temp.y; g_width = temp.width; g_height = temp.height;
		}
		
		//计算画图用的数据
		calcPrecisionRate(PrecisionData, PrecisionThreshold, g_x, g_y, g_width, g_height,box_TLD);
		calcSuccessRate(SuccessData, SuccessThreshold, g_x, g_y, g_width, g_height, box_TLD);

		rectangle(frame, box_TLD, Scalar(255, 0, 0), 2, 1);
		//rectangle(frame, roi, Scalar(255, 0, 0), 2, 1);
		imshow("Tracking", frame);
		int k = waitKey(100);
		if (k == 27) break;
	}
	
	drawPrecisionRate(PrecisionRate, PrecisionData, totalFrameNumber, Scalar(0, 255, 0));
	drawSuccessRate(SuccessRate, SuccessData, totalFrameNumber, Scalar(0, 255, 0));

	imshow("PrecisionRate", PrecisionRate);
	imshow("SuccessRate", SuccessRate);

	groudtruth.close();
	video.~VideoCapture();

	cout << "TLD结果已生成" << endl;
	waitKey(50);




	/*2---------------------------------------------------------KCF*/	
	cout << "开始KCF追踪" << endl;
	//打开图片序列
	video= VideoCapture(videoPath.c_str());
	if (!video.isOpened()) {
		cerr << "cannot read video!" << endl;
		return -1;
	}

	//画图用数据初始化
	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		PrecisionData[i] = 0;
	}	
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		SuccessData[i] = 0;
	}	

	//读取对应视频的groundtruth数据	
	groudtruth.open(groundtruthPath.c_str(), ios::in);

	//读取第一帧
	video.read(frame);

	//初始化包围盒	
	Rect2d box_KCF;
	if (conf.XYWidthHeight)
	{
		groudtruth >> g_x >> g_y >> g_width >> g_height;
		box_KCF = Rect2d(g_x, g_y, g_width, g_height);
	}
	else
	{
		double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
		groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

		box_KCF = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
	}

	//初始化tracker
	KCFTracker->init(frame, box_KCF);
	while (video.read(frame)) 
	{			
		KCFTracker->update(frame, box_KCF);

		if (conf.XYWidthHeight)
		{
			groudtruth >> g_x >> g_y >> g_width >> g_height;
		}
		else
		{
			double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
			groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

			Rect2d temp = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
			g_x = temp.x; g_y = temp.y; g_width = temp.width; g_height = temp.height;
		}

		//计算画图用的数据
		calcPrecisionRate(PrecisionData, PrecisionThreshold, g_x, g_y, g_width, g_height, box_KCF);
		calcSuccessRate(SuccessData, SuccessThreshold, g_x, g_y, g_width, g_height, box_KCF);

		//! [visualization]
		// draw the tracked object
		rectangle(frame, box_KCF, Scalar(255, 0, 0), 2, 1);

		// show image with the tracked object
		imshow("Tracking", frame);
		//! [visualization]
		//quit on ESC button
		if (waitKey(1) == 27)
			break;
	}
	drawPrecisionRate(PrecisionRate, PrecisionData, totalFrameNumber,Scalar(255,0,0));
	drawSuccessRate(SuccessRate, SuccessData, totalFrameNumber, Scalar(255, 0, 0));

	imshow("PrecisionRate", PrecisionRate);
	imshow("SuccessRate", SuccessRate);

	groudtruth.close();
	video.~VideoCapture();

	cout << "KCF追踪结果已生成" << endl;
	waitKey(50);




	/*3---------------------------------------------------------GOTURN*/
	cout << "开始GOTURN 追踪" << endl;
	//打开图片序列
	video = VideoCapture(videoPath.c_str());
	if (!video.isOpened()) {
		cerr << "cannot read video!" << endl;
		return -1;
	}

	//画图用数据初始化
	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		PrecisionData[i] = 0;
	}
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		SuccessData[i] = 0;
	}

	//读取对应视频的groundtruth数据
	groudtruth.open(groundtruthPath.c_str(), ios::in);

	//读取第一帧
	video.read(frame);

	//初始化包围盒
	Rect2d box_GOTURN;
	if (conf.XYWidthHeight)
	{
		groudtruth >> g_x >> g_y >> g_width >> g_height;
		box_GOTURN = Rect2d(g_x, g_y, g_width, g_height);
	}
	else
	{
		double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
		groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

		box_GOTURN = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
	}

	//初始化tracker
	GOTURNTracker->init(frame, box_GOTURN);
	while (video.read(frame)) 
	{
		if (conf.XYWidthHeight)
		{
			groudtruth >> g_x >> g_y >> g_width >> g_height;
		}
		else
		{
			double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
			groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

			Rect2d temp = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
			g_x = temp.x; g_y = temp.y; g_width = temp.width; g_height = temp.height;
		}

		if (GOTURNTracker->update(frame, box_GOTURN))
		{
			//! [visualization]
			// draw the tracked object
			rectangle(frame, box_GOTURN, Scalar(255, 0, 0), 2, 1);
		}

		//计算画图用的数据
		calcPrecisionRate(PrecisionData, PrecisionThreshold, g_x, g_y, g_width, g_height, box_GOTURN);
		calcSuccessRate(SuccessData, SuccessThreshold, g_x, g_y, g_width, g_height, box_GOTURN);		

		// show image with the tracked object
		imshow("Tracking", frame);
		//! [visualization]
		//quit on ESC button
		if (waitKey(1) == 27)
			break;
	}
	drawPrecisionRate(PrecisionRate, PrecisionData, totalFrameNumber, Scalar(0, 0, 255));
	drawSuccessRate(SuccessRate, SuccessData, totalFrameNumber, Scalar(0, 0, 255));

	imshow("PrecisionRate", PrecisionRate);
	imshow("SuccessRate", SuccessRate);

	groudtruth.close();
	video.~VideoCapture();

	cout << "GOTURN 追踪结果已生成" << endl;
	waitKey(50);




	/*4---------------------------------------------------------STRUCK*/
	cout << "开始STRUCK 追踪" << endl;
	//打开图片序列
	video = VideoCapture(videoPath.c_str());
	if (!video.isOpened()) {
		cerr << "cannot read video!" << endl;
		return -1;
	}

	//画图用数据初始化
	for (int i = 0; i < PRECISION_THRESHOLD_NUMBER; i++)
	{
		PrecisionData[i] = 0;
	}
	for (int i = 0; i < SUCCESS_THRESHOLD_NUMBER; i++)
	{
		SuccessData[i] = 0;
	}

	//读取对应视频的groundtruth数据
	groudtruth;
	groudtruth.open(groundtruthPath.c_str(), ios::in);

	//读取配置文件
	if (argc > 1)
	{
		configPath = argv[1];
	}		

	if (conf.features.size() == 0)
	{
		cout << "error: no features specified in config" << endl;
		return EXIT_FAILURE;
	}

	//创建Tracker
	STRUCK::Tracker tracker(conf);
	bool challengeMode = false;

	//是否使用摄像头
	bool useCamera = (conf.sequenceName == "");

	//标定从哪一帧开始
	int startFrame = -1;
	int endFrame = 10000;

	float scaleW = 1.f;
	float scaleH = 1.f;

	//初始化包围盒	
	FloatRect box_STRUCK;
	if (conf.XYWidthHeight)
	{
		groudtruth >> g_x >> g_y >> g_width >> g_height;
		box_STRUCK = FloatRect(g_x, g_y, g_width, g_height);		
	}
	else
	{
		double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
		groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

		Rect2d temp = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
		box_STRUCK = FloatRect(temp.x, temp.y, temp.width, temp.height);
	}

	//初始化包围盒
	if (useCamera)
	{
		if (!video.open(0))
		{
			cout << "error: could not start camera capture" << endl;
			return EXIT_FAILURE;
		}
		startFrame = 0;
		endFrame = INT_MAX;
		Mat tmp;
		video >> tmp;


		scaleW = (float)conf.frameWidth / tmp.cols;
		scaleH = (float)conf.frameHeight / tmp.rows;

		box_STRUCK = IntRect(conf.frameWidth / 2 - kLiveBoxWidth / 2, conf.frameHeight / 2 - kLiveBoxHeight / 2, kLiveBoxWidth, kLiveBoxHeight);
		cout << "press 'i' to initialise tracker" << endl;
	}
	else
	{

	}

	Mat result(conf.frameHeight, conf.frameWidth, CV_8UC3);
	bool paused = false; //是否每一帧追踪之后都要暂停（默认为否）
	bool doInitialise = false;
	srand(conf.seed);

	//开始处理
	for (int frameInd = startFrame; frameInd <= endFrame; ++frameInd)
	{
		Mat frame;
		//初始化
		if (useCamera)
		{
			Mat frameOrig;
			video >> frameOrig;
			resize(frameOrig, frame, Size(conf.frameWidth, conf.frameHeight));
			flip(frame, frame, 1);
			frame.copyTo(result);
			if (doInitialise)
			{
				if (tracker.IsInitialised())
				{
					tracker.Reset();
				}
				else
				{
					tracker.Initialise(frame, box_STRUCK);
				}
				doInitialise = false;
			}
			else if (!tracker.IsInitialised())
			{
				rectangle(result, box_STRUCK, CV_RGB(255, 255, 255));
			}
		}
		else
		{
			video >> frame;
			
			if (frame.empty())
			{
				break;
			}			

			if (frameInd == startFrame)
			{
				//如果是第一帧，执行初始化
				tracker.Initialise(frame, box_STRUCK);
			}
		}

		//进行包围盒更新
		if (tracker.IsInitialised())
		{
			//进行计算
			tracker.Track(frame);

			//输出debug信息
			//if (!conf.quietMode && conf.debugMode)
			//{
			//	tracker.Debug();
			//}

			//这里的tracker.GerBB()就是所得到的新的包围盒
			rectangle(frame, tracker.GetBB(), CV_RGB(0, 255, 0));

			if (conf.XYWidthHeight)
			{
				groudtruth >> g_x >> g_y >> g_width >> g_height;
			}
			else
			{
				double point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y;
				groudtruth >> point1x >> point1y >> point2x >> point2y >> point3x >> point3y >> point4x >> point4y;

				Rect2d temp = changeDataStyle(point1x, point1y, point2x, point2y, point3x, point3y, point4x, point4y);
				g_x = temp.x; g_y = temp.y; g_width = temp.width; g_height = temp.height;
			}

			//计算画图用的数据
			calcPrecisionRate(PrecisionData, PrecisionThreshold, g_x, g_y, g_width, g_height, tracker.GetBB());
			calcSuccessRate(SuccessData, SuccessThreshold, g_x, g_y, g_width, g_height, tracker.GetBB());
		}

		if (!conf.quietMode)
		{
			//如果不是安静模式（也就是说，会把追踪结果打印出来，默认），则展示追踪结果
			imshow("Tracking", frame);
			int key = waitKey(paused ? 0 : 1);
			if (key != -1)
			{
				if (key == 27 || key == 113) // esc q
				{
					break;
				}
				else if (key == 112) // p
				{
					paused = !paused;
				}
				else if (key == 105 && useCamera)
				{
					doInitialise = true;
				}
			}
			if (conf.debugMode && frameInd == endFrame)
			{
				cout << "\n\nend of sequence, press any key to exit" << endl;
				waitKey();
			}
		}
	}
	drawPrecisionRate(PrecisionRate, PrecisionData, totalFrameNumber, STRUCK_color);
	drawSuccessRate(SuccessRate, SuccessData, totalFrameNumber, STRUCK_color);

	imshow("PrecisionRate", PrecisionRate);
	imshow("SuccessRate", SuccessRate);

	groudtruth.close();
	video.~VideoCapture();

	cout << "STRUCK 追踪结果已生成" << endl;

	imwrite(precisionRateDiagramSavingPath.c_str(), PrecisionRate);
	imwrite(successRateDiagramSavingPath.c_str(), SuccessRate);
	waitKey(0);

	return EXIT_SUCCESS;
}
