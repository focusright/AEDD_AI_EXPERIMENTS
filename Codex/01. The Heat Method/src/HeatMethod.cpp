#include "HeatMethod.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace HeatDemo
{
namespace
{
double TriangleArea(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
    return 0.5 * Length(Cross(p1 - p0, p2 - p0));
}

double CotangentAt(const Vec3& center, const Vec3& a, const Vec3& b)
{
    const Vec3 va = a - center;
    const Vec3 vb = b - center;
    const double denominator = Length(Cross(va, vb));
    if (denominator <= 1e-15) {
        return 0.0;
    }
    return Dot(va, vb) / denominator;
}

void AddEdge(DenseMatrix& K, int i, int j, double w)
{
    K.At(i, i) += w;
    K.At(j, j) += w;
    K.At(i, j) -= w;
    K.At(j, i) -= w;
}

template <typename T>
void MinMax(const std::vector<T>& values, double& minValue, double& maxValue)
{
    if (values.empty()) {
        minValue = 0.0;
        maxValue = 0.0;
        return;
    }

    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    minValue = static_cast<double>(*minIt);
    maxValue = static_cast<double>(*maxIt);
}

Color HeatRamp(double t)
{
    const Color warm = {1.0f, 0.72f, 0.18f, 1.0f};
    const Color mid = {0.22f, 0.82f, 0.94f, 1.0f};
    const Color cool = {0.04f, 0.08f, 0.26f, 1.0f};
    if (t < 0.5) {
        return Lerp(warm, mid, t * 2.0);
    }
    return Lerp(mid, cool, (t - 0.5) * 2.0);
}

Color WeightRamp(double w)
{
    const Color cold = {0.09f, 0.11f, 0.14f, 1.0f};
    const Color blue = {0.12f, 0.42f, 0.75f, 1.0f};
    const Color hot = {1.0f, 0.62f, 0.16f, 1.0f};
    if (w < 0.45) {
        return Lerp(cold, blue, w / 0.45);
    }
    return Lerp(blue, hot, (w - 0.45) / 0.55);
}
}

const char* VisualizationModeName(VisualizationMode mode)
{
    switch (mode) {
    case VisualizationMode::Distance: return "Distance";
    case VisualizationMode::DistanceBands: return "Distance Bands";
    case VisualizationMode::SoftWeight: return "Soft Weight";
    case VisualizationMode::DisplacedSoftWeight: return "Displaced Soft Weight";
    case VisualizationMode::AnalyticError: return "Analytic Error";
    default: return "Unknown";
    }
}

bool HeatMethodSolver::RebuildMeshAndFactors()
{
    GenerateSphere();
    BuildMassAndStiffness();
    ComputeMeanEdgeLength();
    BuildHeatMatrix();
    BuildPinnedPoissonMatrix();

    diagnostics.heatFactorOk = FactorMatrixWithReport(heatMatrix, heatFactor, "heat", true);
    diagnostics.poissonFactorOk = FactorMatrixWithReport(poissonMatrix, poissonFactor, "pinned Poisson", false);
    if (!diagnostics.heatFactorOk || !diagnostics.poissonFactorOk) {
        RecomputeWeightsAndVisuals();
        return false;
    }

    return RecomputeDistance();
}

bool HeatMethodSolver::RefactorMatrices()
{
    BuildHeatMatrix();
    BuildPinnedPoissonMatrix();
    diagnostics.heatFactorOk = FactorMatrixWithReport(heatMatrix, heatFactor, "heat", true);
    diagnostics.poissonFactorOk = FactorMatrixWithReport(poissonMatrix, poissonFactor, "pinned Poisson", false);
    if (!diagnostics.heatFactorOk || !diagnostics.poissonFactorOk) {
        return false;
    }
    return RecomputeDistance();
}

bool HeatMethodSolver::RefactorHeatMatrix()
{
    BuildHeatMatrix();
    diagnostics.heatFactorOk = FactorMatrixWithReport(heatMatrix, heatFactor, "heat", true);
    if (!diagnostics.heatFactorOk) {
        return false;
    }
    return RecomputeDistance();
}

void HeatMethodSolver::GenerateSphere()
{
    longitudeSegments = std::max(12, std::min(96, longitudeSegments));
    latitudeSegments = std::max(6, std::min(48, latitudeSegments));

    basePositions.clear();
    normals.clear();
    triangles.clear();
    indices.clear();

    basePositions.push_back({0.0, 1.0, 0.0});
    normals.push_back({0.0, 1.0, 0.0});

    for (int lat = 1; lat < latitudeSegments; ++lat) {
        const double theta = Pi * static_cast<double>(lat) / static_cast<double>(latitudeSegments);
        const double y = std::cos(theta);
        const double ringRadius = std::sin(theta);
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const double angle = 2.0 * Pi * static_cast<double>(lon) / static_cast<double>(longitudeSegments);
            const Vec3 p = {ringRadius * std::cos(angle), y, ringRadius * std::sin(angle)};
            basePositions.push_back(p);
            normals.push_back(Normalize(p));
        }
    }

    const int bottom = static_cast<int>(basePositions.size());
    basePositions.push_back({0.0, -1.0, 0.0});
    normals.push_back({0.0, -1.0, 0.0});

    auto ringIndex = [this](int lat, int lon) {
        const int wrappedLon = (lon + longitudeSegments) % longitudeSegments;
        return 1 + (lat - 1) * longitudeSegments + wrappedLon;
    };

    for (int lon = 0; lon < longitudeSegments; ++lon) {
        AddTriangleOutward(0, ringIndex(1, lon + 1), ringIndex(1, lon));
    }

    for (int lat = 1; lat < latitudeSegments - 1; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const int a = ringIndex(lat, lon);
            const int b = ringIndex(lat, lon + 1);
            const int c = ringIndex(lat + 1, lon);
            const int d = ringIndex(lat + 1, lon + 1);
            AddTriangleOutward(a, b, c);
            AddTriangleOutward(b, d, c);
        }
    }

    const int lastRing = latitudeSegments - 1;
    for (int lon = 0; lon < longitudeSegments; ++lon) {
        AddTriangleOutward(bottom, ringIndex(lastRing, lon), ringIndex(lastRing, lon + 1));
    }

    indices.reserve(triangles.size() * 3u);
    for (const Tri& tri : triangles) {
        indices.push_back(static_cast<uint32_t>(tri.a));
        indices.push_back(static_cast<uint32_t>(tri.b));
        indices.push_back(static_cast<uint32_t>(tri.c));
    }

    const size_t n = basePositions.size();
    displayPositions = basePositions;
    colors.assign(n, {0.8f, 0.8f, 0.8f, 1.0f});
    u.assign(n, 0.0);
    phi.assign(n, 0.0);
    exactPhi.assign(n, 0.0);
    error.assign(n, 0.0);
    weights.assign(n, 0.0);
    faceX.assign(triangles.size(), {});
    sourceVertex = std::max(0, std::min(sourceVertex, static_cast<int>(n) - 1));

    diagnostics = {};
    diagnostics.vertexCount = static_cast<int>(n);
    diagnostics.triangleCount = static_cast<int>(triangles.size());
    diagnostics.status = "Sphere mesh generated.";
}

