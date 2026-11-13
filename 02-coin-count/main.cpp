// ============================================================
//  硬币识别与计数（Coin Detection & Counting）—— OpenCV C++ 课堂演示
//
//  方法一（主）：Otsu 二值化 -> 开运算 -> 连通域分析 -> 面积过滤 -> 局部 Otsu 精化
//  方法二（对比）：霍夫圆变换 HoughCircles
//
//  为什么要"局部 Otsu 精化"：全局阈值对"偏暗的那枚硬币"欠分割（只切到中心一块，
//  半径算小了）。所以在每个硬币的小 ROI 内单独再 Otsu 一次，把它的真实边界找回来。
//
//  用法：F5 直接跑（默认读 src/coins.png）
//        或命令行指定图片：OpenCVDemo.exe D:/xxx/coins2.png
// ============================================================
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
    std::filesystem::create_directories("result");   // 保证结果图有地方可写
    // ---------------- 1. 读图 ----------------
    vector<string> candidates;
    if (argc > 1) candidates.push_back(argv[1]);
    candidates.push_back("images/coins.png");   // 绝对路径，双击 exe 也能找到
    candidates.push_back("../images/coins.png");   // 从 build/ 里直接运行时用这个

    Mat img;
    string usedPath;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        img = imread(candidates[i], IMREAD_COLOR);
        if (!img.empty()) { usedPath = candidates[i]; break; }
    }
    if (img.empty())
    {
        cout << "[ERROR] cannot read image, tried:" << endl;
        for (size_t i = 0; i < candidates.size(); ++i) cout << "        " << candidates[i] << endl;
        return -1;
    }
    cout << "image : " << usedPath << "  (" << img.cols << " x " << img.rows << ")" << endl;

    Mat gray, blur;
    cvtColor(img, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, blur, Size(5, 5), 0);      // 去掉背景颗粒噪点

    // ================= 方法一：二值化 + 连通域 =================
    // 硬币是"亮"，背景是"暗"，所以用 THRESH_BINARY（亮 -> 白色前景）
    Mat mask;
    double otsuTh = threshold(blur, mask, 0, 255, THRESH_BINARY | THRESH_OTSU);
    cout << "[method 1] global otsu threshold = " << otsuTh << endl;

    // 开运算 = 先腐蚀后膨胀：去掉零散噪点，硬币本身大小基本不变
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(mask, mask, MORPH_OPEN, kernel);

    Mat labels, stats, centroids;
    int nLabels = connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);

    const double imgArea = (double)img.cols * img.rows;
    const double minArea = imgArea * 0.002;    // 约 0.2% 图幅，小于它算噪点
    const double maxArea = imgArea * 0.25;     // 大于它算多个硬币粘连

    Mat res1 = img.clone();
    int count1 = 0;

    for (int i = 1; i < nLabels; ++i)                    // 标签 0 是背景
    {
        int x = stats.at<int>(i, CC_STAT_LEFT);
        int y = stats.at<int>(i, CC_STAT_TOP);
        int w = stats.at<int>(i, CC_STAT_WIDTH);
        int h = stats.at<int>(i, CC_STAT_HEIGHT);
        double area = stats.at<int>(i, CC_STAT_AREA);

        if (area < minArea || area > maxArea) continue;   // 面积不合格 -> 不是硬币

        // ---- 局部 Otsu 精化：在这个硬币的小 ROI 里重新分割一次 ----
        int mx = (int)(w * 0.10), my = (int)(h * 0.10);    // ROI 外扩 10%
        int x0 = max(0, x - mx), y0 = max(0, y - my);
        int x1 = min(img.cols, x + w + mx), y1 = min(img.rows, y + h + my);
        Rect roi(x0, y0, x1 - x0, y1 - y0);

        Mat roiBin;
        threshold(blur(roi), roiBin, 0, 255, THRESH_BINARY | THRESH_OTSU);

        Mat roiLabels, roiStats, roiCents;
        int nLocal = connectedComponentsWithStats(roiBin, roiLabels, roiStats, roiCents, 8, CV_32S);

        // 找出"质心所在"的那一块（万一落在背景上，就取面积最大的那块）
        int cx = (int)centroids.at<double>(i, 0) - x0;
        int cy = (int)centroids.at<double>(i, 1) - y0;
        int lid = 0;
        if (cx >= 0 && cy >= 0 && cx < roi.width && cy < roi.height)
            lid = roiLabels.at<int>(cy, cx);
        if (lid == 0)
        {
            double best = -1;
            for (int k = 1; k < nLocal; ++k)
            {
                double a = roiStats.at<int>(k, CC_STAT_AREA);
                if (a > best) { best = a; lid = k; }
            }
        }
        if (lid == 0) { lid = -1; }        // 精化失败，退回用全局结果

        Mat one = (lid > 0) ? (roiLabels == lid) : (labels == i);
        vector<Point> pts;
        findNonZero(one, pts);
        Point2f c;
        float r;
        minEnclosingCircle(pts, c, r);      // 最小包围圆 -> 硬币的圆心和半径
        if (lid > 0) { c.x += (float)x0; c.y += (float)y0; }

        double areaRef = (lid > 0) ? roiStats.at<int>(lid, CC_STAT_AREA) : area;

        ++count1;
        circle(res1, c, (int)r, Scalar(0, 255, 0), 2);                            // 绿色圆：识别结果
        rectangle(res1, Point(x, y), Point(x + w, y + h), Scalar(255, 160, 0), 1); // 蓝色框：连通域外接框
        putText(res1, to_string(count1), Point((int)c.x - 8, (int)c.y + 8),
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);

        cout << "  coin " << count1
             << "  center=(" << (int)c.x << "," << (int)c.y << ")"
             << "  r=" << (int)r
             << "  area=" << (int)areaRef << endl;
    }

    // ================= 方法二：霍夫圆变换（对比） =================
    // 不依赖前景/背景分割，直接找圆；参数敏感是本方法最大的坑
    vector<Vec3f> circles;
    HoughCircles(blur, circles, HOUGH_GRADIENT,
                 1.0,                 // dp = 1：累加器与原图同分辨率
                 img.cols / 12.0,     // minDist：两个圆心的最小距离，防止一枚硬币检出多个
                 100,                 // param1：Canny 高阈值
                 30,                  // param2：累加器阈值，越小越容易误检，越大越漏检
                 15,                  // minRadius
                 img.cols / 8);       // maxRadius

    Mat res2 = img.clone();
    int count2 = 0;
    for (size_t i = 0; i < circles.size(); ++i)
    {
        Point2f c(circles[i][0], circles[i][1]);
        float r = circles[i][2];
        ++count2;
        circle(res2, c, (int)r, Scalar(0, 255, 255), 2);
        circle(res2, c, 2, Scalar(0, 0, 255), -1);
        putText(res2, to_string(count2), Point((int)c.x - 8, (int)c.y + 8),
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);
        cout << "  hough " << count2 << "  center=(" << (int)c.x << "," << (int)c.y
             << ")  r=" << (int)r << endl;
    }

    // ================= 结果汇总 =================
    cout << "----------------------------------------" << endl;
    cout << "method 1 (threshold + connected components) : " << count1 << endl;
    cout << "method 2 (HoughCircles)                     : " << count2 << endl;
    cout << "FINAL COIN COUNT = " << count1 << endl;
    cout << "----------------------------------------" << endl;

    // cv::putText 只支持 ASCII，中文会变问号
    string label = "Coins = " + to_string(count1) + " (threshold)   |   " + to_string(count2) + " (hough)";
    rectangle(res1, Point(8, 8), Point(8 + 14 * (int)label.size() + 10, 44), Scalar(0, 0, 0), -1);
    putText(res1, label, Point(16, 36), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);

    // ================= 保存 + 显示 =================
    imwrite("result/coins_counted.png", res1);      // 方法一结果
    imwrite("result/coins_hough.png", res2);        // 方法二结果
    imwrite("result/coins_binary.png", mask);       // 二值化掩膜（出问题时先看这张）
    cout << "saved : result/coins_counted.png" << endl;
    cout << "saved : result/coins_hough.png" << endl;
    cout << "saved : result/coins_binary.png" << endl;

    imshow("Method 1: Threshold + Connected Components", res1);
    imshow("Method 2: HoughCircles", res2);
    imshow("Binary Mask", mask);
    waitKey(0);                 // 按任意键退出
    destroyAllWindows();
    return 0;
}