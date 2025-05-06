#include "FisheyeCameraModel.h"

#include "Utils/Eigen.h"

FFisheyeCameraModel::FFisheyeCameraModel(const int& InWidth, const int& InHeight, const FVector2d InFocalLength,
    const FVector2d InPrincipalPoint, const double& InK1, const double& InK2, const double& InK3, const double& InK4)
    : Width(InWidth)
    , Height(InHeight)
    , FocalLength(InFocalLength)
    , PrincipalPoint(InPrincipalPoint)
    , K1(InK1)
    , K2(InK2)
    , K3(InK3)
    , K4(InK4)
{
}

FFisheyeCameraModel::~FFisheyeCameraModel()
{
}

TArray<FVector> FFisheyeCameraModel::GenerateDirectionMapping()
{
    // direction in camera frame z=1
    TArray<FVector> RayDirectionMap;
    RayDirectionMap.SetNum(Width * Height);

    for (int j = 0; j < Height; j++)
    {
        for (int i = 0; i < Width; i++)
        {
            const int index = i + j * Width;
            FVector2d Ray = ImageToWorld(i, j);
            // xsim camera RDF(DirectionRay.X, DirectionRay.Y, 1) --> unreal FRU
            FVector Direction(1, Ray.X, -Ray.Y);
            RayDirectionMap[index] = Direction;
        }
    }
    return RayDirectionMap;
}

void FFisheyeCameraModel::Distort(double u, double v, double* du, double* dv) const
{
    const double r = std::sqrt(u * u + v * v);
    constexpr double kThreshold = 1e-6;

    if (r > kThreshold)
    {
        const double theta = std::atan(r);
        const double theta2 = theta * theta;
        const double theta4 = theta2 * theta2;
        const double theta6 = theta4 * theta2;
        const double theta8 = theta4 * theta4;
        const double thetad = theta * (1.0 + K1 * theta2 + K2 * theta4 + K3 * theta6 + K4 * theta8);
        const double ratio = thetad / r;
        *du = u * ratio - u;
        *dv = v * ratio - v;
    }
    else
    {
        *du = 0;
        *dv = 0;
    }
}

void FFisheyeCameraModel::IterativeUndistort(const double du, const double dv, double* u, double* v) const
{
    const size_t kNumIterations = 100;
    const double kMaxStepSquaredNorm = 1e-10;
    const double kRelStepSize = 1e-6;

    Eigen::Matrix2d J;
    const Eigen::Vector2d x0(du, dv);
    Eigen::Vector2d x(du, dv);
    Eigen::Vector2d dx;
    Eigen::Vector2d dx_0b;
    Eigen::Vector2d dx_0f;
    Eigen::Vector2d dx_1b;
    Eigen::Vector2d dx_1f;

    for (size_t i = 0; i < kNumIterations; ++i)
    {
        const double step0 = std::max(std::numeric_limits<double>::epsilon(), std::abs(kRelStepSize * x(0)));
        const double step1 = std::max(std::numeric_limits<double>::epsilon(), std::abs(kRelStepSize * x(1)));
        Distort(x(0), x(1), &dx(0), &dx(1));
        Distort(x(0) - step0, x(1), &dx_0b(0), &dx_0b(1));
        Distort(x(0) + step0, x(1), &dx_0f(0), &dx_0f(1));
        Distort(x(0), x(1) - step1, &dx_1b(0), &dx_1b(1));
        Distort(x(0), x(1) + step1, &dx_1f(0), &dx_1f(1));
        J(0, 0) = 1 + (dx_0f(0) - dx_0b(0)) / (2 * step0);
        J(0, 1) = (dx_1f(0) - dx_1b(0)) / (2 * step1);
        J(1, 0) = (dx_0f(1) - dx_0b(1)) / (2 * step0);
        J(1, 1) = 1 + (dx_1f(1) - dx_1b(1)) / (2 * step1);
        const Eigen::Vector2d step_x = J.inverse() * (x + dx - x0);
        x -= step_x;
        if (step_x.squaredNorm() < kMaxStepSquaredNorm)
        {
            break;
        }
    }

    *u = x(0);
    *v = x(1);
}

FVector2d FFisheyeCameraModel::ImageToWorld(int i, int j)
{
    double normalized_u = (i - PrincipalPoint.X) / FocalLength.X;
    double normalized_v = (j - PrincipalPoint.Y) / FocalLength.Y;

    //double out_u, out_v;
    //IterativeUndistort(normalized_u, normalized_v, &out_u, &out_v);
    return FVector2d(normalized_u, normalized_v);



//    // 1. 归一化像素坐标（消除主点偏移）
//    double xn = (i - cam.cx) / cam.fx;
//    double yn = (j - cam.cy) / cam.fy;
//    // 2. 计算方位角phi（弧度）
//    phi = std::atan2(yn, xn);
//    // 3. 计算极坐标半径r
//    double r = std::sqrt(xn * xn + yn * yn);
//    // 4. 立体投影模型反解入射角theta（弧度）
//    theta = 2.0 * std::atan(r / (2.0 * cam.f));
//    // 5. 生成射线方向向量
//    double sin_theta = std::sin(theta);
//    ray_dir = {
//        sin_theta * std::cos(phi),  // X分量
//        sin_theta * std::sin(phi),  // Y分量
//        std::cos(theta)             // Z分量
//    };
}