void HeatMethodSolver::AddTriangleOutward(int a, int b, int c)
{
    const Vec3& p0 = basePositions[static_cast<size_t>(a)];
    const Vec3& p1 = basePositions[static_cast<size_t>(b)];
    const Vec3& p2 = basePositions[static_cast<size_t>(c)];
    const Vec3 faceNormal = Cross(p1 - p0, p2 - p0);
    const Vec3 centroid = (p0 + p1 + p2) / 3.0;
    if (Dot(faceNormal, centroid) < 0.0) {
        std::swap(b, c);
    }
    triangles.push_back({a, b, c});
}

void HeatMethodSolver::BuildMassAndStiffness()
{
    const int n = static_cast<int>(basePositions.size());
    stiffness.Resize(n);
    stiffness.SetZero();
    mass.assign(static_cast<size_t>(n), 0.0);
    diagnostics.degenerateTriangleCount = 0;

    for (const Tri& tri : triangles) {
        const Vec3& p0 = basePositions[static_cast<size_t>(tri.a)];
        const Vec3& p1 = basePositions[static_cast<size_t>(tri.b)];
        const Vec3& p2 = basePositions[static_cast<size_t>(tri.c)];
        const double area = TriangleArea(p0, p1, p2);
        if (area <= 1e-14) {
            ++diagnostics.degenerateTriangleCount;
            continue;
        }

        mass[static_cast<size_t>(tri.a)] += area / 3.0;
        mass[static_cast<size_t>(tri.b)] += area / 3.0;
        mass[static_cast<size_t>(tri.c)] += area / 3.0;

        const double cot0 = CotangentAt(p0, p1, p2);
        const double cot1 = CotangentAt(p1, p2, p0);
        const double cot2 = CotangentAt(p2, p0, p1);

        AddEdge(stiffness, tri.b, tri.c, 0.5 * cot0);
        AddEdge(stiffness, tri.c, tri.a, 0.5 * cot1);
        AddEdge(stiffness, tri.a, tri.b, 0.5 * cot2);
    }

    diagnostics.stiffnessSymmetryError = stiffness.MaxSymmetryError();
    diagnostics.stiffnessRowSumMax = stiffness.MaxAbsRowSum();
}

