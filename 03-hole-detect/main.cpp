// ============================================================
//  圆孔识别（只保留"完整的圆"）—— OpenCV C++ 课堂演示
//
//  场景：轮毂/法兰盘的 X 光片，图里有小螺栓孔、被画面边缘切掉一半的孔、
//        还有辐条弧形槽、中心大孔——形状都不是"完整的圆"。
//
//  三步走：
//    1) HoughCircles 尽量多召回候选圆（参数放松，宁可多检）
//    2) 两个过滤器筛掉不合格的：
//         a. 边框检查：圆只要有一部分超出画面 -> 丢掉（被切掉的孔不算）
//         b. 完整度检查：沿圆周采样 180 个点，统计"真正有边缘"的比例，
//            完整圆 ~100%，圆弧/弧槽只有 20~40% -> 丢掉
//    3) 通过检查的才算一个"完整的圆孔"
//
//  用法：F5 直接跑（默认读 src/holes.png）
//        或命令行指定图片：OpenCVDemo.exe D:/xxx/holes2.png
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
    candidates.push_back("images/holes.png");
    candidates.push_back("../images/holes.png");   // 从 build/ 里直接运行时用这个

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
    GaussianBlur(gray, blur, Size(7, 7), 1.5);      // 压掉金属表面的加工纹理

    // ---------------- 2. 梯度幅值图（后面用来判断"圆周上到底有没有边缘"） --------
    Mat gx, gy, mag;
    Sobel(blur, gx, CV_32F, 1, 0, 3);
    Sobel(blur, gy, CV_32F, 0, 1, 3);
    magnitude(gx, gy, mag);

    // ---------------- 3. HoughCircles 多召回候选 ----------------
    // param2 故意给小（18）：先把可疑的圆全捞出来，靠后面的检查去伪，比直接漏检安全
    vector<Vec3f> circles;
    HoughCircles(blur, circles, HOUGH_GRADIENT,
                 1.0,          // dp
                 25,           // minDist：圆心之间的最小距离
                 100,          // param1：Canny 高阈值
                 18,           // param2：累加器阈值（小 -> 多召回）
                 8,            // minRadius：小孔下限
                 90);          // maxRadius：上限（排除中心大孔的误检）
    cout << "candidates from HoughCircles : " << circles.size() << endl;

    // ---------------- 4. 两个过滤器：贴边框 / 圆周不完整 ----------------
    const int    SAMPLES      = 180;     // 沿圆周采样点数
    const double MIN_COVERAGE = 0.90;    // 圆周上有边缘的比例 >= 90% 才算完整的圆
    const double EDGE_TH      = 40.0;    // 梯度幅值阈值，>它认为"这里有边缘"

    Mat result = img.clone();
    int count = 0, rejected = 0;

    for (size_t i = 0; i < circles.size(); ++i)
    {
        Point2f c(circles[i][0], circles[i][1]);
        float r = circles[i][2];

        // (a) 边框检查：只要有 1 个像素超出画面范围，就说明圆被切掉了
        bool fullyInside = (c.x - r >= 0 && c.y - r >= 0 &&
                            c.x + r <= img.cols - 1 && c.y + r <= img.rows - 1);

        // (b) 完整度检查：沿圆周找边缘。半径允许 ±2px 抖动，抵消像素量化误差
        int hit = 0;
        for (int k = 0; k < SAMPLES; ++k)
        {
            double ang = 2.0 * CV_PI * k / SAMPLES;
            double ca = cos(ang), sa = sin(ang);
            float best = 0.f;
            for (int d = -2; d <= 2; ++d)
            {
                int px = (int)lround(c.x + (r + d) * ca);
                int py = (int)lround(c.y + (r + d) * sa);
                if (px >= 0 && py >= 0 && px < img.cols && py < img.rows)
                    best = max(best, mag.at<float>(py, px));
            }
            if (best > EDGE_TH) ++hit;
        }
        double coverage = (double)hit / SAMPLES;

        bool ok = fullyInside && coverage >= MIN_COVERAGE;
        if (!ok)
        {
            ++rejected;
            circle(result, c, (int)r, Scalar(0, 0, 255), 1);                        // 红色细圈 = 被淘汰
            putText(result, "x", Point((int)c.x - 5, (int)c.y + 5),
                    FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
            cout << "  REJECT (" << (int)c.x << "," << (int)c.y << ") r=" << (int)r
                 << "  coverage=" << (int)(coverage * 100) << "%"
                 << (fullyInside ? "  -> circle not complete" : "  -> cut by image border") << endl;
            continue;
        }

        ++count;
        circle(result, c, (int)r, Scalar(0, 255, 0), 2);                            // 绿色 = 完整的圆
        circle(result, c, 2, Scalar(0, 0, 255), -1);
        putText(result, to_string(count), Point((int)c.x - 8, (int)c.y + 8),
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);

        cout << "  hole " << count << "  center=(" << (int)c.x << "," << (int)c.y << ")"
             << "  r=" << (int)r << "  coverage=" << (int)(coverage * 100) << "%" << endl;
    }

    // ---------------- 5. 结果汇总 ----------------
    cout << "----------------------------------------" << endl;
    cout << "complete circular holes = " << count << endl;
    cout << "rejected candidates    = " << rejected << "  (cut by border / not a full circle)" << endl;
    cout << "----------------------------------------" << endl;

    // cv::putText 只支持 ASCII
    string label = "Complete holes = " + to_string(count) + "   (rejected " + to_string(rejected) + ")";
    rectangle(result, Point(8, 8), Point(8 + 15 * (int)label.size() + 10, 44), Scalar(0, 0, 0), -1);
    putText(result, label, Point(16, 36), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);

    // ---------------- 6. 保存 + 显示 ----------------
    Mat magShow;
    mag.convertTo(magShow, CV_8U, 255.0 / 900.0);        // 梯度图归一化到 0-255，方便肉眼看
    imwrite("result/holes_detected.png", result);
    imwrite("result/holes_gradient.png", magShow);
    cout << "saved : result/holes_detected.png" << endl;
    cout << "saved : result/holes_gradient.png" << endl;

    imshow("Complete Circular Holes", result);
    imshow("Gradient Magnitude", magShow);
    waitKey(0);                 // 按任意键退出
    destroyAllWindows();
    return 0;
}