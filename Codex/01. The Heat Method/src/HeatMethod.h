#pragma once

#include "DenseCholesky.h"
#include "DenseMatrix.h"
#include "MathTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace HeatDemo
{
enum class MeshKind
{
    Sphere = 0,
    UFoldedPlane,
    SwissRoll,
    BunnySuzanne
};

enum class VisualizationMode
{
    Distance = 0,
    DistanceBands,
    SoftWeight,
    DisplacedSoftWeight,
    AnalyticError
};

const char* MeshKindName(MeshKind kind);
const char* VisualizationModeName(VisualizationMode mode);

struct HeatDiagnostics
{
    std::string meshName;
    int vertexCount = 0;
    int triangleCount = 0;
    int degenerateTriangleCount = 0;
    bool analyticErrorAvailable = false;
    double meanEdgeLength = 0.0;
    double timestep = 0.0;
    bool heatFactorOk = false;
    bool poissonFactorOk = false;
    bool heatRegularized = false;
    bool poissonRegularized = false;
    double heatRegularization = 0.0;
    double poissonRegularization = 0.0;
    double stiffnessSymmetryError = 0.0;
    double stiffnessRowSumMax = 0.0;
    double heatSymmetryError = 0.0;
    double poissonSymmetryError = 0.0;
    double minHeat = 0.0;
    double maxHeat = 0.0;
    double minPhi = 0.0;
    double maxPhi = 0.0;
    double minWeight = 0.0;
    double maxWeight = 0.0;
    double meanAbsError = 0.0;
    double maxAbsError = 0.0;
    double meanRelativeError = 0.0;
    double maxRelativeError = 0.0;
    std::string status;
};

class HeatMethodSolver
{
public:
    MeshKind meshKind = MeshKind::Sphere;
    int longitudeSegments = 48;
    int latitudeSegments = 24;
    int sourceVertex = 0;
    double timestepScale = 1.0;
    double softRadius = 0.8;
    double falloffPower = 2.0;
    bool useSmoothstep = true;
    double displacementAmount = 0.15;
    VisualizationMode visualizationMode = VisualizationMode::SoftWeight;

    bool RebuildMeshAndFactors();
    bool RefactorMatrices();
    bool RefactorHeatMatrix();
    bool RecomputeDistance();
    void RecomputeWeightsAndVisuals();

    const std::vector<Vec3>& BasePositions() const { return basePositions; }
    const std::vector<Vec3>& DisplayPositions() const { return displayPositions; }
    const std::vector<Vec3>& Normals() const { return normals; }
    const std::vector<Tri>& Triangles() const { return triangles; }
    const std::vector<uint32_t>& Indices() const { return indices; }
    const std::vector<Color>& Colors() const { return colors; }
    const HeatDiagnostics& Diagnostics() const { return diagnostics; }

private:
    std::vector<Vec3> basePositions;
    std::vector<Vec3> displayPositions;
    std::vector<Vec3> normals;
    std::vector<Tri> triangles;
    std::vector<uint32_t> indices;
    std::vector<Color> colors;
    std::vector<double> mass;
    std::vector<double> u;
    std::vector<double> phi;
    std::vector<double> exactPhi;
    std::vector<double> error;
    std::vector<double> weights;
    std::vector<Vec3> faceX;
    DenseMatrix stiffness;
    DenseMatrix heatMatrix;
    DenseMatrix poissonMatrix;
    DenseCholesky heatFactor;
    DenseCholesky poissonFactor;
    HeatDiagnostics diagnostics;

    void GenerateMesh();
    void GenerateSphere();
    void GenerateUFoldedPlane();
    void GenerateSwissRoll();
    void GenerateBunnySuzanne();
    void FinalizeGeneratedMesh(const char* meshName, bool analyticErrorAvailable);
    void CenterAndScaleBasePositions(double targetRadius);
    void ComputeVertexNormals();
    void AddTriangle(int a, int b, int c);
    void AddTriangleOutward(int a, int b, int c);
    void BuildMassAndStiffness();
    void BuildHeatMatrix();
    void BuildPinnedPoissonMatrix();
    void ComputeMeanEdgeLength();
    bool FactorMatrixWithReport(DenseMatrix matrix, DenseCholesky& factor, const char* name, bool heatMatrixKind);
    bool SolveHeat();
    void ComputeFaceVectorField();
    bool SolvePoisson();
    void ComputeAnalyticError();
    void UpdateRanges();
    bool CheckFiniteVector(const std::vector<double>& values, const char* name);
    Color ColorForVertex(int vertexIndex) const;
};
}