void HeatMethodSolver::ComputeMeanEdgeLength()
{
    std::set<uint64_t> uniqueEdges;
    for (const Tri& tri : triangles) {
        uniqueEdges.insert(EdgeKey(tri.a, tri.b));
        uniqueEdges.insert(EdgeKey(tri.b, tri.c));
        uniqueEdges.insert(EdgeKey(tri.c, tri.a));
    }

    double sum = 0.0;
    for (uint64_t key : uniqueEdges) {
        const int a = static_cast<int>(key >> 32u);
        const int b = static_cast<int>(key & 0xffffffffu);
        sum += Length(basePositions[static_cast<size_t>(a)] - basePositions[static_cast<size_t>(b)]);
    }

    diagnostics.meanEdgeLength = uniqueEdges.empty() ? 0.0 : sum / static_cast<double>(uniqueEdges.size());
    diagnostics.timestep = timestepScale * diagnostics.meanEdgeLength * diagnostics.meanEdgeLength;
}

void HeatMethodSolver::BuildHeatMatrix()
{
    ComputeMeanEdgeLength();
    const int n = stiffness.n;
    heatMatrix.Resize(n);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            heatMatrix.At(r, c) = diagnostics.timestep * stiffness.At(r, c);
        }
        heatMatrix.At(r, r) += mass[static_cast<size_t>(r)];
    }
    diagnostics.heatSymmetryError = heatMatrix.MaxSymmetryError();
}

void HeatMethodSolver::BuildPinnedPoissonMatrix()
{
    const int n = stiffness.n;
    poissonMatrix = stiffness;
    constexpr int pin = 0;
    for (int i = 0; i < n; ++i) {
        poissonMatrix.At(pin, i) = 0.0;
        poissonMatrix.At(i, pin) = 0.0;
    }
    poissonMatrix.At(pin, pin) = 1.0;
    diagnostics.poissonSymmetryError = poissonMatrix.MaxSymmetryError();
}

bool HeatMethodSolver::FactorMatrixWithReport(DenseMatrix matrix, DenseCholesky& factor, const char* name, bool heatMatrixKind)
{
    if (heatMatrixKind) {
        diagnostics.heatRegularized = false;
        diagnostics.heatRegularization = 0.0;
    } else {
        diagnostics.poissonRegularized = false;
        diagnostics.poissonRegularization = 0.0;
    }

    if (factor.Factor(matrix)) {
        return true;
    }

    constexpr double regularization = 1e-10;
    for (int i = 0; i < matrix.n; ++i) {
        matrix.At(i, i) += regularization;
    }

    if (factor.Factor(matrix)) {
        if (heatMatrixKind) {
            diagnostics.heatRegularized = true;
            diagnostics.heatRegularization = regularization;
        } else {
            diagnostics.poissonRegularized = true;
            diagnostics.poissonRegularization = regularization;
        }

        std::ostringstream stream;
        stream << name << " matrix needed diagonal regularization " << regularization << ".";
        diagnostics.status = stream.str();
        return true;
    }

    std::ostringstream stream;
    stream << name << " Cholesky failed: " << factor.LastError();
    diagnostics.status = stream.str();
    return false;
}

bool HeatMethodSolver::RecomputeDistance()
{
    sourceVertex = std::max(0, std::min(sourceVertex, static_cast<int>(basePositions.size()) - 1));

    if (!diagnostics.heatFactorOk || !diagnostics.poissonFactorOk) {
        diagnostics.status = "Cannot recompute: one or both matrix factors are invalid.";
        return false;
    }

    if (!SolveHeat()) {
        return false;
    }

    ComputeFaceVectorField();

    if (!SolvePoisson()) {
        return false;
    }

    ComputeAnalyticError();
    RecomputeWeightsAndVisuals();
    diagnostics.status = "Distance recomputed with existing Cholesky factors.";
    return true;
}

