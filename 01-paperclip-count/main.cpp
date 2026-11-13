// ============================================================
//  回形针计数（Paperclip Counting）—— OpenCV C++ 课堂演示
//  思路：灰度 -> Otsu 二值化(取暗色笔画) -> 形态学闭运算
//        -> 连通域分析 -> 按面积/边框过滤 -> 计数 + 标注
//  用法：直接 F5 运行（默认读 src/clips.png）
//        也可以命令行传图片：OpenCVDemo.exe D:/xxx/other.png
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
    // ---------------- 1. 读图（多候选路径，F5 或双击 exe 都能跑） ----------------
    vector<string> candidates;
    if (argc > 1) candidates.push_back(argv[1]);
    candidates.push_back("images/clips.png");   // 绝对路径（双击 exe 也找得到）
    candidates.push_back("../images/clips.png");   // 从 build/ 里直接运行时用这个

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

    // ---------------- 2. 灰度 + Otsu 二值化 ----------------
    // 回形针是浅灰背景上的暗色笔画，所以用 THRESH_BINARY_INV 把"暗"变成"白色前景"
    Mat gray;
    cvtColor(img, gray, COLOR_BGR2GRAY);

    Mat mask;
    double otsuTh = threshold(gray, mask, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);
    cout << "otsu threshold = " << otsuTh << endl;

    // ---------------- 3. 形态学闭运算 ----------------
    // 一根回形针本身就是"一条连贯的钢丝"，天然是一个连通域；
    // 闭运算只是补掉笔画上的小断口，让"一个回形针 = 一个连通域"更稳。
    // 核不能给大，否则挨得近的两个回形针会被粘成一个。
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);

    // ---------------- 4. 连通域分析 ----------------
    Mat labels, stats, centroids;
    int nLabels = connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);

    // ---------------- 5. 过滤：面积太小=噪点，面积太大=粘连，贴边框=图片边缘暗影 --------
    const double imgArea = (double)img.cols * img.rows;
    const double minArea = imgArea * 0.002;    // 约 0.2% 图幅
    const double maxArea = imgArea * 0.30;     // 超过 30% 图幅视为多个粘连或背景

    Mat result = img.clone();
    int count = 0;

    for (int i = 1; i < nLabels; ++i)                    // 标签 0 是背景
    {
        int x = stats.at<int>(i, CC_STAT_LEFT);
        int y = stats.at<int>(i, CC_STAT_TOP);
        int w = stats.at<int>(i, CC_STAT_WIDTH);
        int h = stats.at<int>(i, CC_STAT_HEIGHT);
        double area = stats.at<int>(i, CC_STAT_AREA);

        if (area < minArea || area > maxArea) continue;                                   // 面积不合格
        if (x <= 1 || y <= 1 || x + w >= img.cols - 1 || y + h >= img.rows - 1) continue;  // 贴边框

        // 取出这个连通域的像素点，算最小外接旋转矩形（能看出回形针摆放的角度）
        Mat one = (labels == i);
        vector<Point> pts;
        findNonZero(one, pts);
        RotatedRect rr = minAreaRect(pts);

        // 画：外接正矩形(蓝) + 最小外接旋转矩形(绿) + 编号(红)
        rectangle(result, Point(x, y), Point(x + w, y + h), Scalar(255, 160, 0), 1);
        Point2f v[4];
        rr.points(v);
        for (int k = 0; k < 4; ++k) line(result, v[k], v[(k + 1) % 4], Scalar(0, 200, 0), 1);

        ++count;
        putText(result, to_string(count), Point((int)rr.center.x - 8, (int)rr.center.y + 8),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 0, 255), 2);

        cout << "  clip " << count
             << "  area=" << (int)area
             << "  center=(" << (int)rr.center.x << "," << (int)rr.center.y << ")"
             << "  size=" << (int)rr.size.width << "x" << (int)rr.size.height
             << "  angle=" << rr.angle << endl;
    }

    // ---------------- 6. 结果汇总 ----------------
    cout << "----------------------------------------" << endl;
    cout << "TOTAL PAPERCLIPS = " << count << endl;
    cout << "----------------------------------------" << endl;

    // cv::putText 只支持 ASCII，所以写英文
    string label = "Total = " + to_string(count);
    rectangle(result, Point(8, 8), Point(8 + 18 * (int)label.size() + 10, 44), Scalar(0, 0, 0), -1);
    putText(result, label, Point(16, 36), FONT_HERSHEY_SIMPLEX, 0.9, Scalar(0, 255, 255), 2);

    // ---------------- 7. 保存 + 显示 ----------------
    imwrite("result/clips_counted.png", result);
    imwrite("result/clips_binary.png", mask);
    cout << "saved : result/clips_counted.png" << endl;
    cout << "saved : result/clips_binary.png" << endl;

    imshow("Paperclip Counting", result);
    imshow("Binary Mask", mask);
    waitKey(0);                 // 按任意键关闭窗口
    destroyAllWindows();
    return 0;
}