bool HeatMethodSolver::SolveHeat()
{
    const int n = static_cast<int>(basePositions.size());
    std::vector<double> rhs(static_cast<size_t>(n), 0.0);
    rhs[static_cast<size_t>(sourceVertex)] = 1.0;

    if (!heatFactor.Solve(rhs, u)) {
        diagnostics.status = "Heat solve failed.";
        return false;
    }

    if (!CheckFiniteVector(u, "heat u")) {
        return false;
    }

    MinMax(u, diagnostics.minHeat, diagnostics.maxHeat);
    return true;
}

void HeatMethodSolver::ComputeFaceVectorField()
{
    faceX.assign(triangles.size(), {});
    for (size_t faceIndex = 0; faceIndex < triangles.size(); ++faceIndex) {
        const Tri& tri = triangles[faceIndex];
        const Vec3& p0 = basePositions[static_cast<size_t>(tri.a)];
        const Vec3& p1 = basePositions[static_cast<size_t>(tri.b)];
        const Vec3& p2 = basePositions[static_cast<size_t>(tri.c)];
        const Vec3 cross = Cross(p1 - p0, p2 - p0);
        const double doubleArea = Length(cross);
        if (doubleArea <= 1e-14) {
            faceX[faceIndex] = {};
            continue;
        }

        const Vec3 normal = cross / doubleArea;
        const double area = 0.5 * doubleArea;
        const Vec3 e0 = p2 - p1;
        const Vec3 e1 = p0 - p2;
        const Vec3 e2 = p1 - p0;

        const double u0 = u[static_cast<size_t>(tri.a)];
        const double u1 = u[static_cast<size_t>(tri.b)];
        const double u2 = u[static_cast<size_t>(tri.c)];

        const Vec3 grad = (u0 * Cross(normal, e0) +
                           u1 * Cross(normal, e1) +
                           u2 * Cross(normal, e2)) / (2.0 * area);

        const double len = Length(grad);
        if (len > 1e-12 && IsFinite(grad)) {
            faceX[faceIndex] = (-1.0 / len) * grad;
        } else {
            faceX[faceIndex] = {};
        }
    }
}

bool HeatMethodSolver::SolvePoisson()
{
    const int n = static_cast<int>(basePositions.size());
    std::vector<double> rhs(static_cast<size_t>(n), 0.0);

    for (size_t faceIndex = 0; faceIndex < triangles.size(); ++faceIndex) {
        const Tri& tri = triangles[faceIndex];
        const Vec3& p0 = basePositions[static_cast<size_t>(tri.a)];
        const Vec3& p1 = basePositions[static_cast<size_t>(tri.b)];
        const Vec3& p2 = basePositions[static_cast<size_t>(tri.c)];
        const Vec3 cross = Cross(p1 - p0, p2 - p0);
        const double doubleArea = Length(cross);
        if (doubleArea <= 1e-14) {
            continue;
        }

        const Vec3 normal = cross / doubleArea;
        const double area = 0.5 * doubleArea;
        const Vec3 e0 = p2 - p1;
        const Vec3 e1 = p0 - p2;
        const Vec3 e2 = p1 - p0;
        const Vec3 gradLambda0 = Cross(normal, e0) / (2.0 * area);
        const Vec3 gradLambda1 = Cross(normal, e1) / (2.0 * area);
        const Vec3 gradLambda2 = Cross(normal, e2) / (2.0 * area);
        const Vec3 X = faceX[faceIndex];

        rhs[static_cast<size_t>(tri.a)] += area * Dot(X, gradLambda0);
        rhs[static_cast<size_t>(tri.b)] += area * Dot(X, gradLambda1);
        rhs[static_cast<size_t>(tri.c)] += area * Dot(X, gradLambda2);
    }

    rhs[0] = 0.0;
    if (!poissonFactor.Solve(rhs, phi)) {
        diagnostics.status = "Poisson solve failed.";
        return false;
    }

    if (!CheckFiniteVector(phi, "distance phi")) {
        return false;
    }

    const double minPhi = *std::min_element(phi.begin(), phi.end());
    for (double& value : phi) {
        value -= minPhi;
    }

    MinMax(phi, diagnostics.minPhi, diagnostics.maxPhi);
    return true;
}

void HeatMethodSolver::ComputeAnalyticError()
{
    if (basePositions.empty()) {
        return;
    }

    const Vec3 source = Normalize(basePositions[static_cast<size_t>(sourceVertex)]);
    double sumError = 0.0;
    double maxError = 0.0;
    for (size_t i = 0; i < basePositions.size(); ++i) {
        const Vec3 p = Normalize(basePositions[i]);
        const double exact = std::acos(Clamp(Dot(source, p), -1.0, 1.0));
        exactPhi[i] = exact;
        error[i] = std::abs(phi[i] - exact);
        sumError += error[i];
        maxError = std::max(maxError, error[i]);
    }

    diagnostics.meanAbsError = sumError / static_cast<double>(basePositions.size());
    diagnostics.maxAbsError = maxError;
    diagnostics.meanRelativeError = diagnostics.meanAbsError / Pi;
    diagnostics.maxRelativeError = diagnostics.maxAbsError / Pi;
}

void HeatMethodSolver::RecomputeWeightsAndVisuals()
{
    const double radius = std::max(1e-6, softRadius);
    const double power = std::max(0.05, falloffPower);
    displayPositions = basePositions;

    for (size_t i = 0; i < basePositions.size(); ++i) {
        double w = Clamp01(1.0 - phi[i] / radius);
        if (useSmoothstep) {
            w = w * w * (3.0 - 2.0 * w);
        }
        weights[i] = std::pow(w, power);
    }

    for (size_t i = 0; i < basePositions.size(); ++i) {
        if (visualizationMode == VisualizationMode::DisplacedSoftWeight) {
            displayPositions[i] = basePositions[i] + normals[i] * weights[i] * displacementAmount;
        }
        colors[i] = ColorForVertex(static_cast<int>(i));
    }

    if (sourceVertex >= 0 && sourceVertex < static_cast<int>(colors.size())) {
        colors[static_cast<size_t>(sourceVertex)] = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    UpdateRanges();
}

void HeatMethodSolver::UpdateRanges()
{
    MinMax(u, diagnostics.minHeat, diagnostics.maxHeat);
    MinMax(phi, diagnostics.minPhi, diagnostics.maxPhi);
    MinMax(weights, diagnostics.minWeight, diagnostics.maxWeight);
}

bool HeatMethodSolver::CheckFiniteVector(const std::vector<double>& values, const char* name)
{
    for (size_t i = 0; i < values.size(); ++i) {
        if (!IsFinite(values[i])) {
            std::ostringstream stream;
            stream << "Non-finite " << name << " at vertex " << i << ".";
            diagnostics.status = stream.str();
            return false;
        }
    }
    return true;
}

Color HeatMethodSolver::ColorForVertex(int vertexIndex) const
{
    const size_t i = static_cast<size_t>(vertexIndex);
    switch (visualizationMode) {
    case VisualizationMode::Distance: {
        const double t = diagnostics.maxPhi > 1e-12 ? phi[i] / diagnostics.maxPhi : 0.0;
        return HeatRamp(t);
    }
    case VisualizationMode::DistanceBands: {
        const double t = diagnostics.maxPhi > 1e-12 ? phi[i] / diagnostics.maxPhi : 0.0;
        const double bands = 0.5 + 0.5 * std::cos(phi[i] * 28.0);
        Color base = HeatRamp(t);
        const double shade = 0.55 + 0.45 * bands;
        return SaturatedColor(base.r * shade, base.g * shade, base.b * shade, 1.0);
    }
    case VisualizationMode::SoftWeight:
    case VisualizationMode::DisplacedSoftWeight:
        return WeightRamp(weights[i]);
    case VisualizationMode::AnalyticError: {
        const double maxError = std::max(1e-12, diagnostics.maxAbsError);
        const double t = Clamp01(error[i] / maxError);
        const Color low = {0.02f, 0.08f, 0.22f, 1.0f};
        const Color high = {1.0f, 0.12f, 0.04f, 1.0f};
        return Lerp(low, high, t);
    }
    default:
        return {0.7f, 0.7f, 0.7f, 1.0f};
    }
}
}
